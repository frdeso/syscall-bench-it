CC=gcc
CFLAGS=-O3 -Wall
DEFINES=-DNUM_ITER=10000000

all: failing-open failing-close

failing-open: main.c
	$(CC) $(CFLAGS) $(DEFINES) -DFAILING_OPEN -o $@ $^

failing-close: main.c
	$(CC) $(CFLAGS) $(DEFINES) -DFAILING_CLOSE -o $@ $^
clean:
	rm -f a.out failing-*
