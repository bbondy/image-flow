CXX := c++
DEBUG ?= 0
SANITIZE ?= 0
BASE_CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic
OPT_CXXFLAGS := -O2
SAN_FLAGS :=
ifeq ($(DEBUG),1)
OPT_CXXFLAGS := -O0 -g3
endif
ifeq ($(SANITIZE),1)
SAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
endif
CXXFLAGS := $(BASE_CXXFLAGS) $(OPT_CXXFLAGS) $(SAN_FLAGS)
LDFLAGS += $(SAN_FLAGS)
DEPFLAGS := -MMD -MP
ARCH := $(shell uname -m)

BIN_DIR := build/bin
TARGET := $(BIN_DIR)/image_flow
SAMPLES_TARGET := $(BIN_DIR)/generate_samples
TEST_TARGET := $(BIN_DIR)/tests
OBJ_DIR := build/intermediate/$(ARCH)
CORE_SRCS := src/bmp.cpp src/png.cpp src/jpg.cpp src/gif.cpp src/svg.cpp src/webp.cpp src/drawable.cpp src/example_api.cpp src/layer.cpp src/effects.cpp
APP_SRCS := src/main.cpp src/cli.cpp $(CORE_SRCS)
SAMPLES_SRCS := src/generate_samples_main.cpp src/sample_generator.cpp $(CORE_SRCS)
TEST_SRCS := src/tests.cpp src/cli.cpp $(CORE_SRCS)
CLI_SRCS := src/cli_args.cpp src/cli_parse.cpp src/cli_help.cpp src/cli_shared.cpp src/cli_project_cmds.cpp src/cli_impl.cpp
OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(APP_SRCS) $(CLI_SRCS))
SAMPLES_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SAMPLES_SRCS))
TEST_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(TEST_SRCS) $(CLI_SRCS))
DEPS := $(OBJS:.o=.d) $(SAMPLES_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

all: $(TARGET) $(SAMPLES_TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc $(LDFLAGS) -o $@ $(OBJS)

$(SAMPLES_TARGET): $(SAMPLES_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc $(LDFLAGS) -o $@ $(SAMPLES_OBJS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc $(LDFLAGS) -o $@ $(TEST_OBJS)

run: run-sample

run-sample: $(TARGET)
	./$(TARGET)

run-scripts: $(TARGET)
	./sample_scripts/run_all.sh

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -Isrc -c $< -o $@

debug-test:
	$(MAKE) DEBUG=1 test

asan-test:
	$(MAKE) DEBUG=1 SANITIZE=1 test

clean:
	rm -rf build $(TARGET) $(SAMPLES_TARGET) $(TEST_TARGET)

.PHONY: all clean test run run-sample run-scripts debug-test asan-test

-include $(DEPS)
