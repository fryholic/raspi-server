#include "metadata_parser.hpp"
#include <iostream>
#include <regex>
#include <cstdio>

#include <chrono>
#include <queue>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include "ssl.hpp"
#include "config_manager.hpp"



using namespace std;


/**
 * @brief 최근에 파싱된 BBox 목록 (스레드 안전)
 */
vector<ServerBBox> latest_bboxes;

/**
 * @brief latest_bboxes 보호용 뮤텍스
 */
mutex bbox_mutex;

/**
 * @brief 메타데이터 파서 실행 여부 플래그
 */
atomic<bool> parser_running(false);


/**
 * @brief BBox 버퍼 지연(ms), 기본값 2400ms
 */
std::atomic<int> bbox_buffer_delay_ms(2000);

/**
 * @brief BBox 전송 간격(ms), 기본값 50ms
 */
std::atomic<int> bbox_send_interval_ms(50);

/**
 * @brief BBox 버퍼 (타임스탬프 포함)
 */
std::queue<TimestampedBBox> bbox_buffer;

/**
 * @brief bbox_buffer 보호용 뮤텍스
 */
std::mutex bbox_buffer_mutex;


/**
 * @brief 메타데이터 파서를 시작합니다.
 */
void start_metadata_parser() {
    parser_running = true;
}


/**
 * @brief 메타데이터 파서를 중지합니다.
 */
void stop_metadata_parser() {
    parser_running = false;
}

/**
 * @brief RTSP 스트림에서 메타데이터를 파싱하여 BBox 정보를 추출합니다.
 *        파싱된 BBox는 latest_bboxes와 버퍼에 저장됩니다.
 */
void parse_metadata() {
    if (!parser_running) {
        return;
    }

    // 설정 로드 (이미 로드되어 있으면 무시됨)
    if (!load_all_config()) {
        cerr << "[Metadata] 설정 로드 실패" << endl;
        return;
    }

    // 설정에서 RTSP URL 가져오기
    string rtsp_url = get_rtsp_url();
    string cmd = "ffmpeg -i " + rtsp_url + " -map 0:1 -f data -";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        cerr << "[Metadata] ffmpeg 파이프 실행 실패" << endl;
        return;
    }

    constexpr int BUFFER_SIZE = 8192;
    char buffer[BUFFER_SIZE];
    string xml_buffer;

    // Updated regex for better group capturing
    std::regex object_regex("<tt:Object ObjectId=\"(\\d+)\">.*?<tt:BoundingBox left=\"(\\d+\\.?\\d*)\" top=\"(\\d+\\.?\\d*)\" right=\"(\\d+\\.?\\d*)\" bottom=\"(\\d+\\.?\\d*)\"/>(.*?)</tt:Object>");
    std::regex class_regex("<tt:ClassCandidate>\\s*<tt:Type>(\\w+)</tt:Type>\\s*<tt:Likelihood>([\\d\\.]+)</tt:Likelihood>");

    cout << "[MetadataParser] Started parsing metadata..." << endl;
    
    while (parser_running && !feof(pipe)) {
        size_t bytes = fread(buffer, 1, BUFFER_SIZE - 1, pipe);
        if (bytes <= 0) continue;
        buffer[bytes] = '\0';
        xml_buffer += buffer;
        
        cout << "[Debug] Read " << bytes << " bytes from ffmpeg pipe" << endl;

        size_t end_pos;
        while (parser_running && (end_pos = xml_buffer.find("</tt:MetadataStream>")) != string::npos) {
            string packet = xml_buffer.substr(0, end_pos);
            xml_buffer.erase(0, end_pos + strlen("</tt:MetadataStream>"));
            
            cout << "[Debug] Found MetadataStream packet, length: " << packet.length() << endl;
            cout << "[Debug] Packet content (first 200 chars): " << packet.substr(0, 200) << "..." << endl;

            vector<ServerBBox> parsed_boxes;
            sregex_iterator iter(packet.begin(), packet.end(), object_regex);
            sregex_iterator end;
            
            cout << "[Debug] Starting regex matching..." << endl;

            for (; iter != end; ++iter) {
                cout << "[Debug] Found Object match! ObjectId: " << (*iter)[1] << endl;
                ServerBBox box;
                // Corrected group indices based on the new regex
                box.object_id = stoi((*iter)[1]);
                
                // Extract coordinates and assign directly to ServerBBox members
                box.left = static_cast<int>(stof((*iter)[2]));
                box.top = static_cast<int>(stof((*iter)[3]));
                box.right = static_cast<int>(stof((*iter)[4]));
                box.bottom = static_cast<int>(stof((*iter)[5]));
                
                cout << "[Debug] BBox coords: left=" << box.left << ", top=" << box.top 
                     << ", right=" << box.right << ", bottom=" << box.bottom << endl;

                // Extract the inner content for class parsing
                string inner_content = (*iter)[6]; // Group 6 is the inner content

                smatch class_match;
                if (regex_search(inner_content, class_match, class_regex)) {
                    box.type = class_match[1]; // Use std::string directly
                    box.confidence = stof(class_match[2]);
                    cout << "[Debug] Found class info: type=" << box.type << ", confidence=" << box.confidence << endl;
                } else {
                    // Handle cases where class info might be missing or not matched
                    box.type = "Unknown";
                    box.confidence = 0.0f;
                    cout << "[Debug] No class info found, using defaults" << endl;
                }

                parsed_boxes.push_back(box);
                cout << "[Debug] Added box to parsed_boxes. Total count: " << parsed_boxes.size() << endl;
            }
            
            cout << "[Debug] Finished processing packet. Total boxes found: " << parsed_boxes.size() << endl;

            // 항상 버퍼에 데이터 추가 (빈 박스 포함)
            update_bbox_buffer(parsed_boxes);
            if (parsed_boxes.empty()) {
                cout << "[MetadataParser] Added empty box data to buffer (no objects detected)." << endl;
            } else {
                cout << "[MetadataParser] Added " << parsed_boxes.size() << " boxes to buffer." << endl;
            }
            
            // 기존 latest_bboxes도 호환성을 위해 유지
            lock_guard<mutex> lock(bbox_mutex);
            latest_bboxes = move(parsed_boxes);
        }
    }

    pclose(pipe);
}

// ==================== BBox 버퍼 관리 함수들 ====================

/**
 * @brief BBox 버퍼를 완전히 비웁니다.
 */
void clear_bbox_buffer() {
    std::lock_guard<std::mutex> lock(bbox_buffer_mutex);
    
    // 버퍼의 모든 데이터 제거
    std::queue<TimestampedBBox> empty_queue;
    bbox_buffer.swap(empty_queue);
    
    std::cout << "[BUFFER] Buffer cleared completely" << std::endl;
}

/**
 * @brief 새로운 BBox 데이터를 타임스탬프와 함께 버퍼에 추가하고 오래된 데이터를 정리합니다.
 * @param new_bboxes 새로 파싱된 BBox 벡터
 */
void update_bbox_buffer(const std::vector<ServerBBox>& new_bboxes) {
    std::lock_guard<std::mutex> lock(bbox_buffer_mutex);
    
    // 새로운 bbox 데이터를 타임스탬프와 함께 버퍼에 저장
    TimestampedBBox data{
        std::chrono::steady_clock::now(),
        new_bboxes
    };
    bbox_buffer.push(data);
    
    // 버퍼 크기 관리 (너무 많은 데이터가 쌓이지 않도록)
    auto now = std::chrono::steady_clock::now();
    int removed_count = 0;
    
    // 1. 10초보다 오래된 데이터 제거
    while (!bbox_buffer.empty()) {
        const auto& oldest = bbox_buffer.front();
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - oldest.timestamp).count();
            
        if (age > 10000) {  // 10초보다 오래된 데이터 제거
            bbox_buffer.pop();
            removed_count++;
        } else {
            break;
        }
    }
    
    // 2. 버퍼 크기가 너무 크면 강제로 오래된 데이터 제거 (최대 50개 유지)
    while (bbox_buffer.size() > 50) {
        bbox_buffer.pop();
        removed_count++;
    }
    
    // 디버그 로그
    if (removed_count > 0 || bbox_buffer.size() > 10) {
        std::cout << "[BUFFER] Added 1 item, removed " << removed_count 
                 << " items, current size: " << bbox_buffer.size() << std::endl;
    }
}

/**
 * @brief 버퍼에서 BBox 데이터를 꺼내 클라이언트(SSL)로 전송합니다.
 * @param ssl OpenSSL SSL 포인터
 * @return 전송 성공 시 true, 실패 시 false
 */
bool send_bboxes_to_client(SSL* ssl) {
    std::vector<ServerBBox> bboxes_to_send;
    int buffer_size = 0;
    int processed_count = 0;
    
    {
        std::lock_guard<std::mutex> lock(bbox_buffer_mutex);
        
        auto now = std::chrono::steady_clock::now();
        buffer_size = bbox_buffer.size();
        
        // 버퍼에서 지연 시간이 지난 가장 오래된 데이터 찾기
        while (!bbox_buffer.empty()) {
            const auto& oldest = bbox_buffer.front();
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - oldest.timestamp).count();
                
            if (age >= bbox_buffer_delay_ms.load()) {
                bboxes_to_send = oldest.bboxes;
                bbox_buffer.pop();
                processed_count++;
                break;  // 하나만 처리하고 나감
            }
            break;  // 지연 시간이 지나지 않은 데이터는 아직 전송하지 않음
        }
        
        // 디버그 로그 추가
        if (buffer_size > 5) {  // 버퍼에 5개 이상 쌓이면 경고
            std::cout << "[DEBUG] Buffer getting large: " << buffer_size 
                     << " items, processed: " << processed_count << std::endl;
        }
    }
    
    if (bboxes_to_send.empty()) {
        return true;  // 전송할 데이터가 없음 (에러 아님)
    }

    nlohmann::json bbox_array = nlohmann::json::array();
    for (const ServerBBox& box : bboxes_to_send) {
        nlohmann::json j = {
            {"id", box.object_id},
            {"type", box.type},
            {"confidence", box.confidence},
            {"x", box.left},
            {"y", box.top},
            {"width", box.right - box.left},
            {"height", box.bottom - box.top}
        };
        bbox_array.push_back(j);
    }
    
    nlohmann::json response = {
        {"response_id", 200},
        {"bboxes", bbox_array},
        {"buffer_info", {
            {"buffer_size", buffer_size},
            {"processed_count", processed_count}
        }}
    };

    std::string json_str = response.dump();
    uint32_t res_len = json_str.length();
    uint32_t net_res_len = htonl(res_len);

    // 먼저 4바이트 길이 접두사 전송
    if (sendAll(ssl, reinterpret_cast<const char*>(&net_res_len), sizeof(net_res_len), 0) == -1) {
        std::cout << "[TCP Server] Failed to send length prefix" << std::endl;
        return false;
    }
    // 그 다음 실제 JSON 데이터 전송
    int ret = sendAll(ssl, json_str.c_str(), res_len, 0);
    if (ret == -1) {
        std::cout << "[TCP Server] Failed to send JSON data" << std::endl;
    }
    return ret != -1;
}