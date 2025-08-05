#ifndef USER_MANAGER_HPP
#define USER_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <vector>

class UserManager
{
private:
    struct User
    {
        std::string password;
        std::vector<std::string> recovery_codes;
    };

    std::unordered_map<std::string, User> users;

public:
    bool sign_up(const std::string& id, const std::string& password);
    bool log_in(const std::string& id, const std::string& password);
    std::vector<std::string> generate_recovery_codes(const std::string& id);
    bool verify_recovery_code(const std::string& id, const std::string& code);
};

#endif
