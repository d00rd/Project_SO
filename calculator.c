#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

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

typedef struct {
    char user_name[256];
    int total_score;
    int treasure_count;
} UserScore;

#define MAX_USERS 100

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt_id>\n", argv[0]);
        return 1;
    }
    
    const char* hunt_id = argv[1];
    
    char treasure_file[256];
    sprintf(treasure_file, "./hunts/%s/treasure.bin", hunt_id);
    
    int fd = open(treasure_file, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open treasure file for hunt %s\n", hunt_id);
        return 1;
    }
    
    Treasure t;
    UserScore users[MAX_USERS];
    int user_count = 0;
    
    memset(users, 0, sizeof(users));
    
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        int user_found = 0;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].user_name, t.user_name) == 0) {
                users[i].total_score += t.value;
                users[i].treasure_count++;
                user_found = 1;
                break;
            }
        }
        
        if (!user_found && user_count < MAX_USERS) {
            strcpy(users[user_count].user_name, t.user_name);
            users[user_count].total_score = t.value;
            users[user_count].treasure_count = 1;
            user_count++;
        }
    }
    
    close(fd);
    
    printf("Hunt: %s - User Scores\n", hunt_id);
    printf("%-20s %-15s %-15s\n", "User", "Total Score", "Treasures");
    printf("---------------------------------------------------\n");
    
    for (int i = 0; i < user_count; i++) {
        printf("%-20s %-15d %-15d\n", 
               users[i].user_name, 
               users[i].total_score, 
               users[i].treasure_count);
    }
    
    if (user_count == 0) {
        printf("No treasures or users found in this hunt.\n");
    }
    
    return 0;
}