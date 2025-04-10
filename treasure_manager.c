#include<stdio.h>
#include<string.h>
#include<fcntl.h>
#include<sys/stat.h>

typedef struct{
    char treasure_id[256];
    char user_name[256];
    struct GPS{
        float latitude;
        float longitude;
    }GPS;
    //char clue[256];
    //int value;
}Treasure;

void add_log(char* path,char* action,int size){
    int fd=open(path,O_WRONLY | O_CREAT | O_APPEND);
    write(fd,action,size);
    close(fd);
}
void add_treasure(char* hunt_id){

    char path[256],treasure_file[256],log_path[256],action[256];
    sprintf(path,"./hunts/%s",hunt_id);
    sprintf(treasure_file,"%s/treasure.bin",path);
    sprintf(log_path,"%s/%s.log",path,hunt_id);
    
    mkdir(path);

    Treasure t;
    printf("Treasure ID: ");
    scanf("%s", t.treasure_id);
    printf("User name: ");
    scanf("%s", t.user_name);
    printf("Latitude: ");
    scanf("%f", &t.GPS.latitude);
    printf("Longitude: ");
    scanf("%f", &t.GPS.longitude);

    sprintf(action,"treasure:%s was created",t.treasure_id);

    int fd=open(treasure_file, O_WRONLY | O_CREAT | O_APPEND);
    if(fd==-1){
        printf("Failed to open file");
        return;
    }
    write(fd,&t,sizeof(Treasure));

    close(fd);
    add_log(log_path,action,strlen(action));
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
        return 0;
    }else if(strcmp(operation,"--view")==0 && argc==4){
        return 0;
    }else if(strcmp(operation,"--remove")==0 && argc==4){
        return 0;
    }else if(strcmp(operation,"--remove_hunt")==0){
        return 0;
    }else{
        printf("Invalid arguments\n");
        return 1;
    }

    return 0;
}
