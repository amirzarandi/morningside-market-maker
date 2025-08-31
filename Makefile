CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

BUILD_DIR = build
TARGET = $(BUILD_DIR)/market_maker
SOURCES = main.cpp
HEADERS = mm.cpp

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

clean:
	@rm -rf $(BUILD_DIR)
