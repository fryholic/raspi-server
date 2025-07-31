#include "tcp_server.hpp"
#include "request_handlers.hpp"
#include "utils.hpp"

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

// ==================== BBox 관련 함수들 ====================

void clear_bbox_buffer() {
    std::lock_guard<std::mutex> lock(bbox_buffer_mutex);
    
    // 버퍼의 모든 데이터 제거
    std::queue<TimestampedBBox> empty_queue;
    bbox_buffer.swap(empty_queue);
    
    std::cout << "[BUFFER] Buffer cleared completely" << std::endl;
}

void update_bbox_buffer(const std::vector<ServerBBox>& new_bboxes) {
    std::lock_guard<std::mutex> lock(bbox_buffer_mutex);
    
    // 새로운 bbox 데이터를 타임스탬프와 함께 버퍼에 저장
    TimestampedBBox data{
        std::chrono::steady_clock::now(),
        new_bboxes
    };
    bbox_buffer.push(data);
    
    // 버퍼 크기 관리 (너무 많은 데이터가 쌓이지 않도록)
    auto now = std::chrono::steady_clock::now();
    int removed_count = 0;
    
    // 1. 10초보다 오래된 데이터 제거
    while (!bbox_buffer.empty()) {
        const auto& oldest = bbox_buffer.front();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - oldest.timestamp).count();
            
        if (age > 10000) {  // 10초보다 오래된 데이터 제거
            bbox_buffer.pop();
            removed_count++;
        } else {
            break;
        }
    }
    
    // 2. 버퍼 크기가 너무 크면 강제로 오래된 데이터 제거 (최대 50개 유지)
    while (bbox_buffer.size() > 50) {
        bbox_buffer.pop();
        removed_count++;
    }
    
    // 디버그 로그
    if (removed_count > 0 || bbox_buffer.size() > 10) {
        std::cout << "[BUFFER] Added 1 item, removed " << removed_count 
                 << " items, current size: " << bbox_buffer.size() << std::endl;
    }
}

bool send_bboxes_to_client(SSL* ssl) {
    std::vector<ServerBBox> bboxes_to_send;
    int buffer_size = 0;
    int processed_count = 0;
    
    {
        std::lock_guard<std::mutex> lock(bbox_buffer_mutex);
        
        auto now = std::chrono::steady_clock::now();
        buffer_size = bbox_buffer.size();
        
        // 버퍼에서 지연 시간이 지난 가장 오래된 데이터 찾기
        while (!bbox_buffer.empty()) {
            const auto& oldest = bbox_buffer.front();
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - oldest.timestamp).count();
                
            if (age >= bbox_buffer_delay_ms.load()) {
                bboxes_to_send = oldest.bboxes;
                bbox_buffer.pop();
                processed_count++;
                break;  // 하나만 처리하고 나감
            }
            break;  // 지연 시간이 지나지 않은 데이터는 아직 전송하지 않음
        }
        
        // 디버그 로그 추가
        if (buffer_size > 5) {  // 버퍼에 5개 이상 쌓이면 경고
            std::cout << "[DEBUG] Buffer getting large: " << buffer_size 
                     << " items, processed: " << processed_count << std::endl;
        }
    }
    
    if (bboxes_to_send.empty()) {
        return true;  // 전송할 데이터가 없음 (에러 아님)
    }

    json bbox_array = json::array();
    for (const ServerBBox& box : bboxes_to_send) {
        json j = {
            {"id", box.object_id},
            {"type", box.type},
            {"confidence", box.confidence},
            {"x", box.left},
            {"y", box.top},
            {"width", box.right - box.left},
            {"height", box.bottom - box.top}
        };
        bbox_array.push_back(j);
    }
    
    json response = {
        {"response_id", 200},
        {"bboxes", bbox_array},
        {"buffer_info", {
            {"buffer_size", buffer_size},
            {"processed_count", processed_count}
        }}
    };

    std::string json_str = response.dump();
    uint32_t res_len = json_str.length();
    uint32_t net_res_len = htonl(res_len);

    // 먼저 4바이트 길이 접두사 전송
    if (sendAll(ssl, reinterpret_cast<const char*>(&net_res_len), sizeof(net_res_len), 0) == -1) {
        std::cout << "[TCP Server] Failed to send length prefix" << std::endl;
        return false;
    }
    // 그 다음 실제 JSON 데이터 전송
    int ret = sendAll(ssl, json_str.c_str(), res_len, 0);
    if (ret == -1) {
        std::cout << "[TCP Server] Failed to send JSON data" << std::endl;
    }
    return ret != -1;
}
