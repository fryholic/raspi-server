/**
 * @file utils.cpp
 * @brief 유틸리티 함수들의 구현
 * @details 시간 출력, Base64 인코딩, 보안 메모리 정리 등의 유틸리티 기능을 제공합니다.
 */

#include "utils.hpp"
#include <cstring>

/**
 * @brief 현재 시간을 KST(한국 표준시) 기준으로 밀리초 단위까지 출력합니다.
 * @details 시스템 시간을 KST로 변환하여 YYYY-MM-DD HH:MM:SS.mmm KST 형식으로 출력합니다.
 */
void printNowTimeKST()
{
    // 한국 시간 (KST), 밀리초 포함 출력
    chrono::system_clock::time_point now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    auto duration_since_epoch = now.time_since_epoch();
    auto milliseconds = chrono::duration_cast<chrono::milliseconds>(duration_since_epoch).count() % 1000;
    const long KST_OFFSET_SECONDS = 9 * 60 * 60;
    time_t kst_now_c = now_c + KST_OFFSET_SECONDS;
    tm* kst_tm = gmtime(&kst_now_c);
    cout << "\n[" << put_time(kst_tm, "%Y-%m-%d %H:%M:%S") << "." << setfill('0') << setw(3) << milliseconds << " KST]"
         << endl;
}

/**
 * @brief 바이트 배열을 Base64 문자열로 인코딩합니다.
 * @param in 인코딩할 바이트 배열
 * @return Base64로 인코딩된 문자열
 * @details RFC 4648에 따른 표준 Base64 인코딩을 수행합니다.
 *          패딩 문자('=')를 포함하여 4의 배수 길이로 맞춥니다.
 */
string base64_encode(const vector<unsigned char>& in)
{
    string out;
    const string b64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789+/";

    int val = 0, valb = -6;
    for (unsigned char c : in)
    {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            out.push_back(b64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(b64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');
    return out;
}

/**
 * @brief 비밀번호 문자열을 안전하게 메모리에서 지웁니다.
 * @param passwd 지울 비밀번호 문자열 참조
 * @details 메모리 덤프나 스왑 파일에서 비밀번호가 노출되는 것을 방지하기 위해
 *          메모리를 0으로 덮어쓴 후 문자열을 비웁니다.
 */
void secure_clear_password(string& passwd)
{
    if (!passwd.empty())
    {
        memset(&passwd[0], 0, passwd.length());
        passwd.clear();
    }
}
