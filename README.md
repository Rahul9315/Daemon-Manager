Feature 1 -  System Architecture including makefile 

Architecture Diagram.

The Daemon Manager runs in the background to automatically handle system tasks such as file transfers along with backup operations and monitoring activities. The system architecture follows Separation of Concerns (SoC) and Single Responsibility Principle (SRP) principles to achieve modularity and maintainability.

•	Daemonization : It works independently with no communication with the terminal of the linux.
•	Parallel Processing: the system uses fork to create child processes to work independently without any other interference.
•	IPC : to utilise message queues and FIFo pipelines to work with difeerent users.
•	Error handling and logging: to log all the events and errors in the same place for tracing the program in daemon_log.txt 

MakeFile :

Makefile helps the server to automates compilation , execution of the program , stopping and cleaning for the Daemon.

•	make : complies the daemon_manger.c file
•	make run : starts the daemon to work
•	make stop : to pkill the daemon processes and terminate the daemon
•	make restart: to stop and restart the daemon again.
•	Clean : to removes the compiled executable file 

 
 



Feature 2 - Daemon (Setup/ Initialisation/ Management)


Daemon Setup: 

•	The daemonize() function operates by separating the process from terminal control.
•	Changes the working directory to /Downloads/Assignment2. 
•	Closes standard input/output streams. 
•	Uses setsid() to create a new session. 

Startup Script & Init Process :

1.	Run the Makefile command to start the daemon.
 

2.	To stop the daemon: 
 


Daemon Control Options :

•	Start: make run 
•	Stop: make stop 
•	Restart: make restart


Feature 3 - Daemon (Implementation)

The daemon process is created using fork(). The main process: 

•	Creates child processes for wait_until_1am() and listen_for_manual_trigger(). 
•	Parent process remains active for monitoring.
•	Scheduled tasks and manual triggers operate through independent execution by each child process.

 

 

 
Feature 4 - Backup Functionality 

•	Backup system performs data integrity checks before executing file transfers.
•	Reads files from ./uploads 
•	Creates a backup in ./backup 
•	The backup system duplicates file contents before transferring them to reports/.
•	Logs success/failure of each backup operation. 



Feature 5 - Transfer Functionality 
Detailed description of the transfer implementation

•	The file transfer system activates at 1:00 AM to move files.
•	Checks for missing reports. 
•	The system secures the directory against changes while files are being transferred.
•	Moves files from uploads/ to reports/. 
•	The system releases the directory lock as soon as the transfer process finishes.
•	Logs each file transferred with timestamps.

 

•	Files are also transferred with the manual trigger.
 
 

Feature 6 - Lockdown directories for Backup / Transfer 

To ensure secure file transfer file restriction are putted before the files are moved so that only daemon could have access to the files no body else could make changes.

 

Feature 7 – Process management and IPC

IPC is handling the messages queues and FIFO pipe

•	Message Queue : are used to report the status messages
•	FIFO : is used to take manual triggers for backup.
•	Logging : is used to keep the daemon_log.txt updated of each changes .






Feature 8 - Logging and Error Logging 
All system log and operation or process are updated to the daemon_log.txt
For example:
•	Successful transfers
•	Backups
•	Errors
•	Triggers

  



Conclusion

It is a properly functioned and well managed Daemon manager and ensures the proper functionality of :

•	Automated File transfers and backups.
•	Securing Locking and unlock the directories.
•	Parallel execution using fork for scheduled and manual triggers using FIFO.
•	Proper logging and error tracing.

This Program pattern or Design ensures reliability, maintenance and extensibility proving a better and efficient automated reports management System.
