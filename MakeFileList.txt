CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -I..
LDFLAGS = 

# Source files
SOURCES = simple-ga-pipline.cc \
          fitness-fuction.cc \
          ../selection-operator/selection-operator.cc \
          ../crossover/crossover.cc \
          ../mutation/mutation.cc \

# Object files
OBJECTS = $(SOURCES:.cc=.o)

# Target executable
TARGET = simple-ga-test

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET) *.txt

# Run tests
run: $(TARGET)
	./$(TARGET)

# Help
help:
	@echo "Available targets:"
	@echo "  all     - Build the executable"
	@echo "  clean   - Remove build files"
	@echo "  run     - Build and run tests"
	@echo "  help    - Show this help"

.PHONY: all clean run help
