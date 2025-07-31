#include "tcp_server.hpp"
#include "request_handlers.hpp"

#include <unordered_map>
#include <memory>
#include <random>

#include "server_bbox.hpp"
#include "metadata_parser.hpp"

#include <thread>

// BBox 버퍼링 관련 전역 변수
std::atomic<int> bbox_buffer_delay_ms(2400);   // 기본값 1초
std::atomic<int> bbox_send_interval_ms(50);   // 기본값 200ms
std::queue<TimestampedBBox> bbox_buffer;
std::mutex bbox_buffer_mutex;

// ==================== 유틸리티 함수들 ====================

SSL* setup_ssl_connection(int client_socket) {
    SSL* ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        ERR_print_errors_fp(stderr);
        close(client_socket);
        return nullptr;
    }

    SSL_set_fd(ssl, client_socket);
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(client_socket);
        return nullptr;
    }

    printNowTimeKST();
    cout << " [Thread " << std::this_thread::get_id()
         << "] SSL 클라이언트 처리 시작." << endl;
    return ssl;
}

void initialize_database_tables(SQLite::Database& db) {
    create_table_detections(db);
    create_table_lines(db);
    create_table_baseLines(db);
    create_table_verticalLineEquations(db);
    create_table_accounts(db);
    create_table_recovery_codes(db);
}

bool receive_json_message(SSL* ssl, json& received_json) {
    uint32_t net_len;
    if (!recvAll(ssl, reinterpret_cast<char*>(&net_len), sizeof(net_len))) {
        return false;
    }

    uint32_t json_len = ntohl(net_len);
    if (json_len == 0) {
        cerr << "[Thread " << std::this_thread::get_id()
             << "] 비정상적인 데이터 길이 수신: " << json_len << endl;
        return false;
    }

    vector<char> json_buffer(json_len);
    if (!recvAll(ssl, json_buffer.data(), json_len)) {
        return false;
    }

    received_json = json::parse(json_buffer);
    return true;
}

void secure_clear_password(string& passwd) {
    if (!passwd.empty()) {
        memset(&passwd[0], 0, passwd.length());
        passwd.clear();
    }
}

// ==================== 요청 라우터 ====================

void route_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex, 
                   std::atomic<bool>& bbox_push_enabled, std::thread& push_thread, std::thread& metadata_thread) {
    int request_id = received_json.value("request_id", -1);
    
    switch (request_id) {
        case 1:
            handle_detection_request(ssl, received_json, db, db_mutex);
            break;
        case 2:
            handle_line_insert_request(ssl, received_json, db, db_mutex);
            break;
        case 3:
            handle_line_select_all_request(ssl, received_json, db, db_mutex);
            break;
        case 4:
            handle_line_delete_all_request(ssl, received_json, db, db_mutex);
            break;
        case 5:
            handle_baseline_insert_request(ssl, received_json, db, db_mutex);
            break;
        case 6:
            handle_vertical_line_insert_request(ssl, received_json, db, db_mutex);
            break;
        case 7:
            handle_baseline_select_all_request(ssl, received_json, db, db_mutex);
            break;
        case 8:
            handle_login_step1_request(ssl, received_json, db, db_mutex);
            break;
        case 9:
            handle_signup_request(ssl, received_json, db, db_mutex);
            break;
        case 22:
            handle_login_step2_request(ssl, received_json, db, db_mutex);
            break;
        case 31:
            handle_bbox_start_request(ssl, bbox_push_enabled, push_thread, metadata_thread);
            break;
        case 32:
            handle_bbox_stop_request(ssl, bbox_push_enabled, push_thread, metadata_thread);
            break;
        default:
            cout << "[에러] 알 수 없는 request_id: " << request_id << endl;
            break;
    }
}

void cleanup_client_connection(SSL* ssl, int client_socket, std::atomic<bool>& bbox_push_enabled, 
                               std::thread& push_thread, std::thread& metadata_thread) {
    // 연결 종료 직전 정리
    bbox_push_enabled = false;
    
    if (push_thread.joinable()) push_thread.join();
    if (metadata_thread.joinable()) metadata_thread.join();

    SSL_free(ssl);
    close(client_socket);
    printNowTimeKST();
    cout << " [Thread " << std::this_thread::get_id()
         << "] 클라이언트 연결 종료 및 스레드 정리." << endl;
}

// ==================== 메인 클라이언트 처리 함수 ====================

void handle_client(int client_socket, SQLite::Database& db, std::mutex& db_mutex) {
    // SSL 연결 설정
    SSL* ssl = setup_ssl_connection(client_socket);
    if (!ssl) return;

    // 데이터베이스 테이블 초기화
    initialize_database_tables(db);

    // 스레드 관련 변수
    atomic<bool> bbox_push_enabled(false);
    thread push_thread;
    thread metadata_thread;

    // 메인 루프
    while (true) {
        json received_json;
        if (!receive_json_message(ssl, received_json)) {
            break;
        }

        try {
            printNowTimeKST();
            cout << " [Thread " << std::this_thread::get_id() << "] 수신 성공:\n"
                 << received_json.dump(2) << endl;

            route_request(ssl, received_json, db, db_mutex, bbox_push_enabled, push_thread, metadata_thread);

        } catch (const json::parse_error& e) {
            cerr << "[Thread " << std::this_thread::get_id()
                 << "] JSON 파싱 에러: " << e.what() << endl;
        }
    }

    // 정리 작업
    cleanup_client_connection(ssl, client_socket, bbox_push_enabled, push_thread, metadata_thread);
}

// ==================== TCP 서버 메인 로직 ====================

int tcp_run() {
    // OpenSSL 초기화
    if (!init_openssl()) {
        cerr << "OpenSSL 초기화 실패" << endl;
        return -1;
    }

    // SSL 컨텍스트 생성
    ssl_ctx = create_ssl_context();
    if (!ssl_ctx) {
        cleanup_openssl();
        return -1;
    }

    // SSL 컨텍스트 설정
    configure_ssl_context(ssl_ctx);

    SQLite::Database db("server_log.db",
                        SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    cout << "데이터베이스 파일 'server_log.db'에 연결되었습니다.\n";

    // libcurl 전역 초기화
    CURLcode res_global_init = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res_global_init != CURLE_OK) {
        std::cerr << "curl_global_init() 실패: "
                  << curl_easy_strerror(res_global_init) << std::endl;
        return -1;
    }

    std::mutex db_mutex;  // DB 접근을 보호할 뮤텍스 객체

    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        cerr << "Bind Failed" << endl;
        return -1;
    }
    if (listen(server_fd, 10) < 0) {
        cerr << "Listen Failed" << endl;
        return -1;
    }

    printNowTimeKST();
    cout << " 멀티스레드 서버 시작. 클라이언트 연결 대기 중... (Port: " << PORT
         << ")" << endl;

    while (true) {
        int new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket < 0) {
            cerr << "연결 수락 실패" << endl;
            continue;
        }

        printNowTimeKST();
        cout << " 메인 스레드: 클라이언트 연결 수락됨. 처리 스레드 생성..." << endl;

        // 새 클라이언트를 처리할 스레드 생성 및 분리
        std::thread client_thread(handle_client, new_socket, std::ref(db),
                                  std::ref(db_mutex));
        client_thread.detach();  // 메인 스레드는 기다리지 않고 바로 다음
                               // 클라이언트를 받으러 감
    }

    close(server_fd);
    curl_global_cleanup();
    cleanup_openssl();
    return 0;
}
