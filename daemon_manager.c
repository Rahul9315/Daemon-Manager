#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pwd.h>


#define UPLOAD_DIR "./uploads"
#define REPORT_DIR "./reports"
#define LOG_FILE "./daemon_log.txt"
#define BACKUP_DIR "./backup"
#define TRIGGER_FILE "./try"
#define FIFO_PATH "/tmp/daemon_fifo"
#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

struct msg_buffer {
    long msg_type;
    char msg_text[100];
} message;

void log_message(const char *message) {
    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        fprintf(log, "[%s] %s\n", ctime(&now), message);
        fclose(log);
    }
}

void report_status(char *status) {
    key_t key = ftok("report_queue", 65);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    
    message.msg_type = 1;
    strcpy(message.msg_text, status);
    msgsnd(msgid, &message, sizeof(message), 0);
}

void move_files() {
    struct dirent *entry;
    DIR *dir = opendir(UPLOAD_DIR);
    if (!dir) {
        log_message("Error opening upload directory");
        report_status("File move failed: cannot open uploads directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char src_path[256], dest_path[256], backup_path[256];

            // Build source path
            strncpy(src_path, UPLOAD_DIR, sizeof(src_path) - 1);
            strncat(src_path, "/", sizeof(src_path) - strlen(src_path) - 1);
            strncat(src_path, entry->d_name, sizeof(src_path) - strlen(src_path) - 1);

            // Build backup path
            strncpy(backup_path, BACKUP_DIR, sizeof(backup_path) - 1);
            strncat(backup_path, "/", sizeof(backup_path) - strlen(backup_path) - 1);
            strncat(backup_path, entry->d_name, sizeof(backup_path) - strlen(backup_path) - 1);

            // Build destination path
            strncpy(dest_path, REPORT_DIR, sizeof(dest_path) - 1);
            strncat(dest_path, "/", sizeof(dest_path) - strlen(dest_path) - 1);
            strncat(dest_path, entry->d_name, sizeof(dest_path) - strlen(dest_path) - 1);

            // Copy file to backup directory before moving
            FILE *src = fopen(src_path, "rb");
            if (!src) {
                log_message("Error opening source file for backup");
                report_status("Backup failed: cannot open source file");
                continue;
            }

            FILE *backup = fopen(backup_path, "wb");
            if (!backup) {
                log_message("Error creating backup file");
                report_status("Backup failed: cannot create backup file");
                fclose(src);
                continue;
            }

            // Copy file contents
            char buffer[4096];
            size_t bytes;
            while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
                fwrite(buffer, 1, bytes, backup);
            }

            fclose(src);
            fclose(backup);

            log_message("File backed up successfully");
            report_status("File backed up successfully");
/*
            // Move file to report directory
            if (rename(src_path, dest_path) == 0) {
                log_message("File moved successfully");
                report_status("File moved successfully");
            } else {
                log_message("Error moving file to report directory");
                report_status("File move to report directory failed");
            }*/

            if (rename(src_path, dest_path) == 0) {
                char log_msg[512];
            
                strcpy(log_msg, "File moved successfully to reports: ");
                strcat(log_msg, entry->d_name);
            
                log_message(log_msg);
                report_status(log_msg);
            } else {
                char log_msg[512];
            
                strcpy(log_msg, "Error moving file to report directory: ");
                strcat(log_msg, entry->d_name);
            
                log_message(log_msg);
                report_status("File move to report directory failed");
            }
            
        }
    }
    closedir(dir);
}


void check_missing_files() {
    int expected_reports = 2; // Example: Expecting 5 reports per night
    int count = 0;
    struct dirent *entry;
    DIR *dir = opendir(UPLOAD_DIR);
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            count++;
        }
    }
    closedir(dir);

    if (count < expected_reports) {
        log_message("Warning: Some reports were not uploaded by 11:30 PM.");
        report_status("Warning: Missing reports detected.");
    }
}

void lock_directories(int lock) {
    chmod(UPLOAD_DIR, lock ? 0700 : 0777);
    chmod(REPORT_DIR, lock ? 0700 : 0777);
}

void wait_until_1am() {
    time_t now;
    struct tm *timeinfo;

    while (1) {
        now = time(NULL);
        timeinfo = localtime(&now);
        //log_message("checking wait until 1 am");
        if (timeinfo->tm_hour == 1 && timeinfo->tm_min == 00) {
            log_message("Starting report transfer at 1 AM...");
            check_missing_files();
            lock_directories(1);
            move_files();
            lock_directories(0);
            return;
        }
        sleep(5);
    }
    log_message("checked wait until 1 am");
}

void monitor_uploads() {
    int fd, wd;
    char buffer[BUF_LEN];

    fd = inotify_init();
    if (fd < 0) {
        log_message("Error: Unable to initialize inotify");
        return;
    }

    wd = inotify_add_watch(fd, UPLOAD_DIR, IN_MODIFY | IN_CREATE | IN_CLOSE_WRITE);
    if (wd < 0) {
        log_message("Error: Unable to add watch on uploads directory");
        return;
    }

    while (1) {
        int length = read(fd, buffer, BUF_LEN);
        if (length < 0) {
            log_message("Error reading inotify events");
            return;
        }

        for (int i = 0; i < length; i += EVENT_SIZE + buffer[i]) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if (event->len && (event->mask & IN_MODIFY || event->mask & IN_CREATE)) {
                struct passwd *pw = getpwuid(geteuid());
                char log_entry[512];
                snprintf(log_entry, sizeof(log_entry), "User %s modified %s", pw->pw_name, event->name);
                log_message(log_entry);
            }
        }
    }
}

void listen_for_manual_trigger() {
    mkfifo(FIFO_PATH, 0666);
    log_message("Listening for manual trigger...");
    
    while (1) {
       
        int fd = open(FIFO_PATH, O_RDONLY);
        if (fd < 0) {
            log_message("Error opening FIFO.");
            sleep(5);
            continue;
        }
        log_message("chk 1");
        char buffer[20] = {0};
        int bytesRead = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);
        log_message("chk 2");
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';  // Null-terminate the string
            log_message("Received command from FIFO.");
            log_message(buffer);  // Log exact buffer content for debugging

            // Trim newline if present
            if (buffer[strlen(buffer) - 1] == '\n') {
                buffer[strlen(buffer) - 1] = '\0';
            }

            if (strcmp(buffer, "backup") == 0) {
                log_message("Manual backup triggered...");
                move_files();
            } else {
                log_message("Unknown command received.");
            }
        } else {
            log_message("No data read from FIFO.");
            //sleep(10);
        }
        log_message("chk 3");
    }
}


void handle_signal(int sig) {
    if (sig == SIGTERM) {
        log_message("Daemon received SIGTERM. Stopping gracefully...");
        exit(0);
    }
}
/*
void daemonize() {
    log_message("Daemonizing process...");

    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);
    
    umask(0);
    chdir("/Downloads/Assignment2");
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    signal(SIGTERM, handle_signal);

    log_message("Daemon running... Waiting for trigger file.");
    
    while (1) {
        //wait_until_1am();
        listen_for_manual_trigger();
    }
}
*/
void daemonize() {
    log_message("Daemonizing process...");

    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    umask(0);
    chdir("/Downloads/Assignment2");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    signal(SIGTERM, handle_signal);

    log_message("Daemon running... Creating processes for parallel execution.");

    pid_t wait_pid = fork();  // Fork first child process for wait_until_1am()
    if (wait_pid == 0) {
        log_message("Child process: wait_until_1am started.");
        wait_until_1am();
        exit(0); //child process exits after execution
    }

    pid_t fifo_pid = fork();  // Fork second child process for listen_for_manual_trigger()
    if (fifo_pid == 0) {
        log_message("Child process: listen_for_manual_trigger started.");
        listen_for_manual_trigger();
        exit(0);
    }

    // Parent process continues monitoring child processes
    while (1) {
        sleep(10); // Keep daemon alive
    }
}

int main() {
    log_message("Daemon started");
    daemonize();
    return 0;
}
