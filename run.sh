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
	nbThreads=$2
	nbIter=$3
	duration=`./$testcase $nbThreads $nbIter`
	nb_events=-1
}
run_strace() {
	testcase=$1
	nbThreads=$2
	nbIter=$3
	duration=$(strace -f ./$testcase $nbThreads $nbIter 2> /dev/null)
	nb_events=-1
}

run_lttng() {
	testcase=$1
	nbThreads=$2
	nbIter=$3
	lttng-sessiond -d
	lttng create 
	lttng enable-channel --num-subbuf 4 --subbuf-size 2M -k my_channel
	lttng enable-event -k sched_process_exit,sched_switch,signal_deliver --channel my_channel 
	lttng enable-event -k --syscall --all --channel my_channel 
	lttng start 
	sleep 2
	duration=$(./$testcase $nbThreads $nbIter)
	lttng stop 
	sleep 1
	nb_events=$(lttng view | wc -l)
	lttng destroy -a 
	killall lttng-sessiond
}
run_sysdig() {
	testcase=$1
	nbThreads=$2
	nbIter=$3
	tmp=$(mktemp)

	sysdig -w $tmp &
	#save sysdig's pid
	p=$!
	sleep 2
	duration=$(./$testcase $nbThreads $nbIter)
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


output=./results/results-$ITER-3.csv
echo 'testcase,tracer,run,iteration,nbthreads,duration,nbevents' > $output
for nthreads in 1 2 4 8 16; do
	for tcase in failing-open failing-close; do
		for tracer in  baseline strace lttng sysdig; do
			for i in `seq 1 5`; do
				run_$tracer $tcase $nthreads $ITER
				echo $tcase,$tracer,$i,$ITER,$nthreads,$duration,$nb_events >> $output
				sleep 1
			done
		done 
	done
done

