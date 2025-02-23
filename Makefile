# Define the compiler
CC = gcc

# Define the flags
CFLAGS = -Wall -Wextra -std=c11

# Define the target executable
TARGET = fcsv

# Define the directories
SRCDIR = src
HEADERDIR = hdr
OBJDIR = obj

# Define the header files
HEADERS = $(wildcard $(HEADERDIR)/*.h)

# Define the source files
SRCS = $(wildcard $(SRCDIR)/*.c) $(wildcard *.c)

# Define the object files
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

# Create the obj directory if it doesn't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Default target
all: $(OBJDIR) $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile the source files into object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -I$(HEADERDIR) -c $< -o $@

# Clean up the build files
clean:
	rm -f $(OBJDIR)/*.o $(TARGET)

.PHONY: all clean