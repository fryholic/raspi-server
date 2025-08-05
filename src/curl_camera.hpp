/**
 * @file curl_camera.hpp
 * @brief libcurl을 사용한 카메라 통신 헤더 파일
 * @details 이 파일은 카메라와의 HTTP 통신을 처리하기 위한 함수와 구조체의 선언을 포함합니다.
 */

// CURL 클라이언트 모듈
// 라인 크로싱 설정을 위한 HTTP 요청 처리

#pragma once

#include "config_manager.hpp"
#include "db_management.hpp"
#include "json.hpp"
#include <curl/curl.h>
#include <iostream>
#include <string>

using json = nlohmann::json;
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::to_string;

// --- 공통 설정 함수 ---
/**
 * @brief 공통 헤더를 설정하는 함수
 * @details 이 함수는 libcurl 요청에 필요한 공통 헤더를 설정합니다.
 * @param curl_handle libcurl 핸들
 * @return 설정된 헤더 리스트 포인터
 */
struct curl_slist* setup_common_headers(CURL* curl_handle);

/**
 * @brief libcurl의 응답 데이터를 저장하는 콜백 함수
 *
 * @param contents 수신 데이터 포인터
 * @param size 데이터 단위 크기
 * @param nmemb 데이터 단위 개수
 * @param userp 사용자 정의 포인터 (std::string*)
 * @return 처리한 데이터의 총 바이트 수
 */

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

/**
 * @brief 라인 크로싱 설정 정보를 GET 요청으로 받아옵니다.
 * @return HTTP 응답 본문 (JSON 문자열)
 */
string getLines();

/**
 * @brief 라인 크로싱 설정 정보를 PUT 요청으로 서버에 전송합니다.
 * @param crossLine 전송할 라인 정보 구조체
 * @return HTTP 응답 본문 (JSON 문자열)
 */
string putLines(CrossLine crossLine);

/**
 * @brief 지정한 인덱스의 라인 크로싱 설정을 DELETE 요청으로 삭제합니다.
 * @param index 삭제할 라인 인덱스
 * @return HTTP 응답 본문 (JSON 문자열)
 */
string deleteLines(int index);
