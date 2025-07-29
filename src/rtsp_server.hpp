// RTSP 서버 구동 모듈

#pragma once

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <iostream>
#include <string>

using namespace std;

void rtsp_run(int argc, char *argv[]);