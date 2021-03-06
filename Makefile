CC=gcc
CFLAGS=-O3
LDFLAGS=-Wl,--no-as-needed
LIBS=-pthread -lnuma

all: failing-open-efault failing-open-enoent failing-close failing-ioctl success-dup-close lttng-test-filter raw-syscall-getpid

failing-open-efault: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DFAILING_OPEN_NULL -o $@ $^
failing-open-enoent: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DFAILING_OPEN_NEXIST -o $@ $^
failing-close: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DFAILING_CLOSE -o $@ $^
failing-ioctl: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DFAILING_IOCTL -o $@ $^
success-dup-close: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DSUCCESS_DUP_CLOSE -o $@ $^
lttng-test-filter: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DLTTNG_TEST_FILTER -o $@ $^
raw-syscall-getpid: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -DRAW_SYSCALL_GETPID -o $@ $^
clean:
	rm -f failing-* success-*
