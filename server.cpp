
/**
 * @file server.cpp
 * @brief 서버 메인 엔트리 포인트
 * @details RTSP 서버와 TCP 서버를 멀티스레드로 실행하는 메인 프로그램입니다.
 *          각 서버는 독립적인 스레드에서 실행되며, 함수 세부 구현은 별도 파일에 작성되어 있습니다.
 */

#include <fstream>
#include <thread>

#include "src/db_management.hpp"
#include "src/rtsp_server.hpp"
#include "src/tcp_server.hpp"

using namespace std;

/**
 * @brief 서버 애플리케이션의 메인 함수
 * @param argc 명령행 인수 개수
 * @param argv 명령행 인수 배열
 * @return 프로그램 종료 코드 (성공 시 0)
 * @details RTSP 서버와 TCP 서버를 각각 별도의 스레드에서 실행합니다.
 *          두 스레드가 모두 종료될 때까지 대기한 후 프로그램을 종료합니다.
 */
int main(int argc, char* argv[])
{
    // RTSP 서버를 별도 스레드에서 실행
    thread rtsp_run_thread(rtsp_run, argc, argv);

    // TCP 서버를 별도 스레드에서 실행
    thread tcp_run_thread(tcp_run);

    // 두 서버 스레드가 모두 종료될 때까지 대기
    rtsp_run_thread.join();
    tcp_run_thread.join();

    return 0;
}
