CC=gcc
CFLAGS=-O3
LDFLAGS=-Wl,--no-as-needed
LIBS=-pthread -lnuma

all: failing-open-efault failing-open-enoent failing-close success-open-dup-close success-open-close

failing-open-efault: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DFAILING_OPEN_NULL -o $@ $^
failing-open-enoent: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DFAILING_OPEN_NEXIST -o $@ $^
failing-close: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DFAILING_CLOSE -o $@ $^
success-open-dup-close: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DSUCCESS_OPEN_DUP_CLOSE -o $@ $^
success-open-close: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DSUCCESS_OPEN_CLOSE -o $@ $^
clean:
	rm -f failing-* success-*
