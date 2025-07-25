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
        std::cout << "\n=== OTP Test Client ===\n";
        std::cout << "1) Login (2-step with OTP)\n";
        std::cout << "2) SignUp (with OTP setup)\n";
        std::cout << "3) Exit\n";
        std::cout << "Select action: ";
        int action;
        std::cin >> action;
        if (action == 3) break;

        if (action == 1) {
            // 2단계 로그인 테스트
            std::string id, passwd;
            std::cout << "ID: "; std::cin >> id;
            std::cout << "Password: "; std::cin >> passwd;

            // 1단계: ID/PW 검증
            json step1_request;
            step1_request["request_id"] = 8;
            step1_request["data"] = { {"id", id}, {"passwd", passwd} };
            std::string step1_out = step1_request.dump();
            std::cout << "[Debug][Client] Step 1 Request: " << step1_out << std::endl;

            uint32_t len = htonl(static_cast<uint32_t>(step1_out.size()));
            if (!sendAll(ssl, reinterpret_cast<const char*>(&len), sizeof(len))) break;
            if (!sendAll(ssl, step1_out.c_str(), step1_out.size())) break;

            // 1단계 응답 수신
            uint32_t net_len;
            if (!recvAll(ssl, reinterpret_cast<char*>(&net_len), sizeof(net_len))) break;
            uint32_t res_len = ntohl(net_len);
            std::vector<char> buf(res_len);
            if (!recvAll(ssl, buf.data(), res_len)) break;
            std::string step1_resp(buf.begin(), buf.end());
            std::cout << "[Debug][Client] Step 1 Response: " << step1_resp << std::endl;
            
            json step1_res = json::parse(buf);
            if (step1_res.value("step1_success", 0) == 1) {
                std::cout << "✓ Step 1 Success: " << step1_res.value("message", "") << "\n";
                
                // 2단계: OTP/복구코드 입력
                std::string otp_input;
                std::cout << "Enter OTP (6 digits) or Recovery Code: ";
                std::cin >> otp_input;

                json step2_request;
                step2_request["request_id"] = 22;
                step2_request["data"] = { {"id", id}, {"input", otp_input} };
                std::string step2_out = step2_request.dump();
                std::cout << "[Debug][Client] Step 2 Request: " << step2_out << std::endl;

                len = htonl(static_cast<uint32_t>(step2_out.size()));
                if (!sendAll(ssl, reinterpret_cast<const char*>(&len), sizeof(len))) break;
                if (!sendAll(ssl, step2_out.c_str(), step2_out.size())) break;

                // 2단계 응답 수신
                if (!recvAll(ssl, reinterpret_cast<char*>(&net_len), sizeof(net_len))) break;
                res_len = ntohl(net_len);
                buf.resize(res_len);
                if (!recvAll(ssl, buf.data(), res_len)) break;
                std::string step2_resp(buf.begin(), buf.end());
                std::cout << "[Debug][Client] Step 2 Response: " << step2_resp << std::endl;
                
                json step2_res = json::parse(buf);
                if (step2_res.value("final_login_success", 0) == 1) {
                    std::cout << "✓ Login Complete: " << step2_res.value("message", "") << "\n";
                } else {
                    std::cout << "✗ Login Failed: " << step2_res.value("message", "") << "\n";
                }
            } else {
                std::cout << "✗ Step 1 Failed: Invalid ID/Password\n";
            }

        } else if (action == 2) {
            // 회원가입 테스트
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

            json signup_request;
            signup_request["request_id"] = 9;
            signup_request["data"] = { {"id", id}, {"passwd", passwd} };
            std::string signup_out = signup_request.dump();
            std::cout << "[Debug][Client] SignUp Request: " << signup_out << std::endl;

            uint32_t len = htonl(static_cast<uint32_t>(signup_out.size()));
            if (!sendAll(ssl, reinterpret_cast<const char*>(&len), sizeof(len))) break;
            if (!sendAll(ssl, signup_out.c_str(), signup_out.size())) break;

            // 회원가입 응답 수신
            uint32_t net_len;
            if (!recvAll(ssl, reinterpret_cast<char*>(&net_len), sizeof(net_len))) break;
            uint32_t res_len = ntohl(net_len);
            std::vector<char> buf(res_len);
            if (!recvAll(ssl, buf.data(), res_len)) break;
            std::string signup_resp(buf.begin(), buf.end());
            std::cout << "[Debug][Client] SignUp Response: " << signup_resp << std::endl;
            
            json signup_res = json::parse(buf);
            if (signup_res.value("sign_up_success", 0) == 1) {
                std::cout << "✓ SignUp Success!\n";
                std::cout << "\n=== OTP Setup Information ===\n";
                std::cout << "OTP URI: " << signup_res.value("otp_uri", "") << "\n";
                std::cout << "\n=== Recovery Codes (Save these safely!) ===\n";
                if (signup_res.contains("recovery_codes")) {
                    for (const auto& code : signup_res["recovery_codes"]) {
                        std::cout << "- " << code.get<std::string>() << "\n";
                    }
                }
                std::cout << "\n=== QR Code SVG ===\n";
                std::cout << signup_res.value("qr_code_svg", "") << "\n";
                std::cout << "\nScan the QR code with your authenticator app!\n";
            } else {
                std::cout << "✗ SignUp Failed: User may already exist\n";
            }
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}
