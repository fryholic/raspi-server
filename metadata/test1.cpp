#include <iostream>
#include <ctime>
#include <iomanip>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <vector>
#include <cstring>

// --- 프로토콜 정의 ---
#define DLE  0x10
#define STX  0x02
#define ETX  0x03
#define CMD_SYNC_TIME 0x03

// 보드 ID: 1~4
#define BOARD_ID 1
#define DEVICE "/dev/ttyAMA0"

// --- CRC 계산 (STM32 방식과 일치) ---
uint8_t reverse(uint8_t val, int bits) {
    uint8_t res = 0;
    for (int i = 0; i < bits; ++i)
        res = (res << 1) | ((val >> i) & 1);
    return res;
}

uint16_t reverse16(uint16_t val, int bits) {
    uint16_t res = 0;
    for (int i = 0; i < bits; ++i)
        res = (res << 1) | ((val >> i) & 1);
    return res;
}

uint16_t crc16(const std::vector<uint8_t>& data) {
    uint16_t crc = 0;
    for (auto b : data) {
        crc ^= reverse(b, 8) << 8;
        for (int i = 0; i < 8; ++i)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x8005 : crc << 1;
    }
    return reverse16(crc, 16) & 0xFFFF;
}

// --- UART 초기화 ---
int open_serial(const char* device) {
    int fd = open(device, O_RDWR | O_NOCTTY);
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

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;

    tcsetattr(fd, TCSANOW, &tty);
    return fd;
}

// --- 프레임 인코딩 ---
std::vector<uint8_t> encode_frame(uint8_t cmd, int board_id, const std::vector<uint8_t>& extra_data) {
    uint8_t dst_mask = 1 << (board_id - 1);

    std::vector<uint8_t> payload = { dst_mask, cmd };
    payload.insert(payload.end(), extra_data.begin(), extra_data.end());

    // CRC 대상은 payload 전체
    uint16_t crc = crc16(payload);
    payload.push_back((crc >> 8) & 0xFF);
    payload.push_back(crc & 0xFF);

    // 바이트 스터핑 포함된 전체 프레임 구성
    std::vector<uint8_t> frame = { DLE, STX };
    for (uint8_t b : payload) {
        if (b == DLE) {
            frame.push_back(DLE);
            frame.push_back(DLE);  // Byte stuffing
        } else {
            frame.push_back(b);
        }
    }
    frame.push_back(DLE);
    frame.push_back(ETX);
    return frame;
}

int main() {
    int fd = open_serial(DEVICE);

    while (true) {
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);

        int hour = t->tm_hour;
        bool is_pm = false;
        if (hour >= 12) {
            is_pm = true;
            if (hour > 12) hour -= 12;
        }
        if (hour == 0) hour = 12;

        std::vector<uint8_t> time_payload = {
            static_cast<uint8_t>(t->tm_year % 100),
            static_cast<uint8_t>(t->tm_mon + 1),
            static_cast<uint8_t>(t->tm_mday),
            static_cast<uint8_t>(hour),
            static_cast<uint8_t>(t->tm_min),
            static_cast<uint8_t>(t->tm_sec),
            static_cast<uint8_t>(is_pm ? 1 : 0)
        };

        auto frame = encode_frame(CMD_SYNC_TIME, BOARD_ID, time_payload);
        write(fd, frame.data(), frame.size());
        tcdrain(fd); // 송신 완료까지 대기

        std::cout << "[SENT] ";
        for (uint8_t b : frame)
            std::cout << std::hex << std::setw(2) << std::setfill('0') << int(b) << ' ';
        std::cout << std::endl;

        sleep(5);  // 5초마다 전송
    }

    close(fd);
    return 0;
}

/* g++ test1.cpp -o send_time --std=c++17*/

