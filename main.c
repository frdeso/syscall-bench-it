#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <numa.h>
#include <pthread.h>
#include <semaphore.h>
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
static volatile int test_stop;
unsigned long long *tot_nr_iter_per_thread;
sem_t sem_start, sem_stop;
int num_threads;
int cpu_affinity_enabled;

struct thread_arg {
	void *dat;
	int t_no;
};

void set_cpu_affinity(int thread_no)
{
	cpu_set_t set;
	int cpu = 0;

	/*
	 * This code spreads the thread on all the NUMA nodes available on the
	 * machine.
	 * For example, on machine with 2 NUMA nodes ands 16 cores and a run
	 * with 4 threads, thread0 and thread2 will be pinned to the first NUMA
	 * node and thread1 and thread3 will be pinned to the second.
	 */
	int nb_numa_nodes = numa_num_configured_nodes();
	int nb_cores = numa_num_configured_cpus();
	int nb_cores_per_nodes = nb_cores / nb_numa_nodes;

	cpu = (thread_no / nb_numa_nodes) + ((thread_no % nb_numa_nodes) * nb_cores_per_nodes);

	CPU_SET(cpu, &set);

	sched_setaffinity(0, sizeof(set), &set);
}


static void *failing_open_thr(void *a)
{
	int fd, thread_no, ret;
	struct thread_arg *arg;
	char *path;
	unsigned long nb_iter = 0;

	arg = (struct thread_arg *) a;
	path = (char *) arg->dat;
	thread_no = (int) arg->t_no;

	if (cpu_affinity_enabled) {
		set_cpu_affinity(thread_no);
	}

	/* Post on the semaphore to tell main this thread is ready to go */
	ret = sem_post(&sem_start);
	if (ret == -1) {
		printf("sem_post error\n");
		exit(-1);
	}

	while (!test_go) {
		/* loop until the variable is set by main to start looping */
	}

	while (!test_stop) {
		/* Will fail with ENOENT/EFAULT since the file does not exist */
		fd = open(path, O_RDONLY);
		nb_iter++;
	}
	tot_nr_iter_per_thread[thread_no] = nb_iter;
	return (void*)1;
}

static void *failing_close_thr(void *a)
{
	int thread_no, ret;
	int fd = -1;
	unsigned long nb_iter = 0;
	struct thread_arg *arg;

	arg = (struct thread_arg *) a;
	thread_no = (int) arg->t_no;

	if (cpu_affinity_enabled) {
		set_cpu_affinity(thread_no);
	}

	/* Post on the semaphore to tell main this thread is ready to go */
	ret = sem_post(&sem_start);
	if (ret == -1) {
		printf("sem_post error\n");
		exit(-1);
	}

	while (!test_go) {
		/* loop until the variable is set by main to start looping */
	}

	while (!test_stop) {
		/* Will fail since the fd is invalid */
		close(fd);
		nb_iter++;
	}
	tot_nr_iter_per_thread[thread_no] = nb_iter;
	return (void*)1;
}

int main(int argc, char *argv[])
{
	int i, ret;
	unsigned long long time_diff, sleep_sec, sleep_nsec;
	unsigned long long total_nr_iter = 0;
	unsigned long long sleep_time;
	void *tret;
	struct timeval tval_before, tval_after, tval_result;
	struct timespec sleep_duration;
	struct thread_arg *args;
	char *dat;
	pthread_t *tids;
	/* Declare the function pointer that we use to define the testcase */
	void *(*func)(void *);

	if (argc != 4) {
		fprintf(stderr, "Wrong number of arguments. %s cpu_affinity_enabled\
				num_thread sleep_time\nExiting...\n", argv[0]);
		exit(-1);
	}

	cpu_affinity_enabled = atoi(argv[1]);
	num_threads = atoi(argv[2]);
	errno = 0;
	sleep_time = strtoull(argv[3], NULL, 10); // return value of this call
	if (errno == ERANGE) {
		printf("Error during strtoull %d\n", errno);
		exit(-1);
	}

	sleep_sec = sleep_time / NSEC_PER_SEC;
	sleep_nsec = sleep_time - (sleep_sec * NSEC_PER_SEC);

	sleep_duration.tv_sec = sleep_sec;
	sleep_duration.tv_nsec = sleep_nsec;

#ifdef FAILING_OPEN_NULL
	dat = NULL;
	func = &failing_open_thr;
#endif

#ifdef FAILING_OPEN_NEXIST
	dat = "/path/to/file";
	func = &failing_open_thr;
#endif

#ifdef FAILING_CLOSE
	func = &failing_close_thr;
#endif

	tids = calloc(num_threads, sizeof(*tids));
	if (tids == NULL) {
		printf("calloc error\n");
		exit(-1);
	}

	args = calloc(num_threads, sizeof(struct thread_arg));
	if (args == NULL) {
		printf("calloc error\n");
		exit(-1);
	}

	tot_nr_iter_per_thread = calloc(num_threads, sizeof(unsigned long long));
	if (tot_nr_iter_per_thread == NULL) {
		printf("calloc error\n");
		exit(-1);
	}

	/* Initialize the numa librarie */
	numa_available();

	ret = sem_init(&sem_start, 0, 0);
	if (ret == -1) {
		printf("sem_init error\n");
		exit(-1);
	}

	for (i = 0; i < num_threads; i++) {
		args[i].t_no = i;
		args[i].dat = dat;
		ret = pthread_create(&tids[i], NULL, func, &args[i]);
		if (ret != 0 ) {
			printf("pthread_create error: %d\n", ret);
			exit(-1);
		}
	}

	/* Wait for all the threads to be ready */
	for (i = 0; i < num_threads; i++) {
		ret = sem_wait(&sem_start);
		if (ret == -1) {
			printf("sem_wait error\n");
			exit(-1);
		}
	}

	/* Record the before timestamp */
	ret = gettimeofday(&tval_before, NULL);
	if (ret == -1) {
		printf("gettimeofday error\n");
		exit(-1);
	}

	/* Start the test case and let it run for sleep_time second */
	test_go = 1;
	while (nanosleep(&sleep_duration, &sleep_duration) == -1){
		continue;
	}
	test_stop = 1;

	/* Record the after timestamp */
	gettimeofday(&tval_after, NULL);
	if (ret == -1) {
		printf("gettimeofday error\n");
		exit(-1);
	}

	for (i = 0; i < num_threads; i++) {
		ret = pthread_join(tids[i], &tret);
		if (ret != 0) {
			printf("pthread_join error\n");
			exit(-1);
		}
		/* Sum the number of iteration for all the threads */
		total_nr_iter += tot_nr_iter_per_thread[i];
	}

	timersub(&tval_after, &tval_before, &tval_result);
	time_diff = (tval_result.tv_sec * 1000000ULL) + tval_result.tv_usec;

	ret = sem_destroy(&sem_start);
	if (ret == -1) {
		printf("sem_destroy error\n");
		exit(-1);
	}

	printf("%llu %llu", time_diff, total_nr_iter);

	free(tids);
	free(tot_nr_iter_per_thread);
	free(args);
	return 0;
}
