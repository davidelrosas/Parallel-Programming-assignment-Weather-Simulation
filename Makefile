# Compiler
CC = gcc

# Compiler flags
CFLAGS = -g -fopenmp

# Output executable
OUTPUT = weather_simulation

# Source file
SRC = weather_simulation.c

# Target to build the executable
$(OUTPUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SRC)