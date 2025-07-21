#include "board_control.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <iostream>



uint8_t reverse(uint8_t val, int bits);
uint16_t crc16(const std::vector<uint8_t>& data);
uint16_t reverse16(uint16_t val, int bits);

BoardController::BoardController(const std::string& device, int board_id)
    : id(board_id)
{
    open_port(device);
}

BoardController::~BoardController() {
    close(fd);
}

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

void BoardController::send_lcd_on() {
    send_frame(CMD_LCD_ON);
}

void BoardController::send_lcd_off() {
    send_frame(CMD_LCD_OFF);
}

// Send command frame with no extra payload
void BoardController::send_frame(uint8_t command) {
    auto frame = encode_frame(command, {});
    write(fd, frame.data(), frame.size());
}

// Send LED command and wait for ACK with retry
bool BoardController::send_lcd_on_with_ack(int retries, int timeout_ms) {
    return send_frame_with_ack(CMD_LCD_ON, retries, timeout_ms);
}

bool BoardController::send_lcd_off_with_ack(int retries, int timeout_ms) {
    return send_frame_with_ack(CMD_LCD_OFF, retries, timeout_ms);
}



// Send time sync command based on system localtime in 12-hour format
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


// Send a command and wait for ACK/NACK
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

// Encode command + extra data as DLE-STX/ETX framed UART frame
std::vector<uint8_t> BoardController::encode_frame(uint8_t command, const std::vector<uint8_t>& extra_data) {
    uint8_t dst_mask = 1 << (id - 1);

    std::vector<uint8_t> payload = { dst_mask, command };
    payload.insert(payload.end(), extra_data.begin(), extra_data.end());


     // FIX: CRC over full payload
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
