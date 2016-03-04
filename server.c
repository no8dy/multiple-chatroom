#define CLIENTNUM 21  // leave one to refuse
#include<curses.h> /* add stdio.h automatically */
#include<sys/socket.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<sys/types.h>
#include<string.h>
#include<strings.h>
#include<math.h>
#include<pthread.h> //change name
//funcrion
void create_recv_thread(int order);
int cancel_recv_thread(int order);
void command_thread(void);
void recemsg(void *num);
//void recemsg(void);
void sermission(void);
void kill(char *man);
void memberctrl(char *mod,char *name);
void wall(char *sender,char *string);
void close_server(void);
int writeto(char *sender,char *receiver,char *string);
//dimentation for socket
struct clientinfo {
	pthread_t id;//dimentation for thread
	int root;
	int fd;
	char name[20];
	char curname[20];
};

pthread_t id;
pthread_t commandid; 
pthread_t shutdownid;
struct clientinfo client[CLIENTNUM]={
	[0 ... CLIENTNUM-1]={ .fd=-1, .root=0}
};

char myname[60];
int sockfd,clientfd;
int PORT;
char IP[17];
int ch;
int online=0;
//start main program
int main(void){
	int i;
	for(i=0;i<CLIENTNUM;i++){
		client[i].fd=-1;
	}
	// input name
	printf("Set root pw:");
	fgets(myname,sizeof(myname),stdin);
	for(i=0;i<sizeof(myname);i++){
	if(myname[i]=='\n'){
			myname[i]='\0';
		}
	}

	struct sockaddr_in dest1;

	// input port 
	printf("Input port(ex:8889):");
	scanf("%d",&PORT);	

	// create socket , same as client //if i didn't use () outer socket()
	// <0 would be wrong
	if((sockfd=socket(PF_INET, SOCK_STREAM, 0))==-1){
		printf("socket()<0\n");
		exit(1);
	}
	// initialize structure dest1
	bzero(&dest1, sizeof(dest1));
	dest1.sin_family = PF_INET;
	dest1.sin_port = htons(PORT);

	// this line is different from client
	dest1.sin_addr.s_addr = INADDR_ANY;

	// Assign a port number to socket
//	bind(sockfd, (struct sockaddr*)&dest1, sizeof(dest1));
	if(bind(sockfd, (struct sockaddr*)&dest1, sizeof(dest1))==-1){
		printf("bind()==-1\n");
		exit(1);
	}

	// make it listen to socket with max 20 connections
	listen(sockfd,CLIENTNUM);

	// create thread to  Accept commands
	if(pthread_create(&commandid,NULL,(void *)command_thread,NULL)!=0){
		printf("create pthread error!\n");
		return;
	}

	puts("wait to client connecting");
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
			if(pthread_create(&client[order].id,NULL,(void *)recemsg,&order)!=0){
				printf("create client[%d] thread error\n",order);
				client[order].fd=-1;
			}
		}
		sleep(2);//fucking bug
	}
	return 0;
}

int cancel_recv_thread(int order){
	int ret;
	if(!pthread_cancel(client[order].id)){
		printf("cancel %s OK",client[order].name);
		online--;//kill by server
	}
	else{
		printf("cancel pthread error!");
		return -1;
	}
	close(client[order].fd);	
	client[order].fd=-1;
	memberctrl("remove",client[order].name);
//	printf("remove %s 139",client[order].name);
	client[order].curname[0]='\0';
	client[order].name[0]='\0';//after remove!!
	return 0;
}
void wall(char *sender,char *string){
	char output[307];
	int i;
	for(i=0;i<CLIENTNUM;i++){
		if(client[i].fd!=-1){
			sprintf(output,"public %s %s",sender,string);
			if(send(client[i].fd,output,sizeof(output),0)<0){
				printf("send %s err\n",client[i].name);
			}
		}
	}
	return;
}
int writeto(char *sender,char *receiver,char *string){
	char output[307];	
	int i;
//	printf("recv private\n");
	for(i=0;i<CLIENTNUM;i++){
		if(strcmp(receiver,client[i].name)==0 && client[i].fd!=-1){
			sprintf(output,"private (%s to %s)~%s",sender,receiver,string);
			if(send(client[i].fd,output,sizeof(output),0)<=0){
				printf("send %s err\n",client[i].name);
			}
//			sleep(1);
		}
		if(strcmp(sender,client[i].name)==0 && client[i].fd!=-1){
			sprintf(output,"private (%s to %s)~%s",sender,receiver,string);
			if(send(client[i].fd,output,sizeof(output),0)<=0){
				printf("send %s err\n",client[i].name);
			}
//			sleep(1);
		}
	}
	return 0;
}
void memberctrl(char *mod,char *name){
	int i,j;
	char output[25];
//	printf("180 %s",name);
	if(strcmp(mod,"all")==0){
		for(i=0;i<CLIENTNUM;i++){
			if(strcmp(name,client[i].name)==0){
				for(j=0;j<CLIENTNUM;j++){
					if((client[j].fd!=-1)&&(i!=j)){
						sprintf(output,"add %s",client[j].curname);
						send(client[i].fd,output,sizeof(output),0);
						sleep(1);//too fast
					}
				}
				break;
			}
		}
		return;
	}
	//--------------------------------
	if(strcmp(mod,"add")==0){
		sprintf(output,"add %s",name);
	}
	else if(strcmp(mod,"remove")==0){
		sprintf(output,"remove %s",name);
	}
//	printf("202 %s\n",name);
	for(i=0;i<CLIENTNUM;i++){
		if(client[i].fd!=-1 && strcmp(client[i].name,name)!=0){
			send(client[i].fd,output, sizeof(output),0);
			printf("send %s \"%s\"\n",client[i].name,output);
		}
	}
	return;
}
void kill(char *man){
	int i;
	for(i=0;i<CLIENTNUM;i++){
		if(strcmp(man,client[i].name)==0){
			if(cancel_recv_thread(i)==0){
				printf("%s is killed\n",client[i].name);
				return;
			}
			else{
				printf("%s isn't killed\n",client[i].name);
				return;
			}
		}
	}
	printf("there is no client named %s\n",man);
	return;
}
void close_server(void){
	int i;
	wall("root","server will close after 5 seconds");
	printf("will quit after 5 seconds\n5\n");
	for(i=4;i>=0;i--){
		sleep(1);
		printf("%d\n",i);
	}
	for(i=0;i<CLIENTNUM;i++){
		if(client[i].fd!=-1){
			kill(client[i].name);
		}
	}
	close(sockfd);
	exit(0);
}
void recemsg(void *num){
	char buffer0[310];//to receive msg
	char command[10];
	char man[20];
	char string[320]={'\0'};
	char output[30];
	int endsub=0;
	int order=*((int*)num);
	int len;
	int i;

	online++;

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	while(1){
		len = recv(client[order].fd, buffer0, sizeof(buffer0),0);
		if(len==0){
			printf("client[%d] connection break\n",order);
			break;
		}
		else if(len>=1){
			sscanf(buffer0,"%s",command);
			if(strcmp(command,"wall")==0){
				printf("recv wall msg from %s\n",client[order].name);	
				sscanf(buffer0,"%*s %[^\n]",string);
				wall(client[order].curname,string);
			}
			else if(strcmp(command,"write")==0){
				printf("recv write msg from %s\n",client[order].name);
				sscanf(buffer0,"%*s %s %[^\n]",man,string);
				if(strcmp(man,"root")==0){
					printf("%s:%s\n",client[order].name,string);
				}
				else{
					writeto(client[order].curname,man,string);
				}
			}
			else if(strcmp(command,"pw")==0){
				printf("%s is trying to login as root\n",client[order].name);
				sscanf(buffer0,"%*s %[^\n]",string);
				if(strcmp(string,myname)==0){
					send(client[order].fd,"pw accept",sizeof("pw accept"),0);
					printf("%s login successfully\n",client[order].name);
				}
				else{
					send(client[order].fd,"pw refuse",sizeof("pw refuse"),0);
					printf("%s login failed\n",client[order].name);
				}
			}
			else if(strcmp(command,"change")==0){
				sscanf(buffer0,"%*s %[^\n]",man);
				if(strcmp(man,"root")==0){
					strcpy(client[order].curname,"root");
				}
				else{
					strcpy(client[order].curname,client[order].name);
				}
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
			else if(strcmp(command,"kill")==0){
				printf("recv kill msg from %s\n",client[order].name);
				sscanf(buffer0,"%*s %s",man);
				kill(man);
			}
			else if(strcmp(command,"shutdown")==0){
				printf("%s shutdown server",client[order].name);
				pthread_create(&shutdownid,NULL,(void *)close_server,NULL);
			}
			else if(strcmp(command,"name")==0){
				if(sscanf(buffer0,"%*s %[^\n]",client[order].curname)!=1){
					send(client[order].fd,"sys No Name? Bazinga",sizeof("sys No Name? Bazinga"),0);
					break;
				}
				if(strcmp(client[order].curname,"root")==0){
					send(client[order].fd,"sys Name can't be \"root\"",sizeof("sys Name can't be \"root\""),0);
					break;
				}
				for(i=0;i<CLIENTNUM;i++){
					if((i!=order)&&client[i].fd!=-1&&(strcmp(client[order].curname,client[i].name)==0)){
						send(client[order].fd,"sys Name has been used",sizeof("sys Name has been used"),0);
						endsub=1;
						break;
					}
				}
				if(endsub==1){
					break;
				}
				printf("client[%d] %s fd=%d has already connected\n",order,client[order].curname,client[order].fd);
				strcpy(client[order].name,client[order].curname);
				memberctrl("add",client[order].name);
				memberctrl("all",client[order].name);
			}
		}
		else{
			printf("receive error msg[%d] from %s\n",len,client[order].name);
		}
	}

	online--;//if client break by himself
	// Close connection 
	close(client[order].fd);
	client[order].fd=-1;
	memberctrl("remove",client[order].name);
	client[order].curname[0]='\0';//be careful it should after remove
	client[order].name[0]='\0';	
	pthread_exit(0);
	return;
}
void command_thread(void){	
	char command[50];	
	char what[30];
	char who[30];
	char string[300];
	int i;
	getchar();	
	//root command line
	while(1){
		fgets(command,sizeof(command),stdin);
		setbuf(stdin,NULL);
		sscanf(command,"%s",what);
		if(strcmp(what,"wall")==0){	
			sscanf(command,"%*s %[^\n]",string);
			wall("root",string);
		}
		else if(strcmp(what,"write")==0){
			sscanf(command,"%*s %s %s",who,string);
			writeto("root",who,string);
		}
		else if(strcmp(what,"kill")==0){
			sscanf(command,"%*s %[^\n]",who);
			kill(who);
		}
		else if(strcmp(what,"pw")==0){
			printf("pw=%s\n",myname);
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
		else if(strcmp(what,"quit")==0){
			close_server();
			break;
		}
		else if(strcmp(what,"help")==0){
			puts("wall string -> write string for everyone");
			puts("write id string -> write string to someone");
			puts("kill id -> kick somebody out of the room");
			puts("w       -> print everyone on line");
			puts("d string(command) -> send command to every directory(if U know the rule");
			puts("quit    -> end sub");
		}
		else if(strcmp(what,"d")==0){
			sscanf(command,"%*s %[^\n]",string);
			for(i=0;i<CLIENTNUM;i++){
				if(client[i].fd!=-1){
					send(client[i].fd,string,sizeof(string),0);
				}
			}
		}
		else{
			puts("no such command");
		}
	}

	pthread_exit(0);
	return;
}
