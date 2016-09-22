CC=gcc
CFLAGS=-O3
LIBS=-pthread

all: failing-open-st failing-open-mt failing-close-st failing-close-mt

failing-open-st: main.c
	$(CC) $(CFLAGS) -DFAILING_OPEN_ST -o $@ $^

failing-close-st: main.c
	$(CC) $(CFLAGS) -DFAILING_CLOSE_ST -o $@ $^

failing-open-mt: main.c
	$(CC) $(CFLAGS) $(LIBS) -DFAILING_OPEN_MT -o $@ $^

failing-close-mt: main.c
	$(CC) $(CFLAGS) $(LIBS) -DFAILING_CLOSE_MT -o $@ $^
clean:
	rm -f failing-*
