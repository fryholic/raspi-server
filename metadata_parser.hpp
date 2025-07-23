#ifndef METADATA_PARSER_HPP
#define METADATA_PARSER_HPP

#include <vector>
#include <string>
#include <mutex>

#include <cstring>

struct BBox {
    int object_id;
    float left;
    float top;
    float right;
    float bottom;
    float confidence = -1.0f;
    std::string type = "Unknown";
};

extern std::vector<BBox> latest_bboxes;
extern std::mutex bbox_mutex;

void parse_metadata();

#endif // METADATA_PARSER_HPP
