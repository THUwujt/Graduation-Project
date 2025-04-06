#include ../../build-unix/Makefile.config

#PROJECT := simple_bus
#SRCS    := $(wildcard *.cpp)
#OBJS    := $(SRCS:.cpp=.o)

#include ../../build-unix/Makefile.rules
# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra

# Include directories
INCLUDES = -I.

# SystemC library path (modify this to match your system configuration)
SYSTEMC_LIB = -L/opt/systemc/lib -lsystemc #$(LD_LIBRARY_PATH)
SYSTEMC_INC = -I$(SYSTEMC_HOME)/include

# other libraries
SPDLOG_LIB = -L/usr/include/spdlog -lspdlog
FMT_LIB = -L/usr/include/fmt -lfmt
xlsx_LIB = -L/usr/include/xlsxwriter -lxlsxwriter

# Source files
SRCS = 	main.cpp   \
		trace.cpp

		
# Object files
OBJS = $(SRCS:.cpp=.o)

# Target executable
TARGET = testbench

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SYSTEMC_INC) -o $(TARGET) $(OBJS) $(SYSTEMC_LIB) $(SPDLOG_LIB) $(FMT_LIB) $(xlsx_LIB)

# Compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SYSTEMC_INC) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)
# Remake target
remake: clean all
	./$(TARGET)
