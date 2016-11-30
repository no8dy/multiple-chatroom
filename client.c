#include"setting.h"
#define FILENAME "client.txt"
#define FILE_ROUTE LOG_DIR FILENAME
/* include stdio.h by curses.h 
 * automatically */

#include<curses.h> 
#include<sys/socket.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
/* for ctrl-C */
#include<signal.h>
/* for inet_aton */
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
/* for usleep */
#include<sys/timeb.h>
#include<time.h>

struct stat route;
void storehistory(void);
char command[350];
char fileroute[40];

//var for socket
struct clientinfo {
	int hold;
	char name[10];
};
struct clientinfo client[CLIENTNUM+1]={
					   	[1 ... CLIENTNUM]={
							.hold=0 } , 
						[0]={
							.hold=1 , 
					   		.name="All"	}
};
					//   v one for "All"
int memberlist[ CLIENTNUM + 1 ] = {[ 0 ] = 0};//point client array (sorted)
int online = 0;//the num people on line
char myname[11];
char curname[20];
char receivername[11]={ "All" };
int serverfd;
int clientfd;
int PORT;
char IP[25];
int ch;
char non;//for getchar nothing
int root = 0;// whether you have root competence
//normal function
char *trim(char *);
///function for socket
void recemsg(void);
void escape(int a);
int cnt_host(void);
void redraw(int mod);
void memberctrl(char *mod , char *name);
//var for curses
int rooting = 0;
WINDOW *win[7] , *curwin , *rootwin;
int onlinetop=0;//reference point for online box to scrl
char history[300][300]={"\0"};
int topline=0;//for communication box 
int curline=0;//for history
int localine=0;//for communication bodx
int x , y;
int new=1;
char pointer='$';//$(normal) >(just good) #(root)
//function for curses
void initial(void);
void special(int attrs , WINDOW *swin , int y , int x , char *text);
int terminal(WINDOW *twin , char *str , int n);//manipulate insert box
void membermod(char *name);//for member win to select
void selectmod(void);//for opinion win to select
void rootmod(void);
char* omitname(char *str);//if name>8char print "name8 ,  ,  , "
char omitstr[15];

//start main program
int main(){
	char output[17];
	char stroutput[300];
	signal(SIGINT , escape);
	int i;
	strcpy(myname , "nobody");
	// input name
	printf("Input your name(less than 10 letters):");
	//fgets(myname , sizeof(myname) , stdin);
    scanf("%s" , myname);
	for(i=0;i<sizeof(myname);i++){
		if(myname[i]=='\n'){
			myname[i]='\0';
		}
	}
	strcpy(curname , myname);
	setbuf(stdin , NULL);

	if(cnt_host()==-1){
		printf("port not be used\n");
		escape(1);
	}


	// use thread to listen msg //
	pthread_t id;
	int ret;
	ret = pthread_create(&id , NULL , (void *)recemsg , NULL);
	if(ret!=0){
		printf("create pthread error!\n");
		exit(1);
	}
	sprintf(output , "name %s" , curname);

    send(serverfd , output , sizeof(output) , 0);
    
	// build history data
	if(stat(LOG_DIR , &route)==0){
		sprintf(command , "mkdir %s" , LOG_DIR);
		system(command);
	}
	sprintf(command , "date >> %s" , FILE_ROUTE);
	system(command);
	sprintf(command , "echo \"connected to %s:%d\" >> %s" , IP , PORT , FILE_ROUTE);
	system(command);
	// start curses terminal
	int hch;//rootwin ch for getch

	initial();

	win[2]=newwin(CLIENTNUM+2 , 12 , 0 , COLS-13);// online box

	win[3]=newwin(3 , COLS-13 , 0 , 0);//command box
	win[0]=newwin(300 , COLS-13 , 3 , 0); // set communication box

	win[6]=newwin(5 , 2 , LINES-5 , 0);// to whom box (in insert box
	win[1]=newwin(5 , COLS-23 , LINES-5 , 2); // set insert box
	win[4]=newwin(5 , 20 , LINES-5 , COLS-21); //opinion line

	win[5]=newwin(1 , 15 , LINES-5+(win[1]->_maxy) , (win[1]->_maxx)-12);

	keypad(win[0] , TRUE);
   	keypad(win[1] , TRUE);
	keypad(win[2] , TRUE);
	keypad(win[3] , TRUE);
	keypad(rootwin , TRUE);

	wsetscrreg(win[0] , 0 , 299);
	scrollok(win[0] , TRUE);// let box can be scrolled
	scrollok(win[2] , TRUE);
	idlok(win[0] , TRUE);
	leaveok(win[0] , TRUE);

	rootwin=newwin(10 , COLS/3 , 5 , COLS/3);
	curwin=win[1];
	redraw(0);//need curwin
	wmove(win[1] , 1 , 0);
	mvwprintw(win[6] , 1 , 1 , "%c" , pointer);
	wrefresh(win[6]);	
	wrefresh(win[1]);
	mvwprintw(win[2] , 0 , 4 , "All");
	wrefresh(win[2]);
	getyx(win[1] , y , x);
	
	char string[300];
	while(1){
		if (wmove(win[1] , 1 , 0) == ERR){
			printf("wmove ERR\n");
			escape(0);
		}
		wrefresh(win[1]);
		if(terminal(win[1] , string , 280)==ERR){
			printf("terminal ERR\n");
			escape(0);
		}
		if(string[0]!='\0'){
			if(strcmp(receivername , "All")==0){
				sprintf(stroutput , "%s %s" ,  PUBC_MSG , string);
			}
			else{
				sprintf(stroutput , "%s %s %s" , PRVT_MSG , receivername , string);
			}
			send(serverfd , stroutput , sizeof(stroutput) , 0);
		}
		memset(string , 0 , sizeof(string));
		wclear(win[1]);
		mvwprintw(win[6] , 1 , 1 , "%c" , pointer);
		wrefresh(win[6]);
		redraw(1);
	}
	return 0;
}

void escape(int a){
	endwin();
	close(serverfd);
	if(a==0){
		puts("Exit chatroom");
	}
	else if(a==1){
		puts("connection break");
	}
	else if(a==2){
		puts("recvmsg break;");
	}
	exit(0);
	return;
}

void recemsg(void){
	int len , i;
    char recv_str[310];//to receive msg
	char output[400] , string[300]={'\0'};
	char mod[10] , sendername[10];
	while(1){
	// Receive message from the server and print to screen
		len = recv(serverfd ,  recv_str ,  sizeof(recv_str) , 0);
		if(len==0){
			escape(2);
			break;
		}
		else if(len>=1){
            
            int readed = 0 , ch_cnt = 0;
			sscanf(recv_str , "%s%n" , mod , &readed);

			if(!strcmp(mod , PUBC_MSG)){
				sscanf(recv_str + readed  , "%s %[^\n]" , sendername , string);
				sprintf(output , "%s:%s" , sendername , string);
			}
			else if(!strcmp(mod , PRVT_MSG)){
				sscanf(recv_str + readed  , " %[^\n]" , string);
				sprintf(output , "%s" , string);
			}
			else if(!strcmp(mod , TRYTO_SU)){
                //puts(string);
				sscanf(recv_str + readed  , "%[^\n]" , string);
				if(!strcmp(trim(string) , SU_ST_AC)){
					root=1;
					pointer='#';
					if(curwin==win[4]){
						mvwprintw(win[4] , win[4]->_cury , 2 , "%c" , pointer);
						wrefresh(win[4]);
					}
				}
				else{
					root=0;
				}
				continue;
			}
			else if(strcmp(mod , SYST_MSG)==0){
				sscanf(recv_str + readed , "%[^\n]"  , string);
				sprintf(output , "system:%s" , string);
				puts(output);
				escape(1);
			}
			else if(strcmp(mod , ADD_LIST)==0){
                while(~sscanf(recv_str + readed  , "%s%n" , sendername , &ch_cnt))
				    readed += ch_cnt , memberctrl(ADD_LIST , sendername);
                //usleep(10000);
                //sleep(1);
				continue;
			}
			else if(strcmp(mod , RMV_LIST)==0){
				sscanf(recv_str + readed  , "%s" , sendername);
				memberctrl(RMV_LIST , sendername);
                //usleep(10000);
				continue;
			}
			if(win[0]->_curx==0){
				mvwaddstr(win[0] , localine , 0 , output);
			}
			else{
				mvwaddstr(win[0] , localine , 0 , output);
			}
			if(curline==300){
				for(i=0;i<300;i++){
					sprintf(command , "echo \"%s\" >> %s" , history[i] , FILE_ROUTE);
					system(command);
				}
				curline=0;
			}
			strcpy(history[curline] , output);
			for(i=0;i<(strlen(output)/(win[0]->_maxx+1));i++){
				localine++;
				curline++;
				sprintf(history[curline] , "^ scroll up to get complete msg");
			}
			
			if((strlen(output)%(win[0]->_maxx+1))!=0){
				localine++;
				curline++;
				sprintf(history[curline] , "^ scroll up to get complete msg");
			}

			while(localine>LINES-(win[1]->_maxy+1)-(win[3]->_maxy+1)){
				wscrl(win[0] , 1);
				localine--;
				topline++;
			}
            /* WTF ... if don't sleep , the screen will in a mess?! */
            usleep(10000);
			redraw(1);
		}
        else{
        //    puts("err");
        }
	}
	return;
}
void memberctrl(char *mod , char *name){
	int i , j;
	int namelen=strlen(name);
	while(curwin==win[2]){
		sleep(1);
	}
	if(!strcmp(mod , ADD_LIST)){
        for(i=1;i<CLIENTNUM+1;i++)
            if(!strcmp(client[i].name , name))
                    return;
		for(i=1;i<CLIENTNUM+1;i++){
			if(client[i].hold==0){
				client[i].hold=1;
				strcpy(client[i].name , name);
				online++;
				break;
			}
		}
	}
	if(!strcmp(mod , RMV_LIST)){
		for(i=1;i<CLIENTNUM+1;i++){
			if(!strcmp(client[i].name , name)){
		        if(!strcmp(client[i].name , receivername))
                    strcpy(receivername , "All") , redraw(0);
                /*if receiver be removed , point to all*/
                wrefresh(win[0]);
				client[i].name[0]='\0';
				client[i].hold=0;
				online--;
				break;
			}
		}
	}
	j=1;//
	for(i=1;i<CLIENTNUM+1;i++){//i != 0 because 0 is All
		if(client[i].hold==1){
			memberlist[j]=i;
			j++;
		}
	}
	if((j-1)!=online){
		mvwprintw(win[3] , 1 , 0 , "ERR ONLINE NUM j-1=%d online=%d" , j-1 , online);
		wrefresh(win[3]);
	}
	for(i=0;i<CLIENTNUM+1;i++){//clear names on online box
		mvwprintw(win[2] , i , 4 , "         ");
	}
	for(i=onlinetop;i<=online;i++){
		mvwprintw(win[2] , i-onlinetop , 4 , "%s" , omitname(client[memberlist[i]].name));
	}
	redraw(1);
	return;
}
int cnt_host(void){

	struct sockaddr_in dest0;
	/* input IP */

	printf("Input IP\n(ghsot=>ghost.cs.nccu.edu.tw)\n(cherry=>cherry.cs.nccu.edu.tw)\n(enter=>local):");
	fgets(IP , sizeof(IP) , stdin);
	if(IP[0]=='\n'){	
		sprintf(IP , "127.0.0.1");
	}
	if(strcmp(IP , "ghost\n")==0){
		strcpy(IP , "140.119.162.225");
	}
	if(strcmp(IP , "cherry\n")==0){
		strcpy(IP , "140.119.162.51");
	}
	setbuf(stdin , NULL);
	printf("will connect to %s\n" , IP);
	/* input port */	
	printf("Input port(ex:8889):");
	scanf("%d" , &PORT);

	/* create socket */
	serverfd = socket(PF_INET ,  SOCK_STREAM ,  0);

	/* initialize value in dest0 */
	bzero(&dest0 ,  sizeof(dest0));
	dest0.sin_family = PF_INET;
	dest0.sin_port = htons(PORT);
	inet_aton(IP ,  &dest0.sin_addr);

	/* Connecting to server */
	if(connect(serverfd ,  (struct sockaddr*)&dest0 ,  sizeof(dest0))==-1){
		puts("connect()==-1");
		return -1;
	}
	else{
		puts("please wait");
	}
	
	return 0;
}

void initial(void){
	initscr();
	cbreak();
	nonl();
	noecho();
	intrflush(stdscr , FALSE);
	intrflush(win[2] , FALSE);
	keypad(stdscr , TRUE);
	refresh();
	return;
}
void special(int attrs , WINDOW *swin , int y , int x , char *text){
	wattron(swin , attrs);
	mvwprintw(swin , y , x , text);
	wattroff(swin , attrs);
}
void redraw(int mod){
	int w1cy , w1cx;
	int i;

	w1cy=curwin->_cury;
	w1cx=curwin->_curx;

	for(i=0;i<=win[1]->_maxx;i++) // insert box border
		mvwaddch(win[1] , 0 , i , '-');

	curwin->_cury=w1cy;
	curwin->_curx=w1cx;

	for(i=0;i<=win[6]->_maxx;i++)
		mvwaddch(win[6] , 0 , i , '-');
	for(i=0;i<=win[4]->_maxx;i++) //opinion box
		mvwaddch(win[4] , 0 , i , '-');
	for(i=1;i<=win[4]->_maxy;i++)
		mvwaddch(win[4] , i , 0 , '|');

    for(i = 0 ; i < OPTN_NUM ; i++)
        mvwprintw(win[4] , i + 1 , 5 , OPTION[i]);

	for(i=0;i<=win[3]->_maxx;i++)//command line
		mvwaddch(win[3] , 2 , i , '-');

	mvwprintw(win[3] , 1 , 0 , "===CHATROOM===");
	
	if(root==1){
		mvwprintw(win[3] , 0 , 20 , "Administrator");	
		wrefresh(win[3]);
	}

	for(i=0;i<=win[2]->_maxy;i++)//online box
		mvwaddch(win[2] , i , 0 , '|');

	if(onlinetop<(online-(LINES-6))){
		mvwprintw(win[4] , 0 , 12 , "v");
	}
	else{
		mvwprintw(win[4] , 0 , 12 , "-");
	}

	for(i=0;i<win[2]->_maxx;i++)//it's not <= maxx because =maxx it will be scrl
		mvwaddch(win[2] , win[2]->_maxy , i , '-');

	if(mod==0){
		mvwprintw(win[5] , 0 , 0 , "               ");
		special(A_BOLD , win[5] , 0 , 10-strlen(receivername) , " To ");
		special(A_BOLD , win[5] , 0 , 14-strlen(receivername) , receivername);
	}
	mvwprintw(win[3] , 0 , 4 , "Hi!");
	mvwprintw(win[3] , 0 , 7 , "          ");
	special(A_BOLD , win[3] , 0 , 7 , curname);

	touchwin(win[0]);
	wrefresh(win[0]);
	wrefresh(win[2]);
	wrefresh(win[3]);
	touchwin(win[4]);
	wrefresh(win[4]);
	touchwin(win[1]);
	wrefresh(win[1]);
	touchwin(win[5]);
	wrefresh(win[5]);
	touchwin(win[6]);
	wrefresh(win[6]);

	if(mod==1){
		wmove(curwin , curwin->_cury , curwin->_curx);
		wrefresh(curwin);
	}
	
	if(rooting==1){
		touchwin(rootwin);
		wrefresh(rootwin);
	}
	
	return;
}
int terminal(WINDOW *twin , char *str , int n){
	char *ostr ,  ec ,  kc;
	int c ,  oldx ,  remain;
	int prex , prey;
	int i;
	ostr = str;
	ec = erasechar();
	kc = killchar();

	oldx = twin->_curx;
	remain = n - 1;

	while ((c = wgetch(twin)) != ERR && c != '\n' && c != '\r') {

		*str = c;
		touchline(twin ,  twin->_cury ,  1);
		if (c == ec || c == KEY_BACKSPACE || c==263 || c==127 || c==8) {
			*str = '\0';
			if (str != ostr) {
				/* getch() displays the key sequence */
				if(mvwaddch(twin ,  twin->_cury ,  twin->_curx - 1 , ' ')==ERR){
					if(mvwaddch(twin , twin->_cury-1 , twin->_maxx , ' ')==ERR){
					//DEBUG
					}
				}
				if(wmove(twin ,  twin->_cury ,  twin->_curx - 1)==ERR){
					if(wmove(twin , twin->_cury-1 , twin->_maxx)==ERR){
					//DEBUG
					}
				}
				str--;
				*str='\0';
				if (n != -1) {
					/* We're counting chars */
					remain++;
				}
			} 
		} else if (c == kc) {
			*str = '\0';
			if (str != ostr) {
				/* getch() displays the kill character */
//				if(mvwaddch(twin ,  twin->_cury ,  twin->_curx - 1 ,  ' ')==ERR)
//					mvwaddch(twin , twin->_cury-1 , twin->_maxx , ' ');
				/* Clear the characters from screen and str */
				while (str != ostr) {
					if(mvwaddch(twin ,  twin->_cury ,  twin->_curx - 1 , ' ')==ERR){
						if(mvwaddch(twin , twin->_cury-1 , twin->_maxx , ' ')==ERR){
						}
					}
					if(wmove(twin ,  twin->_cury ,  twin->_curx - 1)==ERR){
						if(wmove(twin , twin->_cury-1 , twin->_maxx)==ERR){
						}
					}
					str--;
					if (n != -1)
						/* We're counting chars */
						remain++;
				}
			} else
			/* getch() displays the kill character */
				mvwaddch(twin ,  twin->_cury ,  oldx ,  ' ');
				wmove(twin ,  twin->_cury ,  oldx);
			
		} 
		else if (c == '\t' || c==27){
			prey=twin->_cury;//record text position
			prex=twin->_curx;
			curs_set(0);
			mvwprintw(win[6] , 1 , 1 , " ");
			wrefresh(win[6]);
			selectmod();
			curwin=win[1];
			mvwprintw(win[6] , 1 , 1 , "%c" , pointer);
			wrefresh(win[6]);
			curs_set(1);
			twin->_cury=prey;
			twin->_curx=prex;
			wmove(twin , twin->_cury , twin->_curx);
			wrefresh(win[1]);

		}
		else if(c==14){
			if(pointer=='$'){
				pointer='>';
			}
			else if(pointer=='>'){
				pointer='@';
			}
			else if(pointer=='@'){
				pointer='!';
			}
			else if(pointer=='!'){
				pointer='%';
			}
			else if(pointer=='%'){
				pointer='?';
			}
			else if(pointer=='?'){
				pointer='$';
			}
			mvwprintw(win[6] , 1 , 1 , "%c" , pointer);
			wrefresh(win[6]);

		}
		else if(c==KEY_UP || c==KEY_DOWN || c== KEY_LEFT || c== KEY_UP){
			if(c==KEY_UP){
				if(topline!=0){
					if(strcmp(history[topline] , "^ scroll up to get complete msg")==0){
						mvwprintw(win[0] , 0 , 0 , "                               ");
					}
					wscrl(win[0] , -1);
					mvwprintw(win[0] , 0 , 0 , "%s" , history[topline-1]);
					for(i=0;i<=6;i++){
						if((i!=2)&&(i!=3)){
							touchwin(win[i]);
							wrefresh(win[i]);
						}
					}
					topline--;
					localine++;
				}
			}
			else if(c==KEY_DOWN){
				if(topline<(curline-(LINES-(win[1]->_maxy+1)-(win[3]->_maxy+1)))){
					wscrl(win[0] , 1);
					for(i=0;i<=6;i++){
						if((i!=2)&&(i!=3)){
							touchwin(win[i]);
							wrefresh(win[i]);
						}
					}
					topline++;
					localine--;
				}
			}
		}
		else if(c>=KEY_MIN&&c<= KEY_MAX){}//disable other function key
		else if(c>=32 && c<=126){
			mvwaddch(twin ,  twin->_cury ,  twin->_curx , c);//good job	
			wrefresh(win[1]);
			if (remain) {
				str++;
				remain--;
			} else {
				mvwaddch(twin ,  twin->_cury ,  twin->_curx - 1 ,  ' ');//
				wmove(twin ,  twin->_cury ,  twin->_curx - 1);
			}
		}		
		wrefresh(win[1]);
		touchwin(win[5]);
		wrefresh(win[5]);
		if(wmove(twin , twin->_cury , twin->_curx)==ERR)
			wmove(twin , twin->_cury-1 , twin->_maxx);
	}

	if (c == ERR) {
		*str = '\0';
		return (ERR);
	}
    *str = '\0';
    wrefresh(win[0]);
    return (OK);
}
void membermod(char *name){
	int input;
	static int cursor=0;
	curwin=win[2];
    if(client[memberlist[cursor]].name[0] == '\0')
        cursor--;/*avoid the user being removed ,  s.t. point nothing */
	mvwprintw(win[2] , cursor-onlinetop , 2 , "%c" , pointer);
	special(A_REVERSE , win[2] , cursor-onlinetop , 4 , omitname(client[memberlist[cursor]].name));
	wrefresh(win[2]);
	while(1){
		input=getch();
		mvwprintw(win[2] , cursor-onlinetop , 2 , " ");
		if(input=='\r' || input=='\n'){
			strcpy(name , client[memberlist[cursor]].name);
			mvwprintw(win[5] , 0 , 0 , "          ");
			special(A_BOLD , win[5] , 0 , 10-strlen(name) , " To ");
			special(A_BOLD , win[5] , 0 , 14-strlen(name) , name);
			wrefresh(win[5]);
			mvwprintw(win[2] , cursor-onlinetop , 4 , "%s" , omitname(client[memberlist[cursor]].name));
			break;
		}
		else{
			mvwprintw(win[2] , cursor-onlinetop , 4 , "%s" , omitname(client[memberlist[cursor]].name));
			if(input==KEY_UP){	
				if(cursor>0){
					cursor--;
				}
			}
			else if(input==KEY_DOWN){
				if(cursor<online){
					cursor++;
				}
			}
			if(onlinetop>cursor){
				onlinetop--;
				wscrl(win[2] , -1);
				mvwprintw(win[2] , cursor-onlinetop , 0 , "|");
				//first line will be space but it
				//will be drawn by below codes
			}
			if(onlinetop<(cursor-(LINES-6))){
				onlinetop++;
				wscrl(win[2] , 1);
			}
			special(A_REVERSE , win[2] , cursor-onlinetop , 4 , omitname(client[memberlist[cursor]].name));
			mvwprintw(win[2] , cursor-onlinetop , 2 , "%c" , pointer);
		}
		if(onlinetop<(online-(LINES-6))){
			mvwprintw(win[4] , 0 , 12 , "v");
		}
		else{
			mvwprintw(win[4] , 0 , 12 , "-");
		}	
		wrefresh(win[2]);
		touchwin(win[4]);
		wrefresh(win[4]);//wait to check
	}
	wrefresh(win[2]);
	touchwin(win[4]);
	wrefresh(win[4]);//wait to check
	return;
}
void selectmod(void){
	int input;
	static int selecter=1;
	curwin=win[4];
	mvwprintw(win[4] , selecter , 2 , "%c" , pointer);
	wrefresh(win[4]);
	while(1){
		input=getch();
		mvwprintw(win[4] , selecter , 2 , " ");
		wrefresh(win[4]);
		if(input=='\r' || input=='\n'){
			if(selecter==1){
				break;
			}
			else if(selecter==2){
				membermod(receivername);
				curwin=win[4];
			}
			else if(selecter==3){
				storehistory();
			}
			else if(selecter==4){
				escape(0);
			}
		}
		else{
			if(input==KEY_UP){
				if(selecter>1){
					selecter--;
				}
			}
			else if(input==KEY_DOWN){
				if(selecter<4){
					selecter++;
				}
			}
			else if(input==12){
				rootmod();
				curwin=win[4];
			}
		}
		mvwprintw(win[4] , selecter , 2 , "%c" , pointer);
		wrefresh(win[4]);	
	}
	return;
}
void rootmod(void){
	int selecter=1;
	char pw[20];
	char output[25];
	char kickname[20];
	int rootinput;
	static int hide=0;
	static int hideroot=1;
	static int rootID=0;
	curwin=rootwin;
	rooting=1;
	wclear(rootwin);
	box(rootwin , 'I' , '=');
	mvwaddstr(rootwin , 0 , rootwin->_maxx/3 , "root mod");
	if(root==0){
		mvwaddstr(rootwin , 1 , 4 , " Enter to Login:");
		wrefresh(rootwin);
	}
	else if(root==1){
		mvwprintw(rootwin , 1 , 5 , "Change ID as %s" , strcmp(curname , myname)==0?"root":myname);
		if(hide==0)
			mvwprintw(rootwin , 2 , 5 , "Hide my ID");
		if(hide==1)
			mvwprintw(rootwin , 2 , 5 , "Show my ID");
		if(hideroot==0)
			mvwprintw(rootwin , 3 , 5 , "Hide root ID");
		if(hideroot==1)
			mvwprintw(rootwin , 3 , 5 , "Show root ID");
		mvwprintw(rootwin , 4 , 5 , "Kick member");
		mvwprintw(rootwin , 5 , 5 , "Shutdown server");
		mvwprintw(rootwin , 6 , 5 , "Logout root");
		mvwprintw(rootwin , 7 , 5 , "Exit");
		mvwprintw(rootwin , 8 , 9 , "Cur_ID:%s" , curname);
		wrefresh(rootwin);
	}
	wrefresh(rootwin);
	if(root==0){//login
		rootinput=getch();
		if(rootinput=='\n' || rootinput=='\r'){
			curs_set(1);
			echo();
			mvwgetnstr(rootwin , 2 , 4 , pw , 16);
			setbuf(stdin , NULL);//or it will sent last msg
			noecho();			//but i don't know why
			sprintf(output , "%s %s" , TRYTO_SU , pw);
			send(serverfd , output , sizeof(output) , 0);
			curs_set(0);
			mvwprintw(rootwin , rootwin->_maxy-1 , 2 , "Please wait");
			wrefresh(rootwin);
			sleep(1);
		}
	}
	else if(root==1){
		mvwprintw(rootwin , selecter , 2 , "%c" , pointer);
		touchwin(rootwin);
		wrefresh(rootwin);
		while(1){
			rootinput=getch();
			mvwprintw(rootwin , selecter , 2 , " ");
			wrefresh(rootwin);
			if(rootinput=='\n' || rootinput=='\r'){
				if(selecter==1){//change ID as root
					if(rootID==0){
						rootID=1-rootID;
						strcpy(curname , "root");
						send(serverfd , "change root" , sizeof("change root") , 0);
					}
					else if(rootID==1){
						rootID=1-rootID;
						strcpy(curname , myname);
						send(serverfd , "change old" , sizeof("change old") , 0);
					}
					mvwprintw(rootwin , 1 , 5 , "Change ID as         ");//%*c" , sizeof(myname) , ' ');
					mvwprintw(rootwin , 8 , 9 , "Cur_ID:         ");//" , sizeof(changename) , ' ');
					mvwprintw(rootwin , 1 , 5 , "Change ID as %s" , strcmp(curname , myname)==0?"root":myname);
					mvwprintw(rootwin , 8 , 9 , "Cur_ID:%s" , curname);	
					mvwprintw(win[3] , 0 , 4 , "Hi!");
					mvwprintw(win[3] , 0 , 7 , "          ");
					special(A_BOLD , win[3] , 0 , 7 , curname);
					wrefresh(win[3]);
					wrefresh(rootwin);
				}
				else if(selecter==2){
					if(hide==0){
						hide=1-hide;
						send(serverfd , HIDE_SLF , sizeof(HIDE_SLF) , 0);
						mvwprintw(rootwin , 2 , 5 , "Show my ID");
					}
					else if(hide==1){
						hide=1-hide;
						send(serverfd , U_HD_SLF , sizeof(U_HD_SLF) , 0);
						mvwprintw(rootwin , 2 , 5 , "Hide my ID");
					}
				}
				else if(selecter==3){
					if(hideroot==0){
						hideroot=1-hideroot;	
						send(serverfd , HIDE_ROT , sizeof(HIDE_ROT) , 0);
						mvwprintw(rootwin , 3 , 5 , "Show root ID");
					}
					else if(hideroot==1){
						hideroot=1-hideroot;
						send(serverfd , U_HD_ROT , sizeof(U_HD_ROT) , 0);
						mvwprintw(rootwin , 3 , 5 , "Hide root ID");
						touchwin(rootwin);
						wrefresh(rootwin);
					}

				}
				else if(selecter==4){
					membermod(kickname);
					curwin=rootwin;
					sprintf(output , "%s %s" , KICK_MAN , kickname);
					send(serverfd , output , sizeof(output) , 0);
				}
				else if(selecter==5){
					send(serverfd , SHUTDOWN , sizeof(SHUTDOWN) , 0);
				}
				else if(selecter==6){//logout
					if(rootID==1){
						rootID=0;
						strcpy(curname , myname);
					}
					if(hide==1){
						hide=0;
						send(serverfd , U_HD_SLF , sizeof(U_HD_SLF) , 0);
					}
					if(hideroot==0){
						hideroot=1;
						send(serverfd , HIDE_ROT , sizeof(HIDE_ROT) , 0);
					}	
					pointer='$';
					root=0;
					mvwprintw(win[3] , 0 , 20 , "             ");
					break;
				}
				else if(selecter==7){//exit rootmod
					break;
				}
			}
			else{
				if(rootinput==KEY_UP){
					if(selecter>1){
						selecter--;
					}
				}
				else if(rootinput==KEY_DOWN){
					if(selecter<7){
						selecter++;
					}
				}
			}
			mvwprintw(rootwin , selecter , 2 , "%c" , pointer);
			touchwin(rootwin);
			wrefresh(rootwin);
		}
	}
	rooting=0;
	redraw(0);
	return;
}
void storehistory(void){
	int i;
	for(i=0;i<curline;i++){
		sprintf(command , "echo \"%s\" >> %s" , history[i] , FILE_ROUTE);
		system(command);
	}
	curline=0;
	return;
}
char* omitname(char *str){
	if(strlen(str)>8){
		sprintf(omitstr , "%.5s..." , str);
	}
	else{
		sprintf(omitstr , "%.8s" , str);
	}
	return omitstr;
}
char *trim(char *str){
    int i = strlen(str) - 1;
    while(str[i] == ' ') str[i--] = 0;
    i = 0;
    while(str[0] == ' ') str++;
    return str;
}
