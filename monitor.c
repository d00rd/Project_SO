#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define CMD_FILE "./monitor_cmd.txt"
#define DELAY_US 500000  
#define PIPE_BUF_SIZE 4096

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

int write_pipe_fd = -1;
char pipe_buffer[PIPE_BUF_SIZE] = {0};


void write_to_pipe(const char* message) {
    if (write_pipe_fd != -1) {
        size_t msg_len = strlen(message);
        size_t buf_len = strlen(pipe_buffer);
        
        if (buf_len + msg_len >= PIPE_BUF_SIZE - 1) {
            write(write_pipe_fd, pipe_buffer, buf_len);
            pipe_buffer[0] = '\0';
            buf_len = 0;
        }
        
        strcat(pipe_buffer, message);

        if (message[msg_len-1] == '\n' || buf_len + msg_len > PIPE_BUF_SIZE/2) {
            write(write_pipe_fd, pipe_buffer, strlen(pipe_buffer));
            pipe_buffer[0] = '\0';
        }
    }
}

void flush_pipe_buffer() {
    if (write_pipe_fd != -1 && strlen(pipe_buffer) > 0) {
        write(write_pipe_fd, pipe_buffer, strlen(pipe_buffer));
        pipe_buffer[0] = '\0';
    }
}

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
        write_to_pipe("Cannot open hunts directory\n");
        flush_pipe_buffer();
        return;
    }

    write_to_pipe("Available hunts:\n");
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char treasure_file[256];
            snprintf(treasure_file,sizeof(treasure_file), "./hunts/%s/treasure.bin", entry->d_name);
            
            int fd = open(treasure_file, O_RDONLY);
            if (fd < 0) continue;

            struct stat st;
            if (fstat(fd, &st) == 0) {
                int count = st.st_size / sizeof(Treasure);
                char buffer[256];
                snprintf(buffer,sizeof(buffer), "Hunt: %s, Treasures: %d\n", entry->d_name, count);
                write_to_pipe(buffer);
            }
            close(fd);
        }
    }
    flush_pipe_buffer();
    closedir(dir);
}

void list_treasures(const char* hunt_id) {
    char treasure_file[256];
    snprintf(treasure_file,sizeof(treasure_file), "./hunts/%s/treasure.bin", hunt_id);
    
    int fd = open(treasure_file, O_RDONLY);
    if (fd < 0) {
        write_to_pipe("Hunt not found or cannot be accessed.\n");
        flush_pipe_buffer();
        return;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        write_to_pipe("Failed to get file info\n");
        close(fd);
        flush_pipe_buffer();
        return;
    }
    
    char buffer[256];
    snprintf(buffer,sizeof(buffer), "Hunt: %s\nSize: %ld bytes\n", hunt_id, st.st_size);
    write_to_pipe(buffer);
    
    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        char entry_buffer[512];
        snprintf(entry_buffer,sizeof(entry_buffer), "ID:%d, User:%s, Latitude:%f, Longitude:%f, Clue:%s, Value:%d\n",
               t.treasure_id, t.user_name, t.GPS.latitude, t.GPS.longitude, t.clue, t.value);
        write_to_pipe(entry_buffer);
    }

    flush_pipe_buffer();
    close(fd);
}

void view_treasure(const char* hunt_id, int id) {
    char treasure_file[256];
    snprintf(treasure_file,sizeof(treasure_file), "./hunts/%s/treasure.bin", hunt_id);
    
    int fd = open(treasure_file, O_RDONLY);
    if (fd < 0) {
        write_to_pipe("Hunt not found or cannot be accessed.\n");
        flush_pipe_buffer();
        return;
    }

    Treasure t;
    int found = 0;
    
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (t.treasure_id == id) {
            char buffer[512];
            snprintf(buffer,sizeof(buffer), "ID:%d, User:%s, Latitude:%f, Longitude:%f, Clue:%s, Value:%d\n",
                   t.treasure_id, t.user_name, t.GPS.latitude, t.GPS.longitude, t.clue, t.value);
            write_to_pipe(buffer);
            found = 1;
            break;
        }
    }
    
    if (!found) {
        char buffer[256];
        snprintf(buffer,sizeof(buffer), "Treasure with ID %d not found in hunt %s.\n", id, hunt_id);
        write_to_pipe(buffer);
    }
    
    flush_pipe_buffer();
    close(fd);
}

void handle_command() {
    FILE* file = fopen(CMD_FILE, "r");
    if (!file) {
        write_to_pipe("Failed to open command file\n");
        flush_pipe_buffer();
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
            write_to_pipe("Error: Hunt ID required for list_treasures command.\n");
            flush_pipe_buffer();
        }
    } 
    else if (strcmp(command, "view_treasure") == 0) {
        if (fscanf(file, "%127s %d", hunt_id, &treasure_id) == 2) {
            view_treasure(hunt_id, treasure_id);
        } else {
            write_to_pipe("Error: Hunt ID and treasure ID required for view_treasure command.\n");
            flush_pipe_buffer();
        }
    } 
    else if (strcmp(command, "stop_monitor") == 0) {
        write_to_pipe("Monitor received stop command. Terminating...\n");
        flush_pipe_buffer();
        stop_requested = 1;
    }
    else {
        char buffer[256];
        snprintf(buffer,sizeof(buffer), "Unknown command: %s\n", command);
        write_to_pipe(buffer);
        flush_pipe_buffer();
    }

    fclose(file);
    
    file = fopen(CMD_FILE, "w");
    if (file) {
        fclose(file);
    }
    
    if (stop_requested) {
        write_to_pipe("Preparing to exit monitor process...\n");
        flush_pipe_buffer();
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    write_pipe_fd = atoi(argv[1]);

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
    
    char buffer[256];
    sprintf(buffer, "Monitor started with PID %d\n", getpid());
    write_to_pipe(buffer);
    flush_pipe_buffer();

    while (!stop_requested) {
        if (received_command) {
            handle_command();
            received_command = 0;
        }
        usleep(10000); // Small sleep to prevent CPU hogging
    }

    write_to_pipe("Monitor stopping...\n");
    usleep(DELAY_US); 
    write_to_pipe("Monitor terminated.\n");
    flush_pipe_buffer();

    close(write_pipe_fd);
    return 0;
}