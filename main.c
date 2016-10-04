#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>



long num_iter;
int num_threads;
int cpu_affinity_enabled;

struct thread_arg {
	void *dat;
	int t_no;
};

void set_cpu_affinity(int thread_no)
{
	cpu_set_t set;
	CPU_SET(thread_no, &set);

	sched_setaffinity(0, sizeof(set), &set);
}


static void *failing_open_thr(void *a)
{
	int fd, i, thread_no;
	struct thread_arg *arg;
	char *path;

	arg = (struct thread_arg*) a;
	path = (char*) arg->dat;
	thread_no = (int) arg->t_no;

	if (cpu_affinity_enabled) {
		set_cpu_affinity(thread_no);
	}

	for (i = 0; i < num_iter; i++) {
		/* Will fail with ENOENT/EFAULT since the file does not exist */
		fd = open(a, O_RDONLY);
	}
}
static void *failing_close_thr(void *a)
{
	int i, thread_no;
	int fd = -1;

	struct thread_arg *arg;

	arg = (struct thread_arg*) a;
	thread_no = (int) arg->t_no;

	if (cpu_affinity_enabled) {
		set_cpu_affinity(thread_no);
	}

	for (i = 0; i < num_iter; i++) {
		/* will fail with EBADF since the fd is invalid */
		close(fd);
	}
}

static void failing_open(char *path)
{
	int i, err;
	void *tret;
	pthread_t *tids;
	struct thread_arg arg;

	arg.dat = path;

	tids = calloc(num_threads, sizeof(*tids));
	for (i = 0; i < num_threads; i++) {
		arg.t_no = i;
		err = pthread_create(&tids[i], NULL, failing_open_thr, &arg);
	}

	for (i = 0; i < num_threads; i++) {
		err = pthread_join(tids[i], &tret);
	}
}

static void failing_close(void)
{
	int i, err;
	void *tret;
	pthread_t *tids;
	struct thread_arg arg;

	tids = calloc(num_threads, sizeof(*tids));
	for (i = 0; i < num_threads; i++) {
		arg.t_no = i;
		err = pthread_create(&tids[i], NULL, failing_close_thr, &arg);
	}

	for (i = 0; i < num_threads; i++) {
		err = pthread_join(tids[i], &tret);
	}
}

int main(int argc, char *argv[])
{
	long num_event, time_diff;
	struct timeval tval_before, tval_after, tval_result;


	if (argc != 4) {
		fprintf(stderr, "Wrong number of arguments. %s cpu_affinity_enabled num_thread num_iter\nExiting...\n", argv[0]);
		exit(-1);
	}

	cpu_affinity_enabled = atoi(argv[1]);
	num_threads = atoi(argv[2]);
	num_iter = strtol(argv[3], NULL, 10);

	gettimeofday(&tval_before, NULL);

#ifdef FAILING_OPEN_NULL
	failing_open(NULL);
#endif
#ifdef FAILING_OPEN_NEXIST
	failing_open("/path/to/file");
#endif
#ifdef FAILING_CLOSE
	failing_close();
#endif
	gettimeofday(&tval_after, NULL);
	timersub(&tval_after, &tval_before, &tval_result);
	time_diff = (tval_result.tv_sec * 1000000) + tval_result.tv_usec;

	printf("%ld", time_diff);
	return 0;
}
