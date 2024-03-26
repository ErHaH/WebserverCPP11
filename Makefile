CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = main
OBJS = src/pool/*.hpp \
	   src/logger/*.hpp \
       src/http/*.hpp \
       src/buffer/*.hpp \
	   src/server/*.hpp \
	   src/cfg/*.hpp\
	   src/main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o bin/$(TARGET)  -pthread -lmysqlclient -lyaml-cpp

clean:
	rm -rf bin/$(OBJS) $(TARGET)