#! /bin/bash
cleanup() {
	exit 1
}
trap cleanup SIGTERM SIGINT

ITER=100000

duration=0
nb_events=0

run_baseline() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	nbIter=$4
	duration=$(./$testcase $cpu_affinity $nbThreads $nbIter)
	nb_events=-1
}
run_strace() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	nbIter=$4
	duration=$(strace -f ./$testcase $cpu_affinity $nbThreads $nbIter 2> /dev/null)
	nb_events=-1
}

run_lttng() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	nbIter=$4
	lttng-sessiond -d
	lttng create
	lttng enable-channel --num-subbuf 4 --subbuf-size 2M -k my_channel
	lttng enable-event -k sched_process_exit,sched_switch,signal_deliver --channel my_channel
	lttng enable-event -k --syscall --all --channel my_channel
	lttng start
	sleep 2
	duration=$(./$testcase $cpu_affinity $nbThreads $nbIter)
	lttng stop
	sleep 1
	nb_events=$(lttng view | wc -l)
	lttng destroy -a
	killall lttng-sessiond
}
run_sysdig() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	nbIter=$4
	tmp=$(mktemp)

	sysdig -w $tmp &
	#save sysdig's pid
	p=$!
	sleep 2
	duration=$(./$testcase $cpu_affinity $nbThreads $nbIter)
	sleep 1

	# Send sigint to sysdig and wait for it to exit
	kill -2 $p
	wait $p

	#sync to make sure the entire trace is written to disk
	sync

	# extract the number of events from the read verbose output
	nb_events=$(sysdig -r $tmp -v 2>&1 >/dev/null |grep 'Elapsed'|awk '{print $6}'|sed 's/,//g')
	rm $tmp
}


output=./results/results-$ITER-7.csv
echo 'testcase,tracer,run,iteration,cpu_affinity,nbthreads,duration,nbevents' > $output
for nthreads in 1 2 4 8 16; do
	for cpu_affinity in 0 1; do
		for tcase in failing-open-efault failing-open-enoent failing-close; do
			for tracer in baseline lttng sysdig; do
				for i in `seq 1 10`; do
					run_$tracer $tcase $cpu_affinity $nthreads $ITER
					echo $tcase,$tracer,$i,$ITER,$nthreads,$duration,$nb_events >> $output
					sleep 1
				done
			done
		done
	done
done

