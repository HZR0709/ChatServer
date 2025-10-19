CXX = g++
CXXFLAGS = -std=c++17 -Wall -g -pthread -Isrc
LIBS = -lsqlite3
SRCDIR = src
SOURCES = $(SRCDIR)/main.cpp \
		  $(SRCDIR)/core/server.cpp\
		  $(SRCDIR)/database/database_manager.cpp \
		  $(SRCDIR)/managers/message_history.cpp \
		  $(SRCDIR)/managers/user_manager.cpp \
          $(SRCDIR)/network/network_manager.cpp \
		  $(SRCDIR)/network/connection_manager.cpp \
          $(SRCDIR)/thread/thread_pool.cpp \
          $(SRCDIR)/utils/config_manager.cpp \
          $(SRCDIR)/utils/logger.cpp \
          $(SRCDIR)/web/web_server.cpp \
          $(SRCDIR)/web/web_api.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = chat_server

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install:
	mkdir -p build/bin
	cp $(TARGET) ../bin/
	mkdir -p build/bin/web
	cp web/*.html build/bin/web

.PHONY: clean install