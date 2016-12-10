#include"setting.h"
#define FILENAME "server.txt"
#define FILE_ROUTE LOG_DIR FILENAME 
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
#include<stdarg.h>

/* for usleep */
#include<sys/timeb.h>
#include<time.h>

int sockfd , clientfd , svr_port , ch , online = 0 , curline=0;
char g_cmd[350] , history[500][300] , root_pw[60];
char g_sd_str[307];

pthread_t  cmd_id , shutdown_id;

struct clientinfo {
    pthread_t id;//for thread to recv_msg
    int root , fd;
    char name[20] , curname[20];//curname = [root | self.name]
} client[CLIENTNUM]={ [0 ... CLIENTNUM-1]={ .fd=-1 , .root=0 } };

/* function */
void create_recv_thread(int);
int cancel_recv_thread(int);
void cmd_thread(void);
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
int combsys(char *cmd , unsigned int cmd_t , char *format , ... );
ssize_t combsend(int fd , char *msg , unsigned int msg_t , char *format , ... );

/* start main program */
int main(void){

    int i , len;

    printf("Set root password:") , fgets(root_pw , sizeof(root_pw) , stdin);
    root_pw[strlen(root_pw) - 1] = 0;

    struct sockaddr_in chatroom_addr , client_addr;
    printf("Input port(ex:8889):") , scanf("%d" , &svr_port);

    /* create socket */	
    sockfd = socket(PF_INET , SOCK_STREAM , 0);
    if(!(~sockfd))
        puts("create socket failed") , exit(1);

    // initialize addr structure
    bzero(&chatroom_addr , sizeof(chatroom_addr));
    chatroom_addr.sin_family = PF_INET;
    chatroom_addr.sin_port = htons(svr_port);

    // this line is different from client
    chatroom_addr.sin_addr.s_addr = INADDR_ANY;

    // Assign a port number to socket
    if(!(~bind(sockfd , (struct sockaddr*)&chatroom_addr , sizeof(chatroom_addr))))
        puts("bind addr failed") , exit(1);

    // make it listen to socket with max 20 connections
    listen(sockfd , CLIENTNUM);

    // create thread to  Accept g_cmds
    if(pthread_create(&cmd_id , NULL , (void *)cmd_thread , NULL))
        puts("create thread failed") , exit(1);

    puts("wait connections...");

    //build history data
//    sprintf(g_cmd  , "if [ ! -d %s ];then mkdir %s;fi" , LOG_DIR , LOG_DIR);
//    system(g_cmd);
//
//    sprintf(g_cmd , "date >> %s" , FILE_ROUTE);
//    system(g_cmd);
//    sprintf(g_cmd , "hostname | nslookup >> %s" , FILE_ROUTE);
//    system(g_cmd);

    while(1){
        /* find a space that can add new member */ 
        for(i = 0 ; i < CLIENTNUM ; i++)
            if(!(~client[i].fd)) break;

        printf("space = %d\n" , i);
        /* Wait and Accept connection */
        /* I don't store client info , so the nd rd = NULL */	
        len = sizeof(client_addr);
        client[i].fd = accept(sockfd , (struct sockaddr*)&client_addr , &len);
        if(online==CLIENTNUM - 1){
            combsend(client[i].fd , g_sd_str , sizeof(g_sd_str) , "%s No space in chatroom" , SYST_MSG); 
            close(client[i].fd) , client[i].fd = -1;
        }
        else{
            if(pthread_create(&client[i].id , NULL , (void *)recv_msg , &i)){
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
        member_ctrl(RMV_LIST , client[nth].name);
        client[nth].curname[0] = 0;
        client[nth].name[0] = 0;
    }
    return rtn;
}

void wall(char *sender , char *str){
    int i;
    sender = trim(sender);
    sprintf(history[curline] , "%s:%s" , sender , str);
    puts(history[curline++]);
    for(i = 0 ; i < CLIENTNUM ; i++)
        if(~client[i].fd)
            combsend(client[i].fd , g_sd_str , sizeof(g_sd_str) , "%s %s %s" , PUBC_MSG , sender , str) < 0 ?
                    printf("send %s failed\n" , client[i].name) : 0;
    return;
}
void writeto(char *sender , char *receiver , char *str){
    int i;
    receiver = trim(receiver);
    sprintf(history[curline] , "(%s to %s)~%s" , sender , receiver , str);
    puts(history[curline++]);
    for(i = 0 ; i < CLIENTNUM ; i++)
        if(( (!strcmp(receiver , client[i].name)) ||
                    (!strcmp(sender , client[i].curname)) ) && (~client[i].fd))
            combsend(client[i].fd , g_sd_str , sizeof(g_sd_str) , "%s (%s to %s)~%s" , PRVT_MSG , sender , receiver , str) < 0 ?
                    printf("send %s failed\n" , client[i].name) : 0;
    return;
}

void member_ctrl(char *mod , char *name){
    int i , j , man_loc;
    name = trim(name);
    for(man_loc = 0 ; man_loc < CLIENTNUM ; man_loc++)
        if(!strcmp(client[man_loc].name , name)) break;
    /* send new member a list contains all members*/
    if(!strcmp(mod , INI_LIST)){
        sprintf(g_sd_str , ADD_LIST);
        for(i = 0 ; i < CLIENTNUM ; i++)
            if((~client[i].fd) && (i != man_loc))
                sprintf(g_sd_str , "%s %s" , g_sd_str , client[i].name);
        send(client[man_loc].fd , g_sd_str , sizeof(g_sd_str) , 0);
    }
    else{
        sprintf(g_sd_str , "%s %s" , mod , name);
        send_cmd_all(g_sd_str , sizeof(g_sd_str) , man_loc);
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
                if(!strcmp(man , "All"))
                    printf("%s isn't kicked\n" , client[i].name);
            break;
        }

    if(!strcmp(man , "All"))
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
    combsys(g_cmd , sizeof(g_cmd) , "echo \"server closed\" >> %s" , FILE_ROUTE);
    for(i = 4 ; i >= 0 ; i--)
        sleep(1) , printf("%d\n" , i);
    quick_close_server();
}

void recv_msg(void *num){
    int endsub = 0 , idx = *((int*)num) , len , i;
    char msg_str[310] , g_cmd[10] , man[20] , string[320]= "" , g_sd_str[30];

    online++;

    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS , NULL);

    while(1){
        /* record history */ 	
        auto_save();
        len = recv(client[idx].fd , msg_str , sizeof(msg_str) , 0);
        if(!len){
            printf("client[%d] connection break\n" , idx);
            sprintf(history[curline] , "%s connection break" , client[idx].name);
            curline++;
            break;
        }
        else if(len > 0){	
            int readed = 0 , ch_cnt = 0;
            sscanf(msg_str , "%s%n" , g_cmd , &readed);
            if(!strcmp(g_cmd , PUBC_MSG)){
                wall(client[idx].curname , msg_str + readed);
            } 
            else if(!strcmp(g_cmd , PRVT_MSG)){
                sscanf(msg_str + readed , "%s%n" , man , &ch_cnt) , readed += ch_cnt;
                writeto(client[idx].curname , man , msg_str + readed);
            }
            else if(!strcmp(g_cmd , TRYTO_SU)){
                printf("%s is trying to login as root\n" , client[idx].name);
                combsend(client[idx].fd , g_sd_str , sizeof(g_sd_str) , "%s %s" , TRYTO_SU , 
                        strcmp(trim(msg_str + readed) , root_pw) ? SU_ST_RF : SU_ST_AC);

                if(!strcmp(trim(msg_str + readed) , root_pw)){
                    sprintf(history[curline] , "%s login as root" , client[idx].name);
                    puts(history[curline++]);
                }
                else{
                    printf("'%s' != '%s'\n" , msg_str + readed , root_pw);
                    printf("%s su failed\n" , client[idx].name);
                }
            }
            else if(!strcmp(g_cmd , CH_IDNTY)){
                strcpy(client[idx].curname , trim(msg_str + readed));
                printf("%s's curname is %s\n" , client[idx].name , client[idx].curname);
            }
            else if(!strcmp(g_cmd , HIDE_SLF)){
                sprintf(g_sd_str , "%s %s" , RMV_LIST , client[idx].name);
                send_cmd_all(g_sd_str , sizeof(g_sd_str) , idx);
            }
            else if(!strcmp(g_cmd , U_HD_SLF)){
                sprintf(g_sd_str , "%s %s" , ADD_LIST , client[idx].name);
                send_cmd_all(g_sd_str , sizeof(g_sd_str) , idx);
            }
            else if(!strcmp(g_cmd , HIDE_ROT)){
                sprintf(g_sd_str , "%s root" , RMV_LIST);
                send_cmd_all(g_sd_str , sizeof(g_sd_str) , -1);
            }
            else if(!strcmp(g_cmd , U_HD_ROT)){
                sprintf(g_sd_str , "%s root" , ADD_LIST);
                send_cmd_all(g_sd_str , sizeof(g_sd_str) , -1);
            }
            else if(!strcmp(g_cmd , KICK_MAN)){
                printf("recv kick msg from %s\n" , client[idx].name);
                kick(msg_str + readed);
            }
            else if(!strcmp(g_cmd , SHUTDOWN)){
                printf("%s shutdown server" , client[idx].name);
                combsys(g_cmd , sizeof(g_cmd) , "echo \"%s close server\" >> %s" , client[idx].name , FILE_ROUTE);
                pthread_create(&shutdown_id , NULL , (void *)close_server , NULL);
            }
            else if(!strcmp(g_cmd , "name")){
                if(sscanf(msg_str + readed , "%s" , client[idx].name)!=1){
                    send(client[idx].fd , "sys No Name? Bazinga" , sizeof("sys No Name? Bazinga") , 0);
                    break;
                }
                if(!strcmp(client[idx].name , "root")){
                    send(client[idx].fd , "sys Name can't be \"root\"" , sizeof("sys Name can't be \"root\"") , 0);
                    break;
                }
                for(i=0;i<CLIENTNUM;i++){
                    if((i!=idx)&&client[i].fd!=-1&&(!strcmp(client[idx].name , client[i].name))){
                        combsend(client[idx].fd , g_sd_str , sizeof(g_sd_str) , "%s %s" , SYST_MSG , "Username Invalid");
                        endsub=1;
                        break;
                    }
                }
                if(endsub)
                    break;
                printf("client[%d] %s fd=%d has already connected\n" , idx , client[idx].name , client[idx].fd);
                sprintf(history[curline] , "%s connected" , client[idx].name);
                curline++;
                strcpy(client[idx].curname , client[idx].name);
                member_ctrl(ADD_LIST , client[idx].name);
                member_ctrl(INI_LIST , client[idx].name);
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
    member_ctrl(RMV_LIST , client[idx].name);
    client[idx].curname[0] = '\0';//be careful it should after remove
    client[idx].name[0]='\0';	
    pthread_exit(0);
    sprintf(history[curline] , "%s leave" , client[idx].name);
    curline++;
    return;
}
void cmd_thread(void){	
    int i; /* g_cmd line for root */ 
    char what[30] , who[30] , string[300];
    setbuf(stdin , NULL);	
    while(1){
        fgets(g_cmd , sizeof(g_cmd) , stdin);
        setbuf(stdin , NULL);
        sscanf(g_cmd , "%s" , what);
        if(!strcmp(what , WALL_MSG)){	
            sscanf(g_cmd , "%*s %[^\n]" , string);
            wall("root" , string);
        }
        else if(!strcmp(what , WRIT_MSG)){
            sscanf(g_cmd , "%*s %s %[^\n]" , who , string);
            writeto("root" , who , string);
        }
        else if(!strcmp(what , KICK_MBR)){
            sscanf(g_cmd , "%*s %[^\n]" , who);
            kick(who);
        }
        else if(!strcmp(what , SHOW_PWD)){
            printf("password=%s\n" , root_pw);
        }
        else if(!strcmp(what , SHOW_PRT)){
            printf("port=%d\n" , svr_port);
        }
        else if(!strcmp(what , SHOW_FDS)){
            printf("client num:%d max:%d\n" , online , CLIENTNUM-1);
            for(i=0;i<CLIENTNUM;i++){
                printf("client[%d] %s fd=%d\n" , i , client[i].name , client[i].fd);
            }
        }
        else if(!strcmp(what , SHOW_MBR)){
            printf("client num:%d max:%d\n" , online , CLIENTNUM-1);
            for(i=0;i<CLIENTNUM;i++){
                if(client[i].fd!=-1){
                    printf("client[%d] %s fd=%d\n" , i , client[i].name , client[i].fd);
                }
            }
        }
        else if(!strcmp(what , SAVE_HIS)){

            if(curline>0){
                combsys(g_cmd , sizeof(g_cmd) , "date >> %s" , FILE_ROUTE);
                for(i=0;i<curline;i++){
                    combsys(g_cmd , sizeof(g_cmd) , "echo \"%s\" >> %s" , history[i] , FILE_ROUTE);
                }
                curline=0;
                puts("saved");
            }
            else{
                puts("no history to save");
            }
        }
        else if(!strcmp(what , EXIT_NML)){
            close_server();
            break;
        }
        else if(!strcmp(what , EXIT_QCK)){
            quick_close_server();
            break;
        }
        else if(!strcmp(what , HELP_MAN)){
            puts(HELP_MENU);
        }
        else if(!strcmp(what , DIRC_CMD)){
            sscanf(g_cmd , "%*s %[^\n]" , string);
            send_cmd_all(string , sizeof(string) , -1);
        }
        else{
            printf("no such cmd (do you need \"%s\" ?)\n" , HELP_MAN);
        }
    }

    pthread_exit(0);
    return;
}

void auto_save(void){
    int i;
    if(curline==300){
        combsys(g_cmd , sizeof(g_cmd) , "date >> %s" , FILE_ROUTE);
        for(i=0;i<300;i++){
            combsys(g_cmd , sizeof(g_cmd) , "echo \"%s\" >> %s" , history[i] , FILE_ROUTE);
        }
        curline = 0;
    }
    return;
}

char *trim(char *str){
    int i = strlen(str) - 1;
    while(str[i] == ' ') str[i--] = 0;
    while(str[0] == ' ') str++;
    return str;
}

void send_cmd_all(char *cmd , unsigned int cmd_size , int except_fd){
    int i;
    for(i = 0 ; i < CLIENTNUM ; i++)
        if((~client[i].fd) && i != except_fd)
            send(client[i].fd , cmd , cmd_size , 0);
    return;
}

int combsys(char *cmd , unsigned int cmd_t , char *format , ... ){
    va_list arg;
    va_start(arg , format);
    vsnprintf(cmd , cmd_t , format , arg);
    va_end(arg);
    return system(cmd);
}

ssize_t combsend(int fd , char *msg , unsigned int msg_t , char *format , ... ){
    va_list arg;
    va_start(arg , format);
    vsnprintf(msg , msg_t , format , arg);
    va_end(arg);
    return send(fd , msg , msg_t , 0);
}