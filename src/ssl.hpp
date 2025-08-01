#pragma once
#include <mutex>
#include <openssl/ssl.h>
#include <openssl/err.h>


/**
 * @brief SSL 송신 시 동기화를 위한 뮤텍스
 */
extern std::mutex ssl_write_mutex;

/**
 * @brief 전역 SSL 컨텍스트 포인터
 */
extern SSL_CTX* ssl_ctx;


/**
 * @brief OpenSSL 라이브러리 초기화
 * @return 항상 true 반환
 */
bool init_openssl();

/**
 * @brief OpenSSL 라이브러리 정리
 */
void cleanup_openssl();

/**
 * @brief SSL 컨텍스트 생성 (TLS 서버용)
 * @return 생성된 SSL_CTX 포인터, 실패 시 nullptr
 */
SSL_CTX* create_ssl_context();

/**
 * @brief SSL 컨텍스트에 인증서와 개인키를 설정합니다.
 * @param ctx SSL_CTX 포인터
 */
void configure_ssl_context(SSL_CTX* ctx);

/**
 * @brief SSL을 통해 지정한 길이만큼 데이터를 수신합니다.
 * @param ssl OpenSSL SSL 포인터
 * @param buffer 수신 버퍼
 * @param len 수신할 바이트 수
 * @return 성공 시 true, 실패 시 false
 */
bool recvAll(SSL* ssl, char* buffer, size_t len);

/**
 * @brief SSL을 통해 지정한 길이만큼 데이터를 송신합니다.
 * @param ssl OpenSSL SSL 포인터
 * @param buffer 송신할 데이터 버퍼
 * @param len 송신할 바이트 수
 * @param flags (미사용)
 * @return 송신한 바이트 수, 실패 시 -1
 */
ssize_t sendAll(SSL* ssl, const char* buffer, size_t len, int flags);