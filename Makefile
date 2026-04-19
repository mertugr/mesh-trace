CXX := c++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pthread -I. -Iinclude
LDFLAGS := -pthread

TARGET := raytracer

# Sources shared between the renderer and the test binary (everything except main.cpp)
LIB_SRCS := \
	tinyxml2.cpp \
	src/core/BVH.cpp \
	src/io/Image.cpp \
	src/io/SceneParser.cpp \
	src/render/Renderer.cpp \
	src/scene/Camera.cpp \
	src/scene/Scene.cpp \
	src/scene/Texture.cpp \
	src/scene/Triangle.cpp

LIB_OBJS := $(LIB_SRCS:.cpp=.o)

TEST_TARGET := tests/test_runner

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(LIB_OBJS) src/main.o
	$(CXX) $(LIB_OBJS) src/main.o -o $@ $(LDFLAGS)

$(TEST_TARGET): $(LIB_OBJS) tests/test_main.o
	$(CXX) $(LIB_OBJS) tests/test_main.o -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

run: $(TARGET)
	./$(TARGET) test.xml output.png

clean:
	rm -f $(LIB_OBJS) src/main.o tests/test_main.o $(TARGET) $(TEST_TARGET) output.ppm output.png
