OUT = fmod
OBJECTS = fmod.o fat32.o
SOURCE_DIR = src
CFLAGS = -Wall -Wextra
CC = g++

$(OUT): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(OUT) $^
	rm -rf *.o

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OUT) *.o
