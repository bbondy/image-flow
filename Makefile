CXX := c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2

TARGET := smiley
TEST_TARGET := tests
OBJ_DIR := build/intermediate
CORE_SRCS := src/bmp.cpp src/png.cpp src/jpg.cpp src/gif.cpp src/drawable.cpp src/api.cpp src/layer.cpp
APP_SRCS := src/main.cpp $(CORE_SRCS)
TEST_SRCS := src/tests.cpp $(CORE_SRCS)
OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(APP_SRCS))
TEST_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(TEST_SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ $(OBJS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ $(TEST_OBJS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc -c $< -o $@

clean:
	rm -rf build $(TARGET) $(TEST_TARGET)

.PHONY: all clean test
