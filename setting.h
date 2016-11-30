/* for client.c , one for root   */
/* for server.c , one for refuse */
#define CLIENTNUM 21
/* file setting */
#define LOG_DIR "~/chat_history/"
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

/* CLT SETTING */
#define OPTN_NUM 4
const char OPTION[OPTN_NUM][20] = {
    "Write message" ,
    "Choose member" ,
    "Store history" ,
    "Exit chatroom" 
};
