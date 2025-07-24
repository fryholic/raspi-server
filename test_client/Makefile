CXX = g++
CXXFLAGS = -std=c++17 -I.. 
LDFLAGS = -lssl -lcrypto -lsodium

TARGET = test_client
SRCS = test_client.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
