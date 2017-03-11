/* for client.c , one for root   */
/* for server.c , one for refuse */
#define CLIENTNUM 21
/* file setting */
//#define LOG_DIR "~/chat_history/"
#define LOG_DIR "./chat_history/"
/* command below*/
/* BOTH */
#define PUBC_MSG "wall"
#define PRVT_MSG "write"
/* CLT TO SVR */
#define TRYTO_SU "pw"
#define CH_IDNTY "change"
#define HIDE_SLF "hide"
#define U_HD_SLF "unhide"
#define HIDE_ROT "hideroot"
#define U_HD_ROT "unhideroot"
#define KICK_MAN "kick"
#define SHUTDOWN "shutdown"
/* SVR TO CLT */
#define SU_ST_AC "accept"
#define SU_ST_RF "refuse"
#define INI_LIST "all"
#define ADD_LIST "add"
#define RMV_LIST "remove"
#define SYST_MSG "sys"
/* SVR BUILDIN */
#define WALL_MSG PUBC_MSG
#define WRIT_MSG PRVT_MSG
#define KICK_MBR KICK_MAN
#define SHOW_PRT "port"
#define SHOW_PWD "pw"
#define SHOW_FDS "fd"
#define SHOW_MBR "w"
#define SAVE_HIS "save"
#define EXIT_QCK "exit"
#define EXIT_NML "quit"
#define HELP_MAN "help"
#define DIRC_CMD "d"

#define HELP_MENU \
"\
wall  [str]      -> write string for everyone\n\
write [id] [str] -> write string to someone\n\
kick  [id]       -> kick somebody out of the room\n\
w                -> print everyone on line\n\
save             -> save history\n\
d     [cmd]      -> send cmd to everyone directory(if you know the rule)\n\
quit             -> end sub and warning others\n\
exit             -> exit sub right now\
"
/* CLT SETTING */
#define OPTN_NUM 4
const char OPTION[OPTN_NUM][20] = {
    "Write message" ,
    "Choose member" ,
    "Store history" ,
    "Exit chatroom"
};
