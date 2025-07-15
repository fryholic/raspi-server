// TCP 서버 구동 모듈

#pragma once

#include <iostream>   // 표준 입출력 (std::cout, std::cerr)
#include <string>     // 문자열 처리 (std::string)
#include <vector>     // 동적 배열 (std::vector, 여기서는 사용되지 않지만 이전 컨텍스트에서 포함됨)
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdint> // uint32_t
#include <thread>
#include <mutex>
#include <stdlib.h>

// POSIX 소켓 API 관련 헤더
#include <sys/socket.h> // socket, bind, listen, accept
#include <netinet/in.h> // sockaddr_in
#include <unistd.h>     // close
#include <arpa/inet.h>  // inet_ntoa

#include <curl/curl.h>

// 오류 처리를 위한 추가 헤더
#include <cstring>    // memset, strerror
#include <cerrno>     // errno
#include <system_error>  // strerror
#include <stdexcept>

// json 처리를 위한 외부 헤더파일
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const int PORT = 8080;

string base64_encode(const vector<unsigned char>& in);

int tcp_run();

bool recvAll(int fd, char* buffer, size_t len);

ssize_t sendAll(int socket_fd, const char* buffer, size_t len, int flags);

void printNowTimeKST();