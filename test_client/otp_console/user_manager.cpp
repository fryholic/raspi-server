#include "user_manager.hpp"
#include <random>
#include <algorithm>

bool UserManager::sign_up(const std::string& id, const std::string& password) {
    if (users.find(id) != users.end()) {
        return false; // User already exists
    }

    users[id] = {password, {}};
    return true;
}

bool UserManager::log_in(const std::string& id, const std::string& password) {
    auto it = users.find(id);
    if (it != users.end() && it->second.password == password) {
        return true;
    }
    return false;
}

std::vector<std::string> UserManager::generate_recovery_codes(const std::string& id) {
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

    users[id].recovery_codes = codes;
    return codes;
}

bool UserManager::verify_recovery_code(const std::string& id, const std::string& code) {
    auto it = users.find(id);
    if (it != users.end()) {
        auto& recovery_codes = it->second.recovery_codes;
        auto code_it = std::find(recovery_codes.begin(), recovery_codes.end(), code);
        if (code_it != recovery_codes.end()) {
            recovery_codes.erase(code_it);
            return true;
        }
    }
    return false;
}
