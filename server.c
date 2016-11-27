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
} client[CLIENTNUM]={ [0 ... CLIENTNUM-1]={ .fd=-1, .root=0 } };

/* function */
void create_recv_thread(int order);
int cancel_recv_thread(int order);
void command_thread(void);
void recv_msg(void *num);
void kick(char *man);
void member_ctrl(char *mod,char *name);
void wall(char *sender,char *string);
void close_server(void);
void writeto(char *sender,char *receiver,char *string);

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
	bzero(&chatroom_addr, sizeof(chatroom_addr));
	chatroom_addr.sin_family = PF_INET;
	chatroom_addr.sin_port = htons(PORT);

	// this line is different from client
	chatroom_addr.sin_addr.s_addr = INADDR_ANY;

	// Assign a port number to socket
	if(!(~bind(sockfd, (struct sockaddr*)&chatroom_addr, sizeof(chatroom_addr))))
		puts("bind addr failed") , exit(1);

	// make it listen to socket with max 20 connections
	listen(sockfd , CLIENTNUM);

	// create thread to  Accept commands
	if(pthread_create(&cmd_id , NULL , (void *)command_thread , NULL))
	    puts("create thread failed") , exit(1);

	puts("wait connections...");
    
    //build history data
    sprintf(command ,"if [ ! -d %s ];then mkdir %s;fi", DATAROUTE , DATAROUTE);
    system(command);

    sprintf(command,"date >> %s",FILEROUTE);
	system(command);
	sprintf(command,"hostname | nslookup >> %s",FILEROUTE);
	system(command);

	int order=0;
	while(1){
		for(order=0;order<CLIENTNUM;order++){
			if(client[order].fd==-1){
				break;
			}
		}
		struct sockaddr_in client_addr;
		int addrlen;
		addrlen=sizeof(client_addr);

		// Wait and Accept connection
		client[order].fd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
		if(online==CLIENTNUM-1){
			send(client[order].fd,"sys No space in chatroom",sizeof("sys No space in chatroom"),0);
			close(client[order].fd);
			client[order].fd=-1;
		//	continue;can't use this or order will be CLIENTNUM, means to break the loop rule i set
		}
		else{ //so i use else here
			if(pthread_create(&client[order].id,NULL,(void *)recv_msg,&order)!=0){
				printf("create client[%d] thread error\n",order);
				client[order].fd=-1;
			}
		}
		sleep(2);//fucking bug
	}



//	while(1){
//        //puts("finding space");
//        /* find a space that can add new member */ 
//        for(i = 0 ; i < CLIENTNUM ; i++)
//            if(!(~client[i].fd)) break;
//		
//        printf("space = %d\n" , i);
//        /* Wait and Accept connection */
//	    /* I don't store client info , so the nd rd = NULL */	
//        len = sizeof(client_addr);
//        client[i].fd = accept(sockfd, (struct sockaddr*)&client_addr , &len);
//		if(online == CLIENTNUM - 1){
//			send(client[i].fd , 
//                    "sys No space in chatroom" , 
//                    sizeof("sys No space in chatroom") , 0) , 
//			close(client[i].fd) , client[i].fd = -1;
//        }
//		else{
//			if(pthread_create(&client[i].id , NULL , (void *)recv_msg, &i)){
//				printf("create client[%d] thread failed\n" , i );
//                client[i].fd = -1;
//            }
//        /* sleep two sec , or the "i" passed into thread 
//         * will be revised by loop */
//		    sleep(2);
//        }
//	}
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

void wall(char *sender,char *str){
	int i;
	char send_str[307];
	sprintf(history[curline] , "%s:%s" , sender , str);
	puts(history[curline++]);
    for(i = 0 ; i < CLIENTNUM ; i++)
        if(~client[i].fd)
            sprintf(send_str , "public %s %s" , sender , str) , 
                send(client[i].fd , send_str , sizeof(send_str) , 0) < 0 ?
                    printf("send %s failed\n" , client[i].name) : 0;
    return;
}
void writeto(char *sender,char *receiver,char *str){
	int i;
	char send_str[307];	
	sprintf(history[curline],"(%s to %s)~%s",sender,receiver,str);
	puts(history[curline++]);
    for(i = 0 ; i < CLIENTNUM ; i++)
		if(( (!strcmp(receiver , client[i].name)) ||
             (!strcmp(sender , client[i].curname)) ) && (~client[i].fd))
			sprintf(send_str , "private (%s to %s)~%s" , sender , receiver,str) ,
                send(client[i].fd,send_str,sizeof(send_str),0) <= 0 ?
				printf("send %s failed\n" , client[i].name) : 0;
	return;
}

void member_ctrl(char *mod,char *name){
	int i , j , man_loc;
	char send_str[250];
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
        for(i = 0 ; i < CLIENTNUM ; i++)
            if((~client[i].fd) && (i != man_loc))
                send(client[i].fd , send_str , sizeof(send_str) , 0);
    }
	return;
}

void kick(char *man){
	int i;
    for(i = 0 ; i < CLIENTNUM ; i++)
		if(!strcmp(man , client[i].name)){
			if(!cancel_recv_thread(i))
				printf("%s is kicked\n" , man) , 
				sprintf(history[curline++],"%s is kicked",man);
			else
                if(strcmp(man , "All"))
				    printf("%s isn't kicked\n",client[i].name);
            break;
		}

    if(strcmp(man , "All"))
	    printf("there is no client named %s\n" , man);
	return;
}
void close_server(void){
	int i;
	wall("root" , "server will close after 5 seconds");
	printf("will quit after 5 seconds\n5\n");
	sprintf(command,"echo \"server closed\" >> %s",FILEROUTE);
	system(command);

    for(i = 4 ; i >= 0 ; i--)
		sleep(1) , printf("%d\n" , i);
	
    for(i = 0; i < CLIENTNUM ; i++)
		if(~client[i].fd)
			kick(client[i].name);

	close(sockfd);
	exit(0);
}
void recv_msg(void *num){
	char msg_str[310] , command[10] , man[20] , string[320]= "";
	char output[30];
	int endsub = 0 , order = *((int*)num) , len , i;

    puts("start recv msg");

	online++;
	
    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS , NULL);

    while(1){
		//-----------------------------------------------
		if(curline==300){

			sprintf(command,"date >> %s",FILEROUTE);
			system(command);
			for(i=0;i<300;i++){
				sprintf(command,"echo \"%s\" >> %s",history[i],FILEROUTE);
				system(command);
			}
			curline=0;
		}
		//-----------------------------------------------autosave
		len = recv(client[order].fd, msg_str, sizeof(msg_str),0);
		if(len==0){
			printf("client[%d] connection break\n",order);
			sprintf(history[curline],"%s connection break",client[order].name);
			curline++;
			break;
		}
		else if(len>=1){	
			sscanf(msg_str,"%s",command);
			if(strcmp(command,"wall")==0){
//				printf("recv wall msg from %s\n",client[order].name);	
				sscanf(msg_str,"%*s %[^\n]",string);
				wall(client[order].curname,string);
			}
			else if(strcmp(command,"write")==0){
//				printf("recv write msg from %s\n",client[order].name);
				sscanf(msg_str,"%*s %s %[^\n]",man,string);
				if(strcmp(man,"root")==0){
					printf("%s:%s\n",client[order].name,string);
				}
				else{
					writeto(client[order].curname,man,string);
				}
			}
			else if(strcmp(command,"pw")==0){
				printf("%s is trying to login as root\n",client[order].name);
				sscanf(msg_str,"%*s %[^\n]",string);
				if(strcmp(string,root_pw)==0){
					send(client[order].fd,"pw accept",sizeof("pw accept"),0);
					printf("%s login successfully\n",client[order].name);
					sprintf(history[curline],"%s login as root",client[order].name);
					curline++;

				}
				else{
					send(client[order].fd,"pw refuse",sizeof("pw refuse"),0);
					printf("%s login failed\n",client[order].name);
				}
			}
			else if(strcmp(command,"change")==0){
				sscanf(msg_str,"%*s %[^\n]",man);
				if(strcmp(man,"root")==0){
					strcpy(client[order].curname,"root");
				}
				else{
					strcpy(client[order].curname,client[order].name);
				}
				printf("%s's curname is %s\n",client[order].name,client[order].curname);
			}
			else if(strcmp(command,"hide")==0){
				sprintf(output,"remove %s",client[order].name);
				for(i=0;i<CLIENTNUM;i++){
					if(client[i].fd!=-1 && strcmp(client[i].name,client[order].name)!=0){
						send(client[i].fd,output, sizeof(output),0);
						printf("hide %s to %s\n",client[order].name,client[i].name);
					}
				}
			}
			else if(strcmp(command,"unhide")==0){
				sprintf(output,"add %s",client[order].name);
				for(i=0;i<CLIENTNUM;i++){
					if(client[i].fd!=-1 && strcmp(client[i].name,client[order].name)!=0){
						send(client[i].fd,output, sizeof(output),0);
						printf("unhide %s to %s\n",client[order].name,client[i].name);
					}
				}
			}
			else if(strcmp(command,"hideroot")==0){
				sprintf(output,"remove root");
				for(i=0;i<CLIENTNUM;i++){
					if(client[i].fd!=-1){
						send(client[i].fd,output, sizeof(output),0);
						printf("hide root to %s\n",client[i].name);
					}
				}
			}
			else if(strcmp(command,"unhideroot")==0){
				sprintf(output,"add root");
				for(i=0;i<CLIENTNUM;i++){
					if(client[i].fd!=-1){
						send(client[i].fd,output, sizeof(output),0);
						printf("unhide root to %s\n",client[i].name);
					}
				}
			}
			else if(strcmp(command,"")==0){

			}
			else if(strcmp(command,"kick")==0){
				printf("recv kick msg from %s\n",client[order].name);
				sscanf(msg_str,"%*s %s",man);
				kick(man);
			}
			else if(strcmp(command,"shutdown")==0){
				printf("%s shutdown server",client[order].name);
				sprintf(command,"echo \"%s close server\" >> %s",client[order].name,FILEROUTE);
				system(command);
				pthread_create(&shutdownid,NULL,(void *)close_server,NULL);
			}
			else if(strcmp(command,"name")==0){
				if(sscanf(msg_str,"%*s %[^\n]",client[order].name)!=1){
					send(client[order].fd,"sys No Name? Bazinga",sizeof("sys No Name? Bazinga"),0);
					break;
				}
				if(strcmp(client[order].name,"root")==0){
					send(client[order].fd,"sys Name can't be \"root\"",sizeof("sys Name can't be \"root\""),0);
					break;
				}
				for(i=0;i<CLIENTNUM;i++){
					if((i!=order)&&client[i].fd!=-1&&(strcmp(client[order].name,client[i].name)==0)){
						send(client[order].fd,"sys Name has been used",sizeof("sys Name has been used"),0);
                        printf("client[%d] repeat name\n" , order);
						endsub=1;
						break;
					}
				}
				if(endsub==1){
					break;
				}
				printf("client[%d] %s fd=%d has already connected\n",order,client[order].name,client[order].fd);
				sprintf(history[curline],"%s connected",client[order].name);
				curline++;
				strcpy(client[order].curname,client[order].name);
				member_ctrl("add",client[order].name);
				member_ctrl("all",client[order].name);
			}
		}
		else{
			printf("receive error msg[%d] from %s\n",len,client[order].name);
            printf("who fd is %d\n" , client[order].fd);
            break; 
		}
	}

	online--;//if client break by himself
	// Close connection 
	close(client[order].fd);
	client[order].fd=-1;
	member_ctrl("remove",client[order].name);
	client[order].curname[0]='\0';//be careful it should after remove
	client[order].name[0]='\0';	
	pthread_exit(0);
	sprintf(history[curline],"%s leave",client[order].name);
	curline++;
	return;
}
void command_thread(void){	
	int i; /* command line for root */ 
	char cmd[50] , what[30] , who[30] , string[300];
	setbuf(stdin,NULL);	
	while(1){
		fgets(cmd,sizeof(cmd),stdin);
		setbuf(stdin,NULL);
		sscanf(cmd,"%s",what);
		if(strcmp(what,"wall")==0){	
			sscanf(cmd,"%*s %[^\n]",string);
			wall("root",string);
		}
		else if(strcmp(what,"write")==0){
			sscanf(cmd,"%*s %s %[^\n]",who,string);
			writeto("root",who,string);
		}
		else if(strcmp(what,"kick")==0){
			sscanf(cmd,"%*s %[^\n]",who);
			kick(who);
		}
		else if(strcmp(what,"pw")==0){
			printf("pw=%s\n",root_pw);
		}
		else if(strcmp(what,"port")==0){
			printf("port=%d\n",PORT);
		}
		else if(strcmp(what,"fd")==0){
			printf("client num:%d max:%d\n",online,CLIENTNUM-1);
			for(i=0;i<CLIENTNUM;i++){
				printf("client[%d] %s fd=%d\n",i,client[i].name,client[i].fd);
			}
		}
		else if(strcmp(what,"w")==0){
			printf("client num:%d max:%d\n",online,CLIENTNUM-1);
			for(i=0;i<CLIENTNUM;i++){
				if(client[i].fd!=-1){
					printf("client[%d] %s fd=%d\n",i,client[i].name,client[i].fd);
				}
			}
		}
		else if(strcmp(what,"save")==0){

			if(curline>0){
				sprintf(cmd,"date >> %s",FILEROUTE);
				system(cmd);
				for(i=0;i<curline;i++){
					sprintf(cmd,"echo \"%s\" >> %s",history[i],FILEROUTE);
					system(cmd);
				}
				curline=0;
				puts("saved");
			}
			else{
				puts("no history to save");
			}
		}
		else if(strcmp(what,"quit")==0){
			close_server();
			break;
		}
		else if(strcmp(what,"help")==0){
			puts("wall string -> write string for everyone");
			puts("write id string -> write string to someone");
			puts("kick id -> kick somebody out of the room");
			puts("w       -> print everyone on line");
			puts("save    -> save history");
			puts("d string(cmd) -> send cmd to everyone directory(if U know the rule");
			puts("quit    -> end sub");
		}
		else if(strcmp(what,"d")==0){
			sscanf(cmd,"%*s %[^\n]",string);
			for(i=0;i<CLIENTNUM;i++){
				if(client[i].fd!=-1){
					send(client[i].fd,string,sizeof(string),0);
				}
			}
		}
		else{
			puts("no such cmd (do you need \"help\" ?)");
		}
	}

	pthread_exit(0);
	return;
}
