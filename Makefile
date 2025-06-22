# Makefile for TV Series Renamer

CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c11 -D_GNU_SOURCE
LDFLAGS = 
TARGET = mrename
SOURCE = mrename.c

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)
	@echo "✓ Build complete: $(TARGET)"

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean $(TARGET)
	@echo "✓ Debug build complete"

# Clean build files
clean:
	rm -f $(TARGET)
	@echo "✓ Cleaned build files"

# Install to /usr/local/bin
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/
	@echo "✓ Installed to /usr/local/bin/$(TARGET)"

# Uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)
	@echo "✓ Uninstalled from /usr/local/bin/"

# Run tests with sample files
test: $(TARGET)
	@echo "Creating test directory..."
	@mkdir -p test_files
	@touch "test_files/Show.S01E01.mkv"
	@touch "test_files/Show.S01E02.mkv"
	@touch "test_files/series.1x05.mp4"
	@touch "test_files/Anime.Ep.12.avi"
	@touch "test_files/Daily.Episode.100.mkv"
	@./$(TARGET) test_files
	@rm -rf test_files

# Help
help:
	@echo "TV Series Renamer - Makefile targets:"
	@echo "  make          - Build the program"
	@echo "  make debug    - Build with debugging symbols"
	@echo "  make clean    - Remove build files"
	@echo "  make install  - Install to /usr/local/bin"
	@echo "  make test     - Run with test files"
	@echo "  make help     - Show this help"

.PHONY: all debug clean install uninstall test help
