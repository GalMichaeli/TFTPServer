# Makefile for the ttftps program
CXX = gcc
CXXFLAGS = -std=c99 -Wall -Werror -pedantic-errors -DNDEBUG
OBJECTS = main.o server.o
TARGET = ttftps

# Creating the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

# Creating object files using default rules
main.o: main.c server.h
server.o: server.c server.h

# Clean rule
clean:
	rm -f $(OBJECTS) $(TARGET) *~
