이 콘솔 프로그램은 **회원 가입**, **로그인**, **QR 코드 생성**, **복구 코드 제공**, **OTP 인증**의 기능을 제공하며, 다음과 같은 구조로 작성되었습니다.

---

## **1. 주요 파일 구조**
프로그램은 다음과 같은 파일들로 구성되어 있습니다:

1. **main.cpp**: 프로그램의 진입점으로, 사용자와의 상호작용을 처리합니다.
2. **`user_manager.hpp` / `user_manager.cpp`**: 사용자 관리(회원 가입, 로그인, 복구 코드 생성)를 담당합니다.
3. **`otp_manager.hpp` / `otp_manager.cpp`**: OTP 생성, QR 코드 생성 및 OTP 검증을 담당합니다.
4. **Makefile**: 프로그램을 빌드하기 위한 Makefile입니다.

---

## **2. main.cpp**
### **역할**
- 사용자와의 상호작용을 처리하며, 메뉴를 표시하고 사용자의 선택에 따라 적절한 작업을 수행합니다.

### **구조**
1. **메뉴 표시**:
   ```cpp
   void display_menu() {
       cout << "\n=== OTP Console Program ===\n";
       cout << "1. Sign Up\n";
       cout << "2. Log In\n";
       cout << "3. Exit\n";
       cout << "Select an option: ";
   }
   ```

2. **회원 가입**:
   - 사용자 ID와 비밀번호를 입력받아 `UserManager`를 통해 회원 가입을 처리합니다.
   - OTP URI를 생성하고 QR 코드를 파일로 저장합니다.
   - 복구 코드를 생성하여 사용자에게 출력합니다.
   ```cpp
   if (user_manager.sign_up(id, pw)) {
       string otp_uri = otp_manager.generate_otp_uri(id);
       string qr_file = otp_manager.generate_qr_code(id, otp_uri);
       auto recovery_codes = user_manager.generate_recovery_codes(id);
   }
   ```

3. **로그인**:
   - 사용자 ID와 비밀번호를 입력받아 `UserManager`를 통해 인증합니다.
   - OTP를 입력받아 `OTPManager`를 통해 검증합니다.
   ```cpp
   if (user_manager.log_in(id, pw)) {
       if (otp_manager.verify_otp(id, otp)) {
           cout << "OTP verified! Login complete.\n";
       }
   }
   ```

4. **프로그램 종료**:
   - 사용자가 메뉴에서 "3"을 선택하면 프로그램을 종료합니다.

---

## **3. `user_manager.hpp` / `user_manager.cpp`**
### **역할**
- 사용자 데이터를 관리하며, 회원 가입, 로그인, 복구 코드 생성을 처리합니다.

### **주요 메서드**
1. **`sign_up`**:
   - 사용자 ID와 비밀번호를 저장합니다.
   - 이미 존재하는 사용자라면 실패를 반환합니다.

2. **`log_in`**:
   - 사용자 ID와 비밀번호를 확인하여 인증합니다.

3. **`generate_recovery_codes`**:
   - 복구 코드를 생성하여 반환합니다.

---

## **4. `otp_manager.hpp` / `otp_manager.cpp`**
### **역할**
- OTP 생성, QR 코드 생성, OTP 검증을 처리합니다.

### **주요 메서드**
1. **`generate_otp_uri`**:
   - 사용자 ID를 기반으로 OTP URI를 생성합니다.
   - URI는 Google Authenticator와 같은 앱에서 사용할 수 있습니다.

2. **`generate_qr_code`**:
   - OTP URI를 기반으로 QR 코드를 생성하고 SVG 파일로 저장합니다.
   - QR 코드는 현재 실행 폴더에 저장됩니다.

3. **`verify_otp`**:
   - 사용자가 입력한 OTP를 검증합니다.

---

## **5. Makefile**
### **역할**
- 프로그램을 빌드하고 실행 파일을 생성합니다.

### **구조**
1. **컴파일러 설정**:
   ```makefile
   CXX = g++
   CXXFLAGS = -std=c++17 -I../src -I../src/cotp -I../deps/QR-Code-generator
   LDFLAGS = -lssl -lcrypto
   ```

2. **소스 파일 및 객체 파일 정의**:
   ```makefile
   SRC = main.cpp user_manager.cpp otp_manager.cpp ../src/cotp.cpp ../src/otp_uri.cpp ../src/otp_factory.cpp ../src/qr_code.cpp ../deps/QR-Code-generator/qrcodegen.cpp
   OBJ = $(SRC:.cpp=.o)
   ```

3. **빌드 규칙**:
   ```makefile
   all: otp_console
   otp_console: $(OBJ)
       $(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
   ```

4. **클린 규칙**:
   ```makefile
   clean:
       rm -f $(OBJ) otp_console
   ```

---

## **6. 프로그램 실행 흐름**
1. **프로그램 시작**:
   - 메뉴를 표시하고 사용자의 선택을 기다립니다.

2. **회원 가입**:
   - 사용자 ID와 비밀번호를 입력받아 저장합니다.
   - OTP URI를 생성하고 QR 코드를 파일로 저장합니다.
   - 복구 코드를 생성하여 출력합니다.

3. **로그인**:
   - 사용자 ID와 비밀번호를 입력받아 인증합니다.
   - OTP를 입력받아 검증합니다.

4. **프로그램 종료**:
   - 사용자가 "3"을 선택하면 프로그램이 종료됩니다.

---

## **7. 주요 기능 요약**
- **회원 가입**:
  - 사용자 ID와 비밀번호를 저장.
  - OTP URI 생성 및 QR 코드 저장.
  - 복구 코드 생성.

- **로그인**:
  - 사용자 ID와 비밀번호 인증.
  - OTP 검증.

- **QR 코드 생성**:
  - OTP URI를 기반으로 QR 코드를 생성하고 SVG 파일로 저장.

- **복구 코드 제공**:
  - OTP를 사용할 수 없는 경우를 대비해 복구 코드를 생성.

---

이 프로그램은 Google Authenticator와 같은 앱과 호환되며, OTP 기반의 2단계 인증을 구현하는 데 적합합니다. 추가적인 기능이나 수정이 필요하면 알려주세요!