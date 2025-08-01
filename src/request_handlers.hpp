/**
 * @file request_handlers.hpp
 * @brief 클라이언트 요청 처리 헤더 파일
 * @details 이 파일은 클라이언트 요청을 처리하는 함수들의 선언을 포함합니다. 데이터베이스 접근, JSON 응답 생성, SSL 통신 등을 지원합니다.
 */

#pragma once

#include <string>
#include <atomic>
#include <thread>

#include "json.hpp"
#include "ssl.hpp"
#include "db_management.hpp"

using namespace std;
using json = nlohmann::json;


/**
 * @brief 감지 데이터 조회 요청을 처리합니다. (request_id == 1)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_detection_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex);

/**
 * @brief 감지선 좌표값 삽입 요청을 처리합니다. (request_id == 2)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_line_insert_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex);

/**
 * @brief 감지선 전체 조회 요청을 처리합니다. (request_id == 3)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_line_select_all_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex);

/**
 * @brief 감지선, 기준선, 수직선 전체 삭제 요청을 처리합니다. (request_id == 4)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_line_delete_all_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex);

/**
 * @brief 도로 기준선 삽입 요청을 처리합니다. (request_id == 5)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_baseline_insert_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex);

/**
 * @brief 감지선의 수직선 방정식 삽입 요청을 처리합니다. (request_id == 6)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_vertical_line_insert_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex);

/**
 * @brief 도로 기준선 전체 조회 요청을 처리합니다. (request_id == 7)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_baseline_select_all_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex);

/**
 * @brief 1단계 로그인 요청(ID/PW 검증)을 처리합니다. (request_id == 8)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_login_step1_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex);

/**
 * @brief 2단계 로그인 요청(OTP/복구코드 검증)을 처리합니다. (request_id == 22)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_login_step2_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex);

/**
 * @brief 회원가입 요청을 처리합니다. (request_id == 9)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_signup_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex);

/**
 * @brief BBox push 시작 요청을 처리합니다. (request_id == 31)
 * @param ssl OpenSSL SSL 포인터
 * @param bbox_push_enabled BBox push 활성화 플래그
 * @param push_thread BBox push 스레드 참조
 * @param metadata_thread 메타데이터 파싱 스레드 참조
 */
void handle_bbox_start_request(SSL* ssl, std::atomic<bool>& bbox_push_enabled, std::thread& push_thread, std::thread& metadata_thread);

/**
 * @brief BBox push 중지 요청을 처리합니다. (request_id == 32)
 * @param ssl OpenSSL SSL 포인터
 * @param bbox_push_enabled BBox push 활성화 플래그
 * @param push_thread BBox push 스레드 참조
 * @param metadata_thread 메타데이터 파싱 스레드 참조
 */
void handle_bbox_stop_request(SSL* ssl, std::atomic<bool>& bbox_push_enabled, std::thread& push_thread, std::thread& metadata_thread);


/**
 * @brief JSON 객체를 직렬화하여 SSL을 통해 클라이언트로 전송합니다.
 * @param ssl OpenSSL SSL 포인터
 * @param response 전송할 JSON 객체
 */
void send_json_response(SSL* ssl, const json& response);