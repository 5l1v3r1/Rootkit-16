# Rootkit
Create a attack program and a Sneaky Kernel Module (a Linux Kernel Module – LKM) with the following functions:
1.It will hide the “sneaky_process” executable file from both the ‘ls’ and ‘find’ UNIX commands.

2.In a UNIX environment, every executing process will have a directory under /proc that is named with its process ID (e.g /proc/1480). This directory contains many details about the process. The sneaky kernel module will hide the /proc/<sneaky_process_id> directory

3.It will hide the modifications to the /etc/passwd file that the sneaky_process made. It will
do this by opening the saved “/tmp/passwd” when a request to open the “/etc/passwd” is seen.

4.It will hide the fact that the sneaky_module itself is an installed kernel module. 
