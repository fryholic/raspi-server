// 유틸리티 함수들 헤더 파일

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace std;

// 시간 관련 함수
void printNowTimeKST();

// 인코딩 관련 함수
string base64_encode(const vector<unsigned char>& in);

// 보안 관련 함수
void secure_clear_password(string& passwd);
