#include "utils.hpp"
#include <cstring>

void printNowTimeKST() {
    // 한국 시간 (KST), 밀리초 포함 출력
    chrono::system_clock::time_point now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    auto duration_since_epoch = now.time_since_epoch();
    auto milliseconds =
        chrono::duration_cast<chrono::milliseconds>(duration_since_epoch)
            .count() %
        1000;
    const long KST_OFFSET_SECONDS = 9 * 60 * 60;
    time_t kst_now_c = now_c + KST_OFFSET_SECONDS;
    tm* kst_tm = gmtime(&kst_now_c);
    cout << "\n[" << put_time(kst_tm, "%Y-%m-%d %H:%M:%S") << "." << setfill('0')
         << setw(3) << milliseconds << " KST]" << endl;
}

string base64_encode(const vector<unsigned char>& in) {
    string out;
    const string b64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

void secure_clear_password(string& passwd) {
    if (!passwd.empty()) {
        memset(&passwd[0], 0, passwd.length());
        passwd.clear();
    }
}
