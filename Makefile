CC=gcc
CFLAGS=-O3
LIBS=-pthread

all: failing-open-null failing-open-nexist failing-close


failing-open-null: main.c
	$(CC) $(CFLAGS) $(LIBS) -DFAILING_OPEN_NULL -o $@ $^
failing-open-nexist: main.c
	$(CC) $(CFLAGS) $(LIBS) -DFAILING_OPEN_NEXIST -o $@ $^

failing-close: main.c
	$(CC) $(CFLAGS) $(LIBS) -DFAILING_CLOSE -o $@ $^
clean:
	rm -f failing-*
