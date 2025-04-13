#include<stdio.h>
#include<string.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<time.h>
#include<unistd.h>
#include<dirent.h>
#include<stdlib.h>
#include <errno.h> 

typedef struct{
    int treasure_id;
    char user_name[256];
    struct GPS{
        float latitude;
        float longitude;
    }GPS;
    char clue[256];
    int value;
}Treasure;

void create_symlink(char* path, char* hunt_id) {
    char link[256];
    char real_path[512];

    mkdir("./logs", 0755);
    sprintf(link, "./logs/logged_hunt-%s.log", hunt_id);
    
    if (realpath(path, real_path) == NULL) {
        printf("Failed to resolve real path for symlink");
        return;
    }

    unlink(link);  
    if (symlink(real_path, link) == -1) {
        printf("Failed to create symlink");
    }
}

void add_log(char* path,char* action,int size){
    mkdir("./log",0755);
    int fd=open(path,O_WRONLY | O_CREAT | O_APPEND,0644);
    if(fd==-1){
        printf("failed to open log %s\n",path);
        return;
    }
    write(fd,action,size);
    write(fd,"\n",1);
    close(fd);
}
void add_treasure(char* hunt_id){

    char path[256],treasure_file[256],log_path[256],action[256];
    sprintf(path,"./hunts/%s",hunt_id);
    sprintf(treasure_file,"%s/treasure.bin",path);
    sprintf(log_path,"%s/%s.log",path,hunt_id);
    mkdir("./hunts",0755);
    mkdir(path,0755);

    Treasure t;
    memset(&t, 0, sizeof(Treasure));

    int fd=open(treasure_file, O_WRONLY | O_CREAT | O_APPEND,0644);
    if(fd==-1){
        printf("Failed to open file");
        return;
    }

    struct stat st;
    if(fstat(fd,&st)==-1){
        printf("Failed to get file info\n");
        close(fd);
        return;
    }

    int current_entries=st.st_size/sizeof(Treasure);
    t.treasure_id=current_entries+1;

    lseek(fd,0,SEEK_END);

    printf("The indexing of the treasures is done automatically and starts from 1\n");
    
    printf("User name: ");
    scanf("%255s", t.user_name);
    printf("Latitude: ");
    scanf("%f", &t.GPS.latitude);
    printf("Longitude: ");
    scanf("%f", &t.GPS.longitude);
    printf("Clue: ");
    getchar();
    fgets(t.clue,sizeof(t.clue),stdin);
    t.clue[strcspn(t.clue, "\n")] = 0;
    printf("Value: ");
    scanf("%d", &t.value);


    if(write(fd,&t,sizeof(Treasure))!=sizeof(Treasure)){
        printf("Failed to write/n");
        return;
    }

    close(fd); 
    
    sprintf(action,"treasure %d was created",t.treasure_id);
    add_log(log_path,action,strlen(action));
    create_symlink(log_path,hunt_id);
    add_log("./log/general.log",action,strlen(action));
}

void list(char* hunt_id){
    char treasure_file[256],path_to_log[256];
    sprintf(treasure_file,"./hunts/%s/treasure.bin",hunt_id);
    sprintf(path_to_log,"./hunts/%s/%s.log",hunt_id,hunt_id);
    struct stat st;
    if(stat(treasure_file, &st)==-1){
        printf("file not found");
        return;
    }
    printf("Hunt: %s\nSize: %ld bytes\nLast Modified: %s",hunt_id,st.st_size,ctime(&st.st_ctime));

    int fd=open(treasure_file,O_RDONLY);
    if(fd==-1){
        printf("Failed to open file");
        return;
    }

    Treasure t;
    while(read(fd,&t,sizeof(Treasure))==sizeof(Treasure)){
        printf("ID:%d, User:%s, Latitude:%f, Longitude:%f, Clue:%s, Value:%d\n",t.treasure_id,t.user_name,t.GPS.latitude,t.GPS.longitude,t.clue,t.value);
    }
    close(fd);
    char* message="Listed treasures";
    add_log(path_to_log,message,strlen(message));
    add_log("./log/general.log",message,strlen(message));
}

void view(char* hunt_id,int treasure){
    char treasure_file[256],log_file[256];
    sprintf(treasure_file,"./hunts/%s/treasure.bin",hunt_id);
    sprintf(log_file,"./hunts/%s/%s.log",hunt_id,hunt_id);
    int fd=open(treasure_file,O_RDONLY);
    if(fd==-1){
        printf("Failed to open file");
        return;
    }

    Treasure t;
    while(read(fd,&t,sizeof(Treasure))==sizeof(Treasure)){
        if(t.treasure_id==treasure){
            printf("ID:%d, User:%s, Latitude:%f, Longitude:%f, Clue:%s, Value:%d\n",t.treasure_id,t.user_name,t.GPS.latitude,t.GPS.longitude,t.clue,t.value);
            close(fd);
            char action[255];
            sprintf(action,"Viewed %d treasure",treasure);
            add_log(log_file,action,strlen(action));
            return;
        }
    }

    printf("Treasure not found.\n");
    close(fd);
}

void remove_hunt(char* hunt_id){
    char path[256];
    sprintf(path,"./hunts/%s",hunt_id);

    DIR* dir=opendir(path);
    if(!dir){
        printf("failed to open directory");
        return;
    }

    struct dirent* element;

    while((element=readdir(dir))!=NULL){
        if (strcmp(element->d_name, ".") == 0 || strcmp(element->d_name, "..") == 0)
            continue;
        char new_path[256];
        sprintf(new_path,"%s/%s",path,element->d_name);

        if(remove(new_path)!=0){
            printf("could not remove file %s",new_path);
            return;
        }
    }
    if(rmdir(path)!=0){
        printf("could not remove directory");
        return;
    }

    char action[256];
    sprintf(action,"removed %s",hunt_id);
    add_log("./log/general.log",action,strlen(action));
}

void remove_treasure(char* hunt_id,int position){
    char path[256], treasure_file[256],temp_file[256],log_file[256];
    sprintf(path, "./hunts/%s", hunt_id);
    sprintf(treasure_file, "%s/treasure.bin", path);
    sprintf(temp_file, "%s/temp.bin", path);
    sprintf(log_file, "%s/%s.log", path, hunt_id);

    int fd=open(treasure_file,O_RDONLY);
    if(fd==-1){
        printf("Failed to open file");
        return;
    }

    int fd_temp = open(temp_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_temp == -1) {
        perror("Failed to create temporary file");
        close(fd);
        return;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("Failed to get file stats");
        close(fd);
        close(fd_temp);
        return;
    }
    int total_records = st.st_size / sizeof(Treasure);
    if (position < 1 || position > total_records) {
        printf("Invalid position.");
        close(fd);
        close(fd_temp);
        return;
    }
    
    Treasure t;
    int current_id=1;
    for(int i=1;i<=total_records;i++){
        if(read(fd,&t,sizeof(Treasure))!=sizeof(Treasure)){
            printf("Failed to read treasure");
            break;
        }
        if(i!=position){
            t.treasure_id=current_id;
            current_id++;
            if (write(fd_temp, &t, sizeof(Treasure)) != sizeof(Treasure)) {
                perror("Error writing treasure");
                break;
            }
        }
    }
    close(fd);
    close(fd_temp);

    remove(treasure_file);
    if (rename(temp_file,treasure_file) == -1) {
        perror("Failed to update treasure file");
        return;
    }

    char action[256];
    sprintf(action, "Removed treasure %d", position);
    add_log(log_file, action, strlen(action));
    add_log("./log/general.log",action,strlen(action));
    
}



int main(int argc,char* argv[]){
    if(argc<3){
        printf("Insufficient argumetns\n");
        return 1;
    }

    char* operation=argv[1];
    char* hunt_id=argv[2];

    if(strcmp(operation,"--add")==0){
        add_treasure(hunt_id);
    }else if(strcmp(operation,"--list")==0){
        list(hunt_id);
    }else if(strcmp(operation,"--view")==0 && argc==4){
        view(hunt_id,atoi(argv[3]));
    }else if(strcmp(operation,"--remove")==0 && argc==4){
        remove_treasure(hunt_id,atoi(argv[3]));
    }else if(strcmp(operation,"--remove_hunt")==0){
        remove_hunt(hunt_id);
    }else{
        printf("Invalid arguments\n");
        return 1;
    }

    return 0;
}
