CXX := c++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pthread -I. -Iinclude
LDFLAGS := -pthread

SRCS := \
	tinyxml2.cpp \
	src/main.cpp \
	src/core/BVH.cpp \
	src/io/Image.cpp \
	src/io/SceneParser.cpp \
	src/render/Renderer.cpp \
	src/scene/Camera.cpp \
	src/scene/Scene.cpp \
	src/scene/Texture.cpp \
	src/scene/Triangle.cpp

OBJS := $(SRCS:.cpp=.o)
TARGET := raytracer

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) scene1.xml output.ppm

clean:
	rm -f $(OBJS) $(TARGET) output.ppm
