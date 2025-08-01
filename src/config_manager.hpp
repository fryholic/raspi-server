/**
 * @file config_manager.hpp
 * @brief 설정 관리 헤더 파일
 * @details 이 파일은 서버 설정을 저장하는 구조체와 설정 로드 관련 함수들의 선언을 포함합니다.
 */

#pragma once

#include <string>
#include <map>

using namespace std;


/**
 * @brief 서버 설정을 저장하는 구조체
 *
 * .env 및 config.json에서 로드되는 모든 설정값을 포함합니다.
 */
struct AppConfig {
    /** @brief .env에서 로드되는 사용자 이름 */
    string username;
    /** @brief .env에서 로드되는 비밀번호 */
    string password;
    /** @brief .env에서 로드되는 호스트 주소 */
    string host;
    /** @brief .env에서 로드되는 RTSP 포트 */
    string rtsp_port;
    /** @brief .env에서 로드되는 RTSP 경로 */
    string rtsp_path;
    /** @brief .env에서 로드되는 DB 파일 경로 */
    string db_file;
    /** @brief .env에서 로드되는 트랙 ID (선택적) */
    string trackid;

    /** @brief config.json에서 로드되는 거리 임계값 */
    float dist_threshold;
    /** @brief config.json에서 로드되는 평행 임계값 */
    float parallelism_threshold;
    /** @brief config.json에서 로드되는 프레임 캐시 크기 */
    size_t frame_cache_size;
    /** @brief config.json에서 로드되는 히스토리 크기 */
    int history_size;
    /** @brief config.json에서 로드되는 x 스케일 */
    float scale_x;
    /** @brief config.json에서 로드되는 y 스케일 */
    float scale_y;
    /** @brief config.json에서 로드되는 기준 x 좌표 */
    float base_x;
    /** @brief config.json에서 로드되는 기준 y 좌표 */
    float base_y;
    /** @brief config.json에서 로드되는 보드 포트 매핑 */
    map<string, string> board_ports;
    /** @brief config.json에서 로드되는 재시도 횟수 */
    int retry_count;
    /** @brief config.json에서 로드되는 타임아웃(ms) */
    int timeout_ms;
};


/**
 * @brief 전역 설정 인스턴스
 */
extern AppConfig g_config;


/**
 * @brief .env 파일을 읽어서 환경 변수로 설정하고 AppConfig에 값을 저장합니다.
 * @return 성공 시 true, 실패 시 false
 */
bool load_env_variables();

/**
 * @brief config.json 파일을 읽어서 AppConfig에 설정값을 로드합니다.
 * @return 성공 시 true, 실패 시 false
 */
bool load_json_config();

/**
 * @brief .env와 config.json 파일을 모두 로드합니다.
 * @return 모든 설정이 성공적으로 로드되면 true, 아니면 false
 */
bool load_all_config();

/**
 * @brief AppConfig 정보를 바탕으로 RTSP URL을 생성합니다.
 * @return 생성된 RTSP URL 문자열
 */
string get_rtsp_url();
