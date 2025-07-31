#include "request_handlers.hpp"
#include "tcp_server.hpp"
#include "curl_camera.hpp"
#include "hash.hpp"
#include "otp/otp_manager.hpp"
#include "metadata_parser.hpp"
#include "utils.hpp"

#include <algorithm>
#include <memory>
#include <random>

// OTP 관련 전역 변수
static OTPManager otp_manager;

// ==================== 공통 유틸리티 함수들 ====================

void send_json_response(SSL* ssl, const json& response) {
    string json_string = response.dump();
    uint32_t res_len = json_string.length();
    uint32_t net_res_len = htonl(res_len);
    
    sendAll(ssl, reinterpret_cast<const char*>(&net_res_len), sizeof(net_res_len), 0);
    sendAll(ssl, json_string.c_str(), res_len, 0);
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