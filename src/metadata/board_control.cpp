/**
 * @file board_control.cpp
 * @brief 보드 제어를 위한 BoardController 클래스 구현 파일
 */
#include "board_control.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <iostream>

/**
 * @brief 8비트 값을 비트 단위로 반전합니다.
 * @param val 반전할 값
 * @param bits 반전할 비트 수
 * @return 반전된 값
 */
uint8_t reverse(uint8_t val, int bits);

/**
 * @brief 데이터 벡터에 대해 CRC16을 계산합니다.
 * @param data CRC를 계산할 데이터 벡터
 * @return 계산된 CRC16 값
 */
uint16_t crc16(const std::vector<uint8_t>& data);

/**
 * @brief 16비트 값을 비트 단위로 반전합니다.
 * @param val 반전할 값
 * @param bits 반전할 비트 수
 * @return 반전된 값
 */
uint16_t reverse16(uint16_t val, int bits);


/**
 * @brief BoardController 생성자
 * @param device 시리얼 포트 디바이스 경로
 * @param board_id 보드 ID (1부터 시작)
 */
BoardController::BoardController(const std::string& device, int board_id)
    : id(board_id)
{
    open_port(device);
}


/**
 * @brief BoardController 소멸자. 포트를 닫습니다.
 */
BoardController::~BoardController() {
    close(fd);
}


/**
 * @brief 시리얼 포트를 엽니다.
 * @param device 포트 디바이스 경로
 */
void BoardController::open_port(const std::string& device) {
    fd = open(device.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    termios tty{};
    tcgetattr(fd, &tty);
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tcsetattr(fd, TCSANOW, &tty);
}


/**
 * @brief LCD ON 명령을 보냅니다.
 */
void BoardController::send_lcd_on() {
    send_frame(CMD_LCD_ON);
}


/**
 * @brief LCD OFF 명령을 보냅니다.
 */
void BoardController::send_lcd_off() {
    send_frame(CMD_LCD_OFF);
}


/**
 * @brief 추가 데이터 없이 명령 프레임을 전송합니다.
 * @param command 전송할 명령 코드
 */
void BoardController::send_frame(uint8_t command) {
    auto frame = encode_frame(command, {});
    write(fd, frame.data(), frame.size());
}


/**
 * @brief LCD ON 명령을 전송하고 ACK를 대기합니다.
 * @param retries 재시도 횟수
 * @param timeout_ms 타임아웃(ms)
 * @return 성공 시 true, 실패 시 false
 */
bool BoardController::send_lcd_on_with_ack(int retries, int timeout_ms) {
    return send_frame_with_ack(CMD_LCD_ON, retries, timeout_ms);
}

/**
 * @brief LCD OFF 명령을 전송하고 ACK를 대기합니다.
 * @param retries 재시도 횟수
 * @param timeout_ms 타임아웃(ms)
 * @return 성공 시 true, 실패 시 false
 */
bool BoardController::send_lcd_off_with_ack(int retries, int timeout_ms) {
    return send_frame_with_ack(CMD_LCD_OFF, retries, timeout_ms);
}


/**
 * @brief 시스템의 현재 로컬타임(12시간제)으로 시간 동기화 명령을 보냅니다.
 * @return 항상 true 반환
 */
bool BoardController::send_time_sync_from_system() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);

    uint8_t hour = t->tm_hour;
    bool is_pm = false;
    if (hour >= 12) {
        is_pm = true;
        if (hour > 12) hour -= 12;
    }
    if (hour == 0) hour = 12;

    std::vector<uint8_t> extra_data = {
        static_cast<uint8_t>(t->tm_year % 100), // 2-digit year
        static_cast<uint8_t>(t->tm_mon + 1),    // Month (1-12)
        static_cast<uint8_t>(t->tm_mday),       // Day
        static_cast<uint8_t>(hour),             // Hour (1-12)
        static_cast<uint8_t>(t->tm_min),
        static_cast<uint8_t>(t->tm_sec),
        static_cast<uint8_t>(is_pm ? 1 : 0)     // AM = 0, PM = 1
    };

    auto frame = encode_frame(CMD_SYNC_TIME, extra_data);
    write(fd, frame.data(), frame.size());
    return true;
}



/**
 * @brief 명령을 전송하고 ACK/NACK을 대기합니다.
 * @param command 전송할 명령 코드
 * @param retries 재시도 횟수
 * @param timeout_ms 타임아웃(ms)
 * @return 성공 시 true, 실패 시 false
 */
bool BoardController::send_frame_with_ack(uint8_t command, int retries, int timeout_ms) {
    auto frame = encode_frame(command, {});
    for (int attempt = 0; attempt < retries; ++attempt) {
        tcflush(fd, TCIFLUSH);
        write(fd, frame.data(), frame.size());

        enum { WAIT_DLE, WAIT_STX, IN_FRAME, WAIT_ETX } state = WAIT_DLE;
        std::vector<uint8_t> payload;
        int waited = 0;
        const int step_ms = 10;

        while (waited < timeout_ms) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);

            struct timeval tv = {0, step_ms * 1000};
            if (select(fd + 1, &readfds, NULL, NULL, &tv) > 0 && FD_ISSET(fd, &readfds)) {
                uint8_t rx;
                if (read(fd, &rx, 1) == 1) {
                    switch (state) {
                        case WAIT_DLE: if (rx == DLE) state = WAIT_STX; break;
                        case WAIT_STX:
                            if (rx == STX) {
                                payload.clear();
                                state = IN_FRAME;
                            } else if (rx != DLE) state = WAIT_DLE;
                            break;
                        case IN_FRAME:
                            if (rx == DLE) state = WAIT_ETX;
                            else payload.push_back(rx);
                            break;
                        case WAIT_ETX:
                            if (rx == ETX && payload.size() == 3) {
                                uint8_t resp_cmd = payload[0];
                                uint16_t recv_crc = (payload[1] << 8) | payload[2];
                                uint16_t calc_crc = crc16({ resp_cmd });
                                if (recv_crc == calc_crc) {
                                    if (resp_cmd == ACK) return true;
                                    if (resp_cmd == NACK) break;
                                }
                            }
                            state = WAIT_DLE;
                            payload.clear();
                            break;
                    }
                }
            }
            waited += step_ms;
        }
        std::cout << "[Board " << id << "] Timeout waiting for ACK/NACK, retrying...\n";
        usleep(100000);
    }
    std::cout << "[Board " << id << "] Failed to get ACK after " << retries << " attempts!\n";
    return false;
}


/**
 * @brief 명령 및 추가 데이터를 DLE-STX/ETX 프레임으로 인코딩합니다.
 * @param command 명령 코드
 * @param extra_data 추가 데이터 벡터
 * @return 인코딩된 프레임 벡터
 */
std::vector<uint8_t> BoardController::encode_frame(uint8_t command, const std::vector<uint8_t>& extra_data) {
    uint8_t dst_mask = 1 << (id - 1);

    std::vector<uint8_t> payload = { dst_mask, command };
    payload.insert(payload.end(), extra_data.begin(), extra_data.end());

    // CRC 전체 payload에 대해 계산
    std::vector<uint8_t> crc_input = payload;
    uint16_t crc = crc16(crc_input);

    payload.push_back((crc >> 8) & 0xFF);
    payload.push_back(crc & 0xFF);

    std::vector<uint8_t> frame = { DLE, STX };
    for (auto b : payload) {
        if (b == DLE) frame.insert(frame.end(), { DLE, DLE });
        else frame.push_back(b);
    }
    frame.push_back(DLE);
    frame.push_back(ETX);
    return frame;
}


// --- CRC Functions ---

/**
 * @brief 8비트 값을 비트 단위로 반전합니다.
 * @param val 반전할 값
 * @param bits 반전할 비트 수
 * @return 반전된 값
 */
uint8_t reverse(uint8_t val, int bits) {
    uint8_t res = 0;
    for (int i = 0; i < bits; ++i)
        res = (res << 1) | ((val >> i) & 1);
    return res;
}

/**
 * @brief 16비트 값을 비트 단위로 반전합니다.
 * @param val 반전할 값
 * @param bits 반전할 비트 수
 * @return 반전된 값
 */
uint16_t reverse16(uint16_t val, int bits) {
    uint16_t res = 0;
    for (int i = 0; i < bits; ++i)
        res = (res << 1) | ((val >> i) & 1);
    return res;
}

/**
 * @brief 데이터 벡터에 대해 CRC16을 계산합니다.
 * @param data CRC를 계산할 데이터 벡터
 * @return 계산된 CRC16 값
 */
uint16_t crc16(const std::vector<uint8_t>& data) {
    uint16_t crc = 0;
    for (auto b : data) {
        crc ^= reverse(b, 8) << 8;
        for (int i = 0; i < 8; ++i)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x8005 : crc << 1;
    }
    return reverse16(crc, 16) & 0xFFFF;
}
