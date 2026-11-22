CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -Wpedantic -fopenmp
COMMON = src/lab.cpp src/random.cpp
HEADERS = src/lab.hpp

.PHONY: all info clean
all: build/lab2

build/lab2: src/main.cpp $(COMMON) $(HEADERS) Makefile | build
	$(CXX) $(CXXFLAGS) -Isrc src/main.cpp $(COMMON) -o $@

build:
	mkdir -p build

info:
	@$(CXX) --version | head -1
	@echo "$(CXX) $(CXXFLAGS)"
	@echo | $(CXX) $(CXXFLAGS) -dM -E -x c++ - | grep _OPENMP

clean:
	rm -rf build
