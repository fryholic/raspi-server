/**
 * @file db_management.hpp
 * @brief 데이터베이스 관리 헤더 파일
 * @details 이 파일은 SQLite 데이터베이스와 상호작용하는 함수와 구조체의 선언을 포함합니다.
 */

// SQLite DB detections 테이블 관리 모듈

#pragma once
#include <SQLiteCpp/SQLiteCpp.h> // SQLiteC++ 외부 라이브러리
#include <fstream>               // 이미지 파일 테스트용
#include <iostream>              // 표준 입출력 (std::cout, std::cerr)
#include <string>                // 문자열 처리 (std::string)
#include <vector>                // 동적 배열 (std::vector)

// 비밀번호 / 복구코드 해싱을 위한 헤더 파일
#include "hash.hpp"

/**
 * @brief 감지 이미지와 타임스탬프를 저장하는 구조체
 */
struct Detection
{
    std::vector<unsigned char> imageBlob; ///< 이미지 데이터 (BLOB)
    std::string timestamp;                ///< 감지 시각
};

/**
 * @brief 감지선 정보를 저장하는 구조체
 * @details 이 구조체는 감지선의 좌표, 이름, 감지 모드 등의 정보를 저장합니다.
 */
struct CrossLine
{
    int index;        ///< 라인 인덱스
    int x1;           ///< 시작점 x
    int y1;           ///< 시작점 y
    int x2;           ///< 끝점 x
    int y2;           ///< 끝점 y
    std::string name; ///< 라인 이름
    std::string mode; ///< 감지 모드
};

/**
 * @brief 기준선 좌표 정보를 저장하는 구조체
 * @details 이 구조체는 기준선의 좌표와 관련된 정보를 저장합니다.
 */
struct BaseLine
{
    int index;      ///< 기준선 인덱스
    int matrixNum1; ///< 첫 번째 매트릭스 번호
    int x1;         ///< 첫 번째 점 x
    int y1;         ///< 첫 번째 점 y
    int matrixNum2; ///< 두 번째 매트릭스 번호
    int x2;         ///< 두 번째 점 x
    int y2;         ///< 두 번째 점 y
};

/**
 * @brief 수직선 방정식(ax+b=0) 정보를 저장하는 구조체
 * @details 이 구조체는 수직선의 기울기와 절편 정보를 저장합니다.
 */
struct VerticalLineEquation
{
    int index; ///< 수직선 인덱스
    double a;  ///< 기울기 a
    double b;  ///< 절편 b
};

/**
 * @brief 사용자 계정 정보를 저장하는 구조체
 * @details 이 구조체는 사용자 ID, 비밀번호, OTP 시크릿, OTP 사용 여부를 저장합니다.
 */
struct Account
{
    std::string id;         ///< 사용자 ID
    std::string passwd;     ///< 비밀번호(해시)
    std::string otp_secret; ///< OTP 시크릿
    bool use_otp;           ///< OTP 사용 여부
};

/**
 * @brief 복구 코드 정보를 저장하는 구조체
 * @details 이 구조체는 복구 코드와 사용 여부를 저장합니다.
 */
struct RecoveryCode
{
    std::string id;   ///< 사용자 ID
    std::string code; ///< 복구 코드(해시)
    int used;         ///< 사용 여부(0: 미사용, 1: 사용)
};

/**
 * @brief Detections 테이블을 생성합니다.
 * @param db SQLite 데이터베이스 참조
 */
void create_table_detections(SQLite::Database& db);

/**
 * @brief Detections 테이블에 데이터를 삽입합니다.
 * @param db SQLite 데이터베이스 참조
 * @param detection 삽입할 Detection 구조체
 * @return 성공 시 true, 실패 시 false
 */
bool insert_data_detections(SQLite::Database& db, Detection detection);

/**
 * @brief 주어진 시간 범위 내의 Detection 데이터를 조회합니다.
 * @param db SQLite 데이터베이스 참조
 * @param startTimestamp 시작 타임스탬프
 * @param endTimestamp 종료 타임스탬프
 * @return Detection 벡터
 */
std::vector<Detection> select_data_for_timestamp_range_detections(SQLite::Database& db, std::string startTimestamp,
                                                                  std::string endTimestamp);

/**
 * @brief Detections 테이블의 모든 데이터를 삭제합니다.
 * @param db SQLite 데이터베이스 참조
 */
void delete_all_data_detections(SQLite::Database& db);

/**
 * @brief Lines 테이블을 생성합니다.
 * @param db SQLite 데이터베이스 참조
 */
void create_table_lines(SQLite::Database& db);

/**
 * @brief Lines 테이블에 데이터를 삽입합니다.
 * @param db SQLite 데이터베이스 참조
 * @param crossLine 삽입할 CrossLine 구조체
 * @return 성공 시 true, 실패 시 false
 */
bool insert_data_lines(SQLite::Database& db, CrossLine crossLine);

/**
 * @brief Lines 테이블의 모든 데이터를 조회합니다.
 * @param db SQLite 데이터베이스 참조
 * @return CrossLine 벡터
 */
std::vector<CrossLine> select_all_data_lines(SQLite::Database& db);

/**
 * @brief Lines 테이블에서 특정 인덱스의 데이터를 삭제합니다.
 * @param db SQLite 데이터베이스 참조
 * @param indexNum 삭제할 인덱스 번호
 * @return 성공 시 true, 실패 시 false
 */
bool delete_data_lines(SQLite::Database& db, int indexNum);

/**
 * @brief Lines 테이블의 모든 데이터를 삭제합니다.
 * @param db SQLite 데이터베이스 참조
 * @return 성공 시 true, 실패 시 false
 */
bool delete_all_data_lines(SQLite::Database& db);

/**
 * @brief baseLines 테이블을 생성합니다.
 * @param db SQLite 데이터베이스 참조
 */
void create_table_baseLines(SQLite::Database& db);

/**
 * @brief baseLines 테이블의 모든 데이터를 조회합니다.
 * @param db SQLite 데이터베이스 참조
 * @return BaseLine 벡터
 */
std::vector<BaseLine> select_all_data_baseLines(SQLite::Database& db);

/**
 * @brief baseLines 테이블에 데이터를 삽입합니다.
 * @param db SQLite 데이터베이스 참조
 * @param baseline 삽입할 BaseLine 구조체
 * @return 성공 시 true, 실패 시 false
 */
bool insert_data_baseLines(SQLite::Database& db, BaseLine baseline);

/**
 * @brief baseLines 테이블의 데이터를 수정합니다.
 * @param db SQLite 데이터베이스 참조
 * @param baseline 수정할 BaseLine 구조체
 * @return 성공 시 true, 실패 시 false
 */
bool update_data_baseLines(SQLite::Database& db, BaseLine baseline);

/**
 * @brief baseLines 테이블의 모든 데이터를 삭제합니다.
 * @param db SQLite 데이터베이스 참조
 * @return 성공 시 true, 실패 시 false
 */
bool delete_all_data_baseLines(SQLite::Database& db);

/**
 * @brief verticalLineEquations 테이블을 생성합니다.
 * @param db SQLite 데이터베이스 참조
 */
void create_table_verticalLineEquations(SQLite::Database& db);

/**
 * @brief verticalLineEquations 테이블에서 특정 인덱스의 데이터를 조회합니다.
 * @param db SQLite 데이터베이스 참조
 * @param index 조회할 인덱스
 * @return VerticalLineEquation 구조체
 */
VerticalLineEquation select_data_verticalLineEquations(SQLite::Database& db, int index);

/**
 * @brief verticalLineEquations 테이블에 데이터를 삽입합니다.
 * @param db SQLite 데이터베이스 참조
 * @param verticalLineEquation 삽입할 VerticalLineEquation 구조체
 * @return 성공 시 true, 실패 시 false
 */
bool insert_data_verticalLineEquations(SQLite::Database& db, VerticalLineEquation verticalLineEquation);

/**
 * @brief verticalLineEquations 테이블의 모든 데이터를 삭제합니다.
 * @param db SQLite 데이터베이스 참조
 * @return 성공 시 true, 실패 시 false
 */
bool delete_all_data_verticalLineEquations(SQLite::Database& db);

/**
 * @brief accounts 테이블을 생성합니다.
 * @param db SQLite 데이터베이스 참조
 */
void create_table_accounts(SQLite::Database& db);

/**
 * @brief accounts 테이블에서 id와 passwd로 계정 정보를 조회합니다.
 * @param db SQLite 데이터베이스 참조
 * @param id 사용자 ID
 * @param passwd 사용자 비밀번호
 * @return Account 포인터 (없으면 nullptr)
 */
Account* select_data_accounts(SQLite::Database& db, std::string id, std::string passwd);

/**
 * @brief accounts 테이블에 계정 정보를 삽입합니다.
 * @param db SQLite 데이터베이스 참조
 * @param account 삽입할 Account 구조체
 * @return 성공 시 true, 실패 시 false
 */
bool insert_data_accounts(SQLite::Database& db, Account account);

/**
 * @brief accounts 테이블에서 id로 계정 정보를 조회합니다.
 * @param db SQLite 데이터베이스 참조
 * @param id 사용자 ID
 * @return Account 포인터 (없으면 nullptr)
 */
Account* get_account_by_id(SQLite::Database& db, const std::string& id);

/**
 * @brief recovery_codes 테이블을 생성합니다.
 * @param db SQLite 데이터베이스 참조
 */
void create_table_recovery_codes(SQLite::Database& db);

/**
 * @brief accounts 테이블에 OTP 시크릿을 저장합니다.
 * @param db SQLite 데이터베이스 참조
 * @param id 사용자 ID
 * @param secret 저장할 OTP 시크릿
 * @return 성공 시 true, 실패 시 false
 */
bool store_otp_secret(SQLite::Database& db, const std::string& id, const std::string& secret);

/**
 * @brief recovery_codes 테이블에 복구 코드를 저장합니다.
 * @param db SQLite 데이터베이스 참조
 * @param id 사용자 ID
 * @param codes 저장할 복구 코드 벡터
 * @return 성공 시 true, 실패 시 false
 */
bool store_recovery_codes(SQLite::Database& db, const std::string& id, const std::vector<std::string>& codes);

/**
 * @brief 입력한 복구 코드가 DB에 저장된 해시와 일치하는지 검증합니다.
 * @param db SQLite 데이터베이스 참조
 * @param id 사용자 ID
 * @param code 입력한 복구 코드
 * @return 일치하면 true, 아니면 false
 */
bool verify_recovery_code(SQLite::Database& db, const std::string& id, const std::string& code);

/**
 * @brief 입력한 복구 코드가 일치하면 해당 코드를 used=1로 무효화합니다.
 * @param db SQLite 데이터베이스 참조
 * @param id 사용자 ID
 * @param code 입력한 복구 코드
 * @return 성공 시 true, 실패 시 false
 */
bool invalidate_recovery_code(SQLite::Database& db, const std::string& id, const std::string& code);

/**
 * @brief recovery_codes 테이블에서 사용되지 않은 해시 복구 코드 목록을 조회합니다.
 * @param db SQLite 데이터베이스 참조
 * @param id 사용자 ID
 * @return 해시된 복구 코드 문자열 벡터
 */
std::vector<std::string> get_hashed_recovery_codes(SQLite::Database& db, const std::string& id);
