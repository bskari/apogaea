# Detect platform
UNAME := $(shell uname)

CXX := g++
CXXFLAGS := -DDEMO -std=c++11 -g -Wall -Wextra -Weffc++

LIBS := -lSDL2 -lSDL2_gfx

# Platform-specific include & lib dirs
ifeq ($(UNAME), Darwin)
    INC := -I/opt/homebrew/Cellar/sdl2/2.28.4/include/ \
           -I/opt/homebrew/Cellar/sdl2_gfx/1.0.4/include/
    LDFLAGS := -L/opt/homebrew/lib
else
    INC :=
    LDFLAGS :=
endif

# Source files
DEMO_SRC := demo.cpp
MOVIE_SRC := movie.cpp

# Generated file (Python3 builder)
OFFSETS_HPP := ../offsets.hpp
OFFSETS_PY := ../offsets.py

# Default target
all: demo

# Generate offsets.hpp using Python3 (matches SCons Python3 builder)
$(OFFSETS_HPP): $(OFFSETS_PY)
	python3 $(OFFSETS_PY) > $(OFFSETS_HPP)

# Build demo (depends on offsets.hpp)
demo: $(DEMO_SRC) $(OFFSETS_HPP)
	$(CXX) $(CXXFLAGS) $(INC) $(DEMO_SRC) -o demo $(LDFLAGS) $(LIBS)

# Build movie
movie: $(MOVIE_SRC)
	$(CXX) $(CXXFLAGS) $(INC) $(MOVIE_SRC) -o movie $(LDFLAGS) $(LIBS)

# Clean
clean:
	rm -f demo movie
	rm -f $(OFFSETS_HPP)

.PHONY: all clean
