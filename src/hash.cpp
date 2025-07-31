#include "hash.hpp"


/*
=================================================================
 * 비밀번호 해싱 및 검증 함수들
 * 이 부분은 Argon2id 알고리즘을 사용하여 비밀번호를 안전하게 해싱하고 검증합니다.
 * Libsodium 라이브러리를 사용합니다.
 * 
 * 주의: 이 코드는 libsodium이 설치되어 있어야 하며, C++ 프로젝트에
 *       libsodium 헤더와 라이브러리를 포함해야 합니다.
 * 
 * 예시:
 *   string hashed = hash_password("my_secure_password");
 *   bool is_valid = verify_password(hashed, "my_secure_password");
==================================================================
*/

/**
 * @brief Argon2id를 사용하여 비밀번호를 해싱합니다.
 *
 * @param password 평문 비밀번호
 * @return 해싱된 비밀번호 문자열
 */
string hash_password(const string& password) {
    char hashed_password[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(hashed_password, password.c_str(), password.length(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw runtime_error("비밀번호 해싱 실패");
    }

    return string(hashed_password);
}

/**
 * @brief Argon2id를 사용하여 비밀번호를 검증합니다.
 *
 * @param hashed_password 해싱된 비밀번호
 * @param password 평문 비밀번호
 * @return 비밀번호가 일치하면 true, 그렇지 않으면 false
 */
bool verify_password(const string& hashed_password, const string& password) {
    cout << "[Debug] Hashed password: " << hashed_password << endl;
    cout << "[Debug] Plain password: " << password << endl;
    int result = crypto_pwhash_str_verify(hashed_password.c_str(), password.c_str(),
                                          password.length());
    cout << "[Debug] Password verification result: " << (result == 0 ? "Match" : "Mismatch") << endl;
    return result == 0;
}

/**
 * @brief 복구 코드를 해싱합니다.
 *
 * @param codes 복구 코드 문자열 벡터
 * @return 해싱된 복구 코드 문자열 벡터
 */
std::vector<std::string> hash_recovery_codes(const std::vector<std::string>& codes) {
    std::vector<std::string> hashed_codes;
    for (const auto& code : codes) {
        hashed_codes.push_back(hash_password(code));
    }
    return hashed_codes;
}

/**
 * @brief 복구 코드를 생성합니다.
 *
 * @return 10자리의 랜덤한 알파벳 대소문자와 숫자로 이루어진 복구 코드 문자열 벡터
 */
std::vector<std::string> generate_recovery_codes() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::vector<std::string> codes;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

    for (int i = 0; i < 5; ++i) {
        std::string code;
        for (int j = 0; j < 10; ++j) {
            code += alphanum[dis(gen)];
        }
        codes.push_back(code);
    }
    return codes;
}