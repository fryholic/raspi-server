
/**
 * @file rtsp_server.hpp
 * @brief RTSP 서버 구동 모듈 헤더
 *
 * GStreamer 기반 RTSP 서버 실행 함수 선언
 */

#pragma once

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <iostream>
#include <string>

using namespace std;


/**
 * @brief RTSP 서버를 실행합니다. (GStreamer 기반)
 * @param argc 인자 개수
 * @param argv 인자 배열
 */
void rtsp_run(int argc, char *argv[]);