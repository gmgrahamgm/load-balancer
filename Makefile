CXX = g++
CXXFLAGS = -std=c++20 -pthread -Wall -Werror -Iinclude

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INCLUDE_DIR = include

# Object files
CONFIG_OBJ = $(OBJ_DIR)/Config.o
QUEUE_OBJ = $(OBJ_DIR)/RequestQueue.o
WEBSERVER_OBJ = $(OBJ_DIR)/WebServer.o
LOADBALANCER_OBJ = $(OBJ_DIR)/LoadBalancer.o

# Test executables
TEST_CONFIG = $(BIN_DIR)/test_config
TEST_REQUEST = $(BIN_DIR)/test_request
TEST_QUEUE = $(BIN_DIR)/test_queue
TEST_WEBSERVER = $(BIN_DIR)/test_webserver
TEST_LOADBALANCER = $(BIN_DIR)/test_loadbalancer

# Default target
all: directories $(TEST_CONFIG) $(TEST_REQUEST) $(TEST_QUEUE) $(TEST_WEBSERVER) $(TEST_LOADBALANCER)

# Create directories
directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR) logs

# Test executables
$(TEST_CONFIG): $(CONFIG_OBJ) $(OBJ_DIR)/test_config.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_REQUEST): $(OBJ_DIR)/test_request.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_QUEUE): $(QUEUE_OBJ) $(OBJ_DIR)/test_queue.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_WEBSERVER): $(QUEUE_OBJ) $(WEBSERVER_OBJ) $(OBJ_DIR)/test_webserver.o
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_LOADBALANCER): $(CONFIG_OBJ) $(QUEUE_OBJ) $(WEBSERVER_OBJ) $(LOADBALANCER_OBJ) $(OBJ_DIR)/test_loadbalancer.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile obj files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run tests
test: $(TEST_CONFIG) $(TEST_REQUEST) $(TEST_QUEUE) $(TEST_WEBSERVER) $(TEST_LOADBALANCER)
	@echo "Running Config tests..."
	./$(TEST_CONFIG)
	@echo ""
	@echo "Running Request tests..."
	./$(TEST_REQUEST)
	@echo ""
	@echo "Running RequestQueue tests..."
	./$(TEST_QUEUE)
	@echo ""
	@echo "Running WebServer tests..."
	./$(TEST_WEBSERVER)
	@echo ""
	@echo "Running LoadBalancer tests..."
	./$(TEST_LOADBALANCER)

# Clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all directories test clean
