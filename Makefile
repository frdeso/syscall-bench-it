CC=gcc
CFLAGS=-O3
LDFLAGS=-Wl,--no-as-needed
LIBS=-pthread -lnuma

all: failing-open-efault failing-open-enoent failing-close

failing-open-efault: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DFAILING_OPEN_NULL -o $@ $^
failing-open-enoent: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DFAILING_OPEN_NEXIST -o $@ $^
failing-close: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DFAILING_CLOSE -o $@ $^
clean:
	rm -f failing-*
