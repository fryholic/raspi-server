#include "tcp_server.hpp"

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

// OTP 관련 전역 변수
static OTPManager otp_manager;

// ==================== 새로운 유틸리티 함수들 ====================

void send_json_response(SSL* ssl, const json& response) {
    string json_string = response.dump();
    uint32_t res_len = json_string.length();
    uint32_t net_res_len = htonl(res_len);
    
    sendAll(ssl, reinterpret_cast<const char*>(&net_res_len), sizeof(net_res_len), 0);
    sendAll(ssl, json_string.c_str(), res_len, 0);
}

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

// ==================== 요청 처리 함수들 ====================

void handle_detection_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex) {
    // request_id == 1: 클라이언트의 이미지&텍스트 요청(select) 신호
    string start_ts = received_json["data"].value("start_timestamp", "");
    string end_ts = received_json["data"].value("end_timestamp", "");

    vector<Detection> detections;
    // --- DB 접근 시 Mutex로 보호 ---
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 조회 시작 (Lock 획득)" << endl;
        detections = select_data_for_timestamp_range_detections(db, start_ts, end_ts);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 조회 완료 (Lock 해제)" << endl;
    }

    json root;
    root["request_id"] = 10;
    json data_array = json::array();
    for (const auto& detection : detections) {
        json d_obj;
        d_obj["image"] = base64_encode(detection.imageBlob);
        d_obj["timestamp"] = detection.timestamp;
        data_array.push_back(d_obj);
    }
    root["data"] = data_array;
    
    send_json_response(ssl, root);
    cout << "[Thread " << std::this_thread::get_id() << "] 응답 전송 완료." << endl;

    json res = {{"request_id", 1}, {"result", "ok"}};
    send_json_response(ssl, res);
}

void handle_line_insert_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex) {
    // request_id == 2: 클라이언트의 감지선 좌표값 삽입(insert) 신호
    int index = received_json["data"].value("index", -1);
    int x1 = received_json["data"].value("x1", -1);
    int y1 = received_json["data"].value("y1", -1);
    int x2 = received_json["data"].value("x2", -1);
    int y2 = received_json["data"].value("y2", -1);
    string name = received_json["data"].value("name", "name1");
    string mode = received_json["data"].value("mode", "BothDirections");
    string camera_type = received_json.value("camera_type", "CCTV");

    CrossLine curlCrossLine = {index, x1 * 4, y1 * 4, x2 * 4, y2 * 4, name, mode};
    CrossLine insertCrossLine = {index, x1, y1, x2, y2, name, mode};

    bool mappingSuccess;
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 삽입 시작 (Lock 획득)" << endl;
        mappingSuccess = insert_data_lines(db, insertCrossLine);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 삽입 완료 (Lock 해제)" << endl;
    }

    if (camera_type == "CCTV") {
        putLines(curlCrossLine);

        json root;
        root["request_id"] = 11;
        root["mapping_success"] = (mappingSuccess == true) ? 1 : 0;
        send_json_response(ssl, root);
    } else {
        vector<CrossLine> dbLines;
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            cout << "[Thread " << std::this_thread::get_id()
                 << "] DB 조회 시작 (Lock 획득)" << endl;
            dbLines = select_all_data_lines(db);
            cout << "[Thread " << std::this_thread::get_id()
                 << "] DB 조회 완료 (Lock 해제)" << endl;
        }

        json root;
        root["request_id"] = 18;
        json data_array = json::array();
        for (const auto& line : dbLines) {
            json d_obj;
            d_obj["index"] = line.index;
            d_obj["x1"] = line.x1;
            d_obj["y1"] = line.y1;
            d_obj["x2"] = line.x2;
            d_obj["y2"] = line.y2;
            d_obj["name"] = line.name;
            d_obj["mode"] = line.mode;
            data_array.push_back(d_obj);
        }
        root["data"] = data_array;
        send_json_response(ssl, root);
    }

    cout << "[Thread " << std::this_thread::get_id() << "] 응답 전송 완료." << endl;
}

void handle_line_select_all_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex) {
    // request_id == 3: 클라이언트의 감지선 좌표값 요청(select all) 신호
    vector<CrossLine> httpLines;
    json j = json::parse(getLines());

    // "lineCrossing" 배열의 첫 번째 요소 안에 있는 "line" 배열을 순회
    for (const auto& item : j["lineCrossing"][0]["line"]) {
        CrossLine cl;
        cl.index = item["index"];
        cl.name = item["name"];
        cl.mode = item["mode"];
        cl.x1 = item["lineCoordinates"][0]["x"];
        cl.y1 = item["lineCoordinates"][0]["y"];
        cl.x2 = item["lineCoordinates"][1]["x"];
        cl.y2 = item["lineCoordinates"][1]["y"];
        httpLines.push_back(cl);
    }

    vector<CrossLine> dbLines;
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 조회 시작 (Lock 획득)" << endl;
        dbLines = select_all_data_lines(db);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 조회 완료 (Lock 해제)" << endl;
    }

    // 일치하는 라인만 찾기
    vector<CrossLine> realLines;
    for (auto httpLine : httpLines) {
        for (auto dbLine : dbLines) {
            if (httpLine.index == dbLine.index) {
                realLines.push_back(dbLine);
            }
        }
    }

    // lines 테이블 비우고 실제 CCTV에 있는 가상선으로만 DB 채우기
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 삭제 시작 (Lock 획득)" << endl;
        delete_all_data_lines(db);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 삭제 완료 (Lock 해제)" << endl;
    }
    
    for (auto realLine : realLines) {
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            cout << "[Thread " << std::this_thread::get_id()
                 << "] DB 삽입 시작 (Lock 획득)" << endl;
            insert_data_lines(db, realLine);
            cout << "[Thread " << std::this_thread::get_id()
                 << "] DB 삽입 완료 (Lock 해제)" << endl;
        }
    }

    json root;
    root["request_id"] = 12;
    json data_array = json::array();
    for (const auto& line : realLines) {
        json d_obj;
        d_obj["index"] = line.index;
        d_obj["x1"] = line.x1;
        d_obj["y1"] = line.y1;
        d_obj["x2"] = line.x2;
        d_obj["y2"] = line.y2;
        d_obj["name"] = line.name;
        d_obj["mode"] = line.mode;
        data_array.push_back(d_obj);
    }
    root["data"] = data_array;
    send_json_response(ssl, root);
    cout << "[Thread " << std::this_thread::get_id() << "] 응답 전송 완료." << endl;
}

void handle_line_delete_all_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex) {
    // request_id == 4: 클라이언트의 감지선, 기준선, 수직선 전체 삭제 신호
    vector<int> indexs;
    json j = json::parse(getLines());

    for (const auto& item : j["lineCrossing"][0]["line"]) {
        indexs.push_back(item["index"]);
    }

    for (int index : indexs) {
        deleteLines(index);
    }

    bool deleteSuccess = true;
    // DB에서 모든 라인 데이터 삭제
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 조회 시작 (Lock 획득)" << endl;
        deleteSuccess &= delete_all_data_lines(db);
        deleteSuccess &= delete_all_data_baseLines(db);
        deleteSuccess &= delete_all_data_verticalLineEquations(db);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 조회 완료 (Lock 해제)" << endl;
    }

    json root;
    root["request_id"] = 13;
    root["delete_success"] = (deleteSuccess == true) ? 1 : 0;
    send_json_response(ssl, root);
    cout << "[Thread " << std::this_thread::get_id() << "] 응답 전송 완료." << endl;
}

void handle_baseline_insert_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex) {
    // request_id == 5: 클라이언트의 도로기준선 insert 신호
    int index = received_json["data"].value("index", -1);
    int matrixNum1 = received_json["data"].value("matrixNum1", -1);
    int x1 = received_json["data"].value("x1", -1);
    int y1 = received_json["data"].value("y1", -1);
    int matrixNum2 = received_json["data"].value("matrixNum2", -1);
    int x2 = received_json["data"].value("x2", -1);
    int y2 = received_json["data"].value("y2", -1);

    BaseLine baseLine = {index, matrixNum1, x1, y1, matrixNum2, x2, y2};

    bool insertSuccess, updateSuccess;
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 삽입 시작 (Lock 획득)" << endl;
        insertSuccess = insert_data_baseLines(db, baseLine);
        updateSuccess = update_data_baseLines(db, baseLine);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 삽입/수정 완료 (Lock 해제)" << endl;
    }

    json root;
    root["request_id"] = 14;
    root["insert_success"] = (insertSuccess == true) ? 1 : 0;
    root["update_success"] = (updateSuccess == true) ? 1 : 0;
    send_json_response(ssl, root);
    cout << "[Thread " << std::this_thread::get_id() << "] 응답 전송 완료." << endl;
}

void handle_vertical_line_insert_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex) {
    // request_id == 6: 클라이언트 감지선의 수직선 방정식 insert 신호
    int index = received_json["data"].value("index", -1);
    double a = received_json["data"].value("a", -1.0);  // ax+b = y
    double b = received_json["data"].value("b", -1.0);

    VerticalLineEquation verticalLineEquation = {index, a, b};

    bool insertSuccess;
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 삽입 시작 (Lock 획득)" << endl;
        insertSuccess = insert_data_verticalLineEquations(db, verticalLineEquation);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 삽입 완료 (Lock 해제)" << endl;
    }

    json root;
    root["request_id"] = 14;
    root["insert_success"] = (insertSuccess == true) ? 1 : 0;
    send_json_response(ssl, root);
    cout << "[Thread " << std::this_thread::get_id() << "] 응답 전송 완료." << endl;
}

void handle_baseline_select_all_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex) {
    // request_id == 7: 클라이언트 도로기준선 select all(동기화) 신호
    vector<BaseLine> baseLines;
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 조회 시작 (Lock 획득)" << endl;
        baseLines = select_all_data_baseLines(db);
        cout << "[Thread " << std::this_thread::get_id()
             << "] DB 조회 완료 (Lock 해제)" << endl;
    }

    json root;
    root["request_id"] = 16;
    json data_array = json::array();
    for (const auto& baseLine : baseLines) {
        json d_obj;
        d_obj["index"] = baseLine.index;
        d_obj["matrixNum1"] = baseLine.matrixNum1;
        d_obj["x1"] = baseLine.x1;
        d_obj["y1"] = baseLine.y1;
        d_obj["matrixNum2"] = baseLine.matrixNum2;
        d_obj["x2"] = baseLine.x2;
        d_obj["y2"] = baseLine.y2;
        data_array.push_back(d_obj);
    }
    root["data"] = data_array;
    send_json_response(ssl, root);
    cout << "[Thread " << std::this_thread::get_id() << "] 응답 전송 완료." << endl;
}

void handle_login_step1_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex) {
    // request_id == 8: 1단계 로그인 요청 (ID/PW 검증)
    cout << "[로그인] 1단계 로그인 요청 수신 (ID/PW 검증)" << endl;
    string id = received_json["data"].value("id", "");
    string passwd = received_json["data"].value("passwd", "");

    // 입력 값 유효성 검사
    if (id.empty() || passwd.empty()) {
        cerr << "[에러] 클라이언트가 보낸 ID 또는 비밀번호가 비어 있습니다." << endl;
        return;
    }

    Account* accountPtr = nullptr;
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        cout << "[DB] ID로 계정 조회 시도 (ID: " << id << ")" << endl;
        accountPtr = get_account_by_id(db, id);
    }

    json root;
    root["request_id"] = 19;
    bool step1Success = false;

    if (accountPtr == nullptr) {
        cout << "[로그인] 1단계 실패: 존재하지 않는 ID (" << id << ")" << endl;
        step1Success = false;
    } else {
        if (verify_password(accountPtr->passwd, passwd)) {
            cout << "[로그인] 1단계 성공: ID/PW 검증 완료 (" << id << ")" << endl;
            step1Success = true;
            
            // OTP 객체 생성 (DB에서 secret 로드)
            if (!accountPtr->otp_secret.empty()) {
                otp_manager.register_otp(id, accountPtr->otp_secret);
            }
        } else {
            cout << "[로그인] 1단계 실패: 비밀번호 불일치 (ID: " << id << ")" << endl;
            step1Success = false;
        }
    }

    // 평문 비밀번호 메모리에서 즉시 삭제
    secure_clear_password(passwd);
    cout << "[Debug] 평문 비밀번호 메모리에서 삭제 완료." << endl;

    root["step1_success"] = step1Success ? 1 : 0;
    if (step1Success) {
        root["requires_otp"] = (accountPtr->use_otp ? 1 : 0);
        if (accountPtr->use_otp)
            root["message"] = "ID/PW 검증 완료. OTP 또는 복구 코드를 입력하세요.";
        else
            root["message"] = "ID/PW 검증 완료. 바로 로그인 가능합니다.";
    }
    
    delete accountPtr;
    send_json_response(ssl, root);
    cout << "[응답] 1단계 로그인 결과 전송 완료." << endl;
}

void handle_login_step2_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex) {
    // request_id == 22: OTP/복구코드 검증 요청
    cout << "[로그인] 2단계 로그인 요청 수신 (OTP/복구코드 검증)" << endl;
    string id = received_json["data"].value("id", "");
    string input = received_json["data"].value("input", "");

    json root;
    root["request_id"] = 23;
    bool finalLoginSuccess = false;
    bool is_recovery_code = false;

    try {
        // 입력값이 숫자인지 확인 (OTP는 숫자, 복구코드는 문자열)
        bool is_numeric = !input.empty() && std::all_of(input.begin(), input.end(), ::isdigit);

        if (is_numeric && input.length() == 6) {
            // OTP 검증
            int otp = std::stoi(input);
            if (otp_manager.verify_otp(id, otp)) {
                cout << "[로그인] OTP 검증 성공 (ID: " << id << ")" << endl;
                finalLoginSuccess = true;
            } else {
                cout << "[로그인] OTP 검증 실패 (ID: " << id << ")" << endl;
            }
        } else {
            // 복구 코드 검증
            {
                std::lock_guard<std::mutex> lock(db_mutex);
                cout << "[DB] 복구 코드 검증 시도 (ID: " << id << ")" << endl;
                if (verify_recovery_code(db, id, input)) {
                    cout << "[로그인] 복구 코드 검증 성공 (ID: " << id << ")" << endl;
                    finalLoginSuccess = true;
                    is_recovery_code = true;
                    // 사용된 복구 코드 무효화
                    invalidate_recovery_code(db, id, input);
                    cout << "[DB] 사용된 복구 코드 무효화 완료 (ID: " << id << ")" << endl;
                } else {
                    cout << "[로그인] 복구 코드 검증 실패 (ID: " << id << ")" << endl;
                }
            }
        }
    } catch (const std::exception& e) {
        cerr << "[에러] OTP/복구코드 검증 실패: " << e.what() << endl;
        finalLoginSuccess = false;
    }

    root["final_login_success"] = finalLoginSuccess ? 1 : 0;
    if (finalLoginSuccess) {
        if (is_recovery_code) {
            root["message"] = "복구 코드로 로그인 성공. 해당 복구 코드는 무효화되었습니다.";
        } else {
            root["message"] = "OTP 검증 성공. 로그인 완료.";
        }
    } else {
        root["message"] = "OTP 또는 복구 코드가 올바르지 않습니다.";
    }
    
    send_json_response(ssl, root);
    cout << "[응답] 2단계 로그인 결과 전송 완료." << endl;
}

void handle_signup_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex) {
    // request_id == 9: 회원가입 요청
    cout << "[회원가입] 회원가입 요청 수신" << endl;
    string id = received_json["data"].value("id", "");
    string passwd = received_json["data"].value("passwd", "");
    bool use_otp = received_json["data"].value("use_otp", true); // 기본값 true

    json root;
    root["request_id"] = 20;
    bool signUpSuccess = false;
    string qr_code_svg = "";
    string otp_uri = "";
    vector<string> recovery_codes;
    string otp_secret = "";

    try {
        string hashed_passwd = hash_password(passwd);
        cout << "[회원가입] 비밀번호 해싱 완료 (ID: " << id << ")" << endl;

        // 평문 비밀번호 메모리에서 즉시 삭제
        secure_clear_password(passwd);
        cout << "[Debug] 평문 비밀번호 메모리에서 삭제 완료." << endl;

        if (use_otp) {
            // OTP URI 생성
            otp_uri = otp_manager.generate_otp_uri(id);
            otp_secret = otp_manager.get_secret(id);
            cout << "[회원가입] OTP URI 생성 완료 (ID: " << id << ")" << endl;

            // QR 코드 SVG 생성
            qr_code_svg = otp_manager.generate_qr_code_svg(otp_uri);
            cout << "[회원가입] QR 코드 생성 완료 (ID: " << id << ")" << endl;

            // 복구 코드 생성
            recovery_codes = generate_recovery_codes();
            cout << "[회원가입] 복구 코드 생성 완료 (ID: " << id << ")" << endl;
        }

        Account account = {id, hashed_passwd, otp_secret, use_otp};
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            cout << "[DB] 계정 정보 삽입 시도 (ID: " << id << ")" << endl;
            signUpSuccess = insert_data_accounts(db, account);

            if (signUpSuccess && use_otp) {
                // OTP secret 저장
                store_otp_secret(db, id, otp_secret);
                // 복구 코드 저장
                auto hashed_codes = hash_recovery_codes(recovery_codes);
                store_recovery_codes(db, id, hashed_codes);
            }
        }

        if (signUpSuccess) {
            cout << "[회원가입] 성공 (ID: " << id << ")" << endl;
        } else {
            cout << "[회원가입] 실패: ID 중복 또는 DB 오류 (ID: " << id << ")" << endl;
        }

    } catch (const std::runtime_error& e) {
        cerr << "[에러] 회원가입 처리 실패: " << e.what() << endl;
        signUpSuccess = false;
    }

    root["sign_up_success"] = signUpSuccess ? 1 : 0;
    if (signUpSuccess && use_otp) {
        root["qr_code_svg"] = qr_code_svg;
        root["otp_uri"] = otp_uri;
        json recovery_codes_json = json::array();
        for (const auto& code : recovery_codes) {
            recovery_codes_json.push_back(code);
        }
        root["recovery_codes"] = recovery_codes_json;
    }
    
    send_json_response(ssl, root);
    cout << "[응답] 회원가입 결과 전송 완료." << endl;

    // --- 복구 코드 메모리에서 삭제 ---
    for (auto& code : recovery_codes) {
        secure_clear_password(code);
    }
    recovery_codes.clear();
    cout << "[Debug] 복구 코드 메모리에서 삭제 완료." << endl;
}

void handle_bbox_start_request(SSL* ssl, std::atomic<bool>& bbox_push_enabled, std::thread& push_thread, std::thread& metadata_thread) {
    // request_id == 31: BBox push 시작
    if (!bbox_push_enabled) {
        // 메타데이터 파싱 시작
        cout << "[TCP Server] Starting metadata parser..." << endl;
        start_metadata_parser();
        metadata_thread = std::thread(parse_metadata);

        // bbox push 스레드 시작
        bbox_push_enabled = true;
        cout << "[TCP Server] Starting bbox push thread..." << endl;
        push_thread = std::thread([ssl, &bbox_push_enabled]() {
            cout << "[TCP Server] Bbox push thread started with " 
                 << bbox_send_interval_ms.load() << "ms interval and " 
                 << bbox_buffer_delay_ms.load() << "ms delay" << endl;
            
            auto next_send_time = std::chrono::steady_clock::now();
            
            while (bbox_push_enabled) {
                auto now = std::chrono::steady_clock::now();
                if (now >= next_send_time) {
                    bool success = send_bboxes_to_client(ssl);
                    if (!success) {
                        cout << "[TCP Server] Failed to send bboxes, stopping thread" << endl;
                        break;
                    }
                    // 다음 전송 시간 설정
                    next_send_time = now + std::chrono::milliseconds(bbox_send_interval_ms.load());
                }
                // 더 자주 체크하도록 sleep 시간 단축
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            cout << "[TCP Server] Bbox push thread ended" << endl;
        });
    }
}

void handle_bbox_stop_request(SSL* ssl, std::atomic<bool>& bbox_push_enabled, std::thread& push_thread, std::thread& metadata_thread) {
    // request_id == 32: BBox push 중지
    if (bbox_push_enabled) {
        cout << "[TCP Server] Stopping bbox push..." << endl;
        bbox_push_enabled = false;
        if (push_thread.joinable()) push_thread.join();

        // 메타데이터 파싱 중지
        cout << "[TCP Server] Stopping metadata parser..." << endl;
        stop_metadata_parser();
        if (metadata_thread.joinable()) metadata_thread.join();
        
        // 버퍼 정리 (오래된 데이터 제거)
        clear_bbox_buffer();
        
        cout << "[TCP Server] Bbox push and metadata parser stopped" << endl;
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

// ==================== 유틸리티 함수들 (기존 코드에서 가져옴) ====================

void printNowTimeKST() {
    // 한국 시간 (KST), 밀리초 포함 출력
    chrono::system_clock::time_point now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    auto duration_since_epoch = now.time_since_epoch();
    auto milliseconds =
        chrono::duration_cast<chrono::milliseconds>(duration_since_epoch)
            .count() %
        1000;
    const long KST_OFFSET_SECONDS = 9 * 60 * 60;
    time_t kst_now_c = now_c + KST_OFFSET_SECONDS;
    tm* kst_tm = gmtime(&kst_now_c);
    cout << "\n[" << put_time(kst_tm, "%Y-%m-%d %H:%M:%S") << "." << setfill('0')
         << setw(3) << milliseconds << " KST]" << endl;
}

string base64_encode(const vector<unsigned char>& in) {
    string out;
    const string b64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

// BBox 버퍼 정리 함수
void clear_bbox_buffer() {
    std::lock_guard<std::mutex> lock(bbox_buffer_mutex);
    
    // 버퍼의 모든 데이터 제거
    std::queue<TimestampedBBox> empty_queue;
    bbox_buffer.swap(empty_queue);
    
    std::cout << "[BUFFER] Buffer cleared completely" << std::endl;
}

// BBox 버퍼 업데이트 함수
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

// 바운딩 박스 전송 함수
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

    {
        std::lock_guard<std::mutex> lock(ssl_write_mutex);
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

    // 1. libcurl 전역 초기화
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
        client_thread.detach();  // 메인 스레드는 기다리지 않고 바로 다음 클라이언트를 받으러 감
    }

    close(server_fd);
    curl_global_cleanup();
    return 0;
}
