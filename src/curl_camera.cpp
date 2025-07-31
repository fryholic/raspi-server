#include "curl_camera.hpp"

// --- 응답 데이터를 저장할 콜백 함수 ---
// libcurl은 데이터를 작은 청크로 나눠서 이 함수를 여러 번 호출해요.
// userp 인자는 CURLOPT_WRITEDATA로 설정한 포인터를 받아요.
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  // userp는 std::string* 타입으로 캐스팅하여 응답 본문에 데이터를 추가해요.
  ((std::string*)userp)->append((char*)contents, size * nmemb);
  return size * nmemb;
}

string getLines() {
  string response_buffer;
  try {
    CURL* curl_handle;  // curl 통신을 위한 핸들

    // 2. curl_easy 핸들 생성
    // 각 통신 요청마다 이 핸들을 생성하고 설정해요.
    curl_handle = curl_easy_init();
    if (!curl_handle) {
      std::cerr << "curl_easy_init() 실패" << std::endl;
      curl_global_cleanup();
      return NULL;
    }
    // --- curl 명령어 옵션 설정 시작 ---
    // 2.1. URL 설정
    // 'https://192.168.0.137/opensdk/WiseAI/configuration/linecrossing'
    curl_easy_setopt(
        curl_handle, CURLOPT_URL,
        "https://192.168.0.137/opensdk/WiseAI/configuration/linecrossing");

    // 2.2. HTTP 메서드 설정 (GET)
    // -X GET 옵션은 CURLOPT_CUSTOMREQUEST로도 설정 가능하지만,
    // 기본이 GET이라 명시적으로 할 필요는 거의 없어요.
    // curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "GET");

    // 2.3. 인증 설정 (--digest -u admin:admin123@)
    // 다이제스트 인증 (CURLAUTH_DIGEST)과 사용자명:비밀번호 설정
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
    curl_easy_setopt(curl_handle, CURLOPT_USERPWD, "admin:admin123@");

    // 2.4. HTTPS 인증서 검증 비활성화 (--insecure)
    // 경고: 개발/테스트 환경에서만 사용하세요. 실제 운영 환경에서는 보안상 매우
    // 위험합니다!
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER,
                     0L);  // 피어(서버) 인증서 검증 안 함
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST,
                     0L);  // 호스트 이름 검증 안 함 (0L은 이전 버전과의
                           // 호환성을 위해 사용됨)
    // 최신 curl에서는 CURLOPT_SSL_VERIFYHOST를 0으로 설정하면 경고를 낼 수
    // 있으며, 1 (CA 파일 검증) 또는 2 (호스트네임과 CA 모두 검증)를 권장합니다.

    // 2.5. 커스텀 헤더 설정 (-H 옵션들)
    // struct curl_slist를 사용하여 헤더 목록을 만들고 추가해요.
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(
        headers, "Cookie: TRACKID=0842ca6f0d90294ea7de995c40a4aac6");
    headers = curl_slist_append(headers, "Origin: https://192.168.0.137");
    headers = curl_slist_append(
        headers,
        "Referer: "
        "https://192.168.0.137/home/setup/opensdk/html/WiseAI/index.html");
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER,
                     headers);  // 헤더 목록을 curl에 설정

    // 2.6. 압축 지원 (--compressed)
    // Accept-Encoding 헤더를 자동으로 추가하여 서버가 압축된 응답을 보내도록
    // 요청해요.
    curl_easy_setopt(
        curl_handle, CURLOPT_ACCEPT_ENCODING,
        "");  // 빈 문자열로 설정하면 curl이 지원하는 압축 방식을 모두 보냄

    // 2.7. 응답 본문을 프로그램 내에서 받기 위한 콜백 함수 설정
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA,
                     &response_buffer);  // 콜백 함수에 전달할 사용자 정의
                                         // 포인터 (응답 버퍼의 주소)

    // 2.8. 자세한 로그 출력 설정 (-v)
    // curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L); // 이 줄을 활성화하면
    // curl의 자세한 디버그 출력이 stderr로 나옴

    // --- curl 명령어 옵션 설정 끝 ---

    // 3. 요청 실행
    CURLcode res_perform = curl_easy_perform(curl_handle);

    // 4. 결과 확인
    if (res_perform != CURLE_OK) {
      std::cerr << "curl_easy_perform() 실패: "
                << curl_easy_strerror(res_perform) << std::endl;
    } else {
      long http_code = 0;
      curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE,
                        &http_code);  // HTTP 응답 코드 얻기

      std::cout << "--- HTTP 응답 수신 ---" << std::endl;
      std::cout << "HTTP Status Code: " << http_code << std::endl;
      std::cout << "--- 응답 본문 ---" << std::endl;
      std::cout << response_buffer << std::endl;
      std::cout << "------------------" << std::endl;
    }

    // 5. 자원 해제
    curl_easy_cleanup(curl_handle);  // curl 핸들 해제
    curl_slist_free_all(headers);  // 설정했던 헤더 목록 해제 (중요!)

  } catch (const std::exception& e) {
    // 예외 처리 (예: new 실패 등)
    std::cerr << "예외 발생: " << e.what() << std::endl;
  }
  return response_buffer;
}

string putLines(CrossLine crossLine) {
  string response_buffer;
  try {
    CURL* curl_handle;  // curl 통신을 위한 핸들

    // 2. curl_easy 핸들 생성
    // 각 통신 요청마다 이 핸들을 생성하고 설정해요.
    curl_handle = curl_easy_init();
    if (!curl_handle) {
      std::cerr << "curl_easy_init() 실패" << std::endl;
      curl_global_cleanup();
      return NULL;
    }
    // --- curl 명령어 옵션 설정 시작 ---
    // 2.1. URL 설정
    // 'https://192.168.0.137/opensdk/WiseAI/configuration/linecrossing'
    curl_easy_setopt(
        curl_handle, CURLOPT_URL,
        "https://192.168.0.137/opensdk/WiseAI/configuration/linecrossing");

    curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");

    // 2.3. 인증 설정 (--digest -u admin:admin123@)
    // 다이제스트 인증 (CURLAUTH_DIGEST)과 사용자명:비밀번호 설정
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
    curl_easy_setopt(curl_handle, CURLOPT_USERPWD, "admin:admin123@");

    json curlRoot;
    curlRoot["channel"] = 0;
    curlRoot["enable"] = true;
    json lineArray = json::array();
    json line1;
    line1["index"] = crossLine.index;

    json coodArray = json::array();
    json cood1;
    cood1["x"] = crossLine.x1;
    cood1["y"] = crossLine.y1;
    coodArray.push_back(cood1);
    json cood2;
    cood2["x"] = crossLine.x2;
    cood2["y"] = crossLine.y2;
    coodArray.push_back(cood2);

    line1["lineCoordinates"] = coodArray;
    line1["mode"] = crossLine.mode;
    line1["name"] = crossLine.name;
    json otfArray = json::array();
    otfArray.push_back("Person");
    otfArray.push_back("Vehicle.Bicycle");
    otfArray.push_back("Vehicle.Car");
    otfArray.push_back("Vehicle.Motorcycle");
    otfArray.push_back("Vehicle.Bus");
    otfArray.push_back("Vehicle.Truck");
    line1["objectTypeFilter"] = otfArray;
    // ["Person","Vehicle.Bicycle","Vehicle.Car","Vehicle.Motorcycle","Vehicle.Bus","Vehicle.Truck"]
    lineArray.push_back(line1);

    curlRoot["line"] = lineArray;

    string insert_json_string = curlRoot.dump();
    cout << "--- HTTP Payload 원문 ---\n" << insert_json_string << "\n";

    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS,
                     insert_json_string.c_str());  // 데이터 포인터
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE,
                     insert_json_string.length());  // 데이터 길이

    // 2.4. HTTPS 인증서 검증 비활성화 (--insecure)
    // 경고: 개발/테스트 환경에서만 사용하세요. 실제 운영 환경에서는 보안상 매우
    // 위험합니다!
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER,
                     0L);  // 피어(서버) 인증서 검증 안 함
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST,
                     0L);  // 호스트 이름 검증 안 함 (0L은 이전 버전과의
                           // 호환성을 위해 사용됨)
    // 최신 curl에서는 CURLOPT_SSL_VERIFYHOST를 0으로 설정하면 경고를 낼 수
    // 있으며, 1 (CA 파일 검증) 또는 2 (호스트네임과 CA 모두 검증)를 권장합니다.

    // 2.5. 커스텀 헤더 설정 (-H 옵션들)
    // struct curl_slist를 사용하여 헤더 목록을 만들고 추가해요.
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(
        headers, "Cookie: TRACKID=0842ca6f0d90294ea7de995c40a4aac6");
    headers = curl_slist_append(headers, "Origin: https://192.168.0.137");
    headers = curl_slist_append(
        headers,
        "Referer: "
        "https://192.168.0.137/home/setup/opensdk/html/WiseAI/index.html");
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER,
                     headers);  // 헤더 목록을 curl에 설정

    // 2.6. 압축 지원 (--compressed)
    // Accept-Encoding 헤더를 자동으로 추가하여 서버가 압축된 응답을 보내도록
    // 요청해요.
    curl_easy_setopt(
        curl_handle, CURLOPT_ACCEPT_ENCODING,
        "");  // 빈 문자열로 설정하면 curl이 지원하는 압축 방식을 모두 보냄

    // 2.7. 응답 본문을 프로그램 내에서 받기 위한 콜백 함수 설정
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA,
                     &response_buffer);  // 콜백 함수에 전달할 사용자 정의
                                         // 포인터 (응답 버퍼의 주소)

    // --- curl 명령어 옵션 설정 끝 ---

    // 3. 요청 실행
    CURLcode res_perform = curl_easy_perform(curl_handle);

    // 4. 결과 확인
    if (res_perform != CURLE_OK) {
      std::cerr << "curl_easy_perform() 실패: "
                << curl_easy_strerror(res_perform) << std::endl;
    } else {
      long http_code = 0;
      curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE,
                        &http_code);  // HTTP 응답 코드 얻기

      std::cout << "--- HTTP 응답 수신 ---" << std::endl;
      std::cout << "HTTP Status Code: " << http_code << std::endl;
      std::cout << "--- 응답 본문 ---" << std::endl;
      std::cout << response_buffer << std::endl;
      std::cout << "------------------" << std::endl;
    }

    // 5. 자원 해제
    curl_easy_cleanup(curl_handle);  // curl 핸들 해제
    curl_slist_free_all(headers);  // 설정했던 헤더 목록 해제 (중요!)

  } catch (const std::exception& e) {
    // 예외 처리 (예: new 실패 등)
    std::cerr << "예외 발생: " << e.what() << std::endl;
  }

  return response_buffer;
}

string deleteLines(int index) {
  string response_buffer;
  try {
    CURL* curl_handle;  // curl 통신을 위한 핸들

    // 2. curl_easy 핸들 생성
    // 각 통신 요청마다 이 핸들을 생성하고 설정해요.
    curl_handle = curl_easy_init();
    if (!curl_handle) {
      std::cerr << "curl_easy_init() 실패" << std::endl;
      curl_global_cleanup();
      return NULL;
    }
    // --- curl 명령어 옵션 설정 시작 ---
    // 2.1. URL 설정
    string deleteUrl =
        "https://192.168.0.137/opensdk/WiseAI/configuration/linecrossing/"
        "line?channel=0&index=" +
        to_string(index);
    curl_easy_setopt(curl_handle, CURLOPT_URL, deleteUrl.c_str());

    curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "DELETE");

    // 2.3. 인증 설정 (--digest -u admin:admin123@)
    // 다이제스트 인증 (CURLAUTH_DIGEST)과 사용자명:비밀번호 설정
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
    curl_easy_setopt(curl_handle, CURLOPT_USERPWD, "admin:admin123@");

    // 2.4. HTTPS 인증서 검증 비활성화 (--insecure)
    // 경고: 개발/테스트 환경에서만 사용하세요. 실제 운영 환경에서는 보안상 매우
    // 위험합니다!
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER,
                     0L);  // 피어(서버) 인증서 검증 안 함
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST,
                     0L);  // 호스트 이름 검증 안 함 (0L은 이전 버전과의
                           // 호환성을 위해 사용됨)
    // 최신 curl에서는 CURLOPT_SSL_VERIFYHOST를 0으로 설정하면 경고를 낼 수
    // 있으며, 1 (CA 파일 검증) 또는 2 (호스트네임과 CA 모두 검증)를 권장합니다.

    // 2.5. 커스텀 헤더 설정 (-H 옵션들)
    // struct curl_slist를 사용하여 헤더 목록을 만들고 추가해요.
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(
        headers, "Cookie: TRACKID=0842ca6f0d90294ea7de995c40a4aac6");
    headers = curl_slist_append(headers, "Origin: https://192.168.0.137");
    headers = curl_slist_append(
        headers,
        "Referer: "
        "https://192.168.0.137/home/setup/opensdk/html/WiseAI/index.html");
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER,
                     headers);  // 헤더 목록을 curl에 설정

    // 2.6. 압축 지원 (--compressed)
    // Accept-Encoding 헤더를 자동으로 추가하여 서버가 압축된 응답을 보내도록
    // 요청해요.
    curl_easy_setopt(
        curl_handle, CURLOPT_ACCEPT_ENCODING,
        "");  // 빈 문자열로 설정하면 curl이 지원하는 압축 방식을 모두 보냄

    // 2.7. 응답 본문을 프로그램 내에서 받기 위한 콜백 함수 설정
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA,
                     &response_buffer);  // 콜백 함수에 전달할 사용자 정의
                                         // 포인터 (응답 버퍼의 주소)

    // --- curl 명령어 옵션 설정 끝 ---

    // 3. 요청 실행
    CURLcode res_perform = curl_easy_perform(curl_handle);

    // 4. 결과 확인
    if (res_perform != CURLE_OK) {
      std::cerr << "curl_easy_perform() 실패: "
                << curl_easy_strerror(res_perform) << std::endl;
    } else {
      long http_code = 0;
      curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE,
                        &http_code);  // HTTP 응답 코드 얻기

      std::cout << "--- HTTP 응답 수신 ---" << std::endl;
      std::cout << "HTTP Status Code: " << http_code << std::endl;
      std::cout << "--- 응답 본문 ---" << std::endl;
      std::cout << response_buffer << std::endl;
      std::cout << "------------------" << std::endl;
    }

    // 5. 자원 해제
    curl_easy_cleanup(curl_handle);  // curl 핸들 해제
    curl_slist_free_all(headers);  // 설정했던 헤더 목록 해제 (중요!)

  } catch (const std::exception& e) {
    // 예외 처리 (예: new 실패 등)
    std::cerr << "예외 발생: " << e.what() << std::endl;
  }

  return response_buffer;
}
