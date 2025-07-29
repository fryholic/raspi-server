CXX = g++
CXXFLAGS = -std=c++17 -I../../src/otp -I../../src/otp/cotp -I../../src/otp/QR-Code-generator
LDFLAGS = -lssl -lcrypto

SRC = main.cpp user_manager.cpp otp_manager.cpp ../../src/otp/cotp.cpp ../../src/otp/otp_uri.cpp ../../src/otp/otp_factory.cpp ../../src/otp/qr_code.cpp ../../src/otp/QR-Code-generator/qrcodegen.cpp
OBJ = $(SRC:.cpp=.o)

all: otp_console

otp_console: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

../../src/otp/cotp.o: ../../src/otp/cotp.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) otp_console
