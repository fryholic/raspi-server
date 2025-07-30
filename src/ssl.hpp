#pragma once
#include <mutex>
#include <openssl/ssl.h>
#include <openssl/err.h>

extern std::mutex ssl_write_mutex;
extern SSL_CTX* ssl_ctx;

bool init_openssl();
void cleanup_openssl();
SSL_CTX* create_ssl_context();
void configure_ssl_context(SSL_CTX* ctx);
bool recvAll(SSL* ssl, char* buffer, size_t len);
ssize_t sendAll(SSL* ssl, const char* buffer, size_t len, int flags);