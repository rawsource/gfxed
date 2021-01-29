
# OBJS specifies which files to compile as part of the project
OBJS = gfxed.cpp

# CC specifies which compiler we're using
CC = g++

# COMPILER_FLAGS specifies the additional compilation options we're using
# -Wl,-subsystem,windows gets rid of the console window
# -std=c++11 -s -O3 -fno-exceptions -msse3 -fexceptions
# -Ofast -march=native
# -ffast-math
COMPILER_FLAGS = -Wall -Werror -std=c++11 -Ofast -ffast-math

# LINKER_FLAGS specifies the libraries we're linking against
LINKER_FLAGS = -lSDL2 -lGL

# OBJ_NAME specifies the name of our exectuable
OBJ_NAME = gfxed

# This is the target that compiles our executable
all : $(OBJS)
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME)

