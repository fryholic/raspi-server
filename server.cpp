
// 서버 main
// 이 코드에 함수 세부 구현 작성 X, 파일 나눠서 작성

#include <fstream>
#include <thread>

#include "src/db_management.hpp"
#include "src/rtsp_server.hpp"
#include "src/tcp_server.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  thread rtsp_run_thread(rtsp_run, argc, argv);

  thread tcp_run_thread(tcp_run);

  rtsp_run_thread.join();
  tcp_run_thread.join();

  return 0;
}
