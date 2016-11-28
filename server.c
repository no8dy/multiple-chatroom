#define FILEROUTE "~/chat_history/server.txt"
#define DATAROUTE "~/chat_history/"
#define CLIENTNUM 21  /* leave one to refuse */
/* add stdio.h automatically */
#include<curses.h> 
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<pthread.h>

#include<stdlib.h>
#include<string.h>

/* for usleep */
#include<sys/timeb.h>
#include<time.h>

int sockfd , clientfd , PORT , ch , online = 0 , curline=0;
char command[350] , history[500][300] , root_pw[60] , IP[17];

pthread_t  cmd_id , shutdownid;

struct clientinfo {
    pthread_t id;//for thread to recv_msg
    int root , fd;
    char name[20] , curname[20];//curname = [root | self.name]
} client[CLIENTNUM]={ [0 ... CLIENTNUM-1]={ .fd=-1 ,  .root=0 } };

/* function */
void create_recv_thread(int);
int cancel_recv_thread(int);
void command_thread(void);
void recv_msg(void *);
void kick(char *);//trimed
void member_ctrl(char * , char *);//trimed
void wall(char * , char *);//trimed
void writeto(char * , char * , char *);//trimed
void close_server(void);
void quick_close_server(void);
void auto_save(void);
char *trim(char *);
void send_cmd_all(char * , unsigned int , int);
/* start main program */
int main(void){

    int i , len;

    printf("Set root password:") , fgets(root_pw , sizeof(root_pw) , stdin);
    root_pw[strlen(root_pw) - 1] = 0;

    struct sockaddr_in chatroom_addr , client_addr;
    printf("Input port(ex:8889):") , scanf("%d" , &PORT);

    /* create socket */	
    sockfd = socket(PF_INET , SOCK_STREAM , 0);
    if(!(~sockfd))
        puts("create socket failed") , exit(1);

    // initialize addr structure
    bzero(&chatroom_addr ,  sizeof(chatroom_addr));
    chatroom_addr.sin_family = PF_INET;
    chatroom_addr.sin_port = htons(PORT);

    // this line is different from client
    chatroom_addr.sin_addr.s_addr = INADDR_ANY;

    // Assign a port number to socket
    if(!(~bind(sockfd ,  (struct sockaddr*)&chatroom_addr ,  sizeof(chatroom_addr))))
        puts("bind addr failed") , exit(1);

    // make it listen to socket with max 20 connections
    listen(sockfd , CLIENTNUM);

    // create thread to  Accept commands
    if(pthread_create(&cmd_id , NULL , (void *)command_thread , NULL))
        puts("create thread failed") , exit(1);

    puts("wait connections...");

    //build history data
//    sprintf(command  , "if [ ! -d %s ];then mkdir %s;fi" ,  DATAROUTE , DATAROUTE);
//    system(command);
//
//    sprintf(command , "date >> %s" , FILEROUTE);
//    system(command);
//    sprintf(command , "hostname | nslookup >> %s" , FILEROUTE);
//    system(command);

    while(1){
        /* find a space that can add new member */ 
        for(i = 0 ; i < CLIENTNUM ; i++)
            if(!(~client[i].fd)) break;

        printf("space = %d\n" , i);
        /* Wait and Accept connection */
        /* I don't store client info , so the nd rd = NULL */	
        len = sizeof(client_addr);
        client[i].fd = accept(sockfd ,  (struct sockaddr*)&client_addr , &len);
        if(online==CLIENTNUM - 1){
            send(client[i].fd , 
                    "sys No space in chatroom" , 
                    sizeof("sys No space in chatroom") , 0) , 
                close(client[i].fd) , client[i].fd = -1;
        }
        else{
            if(pthread_create(&client[i].id , NULL , (void *)recv_msg ,  &i)){
                printf("create client[%d] thread failed\n" , i );
                client[i].fd = -1;
            }
            /* sleep two sec , or the "i" passed into thread 
             * will be revised by loop */
            sleep(2);
        }
    }
    return 0;
}

int cancel_recv_thread(int nth){
    int rtn;
    if(pthread_cancel(client[nth].id))
        printf("cancel pthread failed!") , rtn = -1;
    else{
        online--;
        close(client[nth].fd);	
        client[nth].fd = -1;
        member_ctrl("remove" , client[nth].name);
        client[nth].curname[0] = 0;
        client[nth].name[0] = 0;
    }
    return rtn;
}

void wall(char *sender , char *str){
    int i;
    char send_str[307];
    sender = trim(sender);
    sprintf(history[curline] , "%s:%s" , sender , str);
    puts(history[curline++]);
    for(i = 0 ; i < CLIENTNUM ; i++)
        if(~client[i].fd)
            sprintf(send_str , "public %s %s" , sender , str) , 
                send(client[i].fd , send_str , sizeof(send_str) , 0) < 0 ?
                    printf("send %s failed\n" , client[i].name) : 0;
    return;
}
void writeto(char *sender , char *receiver , char *str){
    int i;
    char send_str[307];	
    receiver = trim(receiver);
    sprintf(history[curline] , "(%s to %s)~%s" , sender , receiver , str);
    puts(history[curline++]);
    for(i = 0 ; i < CLIENTNUM ; i++)
        if(( (!strcmp(receiver , client[i].name)) ||
                    (!strcmp(sender , client[i].curname)) ) && (~client[i].fd))
            sprintf(send_str , "private (%s to %s)~%s" , sender , receiver , str)  , 
                send(client[i].fd , send_str , sizeof(send_str) , 0) <= 0 ?
                    printf("send %s failed\n" , client[i].name) : 0;
    return;
}

void member_ctrl(char *mod , char *name){
    int i , j , man_loc;
    char send_str[250];
    name = trim(name);
    for(man_loc = 0 ; man_loc < CLIENTNUM ; man_loc++)
        if(!strcmp(client[man_loc].name , name)) break;
    /* send new member a list contains all members*/
    if(!strcmp(mod , "all")){
        sprintf(send_str , "add");
        for(i = 0 ; i < CLIENTNUM ; i++)
            if((~client[i].fd) && (i != man_loc))
                sprintf(send_str , "%s %s" , send_str , client[i].name);
        send(client[man_loc].fd , send_str , sizeof(send_str) , 0);
    }
    else{
        sprintf(send_str , "%s %s" , mod , name);
        send_cmd_all(send_str , sizeof(send_str) , man_loc);
    }
    return;
}

void kick(char *man){
    int i;
    man = trim(man);
    for(i = 0 ; i < CLIENTNUM ; i++)
        if(!strcmp(man , client[i].name)){
            if(!cancel_recv_thread(i))
                printf("%s is kicked\n" , man) , 
                    sprintf(history[curline++] , "%s is kicked" , man);
            else
                if(strcmp(man , "All"))
                    printf("%s isn't kicked\n" , client[i].name);
            break;
        }

    if(strcmp(man , "All"))
        printf("there is no client named %s\n" , man);
    return;
}
void quick_close_server(void){
    int i;
    for(i = 0; i < CLIENTNUM ; i++)
        if(~client[i].fd)
            kick(client[i].name);

    close(sockfd);
    exit(0);
}
void close_server(void){
    int i;
    wall("root" , "server will close after 5 seconds");
    printf("will quit after 5 seconds\n5\n");
    sprintf(command , "echo \"server closed\" >> %s" , FILEROUTE);
    system(command);
    for(i = 4 ; i >= 0 ; i--)
        sleep(1) , printf("%d\n" , i);
    quick_close_server();
}

void recv_msg(void *num){
    int endsub = 0 , idx = *((int*)num) , len , i;
    char msg_str[310] , command[10] , man[20] , string[320]= "" , send_str[30];

    online++;

    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS , NULL);

    while(1){
        /* record history */ 	
        auto_save();
        len = recv(client[idx].fd ,  msg_str ,  sizeof(msg_str) , 0);
        if(len==0){
            printf("client[%d] connection break\n" , idx);
            sprintf(history[curline] , "%s connection break" , client[idx].name);
            curline++;
            break;
        }
        else if(len > 0){	
            int readed = 0 , ch_cnt = 0;
            sscanf(msg_str , "%s%n" , command , &readed);
            if(!strcmp(command , "wall")){
                wall(client[idx].curname , msg_str + readed);
            } 
            else if(strcmp(command , "write")==0){
                sscanf(msg_str + readed , "%s%n" , man , &ch_cnt) , readed += ch_cnt;
                writeto(client[idx].curname , man , msg_str + readed);
            }
            else if(!strcmp(command , "pw")){
                printf("%s is trying to login as root\n" , client[idx].name);
                if(!strcmp(trim(msg_str + readed) , root_pw)){
                    send(client[idx].fd , "pw accept" , sizeof("pw accept") , 0);
                    printf("%s login successfully\n" , client[idx].name);
                    sprintf(history[curline] , "%s login as root" , client[idx].name);
                    curline++;
                }
                else{
                    send(client[idx].fd , "pw refuse" , sizeof("pw refuse") , 0);
                    printf("'%s' != '%s'\n" , msg_str + readed , root_pw);
                    printf("%s login failed\n" , client[idx].name);
                }
            }
            else if(!strcmp(command , "change")){
                if(!strcmp(trim(msg_str + readed) , "root")){
                    strcpy(client[idx].curname , "root");
                }
                else{
                    strcpy(client[idx].curname , client[idx].name);
                }
                printf("%s's curname is %s\n" , client[idx].name , client[idx].curname);
            }
            else if(!strcmp(command , "hide")){
                sprintf(send_str , "remove %s" , client[idx].name);
                send_cmd_all(send_str , sizeof(send_str) , idx);
            }
            else if(!strcmp(command , "unhide")){
                sprintf(send_str , "add %s" , client[idx].name);
                send_cmd_all(send_str , sizeof(send_str) , idx);
            }
            else if(strcmp(command , "hideroot")==0){
                sprintf(send_str , "remove root");
                send_cmd_all(send_str , sizeof(send_str) , -1);
            }
            else if(strcmp(command , "unhideroot")==0){
                sprintf(send_str , "add root");
                send_cmd_all(send_str , sizeof(send_str) , -1);
            }
            else if(strcmp(command , "kick")==0){
                printf("recv kick msg from %s\n" , client[idx].name);
                kick(msg_str + readed);
            }
            else if(strcmp(command , "shutdown")==0){
                printf("%s shutdown server" , client[idx].name);
                sprintf(command , "echo \"%s close server\" >> %s" , client[idx].name , FILEROUTE);
                system(command);
                pthread_create(&shutdownid , NULL , (void *)close_server , NULL);
            }
            else if(!strcmp(command , "name")){
                if(sscanf(msg_str + readed , "%s" , client[idx].name)!=1){
                    send(client[idx].fd , "sys No Name? Bazinga" , sizeof("sys No Name? Bazinga") , 0);
                    break;
                }
                if(!strcmp(client[idx].name , "root")){
                    send(client[idx].fd , "sys Name can't be \"root\"" , sizeof("sys Name can't be \"root\"") , 0);
                    break;
                }
                for(i=0;i<CLIENTNUM;i++){
                    if((i!=idx)&&client[i].fd!=-1&&(strcmp(client[idx].name , client[i].name)==0)){
                        send(client[idx].fd , "sys Name has been used" , sizeof("sys Name has been used") , 0);
                        printf("client[%d] repeat name\n" , idx);
                        endsub=1;
                        break;
                    }
                }
                if(endsub==1){
                    break;
                }
                printf("client[%d] %s fd=%d has already connected\n" , idx , client[idx].name , client[idx].fd);
                sprintf(history[curline] , "%s connected" , client[idx].name);
                curline++;
                strcpy(client[idx].curname , client[idx].name);
                member_ctrl("add" , client[idx].name);
                member_ctrl("all" , client[idx].name);
            }
        }
        else{
            printf("receive error msg[%d] from %s\n" , len , client[idx].name);
            printf("who fd is %d\n" , client[idx].fd);
            break; 
        }
    }

    online--;//if client break by himself
    // Close connection 
    close(client[idx].fd);
    client[idx].fd = -1;
    member_ctrl("remove" , client[idx].name);
    client[idx].curname[0] = '\0';//be careful it should after remove
    client[idx].name[0]='\0';	
    pthread_exit(0);
    sprintf(history[curline] , "%s leave" , client[idx].name);
    curline++;
    return;
}
void command_thread(void){	
    int i; /* command line for root */ 
    char cmd[50] , what[30] , who[30] , string[300];
    setbuf(stdin , NULL);	
    while(1){
        fgets(cmd , sizeof(cmd) , stdin);
        setbuf(stdin , NULL);
        sscanf(cmd , "%s" , what);
        if(strcmp(what , "wall")==0){	
            sscanf(cmd , "%*s %[^\n]" , string);
            wall("root" , string);
        }
        else if(strcmp(what , "write")==0){
            sscanf(cmd , "%*s %s %[^\n]" , who , string);
            writeto("root" , who , string);
        }
        else if(strcmp(what , "kick")==0){
            sscanf(cmd , "%*s %[^\n]" , who);
            kick(who);
        }
        else if(strcmp(what , "pw")==0){
            printf("pw=%s\n" , root_pw);
        }
        else if(strcmp(what , "port")==0){
            printf("port=%d\n" , PORT);
        }
        else if(strcmp(what , "fd")==0){
            printf("client num:%d max:%d\n" , online , CLIENTNUM-1);
            for(i=0;i<CLIENTNUM;i++){
                printf("client[%d] %s fd=%d\n" , i , client[i].name , client[i].fd);
            }
        }
        else if(strcmp(what , "w")==0){
            printf("client num:%d max:%d\n" , online , CLIENTNUM-1);
            for(i=0;i<CLIENTNUM;i++){
                if(client[i].fd!=-1){
                    printf("client[%d] %s fd=%d\n" , i , client[i].name , client[i].fd);
                }
            }
        }
        else if(strcmp(what , "save")==0){

            if(curline>0){
                sprintf(cmd , "date >> %s" , FILEROUTE);
                system(cmd);
                for(i=0;i<curline;i++){
                    sprintf(cmd , "echo \"%s\" >> %s" , history[i] , FILEROUTE);
                    system(cmd);
                }
                curline=0;
                puts("saved");
            }
            else{
                puts("no history to save");
            }
        }
        else if(strcmp(what , "quit")==0){
            close_server();
            break;
        }
        else if(strcmp(what , "exit")==0){
            quick_close_server();
            break;
        }
        else if(strcmp(what , "help")==0){
            puts("wall string -> write string for everyone");
            puts("write id string -> write string to someone");
            puts("kick id -> kick somebody out of the room");
            puts("w       -> print everyone on line");
            puts("save    -> save history");
            puts("d (cmd) -> send cmd to everyone directory(if U know the rule");
            puts("quit    -> end sub and warning others");
            puts("exit    -> exit sub right now");
        }
        else if(strcmp(what , "d")==0){
            sscanf(cmd , "%*s %[^\n]" , string);
            send_cmd_all(string , sizeof(string) , -1);
        }
        else{
            puts("no such cmd (do you need \"help\" ?)");
        }
    }

    pthread_exit(0);
    return;
}

void auto_save(void){
    int i;
    if(curline==300){
        sprintf(command , "date >> %s" , FILEROUTE);
        system(command);
        for(i=0;i<300;i++){
            sprintf(command , "echo \"%s\" >> %s" , history[i] , FILEROUTE);
            system(command);
        }
        curline = 0;
    }
    return;
}

char *trim(char *str){
    int i = strlen(str) - 1;
    while(str[i] == ' ') str[i--] = 0;
    i = 0;
    while(str[0] == ' ') str++;
    return str;
}

void send_cmd_all(char *cmd , unsigned int cmd_size , int except_fd){
    int i;
    for(i = 0 ; i < CLIENTNUM ; i++)
        if((~client[i].fd) && i != except_fd)
            send(client[i].fd , cmd ,  cmd_size , 0);
    return;
}
