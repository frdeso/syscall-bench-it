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
static volatile int test_stop __attribute__((aligned(256)));
unsigned long long *tot_nr_iter_per_thread;
sem_t sem_thr;
int num_threads;
int cpu_affinity_enabled;

int set_cpu_affinity(int thread_no)
{
	cpu_set_t set;
	int cpu = 0, ret = 0;

	/*
	 * This code spreads the threads on all the NUMA nodes available on the
	 * machine.
	 * For example, on machine with 2 NUMA nodes and 16 cores and a run
	 * with 4 threads, thread0 and thread2 will be pinned to the first NUMA
	 * node and thread1 and thread3 will be pinned to the second.
	 */
	int nb_numa_nodes = numa_num_configured_nodes();
	int nb_cores = numa_num_configured_cpus();
	int nb_cores_per_nodes = nb_cores / nb_numa_nodes;

	cpu = (thread_no / nb_numa_nodes) + ((thread_no % nb_numa_nodes) * nb_cores_per_nodes);

	CPU_ZERO(&set);
	CPU_SET(cpu, &set);

	ret = sched_setaffinity(0, sizeof(set), &set);
	if (ret == -1) {
		perror("sched_setaffinity");
		return ret;
	}
	return 0;
}

/*
 * Every testcase has to define 3 callbacks; init, run and exit.
 * - The init function is called once at the beginning of the testcase
 * - The run function is called in loop and contains the event we want to record
 * on with the tracer.
 * - The exit function is run at teardown
 * If a testcase does not need an init or exit function it can use the `nil()`
 * function.
 */
struct thread_arg {
	void *dat;
	int t_no;
	void *priv_data;
};

struct testcase_cbs {
	void (*init)(void *thr);
	void (*run)(void *priv);
	void (*exit)(void *priv);
	struct thread_arg *thr_arg;
};

/*
 * Function is ran by the threads. It receives the testcase defined in a
 * struct testcase_cbs which contains the init,run and exit callbacks.
 */
static void *run_testcase(void *arg)
{
	int fd, thread_no, ret;
	struct thread_arg *thr_arg;
	char *path;
	unsigned long nb_iter = 0;

	struct testcase_cbs *cbs = (struct testcase_cbs *) arg;
	thr_arg = (struct thread_arg *) cbs->thr_arg;
	thread_no = (int) thr_arg->t_no;

	if (cpu_affinity_enabled) {
		ret = set_cpu_affinity(thread_no);
		if (ret != 0) {
			exit(ret);
		}
	}

	/* Call the init function of the test case */
	cbs->init(thr_arg);

	/* Post on the semaphore to tell main this thread is ready to go */
	ret = sem_post(&sem_thr);
	if (ret == -1) {
		perror("sem_post");
		exit(-1);
	}

	while (!test_go) {
		/* loop until the variable is set by main to start looping */
	}

	/* Call the run function of the test case */
	while (!test_stop) {
		cbs->run(thr_arg->priv_data);
		nb_iter++;
	}

	tot_nr_iter_per_thread[thread_no] = nb_iter;

	/* Call the exit function of the test case */
	cbs->exit(thr_arg->priv_data);

	/* Post on the semaphore to tell main this thread is done */
	ret = sem_post(&sem_thr);
	if (ret == -1) {
		perror("sem_post");
		exit(-1);
	}
	return (void*)1;
}

void nil() {}

struct lttng_test_filter_priv_data {
	int fd;
	char *nb_event_per_call;
	unsigned long event_str_len;
};

void lttng_test_filter_init(void *arg)
{
	struct thread_arg *thr_arg = (struct thread_arg*)arg;
	struct lttng_test_filter_priv_data *priv_data = malloc(sizeof(struct lttng_test_filter_priv_data));
	if (priv_data == NULL) {
		printf("malloc error\n");
		exit(-1);
	}

	char *path = (char *) thr_arg->dat;

	priv_data->nb_event_per_call = malloc(sizeof(char) * 16);
	if (priv_data->nb_event_per_call == NULL) {
		printf("malloc error\n");
		exit(-1);
	}

	/* Create a string to generate 1 event */
	strncpy(priv_data->nb_event_per_call, "1\0", 16);
	priv_data->event_str_len = sizeof('\0') + strnlen(priv_data->nb_event_per_call, 16);

	priv_data->fd = open(path, O_WRONLY);
	if (priv_data->fd  == -1) {
		perror("fopen");
		exit(-1);
	}
	thr_arg->priv_data = priv_data;
}

void lttng_test_filter_run(void *arg)
{
	struct lttng_test_filter_priv_data *lpd = (struct lttng_test_filter_priv_data *)arg;
	unsigned long write_ret;

	/* Write the string containing the number of events to be generated */
	write_ret = write(lpd->fd, lpd->nb_event_per_call, lpd->event_str_len);
	if (write_ret != lpd->event_str_len) {
		printf("write returned %lu, when expected is %lu\n", write_ret, lpd->event_str_len);
		exit(-1);
	}
}

void lttng_test_filter_exit(void *arg)
{
	int ret;
	struct lttng_test_filter_priv_data *lpd = (struct lttng_test_filter_priv_data *)arg;

	ret = close(lpd->fd);
	if (ret == -1) {
		perror("close");
		exit(-1);
	}
	free(lpd->nb_event_per_call);
	free(lpd);
}

struct open_priv_data {
	int fd;
	char *path;
};

void open_init(void *arg)
{
	struct thread_arg *thr_arg = (struct thread_arg*)arg;
	struct open_priv_data *priv_data;
	priv_data = malloc(sizeof(struct open_priv_data));
	if (priv_data == NULL) {
		printf("malloc error\n");
		exit(-1);
	}
	thr_arg->priv_data = priv_data;
}

void open_run(void *arg)
{
	struct open_priv_data *opd = (struct open_priv_data*) arg;
	int fd = open(opd->path, O_RDONLY);
}

void open_exit(void *arg)
{
	free(arg);
}

void dup_close_init(void *arg)
{
	struct thread_arg *thr_arg = (struct thread_arg*)arg;
	struct open_priv_data *priv_data;

	char *path = (char *)thr_arg->dat;

	priv_data = malloc(sizeof(struct open_priv_data));
	if (priv_data == NULL) {
		printf("malloc error\n");
		exit(-1);
	}

	priv_data->fd = open(path, O_RDONLY);
	thr_arg->priv_data = priv_data;
}

void dup_close_run(void *arg)
{
	struct open_priv_data *opd = (struct open_priv_data*) arg;
	int new_fd;
	new_fd = dup(opd->fd);
	close(new_fd);
}

void dup_close_exit(void *arg)
{
	struct open_priv_data *opd = (struct open_priv_data*) arg;
	close(opd->fd);
	free(opd);
}

void failing_close_run(void *arg)
{
	close(-1);
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
	struct testcase_cbs *args;
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
	struct testcase_cbs cbs =
		{
			.init	= open_init,
			.run	= open_run,
			.exit	= open_exit,
		};
#endif /* FAILING_OPEN_NULL */

#ifdef FAILING_OPEN_NEXIST
	dat = "/path/to/file";
	struct testcase_cbs cbs =
		{
			.init	= open_init,
			.run	= open_run,
			.exit	= open_exit,
		};
#endif /* FAILING_OPEN_NEXIST */

#ifdef SUCCESS_DUP_CLOSE
	dat = "/etc/passwd";
	struct testcase_cbs cbs =
		{
			.init	= dup_close_init,
			.run	= dup_close_run,
			.exit	= dup_close_exit,
		};
#endif /* SUCCESS_DUP_CLOSE */
#ifdef FAILING_CLOSE
	struct testcase_cbs cbs =
		{
			.init	= nil,
			.run	= failing_close_run,
			.exit	= nil,
		};
#endif /* FAILING_CLOSE */

#ifdef LTTNG_TEST_FILTER
	dat = "/proc/lttng-test-filter-event";
	struct testcase_cbs cbs =
		{
			.init	= lttng_test_filter_init,
			.run	= lttng_test_filter_run,
			.exit	= lttng_test_filter_exit,
		};
#endif

	tids = calloc(num_threads, sizeof(*tids));
	if (tids == NULL) {
		printf("calloc error\n");
		exit(-1);
	}

	args = calloc(num_threads, sizeof(struct testcase_cbs));
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

	ret = sem_init(&sem_thr, 0, 0);
	if (ret == -1) {
		perror("sem_init");
		exit(-1);
	}

	for (i = 0; i < num_threads; i++) {
		args[i] = cbs;
		args[i].thr_arg = malloc(sizeof(struct thread_arg));
		if (args[i].thr_arg == NULL) {
			printf("calloc error\n");
			exit(-1);
		}
		args[i].thr_arg->t_no = i;
		args[i].thr_arg->dat = dat;
		ret = pthread_create(&tids[i], NULL, &run_testcase, &args[i]);
		if (ret != 0 ) {
			printf("pthread_create error: %d\n", ret);
			exit(-1);
		}
	}

	/* Wait for all the threads to be ready */
	for (i = 0; i < num_threads; i++) {
		ret = sem_wait(&sem_thr);
		if (ret == -1) {
			perror("sem_wait");
			exit(-1);
		}
	}

	/* Record the before timestamp */
	ret = gettimeofday(&tval_before, NULL);
	if (ret == -1) {
		perror("gettimeofday");
		exit(-1);
	}

	/* Start the test case and let it run for sleep_time second */
	test_go = 1;
	while (nanosleep(&sleep_duration, &sleep_duration) == -1){
		continue;
	}
	test_stop = 1;

	/* Wait for all the threads to be done */
	for (i = 0; i < num_threads; i++) {
		ret = sem_wait(&sem_thr);
		if (ret == -1) {
			perror("sem_wait");
			exit(-1);
		}
	}

	/* Record the after timestamp */
	gettimeofday(&tval_after, NULL);
	if (ret == -1) {
		perror("gettimeofday");
		exit(-1);
	}

	for (i = 0; i < num_threads; i++) {
		ret = pthread_join(tids[i], &tret);
		free(args[i].thr_arg);
		if (ret != 0) {
			printf("pthread_join error\n");
			exit(-1);
		}
		/* Sum the number of iteration for all the threads */
		total_nr_iter += tot_nr_iter_per_thread[i];
	}

	timersub(&tval_after, &tval_before, &tval_result);
	time_diff = (tval_result.tv_sec * 1000000ULL) + tval_result.tv_usec;

	ret = sem_destroy(&sem_thr);
	if (ret == -1) {
		perror("sem_destroy");
		exit(-1);
	}

	printf("%llu %llu", time_diff, total_nr_iter);

	free(tids);
	free(tot_nr_iter_per_thread);
	free(args);
	return 0;
}
