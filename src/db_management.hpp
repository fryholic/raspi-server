// SQLite DB detections 테이블 관리 모듈

#pragma once
#include <SQLiteCpp/SQLiteCpp.h> // SQLiteC++ 외부 라이브러리
#include <fstream>   // 이미지 파일 테스트용
#include <iostream>  // 표준 입출력 (std::cout, std::cerr)
#include <string>    // 문자열 처리 (std::string)
#include <vector>    // 동적 배열 (std::vector)

// 비밀번호 / 복구코드 해싱을 위한 헤더 파일
#include "hash.hpp"

// 감지 이미지 & 텍스트
struct Detection {
  std::vector<unsigned char> imageBlob;
  std::string timestamp;
};

// 감지선
struct CrossLine {
  int index;
  int x1;
  int y1;
  int x2;
  int y2;
  std::string name;
  std::string mode;
};

// 기준선 좌표
struct BaseLine {
  int index;
  int matrixNum1;
  int x1;
  int y1;
  int matrixNum2;
  int x2;
  int y2;
};

// 수직선 좌표
struct VerticalLineEquation {
  // ax+b=0
  int index;
  double a;
  double b;
};

struct Account {
  std::string id;
  std::string passwd;
  std::string otp_secret;
  bool use_otp;
};

struct RecoveryCode {
    std::string id;
    std::string code;
    int used;
};

void create_table_detections(SQLite::Database& db);

bool insert_data_detections(SQLite::Database& db, Detection detection);

std::vector<Detection> select_data_for_timestamp_range_detections(
    SQLite::Database& db, std::string startTimestamp, std::string endTimestamp);

void delete_all_data_detections(SQLite::Database& db);

void create_table_lines(SQLite::Database& db);

bool insert_data_lines(SQLite::Database& db, CrossLine crossLine);

std::vector<CrossLine> select_all_data_lines(SQLite::Database& db);

bool delete_data_lines(SQLite::Database& db, int indexNum);

bool delete_all_data_lines(SQLite::Database& db);

void create_table_baseLines(SQLite::Database& db);

std::vector<BaseLine> select_all_data_baseLines(SQLite::Database& db);

bool insert_data_baseLines(SQLite::Database& db, BaseLine baseline);

bool update_data_baseLines(SQLite::Database& db, BaseLine baseline);

bool delete_all_data_baseLines(SQLite::Database& db);

void create_table_verticalLineEquations(SQLite::Database& db);

VerticalLineEquation select_data_verticalLineEquations(SQLite::Database& db,
                                                       int index);

bool insert_data_verticalLineEquations(
    SQLite::Database& db, VerticalLineEquation verticalLineEquation);

bool delete_all_data_verticalLineEquations(SQLite::Database& db);

void create_table_accounts(SQLite::Database& db);

Account* select_data_accounts(SQLite::Database& db, std::string id, std::string passwd);

bool insert_data_accounts(SQLite::Database& db, Account account);

Account* get_account_by_id(SQLite::Database& db, const std::string& id);

void create_table_recovery_codes(SQLite::Database& db);

bool store_otp_secret(SQLite::Database& db, const std::string& id, const std::string& secret);

bool store_recovery_codes(SQLite::Database& db, const std::string& id, const std::vector<std::string>& codes);

bool verify_recovery_code(SQLite::Database& db, const std::string& id, const std::string& code);

bool invalidate_recovery_code(SQLite::Database& db, const std::string& id, const std::string& code);

std::vector<std::string> get_hashed_recovery_codes(SQLite::Database& db, const std::string& id);
