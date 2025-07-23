CXX = g++
CXXFLAGS = -std=c++17 -I../src -I../src/cotp -I../deps/QR-Code-generator
LDFLAGS = -lssl -lcrypto

SRC = main.cpp user_manager.cpp otp_manager.cpp ../src/cotp.cpp ../src/otp_uri.cpp ../src/otp_factory.cpp ../src/qr_code.cpp ../deps/QR-Code-generator/qrcodegen.cpp
OBJ = $(SRC:.cpp=.o)

all: otp_console

otp_console: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

../src/cotp.o: ../src/cotp.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) otp_console
