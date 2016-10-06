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
#include <time.h>
#include <unistd.h>

#define NSEC_PER_SEC 1000000000

static volatile int test_go;
unsigned long *tot_nr_iter_per_thread;
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
	int fd, thread_no;
	struct thread_arg *arg;
	char *path;
	unsigned int nb_iter;

	nb_iter = 0;
	arg = (struct thread_arg*) a;
	path = (char*) arg->dat;
	thread_no = (int) arg->t_no;

	if (cpu_affinity_enabled) {
		set_cpu_affinity(thread_no);
	}

	while(!test_go) {
		/* loop until the variable is set by main to start looping */
	}

	while(test_go) {
		/* Will fail with ENOENT/EFAULT since the file does not exist */
		fd = open(path, O_RDONLY);
		nb_iter++;
	}
	tot_nr_iter_per_thread[thread_no] = nb_iter;
	return (void*)1;
}

static void *failing_close_thr(void *a)
{
	int thread_no;
	int fd = -1;
	unsigned long nb_iter;
	struct thread_arg *arg;

	nb_iter = 0;
	arg = (struct thread_arg*) a;
	thread_no = (int) arg->t_no;

	if (cpu_affinity_enabled) {
		set_cpu_affinity(thread_no);
	}

	while(!test_go) {
		/* loop until the variable is set by main to start looping */
	}

	while(test_go) {
		/* Will fail with ENOENT/EFAULT since the file does not exist */
		close(fd);
		nb_iter++;
	}
	tot_nr_iter_per_thread[thread_no] = nb_iter;
	return (void*)1;
}

int main(int argc, char *argv[])
{
	int i, err;
	long time_diff, sleep_sec, sleep_nsec;
	unsigned long total_nr_iter;
	unsigned long long sleep_time;
	void * tret;
	struct timeval tval_before, tval_after, tval_result;
	struct timespec sleep_duration;
	struct thread_arg arg;
	pthread_t *tids;

	test_go = 0;
	total_nr_iter = 0;

	if (argc != 4) {
		fprintf(stderr, "Wrong number of arguments. %s cpu_affinity_enabled\
				num_thread sleep_time\nExiting...\n", argv[0]);
		exit(-1);
	}

	cpu_affinity_enabled = atoi(argv[1]);
	num_threads = atoi(argv[2]);
	sleep_time = strtoull(argv[3], NULL, 10);

	sleep_sec = sleep_time/NSEC_PER_SEC;
	sleep_nsec = sleep_time - (sleep_sec*NSEC_PER_SEC);

	sleep_duration.tv_sec = sleep_sec;
	sleep_duration.tv_nsec = sleep_nsec;

	/* Declare the function pointer that we use to define the testcase */
	void* (*func)(void*);

#ifdef FAILING_OPEN_NULL
	arg.dat = NULL;
	func = &failing_open_thr;

#endif
#ifdef FAILING_OPEN_NEXIST
	arg.dat= "/path/to/file";
	func = &failing_open_thr;
#endif
#ifdef FAILING_CLOSE
	func = &failing_close_thr;
#endif

	tids = calloc(num_threads, sizeof(*tids));
	tot_nr_iter_per_thread = calloc(num_threads, sizeof(unsigned long));

	for (i = 0; i < num_threads; i++) {
		arg.t_no = i;
		err = pthread_create(&tids[i], NULL, func, &arg);
	}

	/* Wait for all the threads to be ready */
	sleep(1);

	/* Record the before timestamp */
	gettimeofday(&tval_before, NULL);

	/* Start the test case and let it run for sleep_time second */
	test_go = 1;
	nanosleep((const struct timespec*)&sleep_duration, NULL);
	test_go = 0;

	/* Record the after timestamp */
	gettimeofday(&tval_after, NULL);

	for (i = 0; i < num_threads; i++) {
		err = pthread_join(tids[i], &tret);
		if (err != 0) {
			exit(-1);
		}
		/* Sum the number of iteration for all the threads */
		total_nr_iter += tot_nr_iter_per_thread[i];
	}

	timersub(&tval_after, &tval_before, &tval_result);
	time_diff = (tval_result.tv_sec * 1000000) + tval_result.tv_usec;

	printf("%ld %ld", time_diff, total_nr_iter);

	free(tids);
	free(tot_nr_iter_per_thread);
	return 0;
}
