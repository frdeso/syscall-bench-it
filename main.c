#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef NUM_ITER
#define NUM_ITER 10000000
#endif

#ifndef NUM_THREAD
#define NUM_THREAD 16
#endif

#define MISSING_FILE "/var/foo"

static void* failing_open()
{
	int fd, i;

	for(i = 0; i < NUM_ITER; i++) {
		/* Will fail with ENOENT since the file does not exist */
		fd = open(MISSING_FILE, O_RDONLY);
	}
}
static void* failing_close(void *a)
{
	int i;
	int fd = ((long)a)+4242;

	for(i = 0; i < NUM_ITER; i++) {
		/* will fail with EBADF since the fd is invalid */
		close(fd);
	}
}

static void failing_open_mt()
{
	int i, err;
	void *tret;
	pthread_t *tids;
	tids = calloc(NUM_THREAD, sizeof(*tids));
	for (i = 0; i < NUM_THREAD; i++) {
		err = pthread_create(&tids[i], NULL, failing_open, NULL);
	}

	for (i = 0; i < NUM_THREAD; i++) {
		err = pthread_join(tids[i], &tret);
	}
}

static void failing_close_mt()
{
	int i, err;
	void *tret;
	pthread_t *tids;
	tids = calloc(NUM_THREAD, sizeof(*tids));
	for (i = 0; i < NUM_THREAD; i++) {
		err = pthread_create(&tids[i], NULL, failing_close, (void *) (long) i);
	}

	for (i = 0; i < NUM_THREAD; i++) {
		err = pthread_join(tids[i], &tret);
	}
}
int main()
{
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
	long long time_diff = (tval_result.tv_sec*1000000) + tval_result.tv_usec;
	printf("%lld\n", time_diff);
	printf("Time elapsed: %lld usec, time per call:%f usec\n",
	       time_diff, time_diff/(double)NUM_ITER);
	return 0;
}
