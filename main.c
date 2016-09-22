#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>

#ifndef NUM_ITER
#define NUM_ITER 10000000
#endif

#define MISSING_FILE "/var/foo"

void failing_open()
{
	int fd, i;

	for(i = 0; i < NUM_ITER; i++) {
		/* Will fail with ENOENT since the file does not exist */
		fd = open(MISSING_FILE, O_RDONLY);
	}
}
void failing_close()
{
	int i;
	int fd = 4242;

	for(i = 0; i < NUM_ITER; i++) {
		/* will fail with EBADF since the fd is invalid */
		close(fd);
	}
}
int main()
{
	struct timeval tval_before, tval_after, tval_result;
	gettimeofday(&tval_before, NULL);

#ifdef FAILING_OPEN
	failing_open();
#endif
#ifdef FAILING_CLOSE
	failing_close();
#endif
	gettimeofday(&tval_after, NULL);
	timersub(&tval_after, &tval_before, &tval_result);
	long long time_diff = (tval_result.tv_sec*1000000) + tval_result.tv_usec;
	printf("%lld\n", time_diff);
	printf("Time elapsed: %lld usec, time per call:%f usec\n",
	       time_diff, time_diff/(double)NUM_ITER);
	return 0;
}
