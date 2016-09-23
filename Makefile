CC=gcc
CFLAGS=-O3
LIBS=-pthread

all: failing-open failing-close


failing-open: main.c
	$(CC) $(CFLAGS) $(LIBS) -DFAILING_OPEN -o $@ $^

failing-close: main.c
	$(CC) $(CFLAGS) $(LIBS) -DFAILING_CLOSE -o $@ $^
clean:
	rm -f failing-*
