#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>



long num_iter;
int num_threads;

static void *failing_open(void *a)
{
	int fd, i;
	char *path = (char*)a;

	for (i = 0; i < num_iter; i++) {
		/* Will fail with ENOENT since the file does not exist */
		fd = open(a, O_RDONLY);
	}
}
static void *failing_close(void *a)
{
	int i;
	int fd = -1;

	for (i = 0; i < num_iter; i++) {
		/* will fail with EBADF since the fd is invalid */
		close(fd);
	}
}

static void failing_open_mt(char *path)
{
	int i, err;
	void *tret;
	pthread_t *tids;

	tids = calloc(num_threads, sizeof(*tids));
	for (i = 0; i < num_threads; i++) {
		err = pthread_create(&tids[i], NULL, failing_open, path);
	}

	for (i = 0; i < num_threads; i++) {
		err = pthread_join(tids[i], &tret);
	}
}

static void failing_close_mt(void)
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
	long num_event, time_diff;
	struct timeval tval_before, tval_after, tval_result;

	num_threads = strtol(argv[1], NULL, 10);
	num_iter = strtol(argv[2], NULL, 10);

	gettimeofday(&tval_before, NULL);

#ifdef FAILING_OPEN_NULL
	failing_open_mt(NULL);
#endif
#ifdef FAILING_OPEN_NEXIST
	failing_open_mt("/path/to/file");
#endif
#ifdef FAILING_CLOSE
	failing_close_mt();
#endif
	gettimeofday(&tval_after, NULL);
	timersub(&tval_after, &tval_before, &tval_result);
	time_diff = (tval_result.tv_sec * 1000000) + tval_result.tv_usec;

	printf("%ld", time_diff);
	return 0;
}
