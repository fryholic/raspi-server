CXX = g++
CXXFLAGS = -Wall -std=c++17 $(shell pkg-config --cflags gstreamer-rtsp-server-1.0 gstreamer-1.0 glib-2.0 libcurl) -I/usr/include/openssl -I./otp/src -I./otp/deps/QR-Code-generator
LDFLAGS = $(shell pkg-config --libs gstreamer-rtsp-server-1.0 gstreamer-1.0 glib-2.0 libcurl) -pthread -lSQLiteCpp -lsqlite3 -lssl -lcrypto -lsodium

OTP_SRC = $(wildcard otp/src/*.cpp) $(wildcard otp/src/cotp/*.cpp)
OTP_DEPS_SRC = otp/deps/QR-Code-generator/qrcodegen.cpp
OTP_OBJ = $(OTP_SRC:.cpp=.o) $(OTP_DEPS_SRC:.cpp=.o)

all: server metadata/control

clean:
	rm -f *.o server metadata/control $(OTP_OBJ)

# TCP, RTSP 서버

server: server.o rtsp_server.o tcp_server.o db_management.o metadata_parser.o hash.o $(OTP_OBJ)
	$(CXX) server.o rtsp_server.o tcp_server.o db_management.o metadata_parser.o hash.o $(OTP_OBJ) -o server $(LDFLAGS)


server.o: server.cpp server_bbox.hpp
	$(CXX) -c server.cpp $(CXXFLAGS)

rtsp_server.o: rtsp_server.cpp
	$(CXX) -c rtsp_server.cpp $(CXXFLAGS)

tcp_server.o: tcp_server.cpp server_bbox.hpp
	$(CXX) -c tcp_server.cpp $(CXXFLAGS)

db_management.o : db_management.cpp
	$(CXX) -c db_management.cpp -std=c++17

metadata_parser.o: metadata_parser.cpp server_bbox.hpp
	$(CXX) -c $< -std=c++17

hash.o: src/hash.cpp src/hash.hpp
	$(CXX) -c src/hash.cpp -o hash.o $(CXXFLAGS)

# 메타데이터, 감지 처리 서버

metadata/control: metadata/main_control.cpp metadata/board_control.cpp
	$(CXX) metadata/main_control.cpp metadata/board_control.cpp -o metadata/control -lSQLiteCpp -lsqlite3 --std=c++17