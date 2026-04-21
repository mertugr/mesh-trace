CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
LDFLAGS  ?= -pthread

SRC_DIR := src
BIN     := raytracer

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

# stb_impl.cpp compiles vendored single-header libraries that trip modern
# warnings. Build them with warnings relaxed.
STB_OBJ := $(SRC_DIR)/stb_impl.o

.PHONY: all clean debug run-simple run-mirror run-many

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(STB_OBJ): $(SRC_DIR)/stb_impl.cpp
	$(CXX) -std=c++17 -O2 -w -MMD -MP -c $< -o $@

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

debug: CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -pthread
debug: clean all

clean:
	rm -f $(OBJS) $(DEPS) $(BIN)

run-simple: $(BIN)
	./$(BIN) scenes/cube_simple.xml

run-mirror: $(BIN)
	./$(BIN) scenes/mirror_scene.xml

run-many: $(BIN)
	./$(BIN) scenes/grid_large.xml

-include $(DEPS)
