#!/usr/bin/python

import json
import os
import sys
import time
import xmlrpclib

if len(sys.argv) != 7:
    print("Must provide 6 arguments.{} {} {} {} {} {} {}".format(sys.argv[0],
        "job_name","LAVA_KEY", "kernel_image","kernel_modules_archive", "lttng_modules_archive", "tools_commit"))
    sys.exit()

job_name=sys.argv[1]
token=sys.argv[2]
kernel=sys.argv[3]
linux_modules=sys.argv[4]
lttng_modules=sys.argv[5]
tools_commit=sys.argv[6]


job ="""{
    "health_check": false,
    "job_name": "performance-tracker-benchmark-syscalls",
    "device_type": "x86",
    "tags": [ "dev-sda1" ],
    "timeout": 18000,
    "actions": [
        {
            "command": "boot_image"
        },
        {
            "command": "lava_command_run",
            "parameters": {
                "commands": [
                    "ifup eth0",
                    "route -n",
                    "cat /etc/resolv.conf",
                    "echo nameserver 172.18.0.12 > /etc/resolv.conf",
                    "mount /dev/sda1 /tmp",
                    "rm -rf /tmp/*"
                ]
            }
        },
        {
            "command": "lava_command_run",
            "parameters": {
                "commands": [
                    "locale-gen en_US.UTF-8",
                    "apt-get update",
                    "apt-get install -y bsdtar psmisc wget python3 python3-pip libglib2.0-dev libffi-dev elfutils",
                    "apt-get install -y libelf-dev libmount-dev libxml2 python3-pandas python3-numpy babeltrace"
                ]
            }
        },
        {
            "command": "lava_test_shell",
            "parameters": {
                "testdef_repos": [
                    {
                        "git-repo": "https://github.com/frdeso/syscall-bench-it.git",
                        "revision": "master",
                        "testdef": "lava/testcases/failing-close.yml",
                         "parameters": {}
                    },
                    {
                        "git-repo": "https://github.com/frdeso/syscall-bench-it.git",
                        "revision": "master",
                        "testdef": "lava/testcases/failing-open-efault.yml"
                    },
                    {
                        "git-repo": "https://github.com/frdeso/syscall-bench-it.git",
                        "revision": "master",
                        "testdef": "lava/testcases/failing-open-enoent.yml"
                    }
                ],
                "timeout": 18000
            }
        },
         {
            "command": "submit_results",
            "parameters": {
                "server": "http://lava-master.internal.efficios.com/RPC2/",
                "stream": "/anonymous/benchmark-kernel/"
            }
        }
    ]
}"""

# We use the kernel image and modules archive received as argument
deploy_action={"command": "deploy_kernel",
                "metadata": {
                    "jenkins_jobname": job_name,
                    "nb_iterations": 2000000000
                },
            "parameters": {
                "overlays": [
                    "scp://jenkins-lava@storage.internal.efficios.com"+linux_modules,
                    "scp://jenkins-lava@storage.internal.efficios.com"+lttng_modules
                    ],
                "kernel":
                "scp://jenkins-lava@storage.internal.efficios.com"+kernel,
                "nfsrootfs": "scp://jenkins-lava@storage.internal.efficios.com/storage/jenkins-lava/rootfs/rootfs_amd64_trusty_2016-02-23-1134.tar.gz",
                "target_type": "ubuntu"
            }
        }

# We checkout the commit id for tools
setup_action = {
            "command": "lava_command_run",
            "parameters": {
                "commands": [
                	"git clone https://github.com/frdeso/syscall-bench-it.git bm",
                    "pip3 install vlttng",
                    "vlttng --jobs=16 --profile urcu-master \
                    --profile lttng-tools-master -o \
                    projects.lttng-tools.checkout="+tools_commit+ \
                     " /tmp/virtenv"
                ]
            }
        }
job_dict= json.loads(job)
for t in [i for i in job_dict['actions'] if i['command'] == 'lava_test_shell']:
    for a in t['parameters']['testdef_repos']:
        a['parameters'] = {}
        a['parameters']['JENKINS_JOBNAME'] = job_name
job_dict['job_name']=job_name
job_dict['actions'].insert(0, deploy_action)
job_dict['actions'].insert(4, setup_action)


username = 'frdeso'
hostname = 'lava-master.internal.efficios.com'
server = xmlrpclib.ServerProxy('http://%s:%s@%s/RPC2' % (username, token, hostname))

jobid = server.scheduler.submit_job(json.dumps(job_dict))

jobstatus = server.scheduler.job_status(jobid)['job_status']
while jobstatus in 'Submitted' or jobstatus in 'Running':
    time.sleep(30)
    jobstatus = server.scheduler.job_status(jobid)['job_status']

if jobstatus not in 'Complete':
    print(jobstatus)
    sys.exit(-1)
else:
    sys.exit(0)
