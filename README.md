##Description
This repository is an implementation of a simple FCFS user-level thread scheduling
library in C. It allows the user threads to to open sockets with other processes and
read/write to them. Since I/O is a blocking operation, a dedicated kernel-level thread
is created to handle user-threads implementing I/O operations.