#ifndef	__DEFINES_H
#define	__DEFINES_H

#define	PFTP_EDITION		"FOGGY"

#ifdef TLS
#define	PFTP_VERSION		"v-ID$: II@0.11.4@TLS"
#else
#define	PFTP_VERSION		"v-ID$: II@0.11.4"
#endif

#define PATHLEN 4096

#define	BOOKMARK_ID		"II@0.11.0\n"
//#define	BOOKMARK_ID		"II@0.5.0@0\n"

#define KEYBOARD_DELAY          1               // tenths of seconds for curses keyboard-input wait
#define	WELCOME_SLEEP		10		// (x+1) * KEYBOARD_DELAY for splash screen
#define	DISPLAY_MSG_SLEEP	50000		// usecs displayhandler sleeps when no msg available
#define	SPEED_UPDATE_SLEEP	40		// x * DISPLAY_MSG_SLEEP to wait for a new speed update force
#define	ACTION_MSG_SLEEP	50000		// usecs when server thread has no msg
#define	SLEEPS_NEEDED		20		// number of ACTION_MSG_SLEEP's needed until one second is gone

#define	SERVER_DIR_CACHE	10

#define	STYLE_NORMAL		1
#define	STYLE_INVERSE		2
#define	STYLE_MARKED		3
#define	STYLE_MARKED_INVERSE	4
#define	STYLE_WHITE		5
#define STYLE_MARKED_STATUS     6

#define	VIEW_BUFFER_MAX		512

			       //12345678901234567890123456789012345678901234567890123456789012345678901234567890
#define	CONTROL_PANEL_1		" uit     pen     lose    og      witch   cpy.o   rp w p  ormat   sort    kdir  "
#define	CONTROL_PANEL_1_KEYS	"Q...... O...... C...... L...... S...... T.....K P....I. F...... B...... M......"
#define	CONTROL_PANEL_2		" elete   cwd     rename  iew     site    cycle   compar  prefs   tl ame     FTP"
#define	CONTROL_PANEL_2_KEYS	"D...... W...... N...... V...... X...... Y...... A...... E...... U..G... Z......"

#define	MODE_FTP_NOCHAIN	1
#define	MODE_FTP_CHAIN		2
#define	MODE_FXP_NOCHAIN	3
#define	MODE_FXP_CHAIN		4

#define	STATUS_LOG		200
#define	LOG_LINES		500
#define	JUMP_STACK		5

#define	MSG_KEY_EXTENDED		1
#define	MSG_KEY_ESC			2
#define	MSG_KEY_BACKSPACE		3
#define	MSG_KEY_TAB			4
#define	MSG_KEY_LEFT			5
#define	MSG_KEY_RIGHT			6
#define	MSG_KEY_RETURN			8
#define	MSG_KEY_STATUS_UP		9
#define	MSG_KEY_STATUS_DOWN		10
#define	MSG_KEY_UP			11
#define	MSG_KEY_DOWN			12
#define	MSG_KEY_PGUP			13
#define	MSG_KEY_PGDN			14
#define	MSG_KEY_DELLINE                 15
#define	MSG_KEY_ESCAPE                  16
#define	MSG_KEY_HOME                    17
#define	MSG_KEY_END                     18


#define	MSG_DISPLAY_NOBOOKMRK	1000
#define	MSG_DISPLAY_PASSWORD	1001
#define	MSG_DISPLAY_WELCOME	1002
#define	MSG_DISPLAY_CHPASSWORD	1003

#define	DISPLAY_STATE_NORMAL		1
#define	DISPLAY_STATE_OPENSITE		2
#define	DISPLAY_STATE_NOBKMRK		3
#define	DISPLAY_STATE_PASSWORD		4
#define	DISPLAY_STATE_NOPASSWD		5
#define	DISPLAY_STATE_BADPASS		6
#define	DISPLAY_STATE_PREFS		7
#define	DISPLAY_STATE_PREFSINPUT	8
#define	DISPLAY_STATE_OS_NOTICE		9
#define	DISPLAY_STATE_OPENSITE_ADD	10
#define	DISPLAY_STATE_OPENSITE_MODIFY	11
#define	DISPLAY_STATE_LOG		12
#define	DISPLAY_STATE_SWITCH		13
#define	DISPLAY_STATE_NOTICE		14
#define	DISPLAY_STATE_INPUT		15
#define	DISPLAY_STATE_WELCOME		16
#define	DISPLAY_STATE_XSITE		17
#define	DISPLAY_STATE_VIEW		18
#define	DISPLAY_STATE_CHPASSWORD	19
#define	DISPLAY_STATE_CHNOPASSWD	20
#define	DISPLAY_STATE_CHBADPASS		21

#define	INPUT_DO_MKD			100
#define	INPUT_DO_CWD			101
#define	INPUT_DO_RENAME			102
#define INPUT_DO_NUKE			103
#define	INPUT_DO_UNNUKE		        104
#define INPUT_DO_WIPE			105
#define	INPUT_DO_UNDUPE		        106

#define	SERVER_TYPE_LOCAL	1
#define	SERVER_TYPE_REMOTE	2

#define	SERVER_MAGIC_START	1

#define	SERVER_WORKINGDIR_SIZE	2048

#define	SERVER_MSG_NEW_CWD	1
#define	SERVER_MSG_NEW_FILELIST	2
#define	SERVER_MSG_KILLME	3
#define	SERVER_MSG_DEMARK	4
#define	SERVER_MSG_BUSY		5
#define	SERVER_MSG_IDLE		6
#define	SERVER_MSG_TIME		7
#define	SERVER_MSG_NOTICE_VIEW	8
#define	SERVER_MSG_NEWLABEL	9
#define	SERVER_MSG_MARK		10

#define	SERVER_MSG_NOTICE_UPLOAD_SINGLE		20
#define	SERVER_MSG_NOTICE_UPLOAD_SINGLE_AS_OK	21
#define	SERVER_MSG_NOTICE_UPLOAD_MULTI		22

#define	SERVER_MSG_NOTICE_LEECH_SINGLE_MKD	30
#define	SERVER_MSG_NOTICE_LEECH_MULTI_MKD	31
#define	SERVER_MSG_NOTICE_LEECH_SINGLE_CWD	32
#define	SERVER_MSG_NOTICE_LEECH_MULTI_CWD	33

#define	SERVER_MSG_URGENT_OKAY			40
#define	SERVER_MSG_URGENT_ERROR			41

#define	SERVER_MSG_EMIT_REFRESH_SINGLE		50
#define	SERVER_MSG_EMIT_REFRESH_MULTI		51


#define	FOR_SERVER_MSG_CWD	1
#define	FOR_SERVER_MSG_CLOSE	2
#define	FOR_SERVER_MSG_CWD_UP	3
#define	FOR_SERVER_MSG_MKD	4
#define	FOR_SERVER_MSG_REFRESH	7
#define	FOR_SERVER_MSG_UP_NOK	8
#define	FOR_SERVER_MSG_DELFILE	9
#define	FOR_SERVER_MSG_DELDIR	10
#define	FOR_SERVER_MSG_RENFROM	11
#define	FOR_SERVER_MSG_RENTO	12
#define	FOR_SERVER_MSG_SITE	13
#define	FOR_SERVER_MSG_PREP	14
#define	FOR_SERVER_MSG_PREP_IN	15
#define	FOR_SERVER_MSG_UTILGAME	16
#define	FOR_SERVER_MSG_RESET_TIMER	17
#define	FOR_SERVER_MSG_CHANGE_SORTING	18
#define	FOR_SERVER_MSG_VIEWFILE		19

#define	FOR_SERVER_MSG_UPLOAD_NO_WAIT			20
#define	FOR_SERVER_MSG_UPLOAD_NO_WAIT_AS_OK		21
#define	FOR_SERVER_MSG_LEECH_NO_NOTICE			22
#define	FOR_SERVER_MSG_LEECH_NOTICE_SINGLE		23
#define	FOR_SERVER_MSG_LEECH_NOTICE_SINGLE_AS_OK	24
#define	FOR_SERVER_MSG_LEECH_NOTICE_MULTI		25
#define	FOR_SERVER_MSG_UPLOAD				26
#define	FOR_SERVER_MSG_UPLOAD_AS_OK			27

#define	FOR_SERVER_MSG_UPLOAD_DIR_NO_WAIT		30
#define	FOR_SERVER_MSG_UPLOAD_DIR_NO_WAIT_AS_OK		31
#define	FOR_SERVER_MSG_LEECH_DIR_NO_NOTICE		32
#define	FOR_SERVER_MSG_LEECH_DIR_NOTICE_SINGLE		33
#define	FOR_SERVER_MSG_LEECH_DIR_NOTICE_SINGLE_AS_OK	34
#define	FOR_SERVER_MSG_LEECH_DIR_NOTICE_MULTI		35

#define	FOR_SERVER_MSG_UPLOAD_CWD			40
#define	FOR_SERVER_MSG_URGENT_OKAY			41
#define	FOR_SERVER_MSG_URGENT_ERROR			42

#define	FOR_SERVER_MSG_EMIT_REFRESH_SINGLE		50
#define	FOR_SERVER_MSG_EMIT_REFRESH_MULTI		51

#define	FOR_SERVER_MSG_UNDO				60

#define	FOR_SERVER_MSG_FXP_DIR_SRC			70
#define	FOR_SERVER_MSG_FXP_FILE_SRC			71
#define	FOR_SERVER_MSG_FXP_FILE_SRC_AS_OK		72
#define	FOR_SERVER_MSG_FXP_SRC_PREPARE		        73
#define	FOR_SERVER_MSG_FXP_DEST_PREPARE                 74
#define	FOR_SERVER_MSG_FXP_SRC_UNURGENT		        75
#define FOR_SERVER_MSG_FXP_DIR_CWD_UP                   76
#define	FOR_SERVER_MSG_FXP_SRC_PREPARE_AS_OK	        77
#define	FOR_SERVER_MSG_FXP_SRC_PREPARE_QUIT	        78
#define	FOR_SERVER_MSG_FXP_SRC_PREPARE_AS_OK_QUIT       79

#define FOR_SERVER_MSG_NUKE	80
#define	FOR_SERVER_MSG_UNNUKE	81
#define FOR_SERVER_MSG_WIPE	82
#define	FOR_SERVER_MSG_UNDUPE	83

#define	FOR_SERVER_MSG_NOT_IMPLEMENTED			999


#define	NOTICE_TYPE_NO_NOTICE		1
#define	NOTICE_TYPE_NOTICE_SINGLE	2
#define	NOTICE_TYPE_NOTICE_MULTI	3
#define	NOTICE_TYPE_NOTICE_VIEW		4

#define	MSG_TYPE_STD		1
#define	MSG_TYPE_INPUT		2
#define	MSG_TYPE_OLDINPUT	3

#define	PASSWORD_SIZE		60

#define	BOOKMARK_RC		".pftpbookmrks"
#define	BOOKMARK_RC_BAK		".pftpbookmrks.bak"

#define	DEFAULT_OKAY		"[    okay    ]"

#define	PASS_MAGIC		",.,-.%$&%2.--,.-jio433-:52.-$:-.1hinui#+43+.5230.9401431.-54,.-fdioru892032"
#define	TRUE_MAGIC		"5&$/%&$ .-"
#define	FALSE_MAGIC		"-(4gh$%"

#define	ALIAS_MAX		40
#define	HOSTNAME_MAX		1024
#define	USERNAME_MAX		256
#define	PASSWORD_MAX		256
#define	STARTDIR_MAX		256
#define	EXCLUDE_MAX		256
#define	GAMEUTILDIR_MAX		256
#define	SITE_MAX		254
#define	INPUT_TEMP_MAX		256

#define SORTING_METHOD_ALPHA	1
#define SORTING_METHOD_SIZE	2
#define SORTING_METHOD_OTHER	3

#define	NOTICE_NOBOOKMARK	"no bookmarks found in working-directory"
#define	NOTICE_BADPASSWD	"you need to enter at least 6 chars"
#define	NOTICE_NOPASSWD		"you need to enter a custom password"
#define	NOTICE_PASSNOMATCH	"password doesn't match. better luck next time"

#define	NOTICE_NO_SITE		"what? you are kiddin, eh?"
#define	NOTICE_NO_CLOSE		"you can't close your local filesystem."
#define	NOTICE_NO_LOG		"there's no log for this filesystem."
#define	NOTICE_NO_PREFS		"there are no (more) prefs for this site available."
#define	NOTICE_NO_XSITE		"you can't send SITE to the local filesystem, stupid."

#define	DIALOG_ENTERPASS	"enter your custom password (used to encrypt/decrypt):"
#define	DIALOG_ENTERCHPASS	"enter new custom password (used to encrypt/decrypt):"
#define	DIALOG_ENTERCHPASSCON	"re-enter new custom password (used to encrypt/decrypt):"
#define	DIALOG_ENTERMKD		"directory to create:"
#define	DIALOG_ENTERCWD		"directory to change to:"
#define	DIALOG_ENTERRENAME	"enter new name:"

#define	E_NO_ERROR		0

#define	E_BAD_WELCOME		1
#define	E_BAD_USER		2
#define	E_BAD_PASS		3
#define	E_BAD_PWD		4
#define	E_BAD_MSG		5
#define	E_BAD_LOCALFILE		6
#define	E_BAD_TYPE		7
#define	E_BAD_PORT		8
#define	E_BAD_LIST		9
#define	E_BAD_NOOP		10
#define	E_BAD_MKD		11
#define	E_BAD_FILESIZE		12
#define	E_BAD_STOR		13
#define	E_BAD_RETR		14
#define	E_BAD_DELE		15
#define	E_BAD_RMD		16
#define	E_BAD_RENAME		17
#define	E_BAD_SITE		18
#define	E_BAD_STEALTH		19
#ifdef TLS
#define	E_BAD_AUTH		20
#define	E_BAD_TLSCONN		21
#define	E_BAD_PROT		22
#define	E_BAD_PBSZ		23
#endif
#define E_BAD_NUKE	24
#define E_BAD_UNNUKE	25
#define E_BAD_WIPE	26
#define E_BAD_UNDUPE	27
#define E_BAD_TEMP_FILE	28

#define	E_CONTROL_RESET		100
#define	E_CTRL_SOCKET_CREATE	101
#define	E_CTRL_SOCKET_CONNECT	102
#define	E_CTRL_ADDRESS_RESOLVE	103
#define	E_CTRL_TIMEOUT		104
#define	E_SOCKET_BIND		105
#define	E_SOCKET_LISTEN		106
#define	E_SOCKET_ACCEPT_TIMEOUT	107
#define	E_SOCKET_ACCEPT		108
#define	E_DATA_TIMEOUT		109
#define	E_SOCKET_DATA_CREATE	110
#define	E_WRITEFILE_TIMEOUT	111
#define	E_WRITEFILE_ERROR	112
#define	E_DATA_TCPERR		113

#define	E_PASV_FAILED		200
#define	E_BAD_PASV		201

#define	E_FXP_TIMEOUT           300

#define	E_MSG_FXP_TIMEOUT       "Timeout while waiting for SRC server !!!"

#define	E_MSG_PASV_FAILED	"FXP transfer failed, or was rejected by zipscript !!!"
#define	E_MSG_BAD_PASV		"PASV command rejected."

#define	E_MSG_BAD_WELCOME	"server is reachable but accepts no logins."
#define	E_MSG_BAD_USER		"server rejected your username."
#define	E_MSG_BAD_PASS		"server rejected your password."
#define	E_MSG_BAD_PWD		"unable to get current working dir."
#define	E_MSG_BAD_MSG		"received wrong ftp response (but have no idea what that should mean)."
#define	E_MSG_BAD_LOCALFILE	"unable to write to local file."
#define	E_MSG_BAD_TYPE		"TYPE command rejected."
#define	E_MSG_BAD_PORT		"PORT command rejected."
#define	E_MSG_BAD_LIST		"LIST command rejected."
#define	E_MSG_BAD_NOOP		"NOOP command rejected."
#define	E_MSG_BAD_MKD		"server refused to create directory."
#define	E_MSG_BAD_FILESIZE	"couldn't determine size of file to leech, sorry."
#define	E_MSG_BAD_STOR		"file seems to be a dupe (server rejected filename)."
#define	E_MSG_BAD_RETR		"unable to retrieve file (server didn't permit to leech it)."
#define	E_MSG_BAD_DELE		"server refused to delete file."
#define	E_MSG_BAD_RMD		"server refused to delete dir."
#define	E_MSG_BAD_RENAME	"server refused to rename."
#define	E_MSG_BAD_SITE		"server rejected SITE command."
#define E_MSG_BAD_NUKE		"server refused to nuke."
#define	E_MSG_BAD_UNNUKE	"server refused to unnuke."
#define E_MSG_BAD_WIPE		"server refused to wipe."
#define	E_MSG_BAD_UNDUPE	"server refused to undupe."
#ifdef TLS
#define	E_MSG_BAD_AUTH		"server doesnt support AUTH TLS."
#define	E_MSG_BAD_TLSCONN	"error while switching to TLS connection."
#define	E_MSG_BAD_PBSZ		"server rejected PBSZ command."
#define	E_MSG_BAD_PROT		"server rejected PROT command."
#endif
#define	E_MSG_BAD_STEALTH	"stealth mode failed, critical error."

#define	E_MSG_CONTROL_RESET		"server closed control connection."
#define	E_MSG_CTRL_SOCKET_CREATE	"unable to create tcp socket for control connection."
#define	E_MSG_CTRL_SOCKET_CONNECT	"server refused connection."
#define	E_MSG_CTRL_ADDRESS_RESOLVE	"unable to resolve host address."
#define	E_MSG_CTRL_TIMEOUT		"control connection timed out."
#define	E_MSG_SOCKET_BIND		"unable to bind data socket."
#define	E_MSG_SOCKET_LISTEN		"unable to listen on data socket."
#define	E_MSG_SOCKET_ACCEPT_TIMEOUT	"timeout occured while trying to accept data connection."
#define	E_MSG_SOCKET_ACCEPT		"error accepting data connection."
#define	E_MSG_DATA_TIMEOUT		"timeout occured while reading/writing on data connection."
#define	E_MSG_SOCKET_DATA_CREATE	"unable to create tcp socket for data connection."
#define	E_MSG_WRITEFILE_TIMEOUT		"timeout while waiting for local file to upload."
#define E_MSG_WRITEFILE_ERROR		"error uploading file (server closed data connection)."
#define	E_MSG_DATA_TCPERR		"TCP error while leeching."
#define	E_MSG_BAD_TEMP_FILE 		"Unable to create temp file."

#define	E_DISPLAYHANDLER_INIT	"unable to initialize display handler or too small terminal\n"
#define	E_DISPLAYHANDLER_FIREUP	"unable to detach display handler.\n"


// now for the structures used by almost every section

typedef struct _MSGLIST
{
	int		msg, extended;
	struct _MSGLIST	*next;
} MSG_LIST;

typedef struct _SERVERMSGLIST
{
	int			msg, magic;
	char			*data;
	struct _SERVERMSGLIST	*next;
} SERVER_MSG_LIST;

typedef struct _STATUSMSGLIST
{
	char	*line;
	bool	highlight;
	struct _STATUSMSGLIST	*next;
} STATUS_MSG_LIST;

typedef struct _ACTIONMSGLIST
{
	int	msg, magic;
	char	*param;
	struct _ACTIONMSGLIST	*next;
} ACTION_MSG_LIST;

typedef struct _BOOKMARK
{
	char			*label, *host, *user, *pass, *startdir, *exclude, *util_dir, *game_dir, *site_who, *site_user, *site_wkup, *noop_cmd, *first_cmd;
	int			magic, retry_counter, refresh_rate, noop_rate, sorting, retry_delay;
	bool			use_refresh, use_noop, use_startdir, use_exclude, use_autologin, use_chaining, use_jump, use_track, use_utilgames;
	bool			use_pasv, use_ssl, use_ssl_list, use_ssl_data, use_stealth_mode, use_retry, use_firstcmd, use_rndrefr;
	struct _BOOKMARK	*next;
} BOOKMARK;

typedef struct _CACHELIST
{
	char			*name;
	struct _CACHELIST	*next;
} CACHELIST;

typedef struct _DIRCACHE
{
	char			*dirname;
	CACHELIST		*cachelist;
	struct _DIRCACHE	*next;
} DIRCACHE;

typedef struct _XDUPELIST
{
	char			*name;
	struct _XDUPELIST	*next;
} XDUPELIST;

extern void debuglog(char *fmt, ...);

#endif
