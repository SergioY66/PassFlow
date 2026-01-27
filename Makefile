# PassFlow Makefile
# Alternative to CMake for simple builds

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O3
INCLUDES = -I./include
LDFLAGS = -pthread -lstdc++fs

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)

# Source files
SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/MainControl.cpp \
          $(SRC_DIR)/VideoControl.cpp

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Target executable
TARGET = $(BIN_DIR)/passflow

# Default target
all: $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link
$(TARGET): $(BUILD_DIR) $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Compile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Install
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/passflow"

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/passflow
	@echo "Uninstalled"

# Run
run: $(TARGET)
	./$(TARGET)

# Debug build
debug: CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -g -O0 -DDEBUG
debug: clean $(TARGET)

# Show variables
info:
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "TARGET: $(TARGET)"

.PHONY: all clean install uninstall run debug info
