// g++ -o db_management db_management.cpp -l SQLiteCpp -l sqlite3 -std=c++17
#include "db_management.hpp"

///////////////////////////////////////////////
// Detections 테이블

void create_table_detections(SQLite::Database& db) {
  db.exec(
      "CREATE TABLE IF NOT EXISTS detections ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
      "image BLOB, "
      "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL)");
  cout << "'detections' 테이블이 준비되었습니다.\n";
  return;
}

bool insert_data_detections(SQLite::Database& db, Detection detection) {
  try {
    // SQL 인젝션 방지를 위해 Prepared Statement 사용
    SQLite::Statement query(
        db, "INSERT INTO detections (image, timestamp) VALUES (?, ?)");
    query.bind(1, detection.imageBlob.data(), detection.imageBlob.size());
    query.bind(2, detection.timestamp);
    cout << "Prepared SQL for insert: " << query.getExpandedSQL() << endl;
    query.exec();

    cout << "데이터 추가: (시간: " << detection.timestamp << ")" << endl;
  } catch (const exception& e) {
    // 이름이 중복될 경우 (UNIQUE 제약 조건 위반) 오류가 발생할 수 있습니다.
    cerr << "데이터 '" << detection.timestamp << "' 추가 실패: " << e.what()
         << endl;
    return false;
  }
  return true;
}

vector<Detection> select_data_for_timestamp_range_detections(
    SQLite::Database& db, string startTimestamp, string endTimestamp) {
  vector<Detection> detections;
  try {
    SQLite::Statement query(db,
                            "SELECT * FROM detections WHERE timestamp BETWEEN "
                            "? AND ? ORDER BY timestamp");
    query.bind(1, startTimestamp);
    query.bind(2, endTimestamp);
    cout << "Prepared SQL for select data vector: " << query.getExpandedSQL()
         << endl;
    while (query.executeStep()) {
      const unsigned char* ucharBlobData =
          static_cast<const unsigned char*>(query.getColumn("image").getBlob());
      int blobSize = query.getColumn("image").getBytes();
      vector<unsigned char> image(ucharBlobData, ucharBlobData + blobSize);

      string timestamp = query.getColumn("timestamp");

      Detection detection = {image, timestamp};
      detections.push_back(detection);
    }
  } catch (const exception& e) {
    cerr << "사용자 조회 실패: " << e.what() << endl;
  }
  return detections;
}

void delete_all_data_detections(SQLite::Database& db) {
  try {
    SQLite::Statement query(db, "DELETE FROM detections");
    cout << "Prepared SQL for delete all: " << query.getExpandedSQL() << endl;
    int changes = query.exec();
    cout << "테이블의 모든 데이터를 삭제했습니다. 삭제된 행 수: " << changes
         << endl;
  } catch (const exception& e) {
    cerr << "테이블 전체 삭제 실패: " << e.what() << endl;
  }
  return;
}

///////////////////////////////////////////////
// Lines 테이블

void create_table_lines(SQLite::Database& db) {
  db.exec(
      "CREATE TABLE IF NOT EXISTS lines ("
      "indexNum INTEGER PRIMARY KEY NOT NULL, "
      "x1 INTEGER NOT NULL , "
      "y1 INTEGER NOT NULL , "
      "x2 INTEGER NOT NULL , "
      "y2 INTEGER NOT NULL , "
      "name TEXT NOT NULL UNIQUE , "
      "mode TEXT )");  // mode = "Right", "Left", "BothDirections"
  cout << "'lines' 테이블이 준비되었습니다.\n";
  return;
}

bool insert_data_lines(SQLite::Database& db, CrossLine crossLine) {
  try {
    // SQL 인젝션 방지를 위해 Prepared Statement 사용
    SQLite::Statement query(
        db,
        "INSERT INTO lines (indexNum, x1, y1, x2, y2, name, mode) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.bind(1, crossLine.index);
    query.bind(2, crossLine.x1);
    query.bind(3, crossLine.y1);
    query.bind(4, crossLine.x2);
    query.bind(5, crossLine.y2);
    query.bind(6, crossLine.name);
    query.bind(7, crossLine.mode);
    cout << "Prepared SQL for insert: " << query.getExpandedSQL() << endl;
    query.exec();

    cout << "데이터 추가: (인덱스: " << crossLine.index << ")" << endl;
  } catch (const exception& e) {
    // 이름이 중복될 경우 (UNIQUE 제약 조건 위반) 오류가 발생할 수 있습니다.
    cerr << "데이터 '" << crossLine.name << "' 추가 실패: " << e.what() << endl;
    return false;
  }
  return true;
}

vector<CrossLine> select_all_data_lines(SQLite::Database& db) {
  vector<CrossLine> lines;
  try {
    SQLite::Statement query(db, "SELECT * FROM lines ORDER BY name");
    cout << "Prepared SQL for select data vector: " << query.getExpandedSQL()
         << endl;
    while (query.executeStep()) {
      int indexNum = query.getColumn("indexNum").getInt();
      int x1 = query.getColumn("x1").getInt();
      int y1 = query.getColumn("y1").getInt();
      int x2 = query.getColumn("x2").getInt();
      int y2 = query.getColumn("y2").getInt();
      string name = query.getColumn("name");
      string mode = query.getColumn("mode");

      CrossLine line = {indexNum, x1, y1, x2, y2, name, mode};
      lines.push_back(line);
    }
  } catch (const exception& e) {
    cerr << "사용자 조회 실패: " << e.what() << endl;
  }
  return lines;
}

bool delete_data_lines(SQLite::Database& db, int indexNum) {
  try {
    SQLite::Statement query(db, "DELETE FROM lines WHERE indexNum = ?");
    query.bind(1, indexNum);

    cout << "Prepared SQL for delete one: " << query.getExpandedSQL() << endl;
    int changes = query.exec();
    cout << "테이블의 특정 데이터를 삭제했습니다. 삭제된 행 수: " << changes
         << endl;
    if (changes == 0) {
      return false;
    }
  } catch (const exception& e) {
    cerr << "테이블 특정 데이터 삭제 실패: " << e.what() << endl;
    return false;
  }
  return true;
}

bool delete_all_data_lines(SQLite::Database& db) {
  try {
    SQLite::Statement query(db, "DELETE FROM lines");
    cout << "Prepared SQL for delete all: " << query.getExpandedSQL() << endl;
    int changes = query.exec();
    cout << "테이블의 모든 데이터를 삭제했습니다. 삭제된 행 수: " << changes
         << endl;
    if (changes == 0) {
      return false;
    }
  } catch (const exception& e) {
    cerr << "테이블 전체 삭제 실패: " << e.what() << endl;
    return false;
  }
  return true;
}

///////////////////////////////////////////////
// BaseLine 테이블

void create_table_baseLines(SQLite::Database& db) {
  db.exec(
      "CREATE TABLE IF NOT EXISTS baseLines ("
      "indexNum INTEGER PRIMARY KEY, "
      "matrixNum1 INTEGER NOT NULL, "
      "x1 INTEGER NOT NULL, "
      "y1 INTEGER NOT NULL, "
      "matrixNum2 INTEGER NOT NULL, "
      "x2 INTEGER NOT NULL, "
      "y2 INTEGER NOT NULL)");
  cout << "'baseLines' 테이블이 준비되었습니다.\n";
  return;
}

vector<BaseLine> select_all_data_baseLines(SQLite::Database& db) {
  vector<BaseLine> baseLines;
  try {
    SQLite::Statement query(db, "SELECT * FROM baseLines");
    cout << "Prepared SQL for select data vector: " << query.getExpandedSQL()
         << endl;
    while (query.executeStep()) {
      int indexNum = query.getColumn("indexNum").getInt();
      int matrixNum1 = query.getColumn("matrixNum1").getInt();
      int x1 = query.getColumn("x1").getInt();
      int y1 = query.getColumn("y1").getInt();
      int matrixNum2 = query.getColumn("matrixNum2").getInt();
      int x2 = query.getColumn("x2").getInt();
      int y2 = query.getColumn("y2").getInt();

      BaseLine baseLine = {indexNum, matrixNum1, x1, y1, matrixNum2, x2, y2};
      baseLines.push_back(baseLine);
    }
  } catch (const exception& e) {
    cerr << "사용자 조회 실패: " << e.what() << endl;
  }
  return baseLines;
}

bool insert_data_baseLines(SQLite::Database& db, BaseLine baseLine) {
  try {
    // SQL 인젝션 방지를 위해 Prepared Statement 사용
    SQLite::Statement query(
        db,
        "INSERT INTO baseLines (indexNum, matrixNum1, x1, y1, "
        "matrixNum2, x2, y2) VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.bind(1, baseLine.index);
    query.bind(2, baseLine.matrixNum1);
    query.bind(3, baseLine.x1);
    query.bind(4, baseLine.y1);
    query.bind(5, baseLine.matrixNum2);
    query.bind(6, baseLine.x2);
    query.bind(7, baseLine.y2);

    cout << "Prepared SQL for insert: " << query.getExpandedSQL() << endl;
    query.exec();

    cout << "데이터 추가: (기준선 인덱스: " << baseLine.index << ")" << endl;
  } catch (const exception& e) {
    // 이름이 중복될 경우 (UNIQUE 제약 조건 위반) 오류가 발생할 수 있습니다.
    cerr << "데이터 '" << baseLine.index << "' 추가 실패: " << e.what() << endl;
    return false;
  }
  return true;
}

bool update_data_baseLines(SQLite::Database& db, BaseLine baseLine){
  try {
    // SQL 인젝션 방지를 위해 Prepared Statement 사용
    SQLite::Statement query(
        db,
        "UPDATE baseLines "
        "SET matrixNum1 = ?, matrixNum2 = ? "
        "WHERE indexNum = ?");
    query.bind(1, baseLine.matrixNum1);
    query.bind(2, baseLine.matrixNum2);
    query.bind(3, baseLine.index);

    cout << "Prepared SQL for insert: " << query.getExpandedSQL() << endl;
    query.exec();

    cout << "데이터 수정: (기준선 인덱스: " << baseLine.index << ")" << endl;
  } catch (const exception& e) {
    // 이름이 중복될 경우 (UNIQUE 제약 조건 위반) 오류가 발생할 수 있습니다.
    cerr << "데이터 '" << baseLine.index << "' 수정 실패: " << e.what() << endl;
    return false;
  }
  return true;
}

bool delete_all_data_baseLines(SQLite::Database& db) {
  try {
    SQLite::Statement query(db, "DELETE FROM baselines");
    cout << "Prepared SQL for delete all: " << query.getExpandedSQL() << endl;
    int changes = query.exec();
    cout << "테이블의 모든 데이터를 삭제했습니다. 삭제된 행 수: " << changes
         << endl;
    if (changes == 0) {
      return false;
    }
  } catch (const exception& e) {
    cerr << "테이블 전체 삭제 실패: " << e.what() << endl;
    return false;
  }
  return true;
}

///////////////////////////////////////////////
// VerticalLineEquation 테이블

void create_table_verticalLineEquations(SQLite::Database& db) {
  db.exec(
      "CREATE TABLE IF NOT EXISTS verticalLineEquations ("
      "indexNum INTEGER PRIMARY KEY, "
      "a REAL NOT NULL, "
      "b REAL NOT NULL)");
  cout << "'verticalLineEquations' 테이블이 준비되었습니다.\n";
  return;
}

VerticalLineEquation select_data_verticalLineEquations(SQLite::Database& db,
                                                       int index) {
  VerticalLineEquation verticalLineEquation;
  try {
    SQLite::Statement query(
        db, "SELECT * FROM verticalLineEquations WHERE index = ?");
    cout << "Prepared SQL for select data vector: " << query.getExpandedSQL()
         << endl;
    query.bind(1, index);
    query.exec();

    int indexNum = query.getColumn("indexNum").getInt();
    double x = query.getColumn("x");
    double y = query.getColumn("y");

    verticalLineEquation = {indexNum, x, y};

  } catch (const exception& e) {
    cerr << "사용자 조회 실패: " << e.what() << endl;
  }
  return verticalLineEquation;
}

bool insert_data_verticalLineEquations(
    SQLite::Database& db, VerticalLineEquation verticalLineEquation) {
  try {
    // SQL 인젝션 방지를 위해 Prepared Statement 사용
    SQLite::Statement query(
        db,
        "INSERT INTO verticalLineEquations (indexNum, a, b) VALUES (?, ?, ?)");
    query.bind(1, verticalLineEquation.index);
    query.bind(2, verticalLineEquation.a);
    query.bind(3, verticalLineEquation.b);
    cout << "Prepared SQL for insert: " << query.getExpandedSQL() << endl;
    query.exec();

    cout << "데이터 추가: (인덱스: " << verticalLineEquation.index << ")"
         << endl;
  } catch (const exception& e) {
    // 이름이 중복될 경우 (UNIQUE 제약 조건 위반) 오류가 발생할 수 있습니다.
    cerr << "데이터 '" << verticalLineEquation.index
         << "' 추가 실패: " << e.what() << endl;
    return false;
  }
  return true;
}

bool delete_all_data_verticalLineEquations(SQLite::Database& db) {
  try {
    SQLite::Statement query(db, "DELETE FROM verticalLineEquations");
    cout << "Prepared SQL for delete all: " << query.getExpandedSQL() << endl;
    int changes = query.exec();
    cout << "테이블의 모든 데이터를 삭제했습니다. 삭제된 행 수: " << changes
         << endl;
    if (changes == 0) {
      return false;
    }
  } catch (const exception& e) {
    cerr << "테이블 전체 삭제 실패: " << e.what() << endl;
    return false;
  }
  return true;
}

void create_table_accounts(SQLite::Database& db) {
  db.exec(
      "CREATE TABLE IF NOT EXISTS accounts ("
      "id TEXT PRIMARY KEY, "
      "passwd TEXT NOT NULL)"
  );
  cout << "'accounts' 테이블이 준비되었습니다.\n";
  return;
}

Account* select_data_accounts(SQLite::Database& db, string id, string passwd) {
  cout << "[Debug] select_data_accounts called with ID: " << id << ", Password: " << passwd << endl;

  Account* account = new Account;
  try {
    if (id.empty() || passwd.empty()) {
      cerr << "[Error] ID 또는 비밀번호가 비어 있습니다." << endl;
      return nullptr;
    }

    SQLite::Statement query(
        db, "SELECT * FROM accounts WHERE id = ? AND passwd = ?");
    query.bind(1, id);
    query.bind(2, passwd);

    string id = query.getColumn("id");
    string passwd = query.getColumn("passwd");

    account->id = id;
    account->passwd = passwd;

  } catch (const exception& e) {
    cerr << "사용자 조회 실패: " << e.what() << endl;
    return nullptr;
  }
  return account;
}

bool insert_data_accounts(SQLite::Database& db, Account account) {
  try {
    SQLite::Statement query(
        db, "INSERT INTO accounts (id, passwd) VALUES (?, ?)");
    query.bind(1, account.id);
    query.bind(2, account.passwd);
    cout << "Prepared SQL for insert: " << query.getExpandedSQL() << endl;
    query.exec();

    cout << "데이터 추가: (id: " << account.id << ")" << endl;
  } catch (const exception& e) {
    cerr << "데이터 '" << account.id << "' 추가 실패: " << e.what() << endl;
    return false;
  }
  return true;
}

Account* get_account_by_id(SQLite::Database& db, const string& id) {
    try {
        // 오직 ID만으로 계정을 조회하는 SQL 쿼리
        SQLite::Statement query(db, "SELECT id, passwd FROM accounts WHERE id = ?");
        query.bind(1, id);

        if (query.executeStep()) { // 행이 존재하는 경우 (사용자를 찾음)
            // 동적으로 Account 객체를 생성하여 반환
            Account* acc = new Account{
                query.getColumn(0).getString(), // id
                query.getColumn(1).getString()  // passwd (hashed)
            };
            return acc;
        } else {
            // 사용자를 찾지 못한 경우
            return nullptr;
        }
    } catch (const std::exception& e) {
        cerr << "[DB 에러] ID로 계정 조회 중 예외 발생: " << e.what() << endl;
        return nullptr;
    }
}