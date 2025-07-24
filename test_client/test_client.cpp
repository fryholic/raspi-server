#include <iostream>
#include <string>
#include <vector>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../json.hpp"
// libsodium for client-side hash/verify debug
#include <sodium.h>

using json = nlohmann::json;

bool sendAll(SSL* ssl, const char* buffer, size_t len) {
    size_t total = 0;
    while (total < len) {
        int sent = SSL_write(ssl, buffer + total, len - total);
        if (sent <= 0) {
            int err = SSL_get_error(ssl, sent);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) continue;
            ERR_print_errors_fp(stderr);
            return false;
        }
        total += sent;
    }
    return true;
}

bool recvAll(SSL* ssl, char* buffer, size_t len) {
    size_t total = 0;
    while (total < len) {
        int rec = SSL_read(ssl, buffer + total, len - total);
        if (rec <= 0) {
            int err = SSL_get_error(ssl, rec);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            ERR_print_errors_fp(stderr);
            return false;
        }
        total += rec;
    }
    return true;
}

int main() {
    // Initialize libsodium for hashing
    if (sodium_init() < 0) {
        std::cerr << "libsodium 초기화 실패" << std::endl;
        return 1;
    }
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return 1;
    }

    // Load server certificate for verification
    if (!SSL_CTX_load_verify_locations(ctx, "server.crt", NULL)) {
        ERR_print_errors_fp(stderr);
        return 1;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    // Create socket and connect
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_port = htons(8080);
    if (inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr) <= 0) {
        perror("inet_pton");
        return 1;
    }
    if (connect(sock, (sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("connect");
        return 1;
    }

    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        return 1;
    }
    std::cout << "Connected with SSL\n";

    while (true) {
        std::cout << "Select action: 1) Login 2) SignUp 3) Exit: ";
        int action;
        std::cin >> action;
        if (action == 3) break;
        std::string id, passwd;
        std::cout << "ID: "; std::cin >> id;
        std::cout << "Password: "; std::cin >> passwd;
        // Client-side hash & verify for debug
        char client_hash[crypto_pwhash_STRBYTES];
        if (crypto_pwhash_str(client_hash, passwd.c_str(), passwd.size(),
                             crypto_pwhash_OPSLIMIT_INTERACTIVE,
                             crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
            std::cerr << "[Client Debug] Password hashing failed" << std::endl;
        } else {
            std::cout << "[Client Debug] Local hashed passwd: " << client_hash << std::endl;
            bool ok = crypto_pwhash_str_verify(client_hash, passwd.c_str(), passwd.size()) == 0;
            std::cout << "[Client Debug] Local verify result: " << ok << std::endl;
        }
        json root;
        if (action == 1) root["request_id"] = 8;
        else if (action == 2) root["request_id"] = 9;
        root["data"] = { {"id", id}, {"passwd", passwd} };
        std::string out = root.dump();
        std::cout << "[Debug][Client] Request JSON: " << out << std::endl;

        uint32_t len = htonl(static_cast<uint32_t>(out.size()));
        std::cout << "[Debug][Client] Sending length prefix: " << out.size() << std::endl;
        if (!sendAll(ssl, reinterpret_cast<const char*>(&len), sizeof(len))) break;
        std::cout << "[Debug][Client] Length prefix sent" << std::endl;
        if (!sendAll(ssl, out.c_str(), out.size())) break;
        std::cout << "[Debug][Client] Payload sent" << std::endl;

        // Receive response length
        uint32_t net_len;
        if (!recvAll(ssl, reinterpret_cast<char*>(&net_len), sizeof(net_len))) break;
        uint32_t res_len = ntohl(net_len);
        std::cout << "[Debug][Client] Received response length: " << res_len << std::endl;
        std::vector<char> buf(res_len);
        if (!recvAll(ssl, buf.data(), res_len)) break;
        std::string resp(out.begin(), out.end()); // reuse container
        std::string respStr(buf.begin(), buf.end());
        std::cout << "[Debug][Client] Response JSON: " << respStr << std::endl;
        json res = json::parse(buf);
        if (action == 1)
            std::cout << "Login success: " << res.value("login_success", 0) << "\n";
        else if (action == 2)
            std::cout << "SignUp success: " << res.value("sign_up_success", 0) << "\n";
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}
