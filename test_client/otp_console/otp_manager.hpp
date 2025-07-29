#ifndef OTP_MANAGER_HPP
#define OTP_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <memory>
#include "cotp/cotp.hpp"

class OTPManager {
private:
    std::unordered_map<std::string, std::unique_ptr<cotp::TOTP>> user_otps;

public:
    std::string generate_otp_uri(const std::string& id);
    bool verify_otp(const std::string& id, int otp);
    std::string generate_qr_code(const std::string& id, const std::string& otp_uri);
};

#endif
