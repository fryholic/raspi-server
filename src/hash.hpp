#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

// Argon2 해싱을 위한 라이브러리
#include <sodium.h>


using namespace std;

string hash_password(const string& password);

bool verify_password(const string& hashed_password, const string& password);

std::vector<std::string> hash_recovery_codes(const std::vector<std::string>& codes);