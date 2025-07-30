#include "ssl.hpp"
#include <iostream>

std::mutex ssl_write_mutex;
SSL_CTX* ssl_ctx = nullptr;

bool init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    return true;
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX* create_ssl_context() {
    const SSL_METHOD* method = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("SSL 컨텍스트 생성 실패");
        return nullptr;
    }
    return ctx;
}

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