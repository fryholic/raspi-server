#include <iostream>
#include "user_manager.hpp"
#include "otp_manager.hpp"

using namespace std;

void display_menu() {
    cout << "\n=== OTP Console Program ===\n";
    cout << "1. Sign Up\n";
    cout << "2. Log In\n";
    cout << "3. Exit\n";
    cout << "Select an option: ";
}

int main() {
    UserManager user_manager;
    OTPManager otp_manager;

    while (true) {
        display_menu();
        int choice;
        cin >> choice;

        if (choice == 1) {
            string id, pw;
            cout << "Enter ID: ";
            cin >> id;
            cout << "Enter Password: ";
            cin >> pw;

            if (user_manager.sign_up(id, pw)) {
                cout << "Sign-up successful!\n";

                // Generate QR Code for OTP
                string otp_uri = otp_manager.generate_otp_uri(id);
                cout << "Scan this QR code with your authenticator app:\n";
                cout << otp_uri << endl;

                // Save QR Code to file
                string qr_file = otp_manager.generate_qr_code(id, otp_uri);
                cout << "QR Code saved to: " << qr_file << endl;

                // Generate recovery codes
                auto recovery_codes = user_manager.generate_recovery_codes(id);
                cout << "Your recovery codes are:\n";
                for (const auto& code : recovery_codes) {
                    cout << code << endl;
                }
            } else {
                cout << "Sign-up failed. User already exists.\n";
            }
        } else if (choice == 2) {
            string id, pw;
            cout << "Enter ID: ";
            cin >> id;
            cout << "Enter Password: ";
            cin >> pw;

            if (user_manager.log_in(id, pw)) {
                cout << "Login successful!\n";

                // OTP or Recovery Code Verification
                cout << "Enter OTP or Recovery Code: ";
                string input;
                cin >> input;

                if (isdigit(input[0])) {
                    // Handle OTP
                    int otp = std::stoi(input);
                    if (otp_manager.verify_otp(id, otp)) {
                        cout << "OTP verified! Login complete.\n";
                    } else {
                        cout << "Invalid OTP.\n";
                    }
                } else {
                    // Handle Recovery Code
                    if (user_manager.verify_recovery_code(id, input)) {
                        cout << "Recovery code verified! Login complete.\n";
                    } else {
                        cout << "Invalid recovery code.\n";
                    }
                }
            } else {
                cout << "Login failed. Invalid ID or Password.\n";
            }
        } else if (choice == 3) {
            cout << "Exiting program.\n";
            break;
        } else {
            cout << "Invalid option. Try again.\n";
        }
    }

    return 0;
}
