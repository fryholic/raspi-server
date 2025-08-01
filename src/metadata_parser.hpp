/**
 * @file metadata_parser.hpp
 * @brief 메타데이터 파싱 헤더 파일
 * @details 이 파일은 RTSP 스트림에서 메타데이터를 파싱하고 BBox 정보를 관리하는 함수와 구조체의 선언을 포함합니다.
 */

#ifndef METADATA_PARSER_HPP
#define METADATA_PARSER_HPP

#include <vector>
#include <string>
#include <mutex>
#include <cstring>
#include <atomic>

#include <chrono>
#include <queue>
#include "json.hpp"

// Forward declaration for SSL
typedef struct ssl_st SSL;


/**
 * @brief 서버에서 사용하는 BBox(경계 상자) 정보 구조체
 */
struct ServerBBox {
    int object_id;      ///< 객체 ID
    std::string type;   ///< 객체 타입
    float confidence;   ///< 신뢰도
    int left;           ///< 좌측 좌표
    int top;            ///< 상단 좌표
    int right;          ///< 우측 좌표
    int bottom;         ///< 하단 좌표
};


/**
 * @brief JSON 배열에서 ServerBBox 벡터로 변환합니다.
 * @param bboxArray BBox 정보가 담긴 nlohmann::json 배열
 * @return ServerBBox 구조체 벡터
 */
inline std::vector<ServerBBox> parseServerBBoxes(const nlohmann::json& bboxArray) {
    std::vector<ServerBBox> boxes;
    for (const auto& value : bboxArray) {
        ServerBBox bbox;
        bbox.object_id = value.at("object_id").get<int>();
        bbox.type = value.at("type").get<std::string>();
        bbox.confidence = value.at("confidence").get<float>();
        bbox.left = value.at("left").get<int>();
        bbox.top = value.at("top").get<int>();
        bbox.right = value.at("right").get<int>();
        bbox.bottom = value.at("bottom").get<int>();
        boxes.push_back(bbox);
    }
    return boxes;
}



/**
 * @brief BBox와 타임스탬프를 함께 저장하는 구조체 (버퍼용)
 * @details 이 구조체는 BBox 데이터와 해당 데이터의 수신 시각을 함께 저장합니다.
 */
struct TimestampedBBox {
    std::chrono::steady_clock::time_point timestamp; ///< 데이터 수신 시각
    std::vector<ServerBBox> bboxes; ///< BBox 목록
};


/** @brief BBox 버퍼 지연 시간 (ms) */
extern std::atomic<int> bbox_buffer_delay_ms;
/** @brief BBox 전송 주기 (ms) */
extern std::atomic<int> bbox_send_interval_ms;
/** @brief BBox 버퍼 (타임스탬프 포함) */
extern std::queue<TimestampedBBox> bbox_buffer;
/** @brief bbox_buffer 보호용 뮤텍스 */
extern std::mutex bbox_buffer_mutex;

/** @brief 최근에 파싱된 BBox 목록 */
extern std::vector<ServerBBox> latest_bboxes;
/** @brief latest_bboxes 보호용 뮤텍스 */
extern std::mutex bbox_mutex;


/**
 * @brief 메타데이터 파서를 시작합니다.
 */
void start_metadata_parser();


/**
 * @brief 메타데이터 파서를 중지합니다.
 */
void stop_metadata_parser();


/**
 * @brief RTSP 스트림에서 메타데이터를 파싱하여 BBox 정보를 추출합니다.
 *        파싱된 BBox는 latest_bboxes와 버퍼에 저장됩니다.
 */
void parse_metadata();


/**
 * @brief 새로운 BBox 데이터를 타임스탬프와 함께 버퍼에 추가하고 오래된 데이터를 정리합니다.
 * @param new_bboxes 새로 파싱된 BBox 벡터
 */
void update_bbox_buffer(const std::vector<ServerBBox>& new_bboxes);

/**
 * @brief BBox 버퍼를 완전히 비웁니다.
 */
void clear_bbox_buffer();

/**
 * @brief 버퍼에서 BBox 데이터를 꺼내 클라이언트(SSL)로 전송합니다.
 * @param ssl OpenSSL SSL 포인터
 * @return 전송 성공 시 true, 실패 시 false
 */
bool send_bboxes_to_client(SSL* ssl);


#endif // METADATA_PARSER_HPP
