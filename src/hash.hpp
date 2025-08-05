/**
 * @file hash.hpp
 * @brief 비밀번호 및 복구 코드 해싱 헤더 파일
 * @details 이 파일은 Argon2id 알고리즘을 사용하여 비밀번호와 복구 코드를 해싱하고 검증하는 함수의 선언을 포함합니다.
 * 또한 복구 코드를 생성하는 함수도 선언되어 있습니다.
 */

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

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

/**
 * @brief 복구 코드를 생성합니다.
 *
 * @return 10자리의 랜덤한 알파벳 대소문자와 숫자로 이루어진 복구 코드 문자열 벡터.
 */
std::vector<std::string> generate_recovery_codes();