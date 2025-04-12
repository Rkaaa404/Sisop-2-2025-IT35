#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>

void log_process(const char* process_name, const char* status) {
    FILE* log = fopen("debugmon.log", "a");
    if (!log) {
        perror("Failed to open debugmon.log");
        return;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    fprintf(log, "[%02d:%02d:%04d]-%02d:%02d:%02d_%s_%s\n",
        t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
        t->tm_hour, t->tm_min, t->tm_sec,
        process_name, status);

    fclose(log);
}

void show_process(const char* user) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("ps", "ps", "-u", user, "-o", "pid,user,comm,%cpu,%mem", NULL);
        perror("execlp failed");
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("fork failed");
    }
}

void fail_user(const char* user) {
    char command[256], buffer[256];
    snprintf(command, sizeof(command), "ps -u %s -o pid=,comm=", user);

    FILE* fp = popen(command, "r");
    if (!fp) {
        perror("popen fail");
        return;
    }

    while (fgets(buffer, sizeof(buffer), fp)) {
        int pid;
        char proc[128];
        sscanf(buffer, "%d %s", &pid, proc);
        kill(pid, SIGKILL);
        log_process(proc, "FAILED");
    }

    pclose(fp);

    FILE* lock = fopen("debugmon.lock", "w");
    if (lock) {
        fprintf(lock, "%s\n", user);
        fclose(lock);
    }
}

void revert_user(const char* user) {
    FILE* lock = fopen("debugmon.lock", "r");
    if (lock) {
        char buf[128];
        fgets(buf, sizeof(buf), lock);
        fclose(lock);

        if (strncmp(buf, user, strlen(user)) == 0) {
            remove("debugmon.lock");
            log_process("debugmon", "RUNNING");
        }
    }
}

void daemon_mode(const char* user) {
    pid_t pid = fork();
    if (pid < 0) exit(1);
    if (pid > 0) exit(0); 

    setsid(); 
    signal(SIGHUP, SIG_IGN);
    pid = fork();
    if (pid < 0) exit(1);
    if (pid > 0) exit(0); 

    chdir("/");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    while (1) {
        char command[256], buffer[256];
        snprintf(command, sizeof(command), "ps -u %s -o comm=", user);
        FILE* fp = popen(command, "r");
        if (fp) {
            while (fgets(buffer, sizeof(buffer), fp)) {
                buffer[strcspn(buffer, "\n")] = '\0';
                log_process(buffer, "RUNNING");
            }
            pclose(fp);
        }
        sleep(10); 
    }
}

void print_usage() {
    printf("Usage: ./debugmon <command> <username>\n");
    printf("Commands: list, daemon, stop, fail, revert\n");
}

void stop_daemon(const char* user) {
    char command[256];
    snprintf(command, sizeof(command), "pkill -f 'debugmon daemon %s'", user);
    system(command);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage();
        return 1;
    }

    const char* command = argv[1];
    const char* user = argv[2];

    if (strcmp(command, "list") == 0) {
        show_process(user);
    } else if (strcmp(command, "daemon") == 0) {
        daemon_mode(user);
    } else if (strcmp(command, "stop") == 0) {
        stop_daemon(user);
    } else if (strcmp(command, "fail") == 0) {
        fail_user(user);
    } else if (strcmp(command, "revert") == 0) {
        revert_user(user);
    } else {
        print_usage();
    }

    return 0;
}
