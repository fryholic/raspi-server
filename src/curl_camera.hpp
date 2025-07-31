// CURL 클라이언트 모듈
// 라인 크로싱 설정을 위한 HTTP 요청 처리

#pragma once

#include <curl/curl.h>
#include <iostream>
#include <string>
#include "json.hpp"
#include "db_management.hpp"

using json = nlohmann::json;
using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::to_string;

// --- CURL 응답 콜백 함수 ---
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

// --- 라인 크로싱 API 함수들 ---
// 라인 크로싱 설정 조회 (GET)
string getLines();

// 라인 크로싱 설정 업데이트 (PUT)
string putLines(CrossLine crossLine);

// 라인 크로싱 삭제 (DELETE)
string deleteLines(int index);
