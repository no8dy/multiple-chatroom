#include<curses.h> /* add stdio.h automatically */
#include<sys/socket.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<sys/types.h>
#include<string.h>
#include<strings.h>
#include<math.h>
#include<pthread.h>
#include<signal.h>
/* for inet_aton */
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
/* for close */
#include<unistd.h>
//prevent from ctrl C
void escape(int a);
//function for curses
void initial(void);
//function for socket
void recemsg(void);
void sermission(void);
int  climission(void);
//dimentation for socket
char buffer0[128];//to receive msg
char buffer1[100];//to send msg
char myname[60] , yourname[60] , history[300][300];;
int sockfd , clientfd , sockfd_old , PORT , ch;
char IP[17];
char non;//for getchar nothing
//dimentation for curses
WINDOW *win[2],*curwin,*helpwin;
int nowwin , topline = 0 , curline=0 , localine = 0 , x , y , new = 1;
//start main program
int main(void){
	signal(SIGINT,escape);
	int i;
	// input name
	printf("Input your name:");

	fgets(myname,sizeof(myname),stdin);
	for(i=0;i<sizeof(myname);i++){
		if(myname[i]=='\n'){
			myname[i]='\0';
		}
	}

	sprintf(buffer1,"nis%s",myname);

	while(1){
		puts("Input 's' to start server 'c' to connect");
		ch=getchar();
 		if(ch=='c'){
 			while(ch=getchar()=='\n'){
 				break;
			}
			if(climission()!=-1){
				 break;
 			}
		 }
		 else if(ch=='s'){
			 while(ch=getchar()=='\n'){
				 break;
			 }
			 sermission();
			 break;
		 }
		 while(ch=getchar()=='\n'){
			 break;
		 }
	 }

	// use thread to listen msg //
	pthread_t id;
	int ret;
	ret = pthread_create(&id,NULL,(void *)recemsg,NULL);
	if(ret!=0){
		printf("create pthread error!\n");
		exit(1);
	}

	// start to send msg
	if(write(sockfd, buffer1, sizeof(buffer1))==-1){
		puts("send name failed");
	}

	// start curses terminal

	int hch;//helpwin ch for getch

	initial();

	win[0]=newwin(300,COLS-1,0,0); // set communication box
	win[1]=newwin(5,COLS-2,LINES-5,0); // set insert line

	wsetscrreg(win[0],0,299);
	scrollok(win[0],TRUE);// let box can be scrolled
	idlok(win[0],TRUE);
	leaveok(win[0],TRUE);

	helpwin=newwin(3,30,2,COLS/2-15);
	box(helpwin,'|','-');
	mvwaddstr(helpwin,0,10,"help");
	mvwaddstr(helpwin,1,4," press any key to continue...");

	nowwin=1;
	curwin=win[nowwin];

	for(i=0;i<COLS-1;i++) // draw region of box and line
		mvwaddch(win[1],0,i,'-');

	mvwaddstr(win[1],0,0,myname);

	char string[(LINES-1)*COLS];
	char stringout[(LINES-1)*COLS];//use sprintf(string,":%s",string); bug

	for(i=0;i<=(LINES-1)*COLS;i++){
		string[i]='\0';
	}

	wmove(curwin,1,0);
	mvwaddstr(curwin,1,0,"enter to insert");
	wrefresh(win[1]);
	getyx(curwin,y,x);

	while(1){
		ch=getch();
		switch(ch){
			case '\t':
				touchwin(helpwin);
				wrefresh(helpwin);
				getch();
				touchwin(win[0]);
				wrefresh(win[0]);
				touchwin(win[1]);
				wrefresh(win[1]);
				break;
			case 'k':
			case 'K':
			case KEY_UP:
				if(topline!=0){
					wscrl(win[0],-1);
					mvwprintw(win[0],0,0,"%s",history[topline-1]);
					touchwin(win[0]);
					wrefresh(win[0]);
					touchwin(win[1]);
					wrefresh(win[1]);
					topline--;
					localine++;
				}
				break;
			case 'j':
			case 'J':
			case KEY_DOWN:
				if(topline<(curline-(LINES-5))){
					wscrl(win[0],1);
					touchwin(win[0]);
					wrefresh(win[0]);
					touchwin(win[1]);
					wrefresh(win[1]);
					topline++;
					localine--;
				}
				break;
			case KEY_RIGHT:
				if((y-1)*(COLS-3)+x<strlen(string)){
					x++;
				}
				break;
			case KEY_LEFT:
				x--;
				break;
			case '\r':
				mvwaddstr(curwin,1,0,"insert:        ");
				wrefresh(win[0]);
				touchwin(win[1]);
				wrefresh(win[1]);
				echo();
				mvwgetnstr(curwin,1,7,string,sizeof(string));
				noecho();

				if(string[0]!='\0'){//prevent null line
					sprintf(stringout,"%s:%s",myname,string);
					mvwaddstr(win[0],localine,0,stringout);
					strcpy(history[curline],stringout);

					write(sockfd, string, strlen(string)+1);

					new = strlen(stringout)%(COLS-2) != 0;
                    //set newline location

					localine+=((strlen(stringout)/(COLS-3))+new);//new line

					curline+=((strlen(stringout)/(COLS-3))+new);//important

					while(localine>LINES-5){//when new line under screen,scrl
						wscrl(win[0],1);
						localine--;
						topline++;
					}

				}

				wclear(win[1]);

				for(i=0;i<COLS-1;i++) // draw region of box and line
					mvwaddch(win[1],0,i,'-');

				mvwaddstr(win[1],0,0,myname);
				mvwaddstr(curwin,1,0,"enter to insert");
				wrefresh(win[0]);
				touchwin(win[1]);
				wrefresh(win[1]);
				break;
			case 8:
			case 127:
			case 263://	backspace
                break;
			case 27:
				escape(0);
				break;
			default:
			break;
		}
		wmove(win[1],y,x);
		wrefresh(win[1]);//for wmove ,must use refresh
	}
	return 0;

}

void recemsg(void){
	char string[300]={'\0'};
	int i , len , inserty , insertx;

	while(1){
		len = read(sockfd, buffer0, sizeof(buffer0));
		if(len==0){//retire
			escape(0);
			break;
		}
		else if(len>=1){
			if(buffer0[0]=='n'&&buffer0[1]=='i'&&buffer0[2]=='s'){
				for(i=3;i<sizeof(buffer0);i++){
					buffer0[i-3]=buffer0[i];
				}
				strcpy(yourname,buffer0);
			}
			else{
				sprintf(string,"%s:%s",yourname, buffer0);
				getyx(win[1],inserty,insertx);
				mvwaddstr(win[0],localine,0,string);

				strcpy(history[curline],string);

				if(strlen(string)%(COLS-2)==0){//set newline
					new=0;		//location
				}
				else{
					new=1;
				}

				localine+=((strlen(string)/(COLS-3))+new);//new line

				curline+=((strlen(string)/(COLS-3))+new);//important

				while(localine>LINES-5){//when new line under screen,scrl
					wscrl(win[0],1);
					localine--;
					topline++;
				}
				for(i=0;i<COLS-1;i++) // draw region of box and line
					mvwaddch(win[1],0,i,'-');

				mvwaddstr(win[1],0,0,myname);
				wmove(win[1],inserty,insertx);
				wrefresh(win[0]);
				touchwin(win[1]);
				wrefresh(win[1]);
			}
		}
	}
	return;
}

int climission(void){

	struct sockaddr_in dest0;

	// input IP
	printf("Input IP(local:127.0.0.1 Enter to default):\n");
	fgets(IP,sizeof(IP),stdin);
	if(IP[0]=='\n');
	sprintf(IP,"127.0.0.1");

	// input port
	printf("Input port(ex:8889):");
	scanf("%d",&PORT);

	//create socket
	sockfd = socket(PF_INET, SOCK_STREAM, 0);

	// initialize value in dest0
	bzero(&dest0, sizeof(dest0));
	dest0.sin_family = PF_INET;
	dest0.sin_port = htons(PORT);
	inet_aton(IP, &dest0.sin_addr);

	// Connecting to server
	if(connect(sockfd, (struct sockaddr*)&dest0, sizeof(dest0))==-1){
		puts("connected failed");
		return -1;
	}
	else{
		puts("connected as a client");
	}
	bzero(buffer0, 128);
	return 0;
}

void sermission(void){

	struct sockaddr_in dest1;

	// input port
	printf("Input port(ex:8889):");
	scanf("%d",&PORT);

	puts("wait to msn connecting");

	// create socket , same as client
	sockfd_old = socket(PF_INET, SOCK_STREAM, 0);

	// initialize structure dest1
	bzero(&dest1, sizeof(dest1));
	dest1.sin_family = PF_INET;
	dest1.sin_port = htons(PORT);

	// this line is different from client
	dest1.sin_addr.s_addr = INADDR_ANY;

	// Assign a port number to socket
	bind(sockfd_old, (struct sockaddr*)&dest1, sizeof(dest1));

	// make it listen to socket with max 20 connections
	listen(sockfd_old, 25);

	struct sockaddr_in client_addr;
	int addrlen=sizeof(client_addr);

	/* Wait and Accept connection */
	sockfd = accept(sockfd_old, (struct sockaddr*)&client_addr, &addrlen);
	puts("already connected");

}

void initial(void){
	initscr();
	cbreak();
	nonl();
	noecho();
	intrflush(stdscr,FALSE);
	keypad(stdscr,TRUE);
	refresh();
	return;
}
void escape(int a){
	endwin();
	puts("connection break");
	close(sockfd);
	close(sockfd_old);
	exit(0);
}
