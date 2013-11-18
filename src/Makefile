OUT = fmod
OBJECTS = fmod.o
SOURCE_DIR = src
CFLAGS = -Wall -Wextra
CC = g++

$(OUT): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(OUT) $^
	rm -rf *.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OUT)