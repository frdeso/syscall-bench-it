#!/usr/bin/python

import xmlrpclib
import json
import os
import sys

if len(sys.argv) != 5:
    print("Must provide four arguments. {} {} {} {} {}".format(sys.argv[0],
        "LAVA_KEY", "kernel_imager", "modules_archive", "tools_commit"))
    sys.exit()

token=sys.argv[1]
kernel=sys.argv[2]
modules=sys.argv[3]
tools_commit=sys.argv[4]


job = """
{
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
                    "apt-get update",
                    "apt-get install -y bsdtar psmisc wget python3 python3-pip libglib2.0-dev libffi-dev elfutils",
                    "apt-get install -y libelf-dev libmount-dev libxml2 python3-pandas python3-numpy"
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
}
"""
# We use the kernel image and modules archive received as argument
deploy_action={"command": "deploy_kernel",
            "parameters": {
            		"overlays": [
            		    "scp://jenkins-lava@storage.internal.efficios.com"+modules ],
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
job_dict['actions'].insert(0, deploy_action)
job_dict['actions'].insert(4, setup_action)


username = 'frdeso'
hostname = 'lava-master.internal.efficios.com'
server = xmlrpclib.ServerProxy('http://%s:%s@%s/RPC2' % (username, token, hostname))

jobid = server.scheduler.submit_job(json.dumps(job_dict))

jobstatus = server.scheduler.job_status(jobid)
while jobstatus in 'Submitted' or jobstatus in 'Running':
    sleep(30)
    jobstatus = server.scheduler.job_status(jobid)

print(jobstatus)
