#ifndef __DEFINES_H
#define __DEFINES_H

#define DEBUG 1

#define PFTP_EDITION   "shit.v1.12"

#ifdef TLS
#define PFTP_VERSION   "v0.11.4@TLS-mew6"
#else
#define PFTP_VERSION   "v0.11.4-mew6"
#endif

#define PATHLEN         4096

#define BOOKMARK_ID     "II@0.11.2mew6+2\n"

#define KEYBOARD_DELAY      1 // tenths of seconds for curses keyboard-input wait
#define WELCOME_SLEEP       1 // (x+1) * KEYBOARD_DELAY for splash screen
#define DISPLAY_MSG_SLEEP   50000 // usecs displayhandler sleeps when no msg available
#define SPEED_UPDATE_SLEEP  40 // x * DISPLAY_MSG_SLEEP to wait for a new speed update force
#define ACTION_MSG_SLEEP    50000 // usecs when server thread has no msg
#define SLEEPS_NEEDED       20 // number of ACTION_MSG_SLEEP's needed until one second is gone

#define SERVER_DIR_CACHE    10

#define STYLE_NORMAL         1
#define STYLE_INVERSE        2
#define STYLE_MARKED         3
#define STYLE_MARKED_INVERSE 4
#define STYLE_WHITE          5
#define STYLE_MARKED_STATUS  6
#define STYLE_EXCLUDED       7
#define STYLE_EXCLUDED_INVERSE 8
#define STYLE_REPEATED		 9

#define VIEW_BUFFER_MAX   512

//01234567890123456789012345678901234567890123456789012345678901234567890123456789
#define CONTROL_PANEL_1      " [     ::     ]  [     ::     ]  [     ::     ]  [     ::     ]  [     ::     ]"
#define CONTROL_PANEL_1_KEYS "1.............. 2.............. 3.............. 4.............. 5.............."
#define CONTROL_PANEL_2      " [     ::     ]  [     ::     ]  [     ::     ]  [     ::     ]     mode:      "
#define CONTROL_PANEL_2_KEYS "6.............. 7.............. 8.............. 9.............. ..............."
#define	CONTROL_PANEL_3			" [     ::     ]  [     ::     ]  [     ::     ]  [     ::     ]  [     ::     ]"
#define	CONTROL_PANEL_3_KEYS	"!.............. @.............. #.............. $.............. %.............."
#define	CONTROL_PANEL_4			" [     ::     ]  [     ::     ]  [     ::     ]  [     ::     ]  [     ::     ]"
#define	CONTROL_PANEL_4_KEYS	"^.............. &.............. *.............. (.............. ).............."

#define MODE_FTP_NOCHAIN    1
#define MODE_FTP_CHAIN      2
#define MODE_FXP_NOCHAIN    3
#define MODE_FXP_CHAIN      4

#define STATUS_LOG  200
#define LOG_LINES   500
#define JUMP_STACK  5

#define MSG_KEY_EXTENDED    1
#define MSG_KEY_ESC         2
#define MSG_KEY_BACKSPACE   3
#define MSG_KEY_TAB         4
#define MSG_KEY_LEFT        5
#define MSG_KEY_RIGHT       6
#define MSG_KEY_RETURN      8
#define MSG_KEY_STATUS_UP   9
#define MSG_KEY_STATUS_DOWN 10
#define MSG_KEY_UP          11
#define MSG_KEY_DOWN        12
#define MSG_KEY_PGUP        13
#define MSG_KEY_PGDN        14
#define MSG_KEY_DELLINE     15
#define MSG_KEY_ESCAPE      16
#define MSG_KEY_HOME        17
#define MSG_KEY_END         18

#define MSG_DISPLAY_NOBOOKMRK  1000
#define MSG_DISPLAY_PASSWORD   1001
#define MSG_DISPLAY_WELCOME    1002
#define MSG_DISPLAY_CHPASSWORD 1003

#define DISPLAY_STATE_NORMAL            1
#define DISPLAY_STATE_OPENSITE          2
#define DISPLAY_STATE_NOBKMRK           3
#define DISPLAY_STATE_PASSWORD          4
#define DISPLAY_STATE_NOPASSWD          5
#define DISPLAY_STATE_BADPASS           6
#define DISPLAY_STATE_PREFS             7
#define DISPLAY_STATE_PREFSINPUT        8
#define DISPLAY_STATE_OS_NOTICE         9
#define DISPLAY_STATE_OPENSITE_ADD      10
#define DISPLAY_STATE_OPENSITE_MODIFY   11
#define DISPLAY_STATE_LOG               12
#define DISPLAY_STATE_SWITCH            13
#define DISPLAY_STATE_NOTICE            14
#define DISPLAY_STATE_INPUT             15
#define DISPLAY_STATE_WELCOME           16
#define DISPLAY_STATE_XSITE             17
#define DISPLAY_STATE_VIEW              18
#define DISPLAY_STATE_CHPASSWORD        19
#define DISPLAY_STATE_CHNOPASSWD        20
#define DISPLAY_STATE_CHBADPASS         21
#define DISPLAY_STATE_PRE		777

#define INPUT_DO_MKD            100
#define INPUT_DO_CWD            101
#define INPUT_DO_RENAME         102
#define INPUT_DO_NUKE           103
#define INPUT_DO_UNNUKE         104
#define INPUT_DO_WIPE           105
#define INPUT_DO_UNDUPE         106
#define INPUT_DO_RENAME_CHAINED 107

#define SERVER_TYPE_LOCAL   1
#define SERVER_TYPE_REMOTE  2

#define SERVER_MAGIC_START  1

#define SERVER_WORKINGDIR_SIZE 2048

#define SERVER_MSG_NEW_CWD      1
#define SERVER_MSG_NEW_FILELIST 2
#define SERVER_MSG_KILLME       3
#define SERVER_MSG_DEMARK       4
#define SERVER_MSG_BUSY         5
#define SERVER_MSG_IDLE         6
#define SERVER_MSG_TIME         7
#define SERVER_MSG_NOTICE_VIEW  8
#define SERVER_MSG_NEWLABEL     9
#define SERVER_MSG_MARK         10

#define SERVER_MSG_NOTICE_UPLOAD_SINGLE         20
#define SERVER_MSG_NOTICE_UPLOAD_SINGLE_AS_OK   21
#define SERVER_MSG_NOTICE_UPLOAD_MULTI          22

#define SERVER_MSG_NOTICE_LEECH_SINGLE_MKD      30
#define SERVER_MSG_NOTICE_LEECH_MULTI_MKD       31
#define SERVER_MSG_NOTICE_LEECH_SINGLE_CWD      32
#define SERVER_MSG_NOTICE_LEECH_MULTI_CWD       33

#define SERVER_MSG_URGENT_OKAY      40
#define SERVER_MSG_URGENT_ERROR     41

#define SERVER_MSG_EMIT_REFRESH_SINGLE   50
#define SERVER_MSG_EMIT_REFRESH_MULTI    51

#define FOR_SERVER_MSG_CWD                  1
#define FOR_SERVER_MSG_CLOSE                2
#define FOR_SERVER_MSG_CWD_UP               3
#define FOR_SERVER_MSG_MKD                  4
#define FOR_SERVER_MSG_REFRESH              7
#define FOR_SERVER_MSG_UP_NOK               8
#define FOR_SERVER_MSG_DELFILE              9
#define FOR_SERVER_MSG_DELDIR               10
#define FOR_SERVER_MSG_RENFROM              11
#define FOR_SERVER_MSG_RENTO                12
#define FOR_SERVER_MSG_SITE                 13
#define FOR_SERVER_MSG_PREP                 14
#define FOR_SERVER_MSG_PREP_IN              15
#define FOR_SERVER_MSG_SECTIONS             16
#define FOR_SERVER_MSG_RESET_TIMER          17
#define FOR_SERVER_MSG_CHANGE_FILE_SORTING  18
#define FOR_SERVER_MSG_VIEWFILE             19
#define FOR_SERVER_MSG_CHANGE_DIR_SORTING   28
#define FOR_SERVER_MSG_CHANGE_FILE_REVERSE  90
#define FOR_SERVER_MSG_CHANGE_DIR_REVERSE   91

#define FOR_SERVER_MSG_UPLOAD_NO_WAIT               20
#define FOR_SERVER_MSG_UPLOAD_NO_WAIT_AS_OK         21
#define FOR_SERVER_MSG_LEECH_NO_NOTICE              22
#define FOR_SERVER_MSG_LEECH_NOTICE_SINGLE          23
#define FOR_SERVER_MSG_LEECH_NOTICE_SINGLE_AS_OK    24
#define FOR_SERVER_MSG_LEECH_NOTICE_MULTI           25
#define FOR_SERVER_MSG_UPLOAD                       26
#define FOR_SERVER_MSG_UPLOAD_AS_OK                 27

#define FOR_SERVER_MSG_UPLOAD_DIR_NO_WAIT               30
#define FOR_SERVER_MSG_UPLOAD_DIR_NO_WAIT_AS_OK         31
#define FOR_SERVER_MSG_LEECH_DIR_NO_NOTICE              32
#define FOR_SERVER_MSG_LEECH_DIR_NOTICE_SINGLE          33
#define FOR_SERVER_MSG_LEECH_DIR_NOTICE_SINGLE_AS_OK    34
#define FOR_SERVER_MSG_LEECH_DIR_NOTICE_MULTI           35

#define FOR_SERVER_MSG_UPLOAD_CWD   40
#define FOR_SERVER_MSG_URGENT_OKAY  41
#define FOR_SERVER_MSG_URGENT_ERROR 42

#define FOR_SERVER_MSG_EMIT_REFRESH_SINGLE  50
#define FOR_SERVER_MSG_EMIT_REFRESH_MULTI   51

#define FOR_SERVER_MSG_UNDO         60

#define FOR_SERVER_MSG_FXP_DIR_SRC                  70
#define FOR_SERVER_MSG_FXP_FILE_SRC                 71
#define FOR_SERVER_MSG_FXP_FILE_SRC_AS_OK           72
#define FOR_SERVER_MSG_FXP_SRC_PREPARE              73
#define FOR_SERVER_MSG_FXP_DEST_PREPARE             74
#define FOR_SERVER_MSG_FXP_SRC_UNURGENT             75
#define FOR_SERVER_MSG_FXP_DIR_CWD_UP               76
#define FOR_SERVER_MSG_FXP_SRC_PREPARE_AS_OK        77
#define FOR_SERVER_MSG_FXP_SRC_PREPARE_QUIT         78
#define FOR_SERVER_MSG_FXP_SRC_PREPARE_AS_OK_QUIT   79

#define FOR_SERVER_MSG_NUKE     80
#define FOR_SERVER_MSG_UNNUKE   81
#define FOR_SERVER_MSG_WIPE     82
#define FOR_SERVER_MSG_UNDUPE   83

#define FOR_SERVER_MSG_PRE		200
#define FOR_SERVER_MSG_MAGICTRADE	777

#define FOR_SERVER_MSG_NOT_IMPLEMENTED  999


#define NOTICE_TYPE_NO_NOTICE       1
#define NOTICE_TYPE_NOTICE_SINGLE   2
#define NOTICE_TYPE_NOTICE_MULTI    3
#define NOTICE_TYPE_NOTICE_VIEW     4

#define MSG_TYPE_STD        1
#define MSG_TYPE_INPUT      2
#define MSG_TYPE_OLDINPUT   3

#define PASSWORD_SIZE   60

#define BOOKMARK_RC     ".pftp/bookmarks"
#define BOOKMARK_RC_BAK ".pftp/bookmarks.bak"

#define DEFAULT_OKAY  "[    okay    ]"

#define PASS_MAGIC   ",.,-.%$&%2.--,.-jio433-:52.-$:-.1hinui#+43+.5230.9401431.-54,.-fdioru892032"
#define TRUE_MAGIC   "5&$/%&$ .-"
#define FALSE_MAGIC  "-(4gh$%"

#define ALIAS_MAX       40
#define HOSTNAME_MAX    256
#define USERNAME_MAX    256
#define PASSWORD_MAX    256
#define STARTDIR_MAX    256
#define EXCLUDE_MAX     256
#define GAMEUTILDIR_MAX 256
#define SITE_MAX        254
#define INPUT_TEMP_MAX  256

#define SORTING_METHOD_ALPHA 1
#define SORTING_METHOD_SIZE  2
#define SORTING_METHOD_OTHER 3

#define NOTICE_NOBOOKMARK  "no bookmarks found in working-directory"
#define NOTICE_BADPASSWD   "you need to enter at least 6 chars"
#define NOTICE_NOPASSWD    "you need to enter a custom password"
#define NOTICE_PASSNOMATCH "password doesn't match. better luck next time"

#define NOTICE_NO_SITE   "what? you are kiddin, eh?"
#define NOTICE_NO_CLOSE  "you can't close your local filesystem."
#define NOTICE_NO_LOG    "there's no log for this filesystem."
#define NOTICE_NO_PREFS  "there are no (more) prefs for this site available."
#define NOTICE_NO_XSITE  "you can't send SITE CMDs to the local filesystem."

#define DIALOG_ENTERPASS      "enter password"
#define DIALOG_ENTERCHPASS    "enter new custom password (used to encrypt/decrypt):"
#define DIALOG_ENTERCHPASSCON "re-enter new custom password (used to encrypt/decrypt):"
#define DIALOG_ENTERMKD       "MKD - enter directory to create:"
#define DIALOG_ENTERCWD       "CWD - enter directory to change to:"
#define DIALOG_ENTERRENAME    "RENAME - enter new name:"

#define E_NO_ERROR  0

#define E_BAD_WELCOME   1
#define E_BAD_USER      2
#define E_BAD_PASS      3
#define E_BAD_PWD       4
#define E_BAD_MSG       5
#define E_BAD_LOCALFILE 6
#define E_BAD_TYPE      7
#define E_BAD_PORT      8
#define E_BAD_LIST      9
#define E_BAD_NOOP      10
#define E_BAD_MKD       11
#define E_BAD_FILESIZE  12
#define E_BAD_STOR      13
#define E_BAD_RETR      14
#define E_BAD_DELE      15
#define E_BAD_RMD       16
#define E_BAD_RENAME    17
#define E_BAD_SITE      18
#define E_BAD_STEALTH   19
#ifdef TLS
#define E_BAD_AUTH      20
#define E_BAD_TLSCONN   21
#define E_BAD_PROT      22
#define E_BAD_PBSZ      23
#endif
#define E_BAD_NUKE      24
#define E_BAD_UNNUKE    25
#define E_BAD_WIPE      26
#define E_BAD_UNDUPE    27
#define E_BAD_TEMP_FILE 28
#define E_BAD_PRE	50

#define E_CONTROL_RESET         100
#define E_CTRL_SOCKET_CREATE    101
#define E_CTRL_SOCKET_CONNECT   102
#define E_CTRL_ADDRESS_RESOLVE  103
#define E_CTRL_TIMEOUT          104
#define E_SOCKET_BIND           105
#define E_SOCKET_LISTEN         106
#define E_SOCKET_ACCEPT_TIMEOUT 107
#define E_SOCKET_ACCEPT         108
#define E_DATA_TIMEOUT          109
#define E_SOCKET_DATA_CREATE    110
#define E_WRITEFILE_TIMEOUT     111
#define E_WRITEFILE_ERROR       112
#define E_DATA_TCPERR           113

#define E_PASV_FAILED   200
#define E_BAD_PASV      201

#define E_FXP_TIMEOUT   300

#define E_MSG_FXP_TIMEOUT  "waiting for SRC server"

#define E_MSG_PASV_FAILED  "by zipscript - FXP failed"
#define E_MSG_BAD_PASV     "PASV command"

#define E_MSG_BAD_WELCOME   "is reachable but accepts no logins"
#define E_MSG_BAD_USER      "rejected - your username"
#define E_MSG_BAD_PASS      "rejected - your password"
#define E_MSG_BAD_PWD       "failed   - to get current working dir"
#define E_MSG_BAD_MSG       "error    - received wrong ftp response (?)"
#define E_MSG_BAD_LOCALFILE "failed   - to write to local file"
#define E_MSG_BAD_TYPE      "rejected - TYPE command"
#define E_MSG_BAD_PORT      "rejected - PORT command"
#define E_MSG_BAD_LIST      "rejected - LIST command"
#define E_MSG_BAD_NOOP      "rejected - NOOP command"
#define E_MSG_BAD_MKD       "refused  - to create directory"
#define E_MSG_BAD_FILESIZE  "getting filesize"
#define E_MSG_BAD_STOR      "dupe?"
#define E_MSG_BAD_RETR      "getting file"
#define E_MSG_BAD_DELE      "delete file"
#define E_MSG_BAD_RMD       "delete directory"
#define E_MSG_BAD_RENAME    "rename"
#define E_MSG_BAD_SITE      "SITE command"
#define E_MSG_BAD_NUKE      "nukeing"
#define E_MSG_BAD_UNNUKE    "unnuke"
#define E_MSG_BAD_WIPE      "wipeing"
#define E_MSG_BAD_UNDUPE    "undupeing"
#ifdef TLS
#define E_MSG_BAD_AUTH      "warning  - doesnt support AUTH TLS"
#define E_MSG_BAD_TLSCONN   "failed   - switching to TLS connection"
#define E_MSG_BAD_PBSZ      "rejected - PBSZ command"
#define E_MSG_BAD_PROT      "rejected - PROT command"
#endif
#define E_MSG_BAD_STEALTH   "failed   - using stealth mode, critical error"

#define E_MSG_CONTROL_RESET         "closed   - control connection"
#define E_MSG_CTRL_SOCKET_CREATE    "failed   - to create tcp socket for control connection"
#define E_MSG_CTRL_SOCKET_CONNECT   "refused  - connection"
#define E_MSG_CTRL_ADDRESS_RESOLVE  "failed   - to resolve host address"
#define E_MSG_CTRL_TIMEOUT          "timeout  - connection"
#define E_MSG_SOCKET_BIND           "failed   - to bind data socket"
#define E_MSG_SOCKET_LISTEN         "failed   - to listen on data socket"
#define E_MSG_SOCKET_ACCEPT_TIMEOUT "timeout  - occured while trying to accept data connection"
#define E_MSG_SOCKET_ACCEPT         "failed   - accepting data connection"
#define E_MSG_DATA_TIMEOUT          "timeout  - occured while reading/writing on data connection"
#define E_MSG_SOCKET_DATA_CREATE    "failed   - to create tcp socket for data connection"
#define E_MSG_WRITEFILE_TIMEOUT     "waiting for LOCAL file"
#define E_MSG_WRITEFILE_ERROR       "putting file"
#define E_MSG_DATA_TCPERR           "error    - TCP error while getting file"
#define E_MSG_BAD_TEMP_FILE         "failed   - to create temp file"

#define E_DISPLAYHANDLER_INIT       "unable to initialize display handler or too small terminal\n"
#define E_DISPLAYHANDLER_FIREUP     "unable to detach display handler.\n"



// now for the structures used by almost every section

typedef struct _KEYS {
    int function_code;
    int esc_function_code;
};

typedef struct _MSGLIST {
    int msg, extended, function_key;

    struct _MSGLIST *next;
}

MSG_LIST;

typedef struct _SERVERMSGLIST {
    int msg, magic;
    char *data;

    struct _SERVERMSGLIST *next;
}

SERVER_MSG_LIST;

typedef struct _STATUSMSGLIST {
    char *line;
    bool highlight;

    struct _STATUSMSGLIST *next;
}

STATUS_MSG_LIST;

typedef struct _ACTIONMSGLIST {
    int msg, magic;
    char *param;

    struct _ACTIONMSGLIST *next;
}

ACTION_MSG_LIST;

#define NUM_SECTIONS 22
#define NUM_PRECOMMANDS 12

typedef struct _BOOKMARK {
    char *label, *host, *user, *pass, *startdir, *exclude_source, *exclude_target;
    char *section[NUM_SECTIONS], *pre_cmd[NUM_PRECOMMANDS];
    char *site_rules, *site_user, *site_wkup, *noop_cmd, *first_cmd, *nuke_prefix,
        *msg_string;
    int magic, retry_counter, refresh_rate, noop_rate, file_sorting, dir_sorting,
        retry_delay;
    bool use_refresh, use_noop, use_startdir, use_exclude_source,
        use_exclude_target, use_autologin, use_chaining, use_jump, use_track,
        use_sections;
    bool use_pasv, use_ssl, use_ssl_list, use_ssl_data, use_stealth_mode,
        use_retry, use_firstcmd, use_rndrefr;
    bool use_ssl_fxp, use_alt_fxp, use_rev_file, use_rev_dir;

    struct _BOOKMARK *next;
}

BOOKMARK;

typedef struct _CACHELIST {
    char *name;

    struct _CACHELIST *next;
}

CACHELIST;

typedef struct _DIRCACHE {
    char *dirname;
    CACHELIST *cachelist;

    struct _DIRCACHE *next;
}

DIRCACHE;

typedef struct _XDUPELIST {
    char *name;

    struct _XDUPELIST *next;
}

XDUPELIST;

extern void debuglog(char *fmt, ...);

#endif
