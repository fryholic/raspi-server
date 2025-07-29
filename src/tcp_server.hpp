// TCP 서버 구동 모듈

#pragma once

#include <stdlib.h>

#include <algorithm>
#include <chrono>
#include <cstdint>  // uint32_t
#include <ctime>
#include <iomanip>
#include <iostream>  // 표준 입출력 (std::cout, std::cerr)
#include <mutex>
#include <string>  // 문자열 처리 (std::string)
#include <thread>
#include <vector>  // 동적 배열 (std::vector, 여기서는 사용되지 않지만 이전 컨텍스트에서 포함됨)

// POSIX 소켓 API 관련 헤더
#include <arpa/inet.h>  // inet_ntoa
#include <curl/curl.h>
#include <netinet/in.h>  // sockaddr_in
#include <sys/socket.h>  // socket, bind, listen, accept
#include <unistd.h>      // close

// 오류 처리를 위한 추가 헤더
#include <cerrno>   // errno
#include <cstring>  // memset, strerror
#include <stdexcept>
#include <system_error>  // strerror

// json 처리를 위한 외부 헤더파일
#include "json.hpp"

// OpenSSL 관련 헤더
#include <openssl/err.h>
#include <openssl/ssl.h>

extern SSL_CTX* ssl_ctx;

// DB 관련 헤더
#include "db_management.hpp"




#include "metadata_parser.hpp"
#include <atomic>
#include <queue>

// BBox 버퍼링을 위한 구조체
struct TimestampedBBox {
    std::chrono::steady_clock::time_point timestamp;
    std::vector<ServerBBox> bboxes;
};

// 전역 변수 선언
extern std::atomic<int> bbox_buffer_delay_ms;  // 버퍼 지연 시간 (M ms)
extern std::atomic<int> bbox_send_interval_ms; // 전송 주기 (N ms)
extern std::queue<TimestampedBBox> bbox_buffer;
extern std::mutex bbox_buffer_mutex;

// OTP 관련 헤더
#include "otp/cotp/cotp.hpp"
#include "otp/cotp/qr_code.hpp"

// 비밀번호 / 복구코드 해싱을 위한 헤더 파일
#include "hash.hpp"




using namespace std;
using json = nlohmann::json;

const int PORT = 8080;

string getLines();

string putLines(CrossLine);

string deleteLines(int index);

string base64_encode(const vector<unsigned char>& in);

int tcp_run();

bool recvAll(SSL*, char* buffer, size_t len);

ssize_t sendAll(SSL*, const char* buffer, size_t len, int flags);

void printNowTimeKST();

// SSL 초기화 및 정리 함수

bool init_openssl();
void cleanup_openssl();
SSL_CTX* create_ssl_context();
void configure_ssl_context(SSL_CTX* ctx);


/**
 * @brief Argon2id를 사용하여 비밀번호를 해싱합니다.
 *
 * @param password 평문 비밀번호.
 * @return 해싱된 비밀번호 문자열.
 */
string hash_password(const string& password);

/**
 * @brief Argon2id를 사용하여 비밀번호를 검증합니다.
 *
 * @param hashed_password 해싱된 비밀번호.
 * @param password 평문 비밀번호.
 * @return 비밀번호가 일치하면 true, 그렇지 않으면 false.
 */
bool verify_password(const string& hashed_password, const string& password);


bool send_bboxes_to_client(SSL* ssl);

// BBox 버퍼 관리 함수
void update_bbox_buffer(const std::vector<ServerBBox>& new_bboxes);
void clear_bbox_buffer();
