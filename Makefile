CXX := c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2
ARCH := $(shell uname -m)

BIN_DIR := build/bin
TARGET := $(BIN_DIR)/image_flow
SAMPLES_TARGET := $(BIN_DIR)/generate_samples
TEST_TARGET := $(BIN_DIR)/tests
OBJ_DIR := build/intermediate/$(ARCH)
CORE_SRCS := src/bmp.cpp src/png.cpp src/jpg.cpp src/gif.cpp src/svg.cpp src/webp.cpp src/drawable.cpp src/example_api.cpp src/layer.cpp src/effects.cpp
APP_SRCS := src/main.cpp src/cli.cpp $(CORE_SRCS)
SAMPLES_SRCS := src/generate_samples_main.cpp src/sample_generator.cpp $(CORE_SRCS)
TEST_SRCS := src/tests.cpp $(CORE_SRCS)
OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(APP_SRCS))
SAMPLES_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SAMPLES_SRCS))
TEST_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(TEST_SRCS))

all: $(TARGET) $(SAMPLES_TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ $(OBJS)

$(SAMPLES_TARGET): $(SAMPLES_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ $(SAMPLES_OBJS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ $(TEST_OBJS)

run: run-sample

run-sample: $(TARGET)
	./$(TARGET)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc -c $< -o $@

clean:
	rm -rf build $(TARGET) $(SAMPLES_TARGET) $(TEST_TARGET)

.PHONY: all clean test run run-sample
