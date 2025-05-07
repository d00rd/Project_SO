#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMD_FILE "./monitor_cmd.txt"

pid_t monitor_pid = -1;
int monitor_active = 0;
int monitor_stopping = 0;

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
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Failed to set up SIGCHLD handler");
        return 1;
    }
    
    char input[256];
    char command[256];
    char hunt_id[128];
    int treasure_id;
    
    while (1) {
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
                execl("./monitor", "./monitor", NULL);
                perror("Failed to start monitor");
                exit(1);
            } else {
                monitor_active = 1;
                printf("Monitor started with PID %d\n", monitor_pid);
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
            
            int timeout = 10; // 10 seconds max wait
            while (monitor_active && timeout > 0) {
                sleep(1);
                timeout--;
            }
            
            if (monitor_active) {
                printf("Monitor did not terminate gracefully. Sending SIGTERM...\n");
                kill(monitor_pid, SIGTERM);
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
        }
        else if (strcmp(input, "exit") == 0) {
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
    
    return 0;
}