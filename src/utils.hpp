/**
 * @file utils.hpp
 * @brief 유틸리티 함수들의 헤더 파일
 * @details 시간 출력, Base64 인코딩, 보안 메모리 정리 등의 유틸리티 기능을 선언합니다.
 */

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <iostream>

using namespace std;

// ==================== 시간 관련 함수 ====================
/**
 * @brief 현재 시간을 KST(한국 표준시) 기준으로 밀리초 단위까지 출력합니다.
 * @details 시스템 시간을 KST로 변환하여 YYYY-MM-DD HH:MM:SS.mmm KST 형식으로 출력합니다.
 */
void printNowTimeKST();

// ==================== 인코딩 관련 함수 ====================
/**
 * @brief 바이트 배열을 Base64 문자열로 인코딩합니다.
 * @param in 인코딩할 바이트 배열
 * @return Base64로 인코딩된 문자열
 * @details RFC 4648에 따른 표준 Base64 인코딩을 수행합니다.
 *          패딩 문자('=')를 포함하여 4의 배수 길이로 맞춥니다.
 */
string base64_encode(const vector<unsigned char>& in);

// ==================== 보안 관련 함수 ====================
/**
 * @brief 비밀번호 문자열을 안전하게 메모리에서 지웁니다.
 * @param passwd 지울 비밀번호 문자열 참조
 * @details 메모리 덤프나 스왑 파일에서 비밀번호가 노출되는 것을 방지하기 위해
 *          메모리를 0으로 덮어쓴 후 문자열을 비웁니다.
 */
void secure_clear_password(string& passwd);
