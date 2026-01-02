CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Werror -Iinclude

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INCLUDE_DIR = include

# Object files
CONFIG_OBJ = $(OBJ_DIR)/Config.o

# Test executables
TEST_CONFIG = $(BIN_DIR)/test_config

# Default target
all: directories $(TEST_CONFIG)

# Create necessary directories
directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR) logs

# Test executables
$(TEST_CONFIG): $(CONFIG_OBJ) $(OBJ_DIR)/test_config.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run test
test: $(TEST_CONFIG)
	./$(TEST_CONFIG)

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all directories test clean
