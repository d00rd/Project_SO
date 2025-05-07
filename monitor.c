#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CMD_FILE "./monitor_cmd.txt"
#define DELAY_US 500000  


typedef struct {
    int treasure_id;
    char user_name[256];
    struct {
        float latitude;
        float longitude;
    } GPS;
    char clue[256];
    int value;
} Treasure;

volatile sig_atomic_t received_command = 0;
volatile sig_atomic_t stop_requested = 0;

void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        received_command = 1;
    } else if (sig == SIGTERM) {
        stop_requested = 1;
    }
}

void list_hunts() {
    DIR* dir = opendir("./hunts");
    if (!dir) {
        perror("Cannot open hunts directory");
        return;
    }

    printf("Available hunts:\n");
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char treasure_file[256];
            sprintf(treasure_file, "./hunts/%s/treasure.bin", entry->d_name);
            
            int fd = open(treasure_file, O_RDONLY);
            if (fd < 0) continue;

            struct stat st;
            if (fstat(fd, &st) == 0) {
                int count = st.st_size / sizeof(Treasure);
                printf("Hunt: %s, Treasures: %d\n", entry->d_name, count);
            }
            close(fd);
        }
    }

    closedir(dir);
}

void list_treasures(const char* hunt_id) {
    char treasure_file[256];
    sprintf(treasure_file, "./hunts/%s/treasure.bin", hunt_id);
    
    int fd = open(treasure_file, O_RDONLY);
    if (fd < 0) {
        printf("Hunt not found or cannot be accessed.\n");
        return;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        printf("Failed to get file info\n");
        close(fd);
        return;
    }

    printf("Hunt: %s\nSize: %ld bytes\n", 
           hunt_id, st.st_size);

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("ID:%d, User:%s, Latitude:%f, Longitude:%f, Clue:%s, Value:%d\n",
               t.treasure_id, t.user_name, t.GPS.latitude, t.GPS.longitude, t.clue, t.value);
    }

    close(fd);
}

void view_treasure(const char* hunt_id, int id) {
    char treasure_file[256];
    sprintf(treasure_file, "./hunts/%s/treasure.bin", hunt_id);
    
    int fd = open(treasure_file, O_RDONLY);
    if (fd < 0) {
        printf("Hunt not found or cannot be accessed.\n");
        return;
    }

    Treasure t;
    int found = 0;
    
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (t.treasure_id == id) {
            printf("ID:%d, User:%s, Latitude:%f, Longitude:%f, Clue:%s, Value:%d\n",
                   t.treasure_id, t.user_name, t.GPS.latitude, t.GPS.longitude, t.clue, t.value);
            found = 1;
            break;
        }
    }
    
    if (!found) {
        printf("Treasure with ID %d not found in hunt %s.\n", id, hunt_id);
    }
    
    close(fd);
}

void handle_command() {
    FILE* file = fopen(CMD_FILE, "r");
    if (!file) {
        perror("Failed to open command file");
        return;
    }

    char command[256] = {0};
    char hunt_id[128] = {0};
    int treasure_id = 0;
    
    if (fscanf(file, "%255s", command) != 1) {
        fclose(file);
        return;
    }

    if (strcmp(command, "list_hunts") == 0) {
        list_hunts();
    } 
    else if (strcmp(command, "list_treasures") == 0) {
        if (fscanf(file, "%127s", hunt_id) == 1) {
            list_treasures(hunt_id);
        } else {
            printf("Error: Hunt ID required for list_treasures command.\n");
        }
    } 
    else if (strcmp(command, "view_treasure") == 0) {
        if (fscanf(file, "%127s %d", hunt_id, &treasure_id) == 2) {
            view_treasure(hunt_id, treasure_id);
        } else {
            printf("Error: Hunt ID and treasure ID required for view_treasure command.\n");
        }
    } 
    else if (strcmp(command, "stop_monitor") == 0) {
        printf("Monitor received stop command. Terminating...\n");
        stop_requested = 1;
    }
    else {
        printf("Unknown command: %s\n", command);
    }

    fclose(file);
    
    file = fopen(CMD_FILE, "w");
    if (file) {
        fclose(file);
    }
    
    if (stop_requested) {
        printf("Preparing to exit monitor process...\n");
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Failed to set up SIGUSR1 handler");
        return 1;
    }
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Failed to set up SIGTERM handler");
        return 1;
    }

    printf("Monitor started with PID %d\n", getpid());

    while (!stop_requested) {
        if (received_command) {
            handle_command();
            received_command = 0;
        }
        usleep(100000); 
    }

    printf("Monitor stopping...\n");
    usleep(DELAY_US); 
    printf("Monitor terminated.\n");
    return 0;
}