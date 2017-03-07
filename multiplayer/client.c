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

#include<stdarg.h>
struct stat route;
void storehistory(void);
char command[250] , send_str[250];//350 error
char fileroute[40];

//var for socket
struct clientinfo {
    int hold;
    char name[10];
} clt[CLIENTNUM+1] = {	[1 ... CLIENTNUM] = { .hold = 0 } , [0] = { .hold = 1 , .name = "All"	} };
/* GNU style struct initialization */
//   v one for "All"
int mbr_ls[ CLIENTNUM + 1 ] = {[ 0 ] = 0};//point client array (sorted)
int online = 0;//the num people on line
char my_id[11];
char cur_id[20];
char recvr_n[11] = { "All" };
int svr_fd;
int clt_fd;
int PORT;
char IP[25];
int ch;
int root = 0;// whether you have root competence
///function for socket
void recv_msg(void);
void escape(int a);
int cnt_host(void);
void redraw(int mod);
void memberctrl(char *mod , char *name);
//var for curses
int rooting = 0;
WINDOW *win[7] ,
       *win_c , /* current win */
       *rbox , /* root win */
       *mbox , /* member list box */
       *bbox , /* banner box */
       *tbox , /* talk box */
       *ibox , /* msg input box */
       *wbox , /* msg to whom displayer */
       *obox , /* option box */
       *pbox ; /* beside input box to print ps */

/*   bbox                    | mbox   */
/*  -------------------------|        */
/*   tbox   +----------+     |        */
/*          |   rbox   |     |        */
/*          +----------+     |        */
/*                           |        */
/*  -------------------------|------  */
/*  pbox| ibox       ________| obox   */
/*      |           |  wbox  |        */


int mbox_t = 0;//the user current mbox top line point
char history[300][300] = {"\0"};
int tbox_t = 0;//for talk box
int curline = 0;//for history
int tbox_c = 0;//for talk bodx
int x , y;
int new = 1;
char ps = '$';//$(normal) >(just good) #(root)
//function for curses
void initial(void);
void mvwWipen(WINDOW *awin , int y , int x , int n);
void mvwAttrw(WINDOW *awin , int y , int x , int attrs , char *format , ... );
int terminal(WINDOW *twin , char *str , int n);//manipulate insert box
void membermod(char *name);//for member win to select
void selectmod(void);//for opinion win to select
void rootmod(void);
//normal function
char *trim(char *);
int combsys(char *cmd , unsigned int cmd_t , char *format , ... );
ssize_t combsend(int fd , char *msg , unsigned int msg_t , char *format , ... );
char* omit_id(char *str);//if name>8char print "name8 , , , "
char omitstr[15];

//start main program
int main(){
    char output[17];
    char stroutput[300];
    signal(SIGINT , escape);
    int i;
    strcpy(my_id , "nobody");
    // input name
    printf("Input your name(less than 10 letters):");
    //fgets(my_id , sizeof(my_id) , stdin);
    scanf("%s" , my_id);
    for(i = 0;i<sizeof(my_id);i++){
        if(my_id[i] == '\n'){
            my_id[i] = '\0';
        }
    }
    strcpy(cur_id , my_id);
    setbuf(stdin , NULL);

    if(cnt_host() == -1){
        printf("port not be used\n");
        escape(1);
    }

    // use thread to listen msg //
    pthread_t id;
    int ret;
    ret = pthread_create(&id , NULL , (void *)recv_msg , NULL);
    if(ret!= 0){
        printf("create pthread error!\n");
        exit(1);
    }

    combsend(svr_fd , send_str , sizeof(send_str) , "name %s" , cur_id);

    // build history data
    if(stat(LOG_DIR , &route) == 0)
        combsys(command , sizeof(command) , "mkdir %s" , LOG_DIR);


    combsys(command , sizeof(command) , "date >> %s" , FILE_ROUTE);
    combsys(command , sizeof(command) , "echo \"connected to %s:%d\" >> %s" , IP , PORT , FILE_ROUTE);

    // start curses terminal
    int hch;//rbox ch for getch
    initial();

/*   bbox                    | mbox                  */
/*  -------------------------|                       */
/*   tbox   +----------+     |                       */
/*          |   rbox   |     |                       */
/*          +----------+     |                       */
/*                           |                       */
/*  -------------------------|------                 */
/*  pbox| ibox       ________| obox                  */
/*      |           |  wbox  | ctrl-l to call rbox   */


    mbox = newwin(CLIENTNUM+2 , 12 , 0 , COLS-13);
    bbox = newwin(3 , COLS-13 , 0 , 0);
    tbox = newwin(300 , COLS-13 , 3 , 0);
    pbox = newwin(5 , 2 , LINES-5 , 0);
    ibox = newwin(5 , COLS-23 , LINES-5 , 2);
    obox = newwin(5 , 20 , LINES-5 , COLS-21);
    wbox = newwin(1 , 15 , LINES-5+(ibox->_maxy) , (ibox->_maxx)-12);

    win[0] = tbox , win[1] = ibox , win[2] = mbox;
    win[3] = bbox , win[4] = obox , win[5] = wbox , win[6] = pbox;

    for(i = 0 ; i < 7 ; i++)
        if(win[i] == NULL) puts("NULL") , exit(1);
    keypad(tbox , TRUE);
    keypad(ibox , TRUE);
    keypad(mbox , TRUE);
    keypad(bbox , TRUE);
    keypad(rbox , TRUE);

    wsetscrreg(tbox , 0 , 299);
    scrollok(tbox , TRUE);// let box can be scrolled
    scrollok(mbox , TRUE);
    idlok(tbox , TRUE);
    leaveok(tbox , TRUE);

    rbox = newwin(10 , COLS/3 , 5 , COLS/3);
    win_c = ibox;
    redraw(0);//need win_c
    wmove(ibox , 1 , 0);
    mvwprintw(pbox , 1 , 0 , "%c" , ps);
    wrefresh(pbox);
    wrefresh(ibox);
    mvwprintw(mbox , 0 , 4 , "All");
    wrefresh(mbox);
    getyx(ibox , y , x);

    char string[300];
    while(1){
        if (wmove(ibox , 1 , 0)  ==  ERR){
            printf("wmove ERR\n");
            escape(0);
        }
        wrefresh(ibox);
        if(terminal(ibox , string , 280) == ERR){
            printf("terminal ERR\n");
            escape(0);
        }
        if(string[0]!= '\0'){
            if(!strcmp(recvr_n , "All"))
                combsend(svr_fd , send_str , sizeof(send_str) , "%s %s" , PUBC_MSG , string);
            else
                combsend(svr_fd , send_str , sizeof(send_str) , "%s %s %s" , PRVT_MSG , recvr_n , string);
        }
        memset(string , 0 , sizeof(string));
        wclear(ibox);
        mvwprintw(pbox , 1 , 0 , "%c" , ps);
        wrefresh(pbox);
        redraw(1);
    }
    return 0;
}

void escape(int a){
    endwin();
    close(svr_fd);
    if(a == 0){
        puts("Exit chatroom");
    }
    else if(a == 1){
        puts("connection break");
    }
    else if(a == 2){
        puts("recvmsg break;");
    }
    exit(0);
    return;
}

void recv_msg(void){
    int len , i;
    char recv_str[310];//to receive msg
    char output[400] , string[300] = {'\0'};
    char mod[10] , sendername[10];
    while(1){
        // Receive message from the server and print to screen
        len = recv(svr_fd , recv_str , sizeof(recv_str) , 0);
        if(len == 0){
            escape(2);
            break;
        }
        else if(len>= 1){

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
                    root = 1;
                    ps = '#';
                    if(win_c == obox){
                        mvwprintw(obox , obox->_cury , 2 , "%c" , ps);
                        wrefresh(obox);
                    }
                }
                else{
                    root = 0;
                }
                continue;
            }
            else if(strcmp(mod , SYST_MSG) == 0){
                sscanf(recv_str + readed , "%[^\n]"  , string);
                sprintf(output , "system:%s" , string);
                puts(output);
                escape(1);
            }
            else if(strcmp(mod , ADD_LIST) == 0){
                while(~sscanf(recv_str + readed  , "%s%n" , sendername , &ch_cnt))
                    readed +=  ch_cnt , memberctrl(ADD_LIST , sendername);
                //usleep(10000);
                //sleep(1);
                continue;
            }
            else if(strcmp(mod , RMV_LIST) == 0){
                sscanf(recv_str + readed  , "%s" , sendername);
                memberctrl(RMV_LIST , sendername);
                //usleep(10000);
                continue;
            }
            if(tbox->_curx == 0){
                mvwaddstr(tbox , tbox_c , 0 , output);
            }
            else{
                mvwaddstr(tbox , tbox_c , 0 , output);
            }
            if(curline == 300){
                for(i = 0;i<300;i++){
                    combsys(command , sizeof(command) , "echo \"%s\" >> %s" , history[i] , FILE_ROUTE);
                }
                curline = 0;
            }
            strcpy(history[curline] , output);
            for(i = 0;i<(strlen(output)/(tbox->_maxx+1));i++){
                tbox_c++;
                curline++;
                sprintf(history[curline] , "^ scroll up to get complete msg");
            }

            if((strlen(output)%(tbox->_maxx+1))!= 0){
                tbox_c++;
                curline++;
                sprintf(history[curline] , "^ scroll up to get complete msg");
            }

            while(tbox_c>LINES-(ibox->_maxy+1)-(bbox->_maxy+1)){
                wscrl(tbox , 1);
                tbox_c--;
                tbox_t++;
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
    int namelen = strlen(name);
    while(win_c == mbox){
        sleep(1);
    }
    if(!strcmp(mod , ADD_LIST)){
        for(i = 1;i<CLIENTNUM+1;i++)
            if(!strcmp(clt[i].name , name))
                return;
        for(i = 1;i<CLIENTNUM+1;i++){
            if(clt[i].hold == 0){
                clt[i].hold = 1;
                strcpy(clt[i].name , name);
                online++;
                break;
            }
        }
    }
    if(!strcmp(mod , RMV_LIST)){
        for(i = 1;i<CLIENTNUM+1;i++){
            if(!strcmp(clt[i].name , name)){
                if(!strcmp(clt[i].name , recvr_n))
                    strcpy(recvr_n , "All") , redraw(0);
                /*if receiver be removed , point to all*/
                wrefresh(tbox);
                clt[i].name[0] = '\0';
                clt[i].hold = 0;
                online--;
                break;
            }
        }
    }
    j = 1;//
    for(i = 1;i<CLIENTNUM+1;i++){//i !=  0 because 0 is All
        if(clt[i].hold == 1){
            mbr_ls[j] = i;
            j++;
        }
    }
    if((j-1)!= online){
        mvwprintw(bbox , 1 , 0 , "ERR ONLINE NUM j-1 = %d online = %d" , j-1 , online);
        wrefresh(bbox);
    }
    for(i = 0;i<CLIENTNUM+1;i++){//clear names on member box
        mvwWipen(mbox , i , 4 , 9);
        //mvwprintw(mbox , i , 4 , "         ");
    }
    for(i = mbox_t;i<= online;i++){
        mvwprintw(mbox , i-mbox_t , 4 , "%s" , omit_id(clt[mbr_ls[i]].name));
    }
    redraw(1);
    return;
}
int cnt_host(void){

    struct sockaddr_in dest0;
    /* input IP */

    printf("Input IP\n(ghsot = >ghost.cs.nccu.edu.tw)\n(cherry = >cherry.cs.nccu.edu.tw)\n(enter = >local):");
    fgets(IP , sizeof(IP) , stdin);
    if(IP[0] == '\n'){
        sprintf(IP , "127.0.0.1");
    }
    if(strcmp(IP , "ghost\n") == 0){
        strcpy(IP , "140.119.162.225");
    }
    if(strcmp(IP , "cherry\n") == 0){
        strcpy(IP , "140.119.162.51");
    }
    setbuf(stdin , NULL);
    printf("will connect to %s\n" , IP);
    /* input port */
    printf("Input port(ex:8889):");
    scanf("%d" , &PORT);

    /* create socket */
    svr_fd = socket(PF_INET , SOCK_STREAM , 0);

    /* initialize value in dest0 */
    bzero(&dest0 , sizeof(dest0));
    dest0.sin_family = PF_INET;
    dest0.sin_port = htons(PORT);
    inet_aton(IP , &dest0.sin_addr);

    /* Connecting to server */
    if(connect(svr_fd , (struct sockaddr*)&dest0 , sizeof(dest0)) == -1){
        puts("connect() == -1");
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
    intrflush(mbox , FALSE);
    keypad(stdscr , TRUE);
    refresh();
    return;
}

void mvwWipen(WINDOW *awin , int y , int x , int n){
    for(int i = x ; i < x + n ; i++)
        mvwaddch(awin , y , i , ' ');
    return;
}

void mvwAttrw(WINDOW *awin , int y , int x , int attrs , char *format , ... ){
    char text[300];
    wattron(awin , attrs);
    va_list arg;
    va_start(arg , format);
    vsnprintf(text , sizeof(text) , format , arg);
    va_end(arg);
    mvwprintw(awin , y , x , text);
    wattroff(awin , attrs);
}

void redraw(int mod){
    int w1cy , w1cx;
    int i;

    w1cy = win_c->_cury;
    w1cx = win_c->_curx;

    for(i = 0;i<= ibox->_maxx;i++) // insert box border
        mvwaddch(ibox , 0 , i , '-');

    win_c->_cury = w1cy;
    win_c->_curx = w1cx;

    for(i = 0;i<= pbox->_maxx;i++)
        mvwaddch(pbox , 0 , i , '-');
    for(i = 0;i<= obox->_maxx;i++) //opinion box
        mvwaddch(obox , 0 , i , '-');
    for(i = 1;i<= obox->_maxy;i++)
        mvwaddch(obox , i , 0 , '|');

    for(i = 0 ; i < OPTN_NUM ; i++)
        mvwprintw(obox , i + 1 , 5 , OPTION[i]);

    for(i = 0;i<= bbox->_maxx;i++)//command line
        mvwaddch(bbox , 2 , i , '-');

    mvwprintw(bbox , 1 , 0 , "===CHATROOM===");

    if(root == 1){
        mvwprintw(bbox , 0 , 20 , "Administrator");
        wrefresh(bbox);
    }

    for(i = 0;i<= mbox->_maxy;i++)//member box
        mvwaddch(mbox , i , 0 , '|');

    if(mbox_t<(online-(LINES-6))){
        mvwprintw(obox , 0 , 12 , "v");
    }
    else{
        mvwprintw(obox , 0 , 12 , "-");
    }

    for(i = 0;i<mbox->_maxx;i++)//it's not <=  maxx because  = maxx it will be scrl
        mvwaddch(mbox , mbox->_maxy , i , '-');

    if(mod == 0){
        mvwWipen(wbox , 0 , 0 , 15);
        //mvwprintw(wbox , 0 , 0 , "               ");
        mvwAttrw(wbox , 0 , 10 - strlen(recvr_n) , A_BOLD , " To %s" , recvr_n);
        //mvwAttrw(A_BOLD , wbox , 0 , 10-strlen(recvr_n) , " To ");
        //mvwAttrw(A_BOLD , wbox , 0 , 14-strlen(recvr_n) , recvr_n);
    }
    mvwprintw(bbox , 0 , 4 , "Hi!");
    mvwWipen(bbox , 0 , 7 , 10);
    //mvwprintw(bbox , 0 , 7 , "          ");
    mvwAttrw(bbox , 0 , 7 , A_BOLD , "%s" , cur_id);
    //mvwAttrw(A_BOLD , bbox , 0 , 7 , cur_id);

    touchwin(tbox);
    wrefresh(tbox);
    wrefresh(mbox);
    wrefresh(bbox);
    touchwin(obox);
    wrefresh(obox);
    touchwin(ibox);
    wrefresh(ibox);
    touchwin(wbox);
    wrefresh(wbox);
    touchwin(pbox);
    wrefresh(pbox);

    if(mod == 1){
        wmove(win_c , win_c->_cury , win_c->_curx);
        wrefresh(win_c);
    }

    if(rooting == 1){
        touchwin(rbox);
        wrefresh(rbox);
    }

    return;
}
int terminal(WINDOW *twin , char *str , int n){
    char *ostr , ec , kc;
    int c , oldx , remain;
    int prex , prey;
    int i;
    ostr = str;
    ec = erasechar();
    kc = killchar();

    oldx = twin->_curx;
    remain = n - 1;

    while ((c = wgetch(twin)) !=  ERR && c !=  '\n' && c !=  '\r') {

        *str = c;
        touchline(twin , twin->_cury , 1);
        if (c  ==  ec || c  ==  KEY_BACKSPACE || c == 263 || c == 127 || c == 8) {
            *str = '\0';
            if (str !=  ostr) {
                /* getch() displays the key sequence */
                if(mvwaddch(twin , twin->_cury , twin->_curx - 1 , ' ') == ERR){
                    if(mvwaddch(twin , twin->_cury-1 , twin->_maxx , ' ') == ERR){
                        //DEBUG
                    }
                }
                if(wmove(twin , twin->_cury , twin->_curx - 1) == ERR){
                    if(wmove(twin , twin->_cury-1 , twin->_maxx) == ERR){
                        //DEBUG
                    }
                }
                str--;
                *str = '\0';
                if (n !=  -1) {
                    /* We're counting chars */
                    remain++;
                }
            }
        } else if (c  ==  kc) {
            *str = '\0';
            if (str !=  ostr) {
                /* getch() displays the kill character */
                //				if(mvwaddch(twin , twin->_cury , twin->_curx - 1 , ' ') == ERR)
                //					mvwaddch(twin , twin->_cury-1 , twin->_maxx , ' ');
                /* Clear the characters from screen and str */
                while (str !=  ostr) {
                    if(mvwaddch(twin , twin->_cury , twin->_curx - 1 , ' ') == ERR){
                        if(mvwaddch(twin , twin->_cury-1 , twin->_maxx , ' ') == ERR){
                        }
                    }
                    if(wmove(twin , twin->_cury , twin->_curx - 1) == ERR){
                        if(wmove(twin , twin->_cury-1 , twin->_maxx) == ERR){
                        }
                    }
                    str--;
                    if (n !=  -1)
                        /* We're counting chars */
                        remain++;
                }
            } else
                /* getch() displays the kill character */
                mvwaddch(twin , twin->_cury , oldx , ' ');
            wmove(twin , twin->_cury , oldx);

        }
        else if (c  ==  '\t' || c == 27){
            prey = twin->_cury;//record text position
            prex = twin->_curx;
            curs_set(0);
            mvwprintw(pbox , 1 , 0 , " ");
            wrefresh(pbox);
            selectmod();
            win_c = ibox;
            mvwprintw(pbox , 1 , 0 , "%c" , ps);
            wrefresh(pbox);
            curs_set(1);
            twin->_cury = prey;
            twin->_curx = prex;
            wmove(twin , twin->_cury , twin->_curx);
            wrefresh(ibox);

        }
        else if(c == 14){
            if(ps == '$'){
                ps = '>';
            }
            else if(ps == '>'){
                ps = '@';
            }
            else if(ps == '@'){
                ps = '!';
            }
            else if(ps == '!'){
                ps = '%';
            }
            else if(ps == '%'){
                ps = '?';
            }
            else if(ps == '?'){
                ps = '$';
            }
            mvwprintw(pbox , 1 , 0 , "%c" , ps);
            wrefresh(pbox);

        }
        else if(c == KEY_UP || c == KEY_DOWN || c ==  KEY_LEFT || c ==  KEY_UP){
            if(c == KEY_UP){
                if(tbox_t!= 0){
                    if(strcmp(history[tbox_t] , "^ scroll up to get complete msg") == 0){
                        mvwWipen(tbox , 0 , 0 , 11);
                        //mvwprintw(tbox , 0 , 0 , "                               ");
                    }
                    wscrl(tbox , -1);
                    mvwprintw(tbox , 0 , 0 , "%s" , history[tbox_t-1]);
                    for(i = 0;i<= 6;i++){
                        if((i!= 2)&&(i!= 3)){
                            touchwin(win[i]);
                            wrefresh(win[i]);
                        }
                    }
                    tbox_t--;
                    tbox_c++;
                }
            }
            else if(c == KEY_DOWN){
                if(tbox_t<(curline-(LINES-(ibox->_maxy+1)-(bbox->_maxy+1)))){
                    wscrl(tbox , 1);
                    for(i = 0;i<= 6;i++){
                        if((i!= 2)&&(i!= 3)){
                            touchwin(win[i]);
                            wrefresh(win[i]);
                        }
                    }
                    tbox_t++;
                    tbox_c--;
                }
            }
        }
        else if(c>= KEY_MIN&&c<=  KEY_MAX){}//disable other function key
        else if(c>= 32 && c<= 126){
            mvwaddch(twin , twin->_cury , twin->_curx , c);//good job
            wrefresh(ibox);
            if (remain) {
                str++;
                remain--;
            } else {
                mvwaddch(twin , twin->_cury , twin->_curx - 1 , ' ');//
                wmove(twin , twin->_cury , twin->_curx - 1);
            }
        }
        wrefresh(ibox);
        touchwin(wbox);
        wrefresh(wbox);
        if(wmove(twin , twin->_cury , twin->_curx) == ERR)
            wmove(twin , twin->_cury-1 , twin->_maxx);
    }

    if (c  ==  ERR) {
        *str = '\0';
        return (ERR);
    }
    *str = '\0';
    wrefresh(tbox);
    return (OK);
}
void membermod(char *name){
    int input;
    static int mbr_c = 0;//cursor for member
    win_c = mbox;
    if(clt[mbr_ls[mbr_c]].name[0]  ==  '\0')
        mbr_c--;/*avoid the user being removed , s.t. point nothing */
    mvwprintw(win_c , mbr_c - mbox_t , 2 , "%c" , ps);
    mvwAttrw(win_c , mbr_c - mbox_t , 4 , A_REVERSE , omit_id(clt[mbr_ls[mbr_c]].name));
    //	mvwAttrw(A_REVERSE , win_c , mbr_c-mbox_t , 4 , omit_id(clt[mbr_ls[mbr_c]].name));
    wrefresh(win_c);
    while(1){
        input = getch();
        mvwprintw(win_c , mbr_c-mbox_t , 2 , " ");
        if(input == '\r' || input == '\n'){
            strcpy(name , clt[mbr_ls[mbr_c]].name);
            mvwWipen(wbox , 0 , 0 , 10);
            //mvwprintw(wbox , 0 , 0 , "          ");
            mvwAttrw(wbox , 0 , 10 - strlen(name) , A_BOLD , " To %s" , name);
            //			mvwAttrw(A_BOLD , wbox , 0 , 10-strlen(name) , " To ");
            //			mvwAttrw(A_BOLD , wbox , 0 , 14-strlen(name) , name);
            wrefresh(wbox);
            mvwprintw(win_c , mbr_c-mbox_t , 4 , "%s" , omit_id(clt[mbr_ls[mbr_c]].name));
            break;
        }
        else{
            mvwprintw(win_c , mbr_c-mbox_t , 4 , "%s" , omit_id(clt[mbr_ls[mbr_c]].name));
            if(input == KEY_UP){
                if(mbr_c>0){
                    mbr_c--;
                }
            }
            else if(input == KEY_DOWN){
                if(mbr_c<online){
                    mbr_c++;
                }
            }
            if(mbox_t>mbr_c){
                mbox_t--;
                wscrl(win_c , -1);
                mvwprintw(win_c , mbr_c-mbox_t , 0 , "|");
                //first line will be space but it
                //will be drawn by below codes
            }
            if(mbox_t<(mbr_c-(LINES-6))){
                mbox_t++;
                wscrl(win_c , 1);
            }
            mvwAttrw(win_c , mbr_c - mbox_t , 4 , A_REVERSE , omit_id(clt[mbr_ls[mbr_c]].name));
            //			mvwAttrw(A_REVERSE , win_c , mbr_c-mbox_t , 4 , omit_id(clt[mbr_ls[mbr_c]].name));
            mvwprintw(win_c , mbr_c-mbox_t , 2 , "%c" , ps);
        }
        if(mbox_t<(online-(LINES-6))){
            mvwprintw(obox , 0 , 12 , "v");
        }
        else{
            mvwprintw(obox , 0 , 12 , "-");
        }
        wrefresh(win_c);
        touchwin(obox);
        wrefresh(obox);//wait to check
    }
    wrefresh(win_c);
    touchwin(obox);
    wrefresh(obox);//wait to check
    return;
}
void selectmod(void){
    int input;
    static int selecter = 1;
    win_c = obox;
    mvwprintw(obox , selecter , 2 , "%c" , ps);
    wrefresh(obox);
    while(1){
        input = getch();
        mvwprintw(obox , selecter , 2 , " ");
        wrefresh(obox);
        if(input == '\r' || input == '\n'){
            if(selecter == 1){
                break;
            }
            else if(selecter == 2){
                membermod(recvr_n);
                win_c = obox;
            }
            else if(selecter == 3){
                storehistory();
            }
            else if(selecter == 4){
                escape(0);
            }
        }
        else{
            if(input == KEY_UP){
                if(selecter>1){
                    selecter--;
                }
            }
            else if(input == KEY_DOWN){
                if(selecter<4){
                    selecter++;
                }
            }
            else if(input == 12){
                rootmod();
                win_c = obox;
            }
        }
        mvwprintw(obox , selecter , 2 , "%c" , ps);
        wrefresh(obox);
    }
    return;
}
void rootmod(void){
    int opt = 1; //option
    char pw[20];
    char output[25];
    char kickname[20];
    int rootinput;
    static int hide = 0;
    static int hideroot = 1;
    static int rootID = 0;
    win_c = rbox;
    rooting = 1;
    wclear(rbox);
    box(rbox , 'I' , '=');
    mvwaddstr(rbox , 0 , rbox->_maxx/3 , "root mod");
    if(root == 0){
        mvwaddstr(rbox , 1 , 4 , " Enter to Login:");
        wrefresh(rbox);
    }
    else if(root == 1){
        mvwprintw(rbox , 1 , 5 , "Change ID as %s" , strcmp(cur_id , my_id) == 0?"root":my_id);
        if(hide == 0)
            mvwprintw(rbox , 2 , 5 , "Hide my ID");
        if(hide == 1)
            mvwprintw(rbox , 2 , 5 , "Show my ID");
        if(hideroot == 0)
            mvwprintw(rbox , 3 , 5 , "Hide root ID");
        if(hideroot == 1)
            mvwprintw(rbox , 3 , 5 , "Show root ID");
        mvwprintw(rbox , 4 , 5 , "Kick member");
        mvwprintw(rbox , 5 , 5 , "Shutdown server");
        mvwprintw(rbox , 6 , 5 , "Logout root");
        mvwprintw(rbox , 7 , 5 , "Exit");
        mvwprintw(rbox , 8 , 9 , "Cur_ID:%s" , cur_id);
        wrefresh(rbox);
    }
    wrefresh(rbox);
    if(root == 0){//login
        rootinput = getch();
        if(rootinput == '\n' || rootinput == '\r'){
            curs_set(1);
            echo();
            mvwgetnstr(rbox , 2 , 4 , pw , 16);
            setbuf(stdin , NULL);//or it will sent last msg
            noecho();			//but i don't know why
            combsend(svr_fd , output , sizeof(output) , "%s %s" , TRYTO_SU , pw);
            curs_set(0);
            mvwprintw(rbox , rbox->_maxy-1 , 2 , "Please wait");
            wrefresh(rbox);
            sleep(1);
        }
    }
    else if(root == 1){
        mvwprintw(rbox , opt , 2 , "%c" , ps);
        touchwin(rbox);
        wrefresh(rbox);
        while(1){
            rootinput = getch();
            mvwprintw(rbox , opt , 2 , " ");
            wrefresh(rbox);
            if(rootinput == '\n' || rootinput == '\r'){
                if(opt == 1){//change ID as root
                    if(rootID == 0){
                        rootID = 1-rootID;
                        strcpy(cur_id , "root");
                        combsend(svr_fd , send_str , sizeof(send_str) , "%s %s" , CH_IDNTY , "root");
                    }
                    else if(rootID == 1){
                        rootID = 1-rootID;
                        strcpy(cur_id , my_id);
                        combsend(svr_fd , send_str , sizeof(send_str) , "%s %s" , CH_IDNTY , "old");
                    }
                    mvwprintw(rbox , 1 , 5 , "Change ID as         ");//%*c" , sizeof(my_id) , ' ');
                    mvwprintw(rbox , 8 , 9 , "Cur_ID:         ");//" , sizeof(changename) , ' ');
                    mvwprintw(rbox , 1 , 5 , "Change ID as %s" , strcmp(cur_id , my_id) == 0?"root":my_id);
                    mvwprintw(rbox , 8 , 9 , "Cur_ID:%s" , cur_id);
                    mvwprintw(bbox , 0 , 4 , "Hi!");
                    mvwWipen(bbox , 0 , 7 , 10);
                    //mvwprintw(bbox , 0 , 7 , "          ");
                    mvwAttrw(bbox , 0 , 7 , A_BOLD , cur_id);
                    //mvwAttrw(A_BOLD , bbox , 0 , 7 , cur_id);
                    wrefresh(bbox);
                    wrefresh(rbox);
                }
                else if(opt == 2){
                    if(hide == 0){
                        hide = 1-hide;
                        send(svr_fd , HIDE_SLF , sizeof(HIDE_SLF) , 0);
                        mvwprintw(rbox , 2 , 5 , "Show my ID");
                    }
                    else if(hide == 1){
                        hide = 1-hide;
                        send(svr_fd , U_HD_SLF , sizeof(U_HD_SLF) , 0);
                        mvwprintw(rbox , 2 , 5 , "Hide my ID");
                    }
                }
                else if(opt == 3){
                    if(hideroot == 0){
                        hideroot = 1-hideroot;
                        send(svr_fd , HIDE_ROT , sizeof(HIDE_ROT) , 0);
                        mvwprintw(rbox , 3 , 5 , "Show root ID");
                    }
                    else if(hideroot == 1){
                        hideroot = 1-hideroot;
                        send(svr_fd , U_HD_ROT , sizeof(U_HD_ROT) , 0);
                        mvwprintw(rbox , 3 , 5 , "Hide root ID");
                        touchwin(rbox);
                        wrefresh(rbox);
                    }

                }
                else if(opt == 4){
                    membermod(kickname);
                    win_c = rbox;
                    combsend(svr_fd , send_str , sizeof(send_str) , "%s %s" , KICK_MAN , kickname);
                }
                else if(opt == 5){
                    send(svr_fd , SHUTDOWN , sizeof(SHUTDOWN) , 0);
                }
                else if(opt == 6){//logout
                    if(rootID == 1){
                        rootID = 0;
                        strcpy(cur_id , my_id);
                    }
                    if(hide == 1){
                        hide = 0;
                        send(svr_fd , U_HD_SLF , sizeof(U_HD_SLF) , 0);
                    }
                    if(hideroot == 0){
                        hideroot = 1;
                        send(svr_fd , HIDE_ROT , sizeof(HIDE_ROT) , 0);
                    }
                    ps = '$';
                    root = 0;
                    mvwWipen(bbox , 0 , 20 , 13);
                    //mvwprintw(bbox , 0 , 20 , "             ");
                    break;
                }
                else if(opt == 7){//exit rootmod
                    break;
                }
            }
            else{
                if(rootinput == KEY_UP){
                    if(opt>1){
                        opt--;
                    }
                }
                else if(rootinput == KEY_DOWN){
                    if(opt<7){
                        opt++;
                    }
                }
            }
            mvwprintw(rbox , opt , 2 , "%c" , ps);
            touchwin(rbox);
            wrefresh(rbox);
        }
    }
    rooting = 0;
    redraw(0);
    return;
}
void storehistory(void){
    int i;
    for(i = 0;i<curline;i++)
        combsys(command , sizeof(command) , "echo \"%s\" >> %s" , history[i] , FILE_ROUTE);
    curline = 0;
    return;
}

int combsys(char *cmd , unsigned int cmd_t , char *format , ... ){
    va_list arg;
    va_start(arg , format);
    vsnprintf(cmd , cmd_t , format , arg);
    va_end(arg);
    system(cmd);
}

ssize_t combsend(int fd , char *msg , unsigned int msg_t , char *format , ... ){
    va_list arg;
    va_start(arg , format);
    vsnprintf(msg , msg_t , format , arg);
    va_end(arg);
    send(fd , msg , msg_t , 0);
    printf("send %s \n" , msg);
}

char *omit_id(char *str){
    if(strlen(str)>8)
        sprintf(omitstr , "%.5s..." , str);
    else
        sprintf(omitstr , "%.8s" , str);
    return omitstr;
}

char *trim(char *str){
    int i = strlen(str) - 1;
    while(str[i]  ==  ' ') str[i--] = 0;
    i = 0;
    while(str[0]  ==  ' ') str++;
    return str;
}
