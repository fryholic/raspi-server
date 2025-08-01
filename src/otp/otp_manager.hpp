#ifndef OTP_MANAGER_HPP
#define OTP_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <memory>
#include "cotp/cotp.hpp"
#include "cotp/qr_code.hpp"

class OTPManager {
private:
    std::unordered_map<std::string, std::unique_ptr<cotp::TOTP>> user_otps;
    std::unordered_map<std::string, std::string> user_secrets;

public:
    std::string generate_otp_uri(const std::string& id);
    bool verify_otp(const std::string& id, int otp);
    void register_otp(const std::string& id, const std::string& secret);
    std::string generate_qr_code_svg(const std::string& otp_uri);
    std::string get_secret(const std::string& id);
};

#endif
