/**
 * @file ssl.cpp
 * @brief SSL 통신 구현 파일
 * @details 이 파일은 OpenSSL을 사용하여 SSL/TLS 통신을 초기화, 설정 및 관리하는 기능을 제공합니다.
 */

#include "ssl.hpp"
#include <iostream>


/**
 * @brief SSL 송신 시 동기화를 위한 뮤텍스
 */
std::mutex ssl_write_mutex;

/**
 * @brief 전역 SSL 컨텍스트 포인터
 */
SSL_CTX* ssl_ctx = nullptr;

/**
 * @brief OpenSSL 라이브러리 초기화
 * @return 항상 true 반환
 */
bool init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    return true;
}

/**
 * @brief OpenSSL 라이브러리 정리
 * @details OpenSSL에서 사용된 리소스를 정리합니다.
 */
void cleanup_openssl() {
    EVP_cleanup();
}

/**
 * @brief SSL 컨텍스트 생성 (TLS 서버용)
 * @return 생성된 SSL_CTX 포인터, 실패 시 nullptr
 */
SSL_CTX* create_ssl_context() {
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("SSL 컨텍스트 생성 실패");
        return nullptr;
    }
    return ctx;
}

/**
 * @brief SSL 컨텍스트에 인증서와 개인키를 설정합니다.
 * @details 이 함수는 SSL 컨텍스트에 서버 인증서와 개인키를 설정하여 TLS 통신을 준비합니다.
 * @param ctx SSL_CTX 포인터
 */
void configure_ssl_context(SSL_CTX* ctx) {
    if (SSL_CTX_use_certificate_file(ctx, "fullchain.crt", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

// SSL 버전의 송수신 함수
/**
 * @brief SSL을 통해 지정한 길이만큼 데이터를 수신합니다.
 * @param ssl OpenSSL SSL 포인터
 * @param buffer 수신 버퍼
 * @param len 수신할 바이트 수
 * @return 성공 시 true, 실패 시 false
 */
bool recvAll(SSL* ssl, char* buffer, size_t len) {
  size_t total_received = 0;
  while (total_received < len) {
    int bytes_received =
        SSL_read(ssl, buffer + total_received, len - total_received);

    if (bytes_received <= 0) {
      int error = SSL_get_error(ssl, bytes_received);
      if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
        continue;
      }
      ERR_print_errors_fp(stderr);
      return false;
    }
    total_received += bytes_received;
  }
  return true;
}

/**
 * @brief SSL을 통해 지정한 길이만큼 데이터를 송신합니다.
 * @param ssl OpenSSL SSL 포인터
 * @param buffer 송신할 데이터 버퍼
 * @param len 송신할 바이트 수
 * @param flags (미사용)
 * @return 송신한 바이트 수, 실패 시 -1
 */
ssize_t sendAll(SSL* ssl, const char* buffer, size_t len, int flags) {
  size_t total_sent = 0;
  while (total_sent < len) {
    int bytes_sent = SSL_write(ssl, buffer + total_sent, len - total_sent);

    if (bytes_sent <= 0) {
      int error = SSL_get_error(ssl, bytes_sent);
      if (error == SSL_ERROR_WANT_WRITE || error == SSL_ERROR_WANT_READ) {
        continue;
      }
      ERR_print_errors_fp(stderr);
      return -1;
    }

    total_sent += bytes_sent;
  }

  return total_sent;
}