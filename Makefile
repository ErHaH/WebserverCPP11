CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = server
OBJS = src/pool/*.hpp \
       src/http/*.hpp \
       src/buffer/*.hpp \
	   src/server/*.hpp \
	   src/main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o bin/$(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf bin/$(OBJS) $(TARGET)



