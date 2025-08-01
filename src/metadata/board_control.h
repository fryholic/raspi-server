
/**
 * @file board_control.h
 * @brief 보드 제어를 위한 BoardController 클래스 헤더 파일
 */

#pragma once
#include <vector>
#include <string>
#include <cstdint>


/// @def DLE
/// @brief Data Link Escape (프레임 구분용)
#define DLE 0x10
/// @def STX
/// @brief Start of Text (프레임 시작)
#define STX 0x02
/// @def ETX
/// @brief End of Text (프레임 끝)
#define ETX 0x03
/// @def ACK
/// @brief Acknowledge (응답)
#define ACK  0xAA
/// @def NACK
/// @brief Not Acknowledge (부정 응답)
#define NACK 0x55
/// @def CMD_LCD_ON
/// @brief LCD ON 명령 코드
#define CMD_LCD_ON  0x01
/// @def CMD_LCD_OFF
/// @brief LCD OFF 명령 코드
#define CMD_LCD_OFF 0x02
/// @def CMD_SYNC_TIME
/// @brief 시간 동기화 명령 코드
#define CMD_SYNC_TIME 0x03 // 시간 프레임 추가!


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
 * @class BoardController
 * @brief 보드 제어를 위한 클래스 (시리얼 통신 기반)
 */
class BoardController {
public:
    /**
     * @brief 생성자
     * @param device 시리얼 포트 디바이스 경로
     * @param board_id 보드 ID (1부터 시작)
     */
    BoardController(const std::string& device, int board_id);

    /**
     * @brief 소멸자. 포트를 닫음
     */
    ~BoardController();

    /**
     * @brief LCD ON 명령을 보냄
     */
    void send_lcd_on();

    /**
     * @brief LCD OFF 명령을 보냄
     */
    void send_lcd_off();

    /**
     * @brief 시스템 시간 기반 시간 동기화 명령을 보냄
     * @return 항상 true 반환
     */
    bool send_time_sync_from_system();

    /**
     * @brief LCD ON 명령을 전송하고 ACK를 대기
     * @param retries 재시도 횟수
     * @param timeout_ms 타임아웃(ms)
     * @return 성공 시 true, 실패 시 false
     */
    bool send_lcd_on_with_ack(int retries = 3, int timeout_ms = 1000);

    /**
     * @brief LCD OFF 명령을 전송하고 ACK를 대기
     * @param retries 재시도 횟수
     * @param timeout_ms 타임아웃(ms)
     * @return 성공 시 true, 실패 시 false
     */
    bool send_lcd_off_with_ack(int retries = 3, int timeout_ms = 1000);
    
private:
    /**
     * @brief 시리얼 포트 파일 디스크립터
     */
    int fd;

    /**
     * @brief 보드 ID
     */
    int id;

    /**
     * @brief 시리얼 포트를 염
     * @param device 포트 디바이스 경로
     */
    void open_port(const std::string& device);

    /**
     * @brief 추가 데이터 없이 명령 프레임을 전송
     * @param command 전송할 명령 코드
     */
    void send_frame(uint8_t command);

    /**
     * @brief 명령을 전송하고 ACK/NACK을 대기
     * @param command 전송할 명령 코드
     * @param retries 재시도 횟수
     * @param timeout_ms 타임아웃(ms)
     * @return 성공 시 true, 실패 시 false
     */
    bool send_frame_with_ack(uint8_t command, int retries, int timeout_ms);

    /**
     * @brief 명령 및 추가 데이터를 DLE-STX/ETX 프레임으로 인코딩
     * @param command 명령 코드
     * @param extra_data 추가 데이터 벡터
     * @return 인코딩된 프레임 벡터
     */
    std::vector<uint8_t> encode_frame(uint8_t command, const std::vector<uint8_t>& extra_data);
};
