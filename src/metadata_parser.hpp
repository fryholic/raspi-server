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

// Server-side BBox structure
struct ServerBBox {
    int object_id;
    std::string type;
    float confidence;
    int left;
    int top;
    int right;
    int bottom;
};

// Server-side parseBBoxes function
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

// BBox 버퍼링을 위한 구조체
struct TimestampedBBox {
    std::chrono::steady_clock::time_point timestamp;
    std::vector<ServerBBox> bboxes;
};

// 전역 변수 선언
extern std::atomic<int> bbox_buffer_delay_ms;  // 버퍼 지연 시간 (M ms)
extern std::atomic<int> bbox_send_interval_ms; // 전송 주기 (N ms)
extern std::queue<TimestampedBBox> bbox_buffer;
extern std::mutex bbox_buffer_mutex;

extern std::vector<ServerBBox> latest_bboxes;
extern std::mutex bbox_mutex;

void start_metadata_parser();

void stop_metadata_parser();

void parse_metadata();

// BBox 버퍼 관리 함수들
void update_bbox_buffer(const std::vector<ServerBBox>& new_bboxes);
void clear_bbox_buffer();
bool send_bboxes_to_client(SSL* ssl);


#endif // METADATA_PARSER_HPP
