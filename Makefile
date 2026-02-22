CXX := c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2

TARGET := smiley
TEST_TARGET := tests
CORE_SRCS := src/bmp.cpp src/png.cpp src/jpg.cpp src/gif.cpp src/drawable.cpp src/api.cpp src/layer.cpp
APP_SRCS := src/main.cpp $(CORE_SRCS)
TEST_SRCS := src/tests.cpp $(CORE_SRCS)
OBJS := $(APP_SRCS:.cpp=.o)
TEST_OBJS := $(TEST_SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ $(OBJS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ $(TEST_OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -Isrc -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGET) smiley.bmp smiley_copy.bmp smiley.png smiley_copy.png smiley.jpg smiley_copy.jpg smiley.gif smiley_copy.gif layered_blend.png smiley_direct.png smiley_layered.png smiley_layer_diff.png test_ref.bmp test_ref.png test_ref.jpg test_ref.gif

.PHONY: all clean test
