CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
whisperer: whisperer.c
	$(CC) $(CFLAGS) whisperer.c -lmicrohttpd -o whisperer
clean:
	rm -f whisperer
