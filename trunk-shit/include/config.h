// defaults
// file/dir sorting: 1 = name, 2 = size, 3 = date

#define SLEEP_AWAY 60

#define DEFAULT_USERNAME "<enter your default ftp username here>"
#define DEFAULT_PASSWORD ""
#define DEFAULT_STARTDIR "/"
#define DEFAULT_EXCLUDE_SOURCE "*.bad *[* .* *.diz *missing *approve* *NUKED* imdb.nfo *incomplete*"
#define DEFAULT_EXCLUDE_TARGET "*-CRAPGROUP"
#define DEFAULT_NOOP_CMD "STAT -la"
#define DEFAULT_FIRST_CMD "SITE IDLE 180"
        
#define DEFAULT_REFRESH_RATE 20
#define DEFAULT_NOOP_RATE 60
#define DEFAULT_RETRY_DELAY 60
#define DEFAULT_FILE_SORTING 2
#define DEFAULT_DIR_SORTING 3
#define DEFAULT_RETRY_COUNTER 10
        
#define DEFAULT_USE_NOOP FALSE
#define DEFAULT_USE_JUMP TRUE
#define DEFAULT_USE_TRACK TRUE
#define DEFAULT_USE_SECTIONS TRUE
#define DEFAULT_USE_REFRESH FALSE
#define DEFAULT_USE_STEALTH_MODE TRUE
        
#define DEFAULT_USE_AUTOLOGIN FALSE
#define DEFAULT_USE_SSL TRUE
#define DEFAULT_USE_SSL_LIST TRUE
#define DEFAULT_USE_SSL_DATA TRUE
#define DEFAULT_USE_EXCLUDE_SOURCE TRUE //exclude from copy/detect 
#define DEFAULT_USE_EXCLUDE_TARGET FALSE //exclude from changing
#define DEFAULT_USE_CHAINING FALSE
#define DEFAULT_USE_STARTDIR FALSE
#define DEFAULT_USE_PASV TRUE
#define DEFAULT_USE_RETRY FALSE
#define DEFAULT_USE_FIRSTCMD FALSE
#define DEFAULT_USE_RNDREFR FALSE 

// new stuff
#define SHOW_DUPES TRUE //if FALSE, E_MSG_BAD_STOR, ie "file rejected (dupe?)", wont show in the status line

#define REPEAT_MAX 20
#define REPEAT_SLEEP 1

#define SAFE_MAGIC TRUE

// here comes the config stuff for the udp listener inside the program
#define IP "0"                  		       // if u want the program to listen on all ips available, use "0" 
#define PORT 12345                		       // the udp port the program will listen on
#define PASS "urpasshere"            		       // authentication password.
#define BSIZE 1024

//here is the theft protection for pftp program within shell
//this makes it so it can only be run from that folder. Should be more advanced
//it does give a nice error when u run it from a diff folder
#define WORKINGPATH "/home/<username>"
#define USEWORKINGPATH FALSE
