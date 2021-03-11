CC=gcc
CFLAGS=-pedantic -Werror -Wall -Wextra
BIN=dns
OBJ_FILES=charPList.o dns.o

all: $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ_FILES)

strPList.o: charPList.c
	$(CC) $(CFLAGS) -c charPList.c

dns.o: dns.c
	$(CC) $(CFLAGS) -c dns.c

pack:
	tar -cf xnejed09.tar manual.pdf Makefile README dns.c dns.h charPList.c charPList.h

clean:
	rm -f $(OBJ_FILES) $(BIN)