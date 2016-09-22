#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


#define MISSING_FILE "/var/foo"

long num_iter;
int num_threads;

static void* failing_open()
{
	int fd, i;

	for(i = 0; i < num_iter; i++) {
		/* Will fail with ENOENT since the file does not exist */
		fd = open(MISSING_FILE, O_RDONLY);
	}
}
static void* failing_close(void *a)
{
	int i;
	int fd = ((long)a)+4242;

	for(i = 0; i < num_iter; i++) {
		/* will fail with EBADF since the fd is invalid */
		close(fd);
	}
}

static void failing_open_mt()
{
	int i, err;
	void *tret;
	pthread_t *tids;
	tids = calloc(num_threads, sizeof(*tids));
	for (i = 0; i < num_threads; i++) {
		err = pthread_create(&tids[i], NULL, failing_open, NULL);
	}

	for (i = 0; i < num_threads; i++) {
		err = pthread_join(tids[i], &tret);
	}
}

static void failing_close_mt()
{
	int i, err;
	void *tret;
	pthread_t *tids;
	tids = calloc(num_threads, sizeof(*tids));
	for (i = 0; i < num_threads; i++) {
		err = pthread_create(&tids[i], NULL, failing_close, (void *) (long) i);
	}

	for (i = 0; i < num_threads; i++) {
		err = pthread_join(tids[i], &tret);
	}
}

int main(int argc, char *argv[])
{
	num_threads = strtol(argv[1], NULL, 10);
	num_iter = strtol(argv[2], NULL, 10);

	long num_event;
	struct timeval tval_before, tval_after, tval_result;
	gettimeofday(&tval_before, NULL);

#ifdef FAILING_OPEN_ST
	failing_open();
#endif

#ifdef FAILING_CLOSE_ST
	failing_close(0);
#endif

#ifdef FAILING_OPEN_MT
	failing_open_mt();
#endif
#ifdef FAILING_CLOSE_MT
	failing_close_mt();
#endif
	gettimeofday(&tval_after, NULL);
	timersub(&tval_after, &tval_before, &tval_result);
	long time_diff = (tval_result.tv_sec*1000000) + tval_result.tv_usec;
	printf("%ld", time_diff);
	return 0;
}
