/**
 * @file tcp_server.hpp
 * @brief TCP 서버 구동 모듈 (리팩토링된 버전)
 * @details SSL/TLS를 지원하는 멀티스레드 TCP 서버로 클라이언트 요청을 처리합니다.
 *          JSON 기반 프로토콜을 사용하며 데이터베이스 연동과 실시간 메타데이터 처리를 지원합니다.
 */

#pragma once

#include <stdlib.h>

#include <algorithm>
#include <chrono>
#include <cstdint> // uint32_t
#include <ctime>
#include <iomanip>
#include <iostream> // 표준 입출력 (std::cout, std::cerr)
#include <mutex>
#include <string> // 문자열 처리 (std::string)
#include <thread>
#include <vector> // 동적 배열 (std::vector, 여기서는 사용되지 않지만 이전 컨텍스트에서 포함됨)

// POSIX 소켓 API 관련 헤더
#include <arpa/inet.h> // inet_ntoa
#include <curl/curl.h>
#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket, bind, listen, accept
#include <unistd.h>     // close

// 오류 처리를 위한 추가 헤더
#include <cerrno>  // errno
#include <cstring> // memset, strerror
#include <stdexcept>
#include <system_error> // strerror

// json 처리를 위한 외부 헤더파일
#include "json.hpp"

// OpenSSL 관련 헤더
#include "ssl.hpp"

// DB 관련 헤더
#include "db_management.hpp"

// 카메라에 line을 설정하기 위한 헤더
#include "curl_camera.hpp"

#include "metadata_parser.hpp"
#include <atomic>
#include <queue>

// OTP 관련 헤더
#include "otp/otp_manager.hpp"

// 비밀번호 / 복구코드 해싱을 위한 헤더 파일
#include "hash.hpp"

using namespace std;
using json = nlohmann::json;

/**
 * @brief TCP 서버 포트 번호
 */
const int PORT = 8080;

// ==================== 기존 함수들 ====================
/**
 * @brief 카메라에서 라인 크로싱 설정을 가져옵니다.
 * @return HTTP 응답 본문 (JSON 문자열)
 */
string getLines();

/**
 * @brief 카메라에 라인 크로싱 설정을 전송합니다.
 * @param crossLine 설정할 라인 정보
 * @return HTTP 응답 본문 (JSON 문자열)
 */
string putLines(CrossLine);

/**
 * @brief 카메라에서 지정한 라인을 삭제합니다.
 * @param index 삭제할 라인 인덱스
 * @return HTTP 응답 본문 (JSON 문자열)
 */
string deleteLines(int index);

/**
 * @brief TCP 서버를 시작하고 클라이언트 연결을 대기합니다.
 * @return 성공 시 0, 실패 시 -1
 */
int tcp_run();

// ==================== 새로운 유틸리티 함수들 ====================
/**
 * @brief 클라이언트 소켓에 SSL 연결을 설정합니다.
 * @param client_socket 클라이언트 소켓 파일 디스크립터
 * @return 성공 시 SSL 포인터, 실패 시 nullptr
 */
SSL* setup_ssl_connection(int client_socket);

/**
 * @brief 데이터베이스 테이블들을 초기화합니다.
 * @param db SQLite 데이터베이스 참조
 */
void initialize_database_tables(SQLite::Database& db);

/**
 * @brief SSL 연결에서 JSON 메시지를 수신합니다.
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON을 저장할 참조
 * @return 성공 시 true, 실패 시 false
 */
bool receive_json_message(SSL* ssl, json& received_json);

// ==================== 요청 처리 함수들 ====================
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
void handle_vertical_line_insert_request(SSL* ssl, const json& received_json, SQLite::Database& db,
                                         std::mutex& db_mutex);

/**
 * @brief 도로 기준선 전체 조회 요청을 처리합니다. (request_id == 7)
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_baseline_select_all_request(SSL* ssl, const json& received_json, SQLite::Database& db,
                                        std::mutex& db_mutex);

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
void handle_bbox_start_request(SSL* ssl, std::atomic<bool>& bbox_push_enabled, std::thread& push_thread,
                               std::thread& metadata_thread);

/**
 * @brief BBox push 중지 요청을 처리합니다. (request_id == 32)
 * @param ssl OpenSSL SSL 포인터
 * @param bbox_push_enabled BBox push 활성화 플래그
 * @param push_thread BBox push 스레드 참조
 * @param metadata_thread 메타데이터 파싱 스레드 참조
 */
void handle_bbox_stop_request(SSL* ssl, std::atomic<bool>& bbox_push_enabled, std::thread& push_thread,
                              std::thread& metadata_thread);

// ==================== 요청 라우터 및 정리 함수들 ====================
/**
 * @brief 수신된 JSON 요청을 적절한 처리 함수로 라우팅합니다.
 * @param ssl OpenSSL SSL 포인터
 * @param received_json 수신된 JSON 요청
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 * @param bbox_push_enabled BBox push 활성화 플래그
 * @param push_thread BBox push 스레드 참조
 * @param metadata_thread 메타데이터 파싱 스레드 참조
 */
void route_request(SSL* ssl, const json& received_json, SQLite::Database& db, std::mutex& db_mutex,
                   std::atomic<bool>& bbox_push_enabled, std::thread& push_thread, std::thread& metadata_thread);

/**
 * @brief 클라이언트 연결을 정리하고 관련 리소스를 해제합니다.
 * @param ssl OpenSSL SSL 포인터
 * @param client_socket 클라이언트 소켓 파일 디스크립터
 * @param bbox_push_enabled BBox push 활성화 플래그
 * @param push_thread BBox push 스레드 참조
 * @param metadata_thread 메타데이터 파싱 스레드 참조
 */
void cleanup_client_connection(SSL* ssl, int client_socket, std::atomic<bool>& bbox_push_enabled,
                               std::thread& push_thread, std::thread& metadata_thread);

// ==================== 메인 클라이언트 처리 함수 ====================
/**
 * @brief 개별 클라이언트와의 연결을 처리하는 메인 함수입니다.
 * @details SSL 연결 설정, JSON 메시지 수신, 요청 라우팅, 연결 정리를 담당합니다.
 * @param client_socket 클라이언트 소켓 파일 디스크립터
 * @param db SQLite 데이터베이스 참조
 * @param db_mutex DB 접근 뮤텍스
 */
void handle_client(int client_socket, SQLite::Database& db, std::mutex& db_mutex);
