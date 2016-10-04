#! /bin/bash
cleanup() {
	exit 1
}
trap cleanup SIGTERM SIGINT

sleep_time=1

duration=0
tot_nb_iter=0
nb_events=0

run_baseline() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	sleepTime=$4
	output=$(./$testcase $cpu_affinity $nbThreads $sleepTime)
	duration=$(echo $output | cut -f1 -d ' ')
	tot_nb_iter=$(echo $output | cut -f2 -d ' ')
	nb_events=-1
}
run_strace() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	sleepTime=$4
	output=$(strace -f ./$testcase $cpu_affinity $nbThreads $sleepTime 2> /dev/null)
	duration=$(echo $output | cut -f1 -d-)
	tot_nb_iter=$(echo $output | cut -f2 -d-)
	nb_events=-1
}

run_lttng() {
	testcase=$1
	cpu_affinity=$2
	nbThreads=$3
	sleepTime=$4
	lttng-sessiond -d
	lttng create
	lttng enable-channel --num-subbuf 4 --subbuf-size 2M -k my_channel
	lttng enable-event -k sched_process_exit,sched_switch,signal_deliver --channel my_channel
	lttng enable-event -k --syscall --all --channel my_channel
	lttng start
	sleep 2
	output=$(./$testcase $cpu_affinity $nbThreads $sleepTime)
	duration=$(echo $output | cut -f1 -d-)
	tot_nb_iter=$(echo $output | cut -f2 -d-)
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
	sleepTime=$4
	tmp=$(mktemp)

	sysdig -w $tmp &
	#save sysdig's pid
	p=$!
	sleep 2
	output=$(./$testcase $cpu_affinity $nbThreads $sleepTime)
	duration=$(echo $output | cut -f1 -d-)
	tot_nb_iter=$(echo $output | cut -f2 -d-)
	sleep 1

	# Send sigint to sysdig and wait for it to exit
	kill -2 $p
	wait $p

	#sync to make sure the entire trace is written to disk
	sync

	# extract the number of events from the read verbose output
	sysdig_output=$(sysdig -r $tmp -v 2>&1 >/dev/null)
	nb_events=$(echo $sysdig_output |grep 'Elapsed'|awk '{print $6}'|sed 's/,//g')
	echo $sysdig_output | grep 'Driver'
	rm $tmp
}


file_output=./results/results-$sleep_time-7.csv
echo 'testcase,tracer,run,sleeptime,cpu_affinity,nbthreads,duration,nbiter,nbevents' > $file_output
for nthreads in 1 2; do
	for cpuaffinity in 0 1; do
		for tcase in failing-open-efault failing-open-enoent failing-close; do
			for tracer in sysdig; do
				for i in `seq 1 2`; do
					run_$tracer $tcase $cpuaffinity $nthreads $sleep_time
					echo $tcase,$tracer,$i,$sleep_time,$cpuaffinity,$nthreads,$duration,$tot_nb_iter,$nb_events >> $file_output
					sleep 1
				done
			done
		done
	done
done

