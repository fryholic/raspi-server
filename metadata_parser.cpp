#include "metadata_parser.hpp"
#include <iostream>
#include <regex>
#include <cstdio>

using namespace std;

vector<BBox> latest_bboxes;
mutex bbox_mutex;

void parse_metadata() {
    const string cmd =
        "ffmpeg -i rtsp://admin:admin123@192.168.0.46:554/0/onvif/profile2/media.smp "
        "-map 0:1 -f data -";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        cerr << "[Metadata] ffmpeg 파이프 실행 실패" << endl;
        return;
    }

    constexpr int BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];
    string xml_buffer;

    // 전체 객체 블록 정규식 (BoundingBox + 내부 전체 블록 포함)
    std::regex object_regex(
        R"(<tt:Object ObjectId=\"(\d+)\">[\s\S]*?<tt:BoundingBox left=\"(\d+\.?\d*)\" top=\"(\d+\.?\d*)\" right=\"(\d+\.?\d*)\" bottom=\"(\d+\.?\d*)\"/>([\s\S]*?)</tt:Object>)"
    );
    std::regex class_regex(
        R"(<tt:ClassCandidate>\s*<tt:Type>(\w+)</tt:Type>\s*<tt:Likelihood>([\d\.]+)</tt:Likelihood>)"
    );

    while (!feof(pipe)) {
        size_t bytes = fread(buffer, 1, BUFFER_SIZE - 1, pipe);
        if (bytes <= 0) continue;
        buffer[bytes] = '\0';
        xml_buffer += buffer;

        size_t end_pos;
        while ((end_pos = xml_buffer.find("</tt:MetadataStream>")) != string::npos) {
            string packet = xml_buffer.substr(0, end_pos);
            xml_buffer.erase(0, end_pos + strlen("</tt:MetadataStream>"));

            vector<BBox> parsed_boxes;
            sregex_iterator iter(packet.begin(), packet.end(), object_regex);
            sregex_iterator end;

            for (; iter != end; ++iter) {
                BBox box;
                box.object_id = stoi((*iter)[1]);
                string full_block = (*iter)[2].str() + (*iter)[6].str();  // 전체 블록
                box.left = stof((*iter)[3]);
                box.top = stof((*iter)[4]);
                box.right = stof((*iter)[5]);
                box.bottom = stof((*iter)[6]);

                smatch class_match;
                if (regex_search(full_block, class_match, class_regex)) {
                    box.type = class_match[1];
                    box.confidence = stof(class_match[2]);
                }

                parsed_boxes.push_back(box);
            }

            if (!parsed_boxes.empty()) {
                lock_guard<mutex> lock(bbox_mutex);
                latest_bboxes = move(parsed_boxes);
            }
        }
    }

    pclose(pipe);
}
