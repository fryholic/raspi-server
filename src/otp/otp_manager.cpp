#include "otp_manager.hpp"
#include "cotp/qr_code.hpp"
#include <fstream>

std::unordered_map<std::string, std::unique_ptr<cotp::TOTP>> user_otps;

std::string OTPManager::generate_otp_uri(const std::string& id) {
    std::string secret = cotp::OTP::random_base32(16);
    auto totp = std::make_unique<cotp::TOTP>(secret, "SHA1", 6, 30);

    totp->set_account(id);
    totp->set_issuer("OTP Console Program");
    std::string uri = totp->build_uri();

    user_otps[id] = std::move(totp);
    user_secrets[id] = secret; // secret 저장
    return uri;
}

bool OTPManager::verify_otp(const std::string& id, int otp) {
    auto it = user_otps.find(id);
    if (it != user_otps.end()) {
        return it->second->verify(otp, time(nullptr), 0);
    }
    return false;
}

std::string OTPManager::generate_qr_code_svg(const std::string& otp_uri) {
    cotp::QR_code qr;
    qr.set_content(otp_uri);
    return qr.get_svg();
}

std::string OTPManager::get_secret(const std::string& id) {
    auto it = user_secrets.find(id);
    if (it != user_secrets.end()) {
        return it->second;
    }
    return "";
}

void OTPManager::register_otp(const std::string& id, const std::string& secret) {
    if (secret.empty()) return;
    auto totp = std::make_unique<cotp::TOTP>(secret, "SHA1", 6, 30);
    totp->set_account(id);
    totp->set_issuer("OTP Console Program");
    user_otps[id] = std::move(totp);
    user_secrets[id] = secret;
}