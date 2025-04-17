// like libc to heaven
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define FOLDER_PATH "starter_kit"
#define DAEMON_FILE "daemon.pid"
#define QUARANTINE_FOLDER "quarantine"

// Global flag to control the loop
volatile sig_atomic_t keep_running = 1;

const char *suspicious_contain[16] = {".exe",    ".dll",     ".bin",    ".bat",
                                      ".sh",     "backdoor", "exploit", "worm",
                                      "rootkit", "stealer",  "phishing"};

// proto
int rename_file();
void quarantine_files();
void shutdown_daemon();
void return_files();
void eradicate_files();
int is_suspicious(const char *filename);
void create_directory_if_not_exists(const char *dir);

// system
//  Function to scan directory and decode filenames
int scan_directory(const char *folder_path) {
  DIR *dir;
  struct dirent *entry;
  int files_renamed = 0;

  // Check if folder exists
  struct stat st;
  if (stat(folder_path, &st) == -1) {
    syslog(LOG_ERR, "%s folder not found: %m", folder_path);
    return -1;
  }

  // Open directory
  dir = opendir(folder_path);
  if (!dir) {
    syslog(LOG_ERR, "Failed to open %s directory: %m", folder_path);
    return -1;
  }

  // Process each file in the directory
  while ((entry = readdir(dir)) != NULL) {
    if (rename_file(folder_path, entry->d_name)) {
      files_renamed++;
    }
  }

  closedir(dir);

  if (files_renamed > 0) {
    syslog(LOG_NOTICE, "Decoded %d filenames", files_renamed);
  }

  return files_renamed;
}

// Function to check if a string is valid base64
int is_valid_base64(const char *str) {
  size_t len = strlen(str);

  // Base64 length should be a multiple of 4
  if (len % 4 != 0 && len > 0) {
    return 0;
  }

  // Check for valid base64 characters
  for (size_t i = 0; i < len; i++) {
    char c = str[i];
    if (!(isalnum(c) || c == '+' || c == '/' || c == '=')) {
      return 0;
    }
  }

  return 1;
}

#include <stdio.h>

// If you *must* use the name "write", wrap it to avoid conflict
int write_to_file(const char *name, const char *content) {
  FILE *fp = fopen(name, "w");
  if (!fp) {
    perror("fopen");
    return -1;
  }

  if (fputs(content, fp) == EOF) {
    perror("fputs");
    fclose(fp);
    return -1;
  }

  fclose(fp);
  return 0;
}

// stream log message to activity.log with timestamp
int act_log(const char *msg) {
  time_t now = time(NULL);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

  FILE *log_file = fopen("activity.log", "a");
  if (log_file) {
    fprintf(log_file, "[%s] %s\n", timestamp, msg);
    fclose(log_file);
    return 0;
  }
  return -1;
}

// Function to decode a base64 filename using system's base64 command
char *decode_base64_filename(const char *encoded_filename) {
  FILE *pipe;
  char command[BUFFER_SIZE];
  char *decoded_filename = malloc(BUFFER_SIZE);

  if (!decoded_filename) {
    syslog(LOG_ERR, "Memory allocation failed");
    return NULL;
  }

  // Create command to decode the base64 string
  snprintf(command, BUFFER_SIZE, "echo '%s' | base64 -d 2>/dev/null",
           encoded_filename);

  // Open pipe to command
  pipe = popen(command, "r");
  if (!pipe) {
    syslog(LOG_ERR, "popen failed");
    free(decoded_filename);
    return NULL;
  }

  // Read the output of the command
  if (fgets(decoded_filename, BUFFER_SIZE, pipe) == NULL) {
    // No output means decoding failed
    pclose(pipe);
    free(decoded_filename);
    return NULL;
  }

  // Check the exit status of the command
  int status = pclose(pipe);
  if (status != 0) {
    free(decoded_filename);
    return NULL;
  }

  // Remove newline character if present
  size_t len = strlen(decoded_filename);
  if (len > 0 && decoded_filename[len - 1] == '\n') {
    decoded_filename[len - 1] = '\0';
  }

  return decoded_filename;
}

// Function to check if a decoded filename is valid (contains only printable
// characters)
int is_valid_filename(const char *filename) {
  size_t len = strlen(filename);

  // Empty filename is invalid
  if (len == 0) {
    return 0;
  }

  // Check for non-printable characters or characters that are invalid in
  // filenames
  for (size_t i = 0; i < len; i++) {
    unsigned char c = filename[i];

    // Check for non-printable or control characters
    if (!isprint(c) || c == '/' || c == '\0') {
      return 0;
    }
  }

  return 1;
}

// Function to rename a file with its decoded name
int rename_file(const char *folder_path, const char *old_name) {
  char old_path[BUFFER_SIZE];
  char new_path[BUFFER_SIZE];
  char *decoded_name;
  int result = 0;

  // Skip files that start with '.' (hidden files)
  if (old_name[0] == '.') {
    return 0;
  }

  // Check if filename appears to be base64 encoded
  if (!is_valid_base64(old_name)) {
    syslog(LOG_DEBUG, "Skipping non-base64 filename: %s", old_name);
    return 0;
  }

  // Get decoded filename
  decoded_name = decode_base64_filename(old_name);
  if (!decoded_name) {
    syslog(LOG_DEBUG, "Failed to decode: %s (not valid base64)", old_name);
    return 0; // Not an error, just not a base64 encoded filename
  }

  // Check if the decoded filename is valid
  if (!is_valid_filename(decoded_name)) {
    syslog(LOG_DEBUG, "Skipping %s: decoded result is not a valid filename",
           old_name);
    free(decoded_name);
    return 0;
  }

  // Build full paths
  snprintf(old_path, BUFFER_SIZE, "%s/%s", folder_path, old_name);
  snprintf(new_path, BUFFER_SIZE, "%s/%s", folder_path, decoded_name);

  // Check if target file already exists
  struct stat st;
  if (stat(new_path, &st) == 0) {
    syslog(LOG_DEBUG, "Skipping %s: target filename %s already exists",
           old_name, decoded_name);
    free(decoded_name);
    return 0;
  }

  syslog(LOG_NOTICE, "Renaming: %s -> %s", old_name, decoded_name);

  // Rename the file
  if (rename(old_path, new_path) != 0) {
    syslog(LOG_ERR, "Failed to rename file %s to %s: %m", old_name,
           decoded_name);
    result = -1;
  } else {
    result = 1;
  }

  free(decoded_name);
  return result;
}

// decrypt
void handle_signal(int sig) {
  if (sig == SIGTERM || sig == SIGINT) {
    keep_running = 0;
    act_log("Received termination signal, exiting...");
  }
}

// Process single execution - scan once and exit
void decrypt(const char *workdir) {
  // This function runs once per loop iteration
  time_t now = time(NULL);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

  char message[256];
  snprintf(message, sizeof(message), "Job executed at %s in directory: %s",
           timestamp, workdir);
  // syslog(message);

  // Example job: Just print to stdout if not in background
  if (getppid() != 1) {
    printf("Job executed at %s in directory: %s\n", timestamp, workdir);
  }

  // Check if folder exists
  struct stat st;
  if (stat(FOLDER_PATH, &st) == -1) {
    fprintf(stderr, "Error: %s folder not found\n", FOLDER_PATH);
    exit(EXIT_FAILURE);
  }

  printf("Scanning %s for base64 encoded filenames...\n", FOLDER_PATH);
  int files_renamed = scan_directory(FOLDER_PATH);

  if (files_renamed > 0) {
    printf("Successfully decoded %d filenames\n", files_renamed);
  } else if (files_renamed == 0) {
    printf("No base64 encoded filenames found\n");
  } else {
    printf("Error scanning directory\n");
  }
}

// Main loop that runs every 5 seconds
int decrypt_job(const char *workdir) {
  while (keep_running) {
    decrypt(workdir);
    sleep(5); // Sleep for 5 seconds
  }
  return 0;
}

void log_pid_info(const char *workdir) {
  char pid_msg[128];
  char pid_str[16];
  pid_t pid = getpid();

  snprintf(pid_str, sizeof(pid_str), "%d", pid);
  snprintf(pid_msg, sizeof(pid_msg),
           "Daemon started with PID: %s in directory: %s", pid_str, workdir);

  act_log(pid_msg);
  write_to_file(DAEMON_FILE, pid_str);
}

// background
int background(const char *workdir) {
  pid_t pid, sid;

  // Fork the parent process
  pid = fork();

  // Error occurred
  if (pid < 0) {
    perror("Fork failed");
    exit(EXIT_FAILURE);
  }

  // Success: Exit the parent process
  if (pid > 0) {
    printf("Daemon started with PID: %d\n", pid);
    exit(EXIT_SUCCESS); // Free the parent process
  }

  // Change the file mode mask
  umask(0);

  // Create a new SID for the child process
  sid = setsid();
  if (sid < 0) {
    exit(EXIT_FAILURE);
  }

  // Change the current working directory to the saved workdir
  if (chdir(workdir) < 0) {
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Failed to change directory to %s",
             workdir);
    act_log(error_msg);
    exit(EXIT_FAILURE);
  }

  // Close standard file descriptors
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  log_pid_info(workdir);

  // Set up signal handlers
  signal(SIGTERM, handle_signal);
  signal(SIGINT, handle_signal);

  // Start the main loop
  return decrypt_job(workdir);
}

// for testing
int no_background(const char *workdir) {
  printf("Running in foreground mode. Use --background to run as daemon.\n");
  printf("Current working directory: %s\n", workdir);

  log_pid_info(workdir);

  // Set up signal handlers for foreground mode too
  signal(SIGTERM, handle_signal);
  signal(SIGINT, handle_signal);

  return decrypt_job(workdir);
}

// Check if a filename contains suspicious strings
int is_suspicious(const char *filename) {
  for (int i = 0; i < 11; i++) {
    if (strstr(filename, suspicious_contain[i]) != NULL) {
      return 1;
    }
  }
  return 0;
}

// Create directory if it doesn't exist
void create_directory_if_not_exists(const char *dir) {
  struct stat st = {0};
  if (stat(dir, &st) == -1) {
    mkdir(dir, 0700);
    char log_msg[BUFFER_SIZE];
    snprintf(log_msg, BUFFER_SIZE, "Created directory: %s", dir);
    act_log(log_msg);
  }
}

// Move files from starter_kit to quarantine if suspicious
void quarantine_files() {
  DIR *dir;
  struct dirent *entry;
  char src_path[BUFFER_SIZE];
  char dst_path[BUFFER_SIZE];
  char log_msg[BUFFER_SIZE];
  int quarantined_count = 0;

  // Open starter_kit directory
  dir = opendir(FOLDER_PATH);
  if (dir == NULL) {
    act_log("Failed to open starter_kit directory");
    return;
  }

  // Loop through all files in the directory
  while ((entry = readdir(dir)) != NULL) {
    // Skip . and .. entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Check if file is suspicious
    if (is_suspicious(entry->d_name)) {
      // Create source and destination paths
      snprintf(src_path, BUFFER_SIZE, "%s/%s", FOLDER_PATH, entry->d_name);
      snprintf(dst_path, BUFFER_SIZE, "%s/%s", QUARANTINE_FOLDER,
               entry->d_name);

      // Move the file
      if (rename(src_path, dst_path) == 0) {
        snprintf(log_msg, BUFFER_SIZE, "Quarantined: %s", entry->d_name);
        act_log(log_msg);
        quarantined_count++;
      } else {
        snprintf(log_msg, BUFFER_SIZE, "Failed to quarantine file: %s - %s",
                 entry->d_name, strerror(errno));
        act_log(log_msg);
      }
    }
  }

  closedir(dir);
  snprintf(log_msg, BUFFER_SIZE,
           "Quarantine complete. Processed %d suspicious files",
           quarantined_count);
  act_log(log_msg);
}

// Kill process from daemon.pid and delete the file
void shutdown_daemon() {
  FILE *file;
  char pid_str[8];
  pid_t pid;
  char log_msg[BUFFER_SIZE];

  // Open daemon.pid file
  file = fopen(DAEMON_FILE, "r");
  if (file == NULL) {
    act_log("Failed to open daemon.pid file");
    return;
  }

  // Read PID from file
  if (fgets(pid_str, 8, file) == NULL) {
    act_log("Failed to read PID from daemon.pid file");
    fclose(file);
    return;
  }
  fclose(file);

  // Convert string to integer
  pid = atoi(pid_str);
  if (pid <= 0) {
    snprintf(log_msg, BUFFER_SIZE, "Invalid PID: %s", pid_str);
    act_log(log_msg);
    return;
  }

  // Kill the process
  if (kill(pid, SIGTERM) == 0) {
    snprintf(log_msg, BUFFER_SIZE,
             "Successfully terminated process with PID: %d", pid);
    act_log(log_msg);

    // Delete the daemon.pid file after killing the process
    if (remove(DAEMON_FILE) == 0) {
      act_log("Successfully deleted daemon.pid file");
    } else {
      snprintf(log_msg, BUFFER_SIZE, "Failed to delete daemon.pid file: %s",
               strerror(errno));
      act_log(log_msg);
    }
  } else {
    snprintf(log_msg, BUFFER_SIZE,
             "Failed to terminate process with PID %d: %s", pid,
             strerror(errno));
    act_log(log_msg);
  }
}

// Move files from quarantine back to starter_kit
void return_files() {
  DIR *dir;
  struct dirent *entry;
  char src_path[BUFFER_SIZE];
  char dst_path[BUFFER_SIZE];
  char log_msg[BUFFER_SIZE];
  int returned_count = 0;

  // Open quarantine directory
  dir = opendir(QUARANTINE_FOLDER);
  if (dir == NULL) {
    act_log("Failed to open quarantine directory");
    return;
  }

  // Loop through all files in the directory
  while ((entry = readdir(dir)) != NULL) {
    // Skip . and .. entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Create source and destination paths
    snprintf(src_path, BUFFER_SIZE, "%s/%s", QUARANTINE_FOLDER, entry->d_name);
    snprintf(dst_path, BUFFER_SIZE, "%s/%s", FOLDER_PATH, entry->d_name);

    // Move the file
    if (rename(src_path, dst_path) == 0) {
      snprintf(log_msg, BUFFER_SIZE, "Returned: %s", entry->d_name);
      act_log(log_msg);
      returned_count++;
    } else {
      snprintf(log_msg, BUFFER_SIZE, "Failed to return file %s: %s",
               entry->d_name, strerror(errno));
      act_log(log_msg);
    }
  }

  closedir(dir);
  snprintf(log_msg, BUFFER_SIZE, "Return complete. Processed %d files",
           returned_count);
  act_log(log_msg);
}

// Delete all files in quarantine folder
void eradicate_files() {
  DIR *dir;
  struct dirent *entry;
  char file_path[BUFFER_SIZE];
  char log_msg[BUFFER_SIZE];
  int eradicated_count = 0;

  // Open quarantine directory
  dir = opendir(QUARANTINE_FOLDER);
  if (dir == NULL) {
    act_log("Failed to open quarantine directory");
    return;
  }

  // Loop through all files in the directory
  while ((entry = readdir(dir)) != NULL) {
    // Skip . and .. entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Create file path
    snprintf(file_path, BUFFER_SIZE, "%s/%s", QUARANTINE_FOLDER, entry->d_name);

    // Delete the file
    if (remove(file_path) == 0) {
      snprintf(log_msg, BUFFER_SIZE, "Eradicated: %s", entry->d_name);
      act_log(log_msg);
      eradicated_count++;
    } else {
      snprintf(log_msg, BUFFER_SIZE, "Failed to eradicate file %s: %s",
               entry->d_name, strerror(errno));
      act_log(log_msg);
    }
  }

  closedir(dir);
  snprintf(log_msg, BUFFER_SIZE, "Eradication complete. Removed %d files",
           eradicated_count);
  act_log(log_msg);
}

// Main entry point
int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf(
        "Usage: %s [--decrypt|--quarantine|--shutdown|--return|--eradicate]\n",
        argv[0]);
    return 1;
  }

  // Get current working directory
  char workdir[PATH_MAX];
  if (getcwd(workdir, sizeof(workdir)) == NULL) {
    perror("Failed to get current working directory");
    return EXIT_FAILURE;
  }

  act_log("Program started");

  if (strcmp(argv[1], "--decrypt") == 0) {
    background(workdir);
  } else if (strcmp(argv[1], "--quarantine") == 0) {
    quarantine_files();
  } else if (strcmp(argv[1], "--shutdown") == 0) {
    shutdown_daemon();
  } else if (strcmp(argv[1], "--return") == 0) {
    return_files();
  } else if (strcmp(argv[1], "--eradicate") == 0) {
    eradicate_files();
  } else {
    printf("Unknown option: %s\n", argv[1]);
    printf(
        "Usage: %s [--decrypt|--quarantine|--shutdown|--return|--eradicate]\n",
        argv[0]);
  }
}
