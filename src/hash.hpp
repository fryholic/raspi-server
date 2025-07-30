#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

// Argon2 해싱을 위한 라이브러리
#include <sodium.h>


using namespace std;

/**
 * @brief Argon2id를 사용하여 비밀번호를 해싱합니다.
 *
 * @param password 평문 비밀번호.
 * @return 해싱된 비밀번호 문자열.
 */
string hash_password(const string& password);

/**
 * @brief Argon2id를 사용하여 비밀번호를 검증합니다.
 *
 * @param hashed_password 해싱된 비밀번호.
 * @param password 평문 비밀번호.
 * @return 비밀번호가 일치하면 true, 그렇지 않으면 false.
 */
bool verify_password(const string& hashed_password, const string& password);

/**
 * @brief 복구 코드를 해싱합니다.
 *
 * @param codes 복구 코드 문자열 벡터.
 * @return 해싱된 복구 코드 문자열 벡터.
 */
std::vector<std::string> hash_recovery_codes(const std::vector<std::string>& codes);