#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#define CMD_FILE "./monitor_cmd.txt"
#define PIPE_BUF_SIZE 4096

pid_t monitor_pid = -1;
int monitor_active = 0;
int monitor_stopping = 0;
int pipe_fd[2];

void handle_sigchld(int sig) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    
    if (pid == monitor_pid) {
        monitor_active = 0;
        monitor_stopping = 0;
        if (WIFEXITED(status)) {
            printf("Monitor process ended with exit status: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Monitor process terminated by signal: %d\n", WTERMSIG(status));
        } else {
            printf("Monitor process ended with status: %d\n", status);
        }
    }
}


void read_from_pipe() {
    char buffer[PIPE_BUF_SIZE];
    int flags, bytes_read;
    
    flags = fcntl(pipe_fd[0], F_GETFL, 0);
    fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);
    
    memset(buffer, 0, PIPE_BUF_SIZE);
    
    usleep(50000);

    while ((bytes_read = read(pipe_fd[0], buffer, PIPE_BUF_SIZE - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
        memset(buffer, 0, PIPE_BUF_SIZE);
    }
    
    fcntl(pipe_fd[0], F_SETFL, flags);
}

void send_command(const char* cmd) {
    if (!monitor_active) {
        printf("Error: Monitor is not running.\n");
        return;
    }
    
    if (monitor_stopping) {
        printf("Error: Monitor is in the process of stopping. Please wait.\n");
        return;
    }
    
    FILE* f = fopen(CMD_FILE, "w");
    if (!f) {
        perror("Failed to write command file");
        return;
    }
    fprintf(f, "%s\n", cmd);
    fclose(f);
    
    if (kill(monitor_pid, SIGUSR1) == -1) {
        perror("Failed to send signal to monitor");
        return;
    }

    read_from_pipe();
}

void calculate_hunt_scores(const char* hunt_id) {
    int score_pipe[2];
    if (pipe(score_pipe) == -1) {
        perror("Failed to create pipe for score calculator");
        return;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork process for score calculation");
        close(score_pipe[0]);
        close(score_pipe[1]);
        return;
    }
    
    if (pid == 0) { 
        close(score_pipe[0]); 
        
        dup2(score_pipe[1], STDOUT_FILENO);
        close(score_pipe[1]);
        
        execl("./calculator", "./calculator", hunt_id, NULL);
        perror("Failed to execute score calculator");
        exit(1);
    } else {
        close(score_pipe[1]);
        
        char buffer[512];
        ssize_t bytes_read = read(score_pipe[0], buffer, sizeof(buffer) - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("Scores for hunt %s:\n%s", hunt_id, buffer);
        } else {
            printf("No score data received for hunt %s\n", hunt_id);
        }
        
        close(score_pipe[0]);
        waitpid(pid, NULL, 0);
    }
}

void calculate_scores() {
    DIR* dir = opendir("./hunts");
    if (!dir) {
        printf("Cannot open hunts directory\n");
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_DIR && 
            strcmp(entry->d_name, ".") != 0 && 
            strcmp(entry->d_name, "..") != 0) {

            calculate_hunt_scores(entry->d_name);
        }
    }
    
    closedir(dir);
}

int main() {

    if (pipe(pipe_fd) == -1) {
        perror("Failed to create pipe");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Failed to set up SIGCHLD handler");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return 1;
    }
    
    char input[256];
    char command[256];
    char hunt_id[128];
    int treasure_id;
    
    while (1) {
        printf(">>");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "help") == 0) {
            printf("Available commands:\n");
            printf("  start_monitor   - Start the monitoring process\n");
            printf("  stop_monitor    - Stop the monitoring process\n");
            printf("  list_hunts      - List all available hunts\n");
            printf("  list_treasures <hunt_id> - List treasures in a specific hunt\n");
            printf("  view_treasure <hunt_id> <treasure_id> - View a specific treasure\n");
            printf("  calculate_score - Calculate the score for each user in a hunt\n");
            printf("  exit            - Exit the program (monitor must be stopped first)\n");
        }
        else if (strcmp(input, "start_monitor") == 0) {
            if (monitor_active) {
                printf("Monitor is already running with PID %d\n", monitor_pid);
                continue;
            }
            
            monitor_pid = fork();
            if (monitor_pid == -1) {
                perror("Failed to fork process");
                continue;
            }
            
            if (monitor_pid == 0) {
                close(pipe_fd[0]);
                
                char pipe_str[16];
                sprintf(pipe_str, "%d", pipe_fd[1]);
                
                execl("./monitor", "./monitor", pipe_str, NULL);
                perror("Failed to start monitor");
                exit(1);
            } else {
                close(pipe_fd[1]); 
                monitor_active = 1;

                read_from_pipe();
            }
        }
        else if (strcmp(input, "stop_monitor") == 0) {
            if (!monitor_active) {
                printf("Monitor is not running.\n");
                continue;
            }
            
            monitor_stopping = 1;
            send_command("stop_monitor");
            printf("Stopping monitor...\n");
            
            int timeout = 10;
            while (monitor_active && timeout > 0) {
                sleep(1);
                timeout--;
            }
            
            if (monitor_active) {
                printf("Monitor did not terminate gracefully. Sending SIGTERM...\n");
                kill(monitor_pid, SIGTERM);
                
                read_from_pipe();
            }
        }
        else if (strcmp(input, "list_hunts") == 0) {
            send_command("list_hunts");
        }
        else if (strncmp(input, "list_treasures", 14) == 0) {
            if (sscanf(input, "list_treasures %127s", hunt_id) == 1) {
                snprintf(command, sizeof(command), "list_treasures %s", hunt_id);
                send_command(command);
            } else {
                printf("Error: Hunt ID required. Usage: list_treasures <hunt_id>\n");
            }
        }
        else if (strncmp(input, "view_treasure", 13) == 0) {
            if (sscanf(input, "view_treasure %127s %d", hunt_id, &treasure_id) == 2) {
                snprintf(command, sizeof(command), "view_treasure %s %d", hunt_id, treasure_id);
                send_command(command);
            } else {
                printf("Error: Hunt ID and Treasure ID required. Usage: view_treasure <hunt_id> <treasure_id>\n");
            }
        }else if (strcmp(input, "calculate_score") == 0) {
            calculate_scores();
        }else if (strcmp(input, "exit") == 0) {
            if (monitor_active) {
                printf("Cannot exit while monitor is running. Please stop the monitor first.\n");
            } else {
                printf("Exiting Treasure Hunt Hub.\n");
                break;
            }
        }
        else {
            printf("Unknown command. Type 'help' for available commands.\n");
        }
    }
    

    close(pipe_fd[0]);
    
    return 0;
}