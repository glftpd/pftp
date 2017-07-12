#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <panel.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pwd.h>

#include "config.h"

#include "defines.h"
#include "tcp.h"
#include "server.h"
#include "displayhandler.h"
#include "keyhandler.h"
#include <sys/stat.h>

#ifdef TLS
#include "tlsutil.h"
#endif

CDisplayHandler *display = NULL;
CKeyHandler *keyhandler;
BOOKMARK *global_bookmark = NULL;
SERVERLIST *global_server = NULL;
char *localwindowlog[LOG_LINES];
int displayinitdone;
int bm_magic_max = -1;
int mode, leftformat, rightformat;
char okay_dir[1024], uselocal_label[256], localdir[1024];
bool use_own_ip = FALSE, use_okay_dir = FALSE, no_autologin = FALSE;
bool need_resize;
int filtermethod = 0;
pthread_mutex_t syscall_lock;
pthread_mutex_t sigwinch_lock;
char startcwd[SERVER_WORKINGDIR_SIZE];
char *sites2login = NULL;
char *section = NULL;
char *connectSites = NULL;
char *passwdcmdline = NULL;
//bool is_away = TRUE;
bool is_afkish = FALSE;
int debug = 0, filter_method = 0, freeoldfl = 1;
bool DetermineOwnIP(char *device);
extern char own_ip[256];
extern char *localwindowlog[LOG_LINES];
extern pthread_mutex_t localwindowloglock;
char defaultfilter[256];
char firstfilesfilter[256];
char firstdirsfilter[256];
char *sectionlabels[NUM_SECTIONS];
char *prelabels[NUM_PRECOMMANDS];
int function_counter = 1;
struct _KEYS key_list[127];
void *listenUdp(void *);
int threadNr = 2;
void mywborder(WINDOW *win) {
    wborder(win, ACS_VLINE | COLOR_PAIR(STYLE_NORMAL),
            ACS_VLINE | COLOR_PAIR(STYLE_NORMAL),
            ACS_HLINE | COLOR_PAIR(STYLE_NORMAL),
            ACS_HLINE | COLOR_PAIR(STYLE_NORMAL),
            ACS_ULCORNER | COLOR_PAIR(STYLE_NORMAL),
            ACS_URCORNER | COLOR_PAIR(STYLE_NORMAL),
            ACS_LLCORNER | COLOR_PAIR(STYLE_NORMAL),
            ACS_LRCORNER | COLOR_PAIR(STYLE_NORMAL));
}

// uhm, stolen from epic 4 sources...
//typedef RETSIGTYPE sigfunc (int);
#define RETSIGTYPE void
typedef RETSIGTYPE sigfunc(int);
#define SIGNAL_HANDLER(x) \
    RETSIGTYPE x (int)
//    RETSIGTYPE x (int sig)

sigfunc *my_signal(int sig_no, sigfunc * sig_handler) {
    struct sigaction sa, osa;

    if (sig_no < 0)
        return NULL; // Signal not implemented

    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, sig_no);
    sa.sa_flags = 0;

#if defined(SA_RESTART) || defined(SA_INTERRUPT)
    if (SIGALRM == sig_no || SIGINT == sig_no) {
# if defined(SA_INTERRUPT)
        sa.sa_flags |= SA_INTERRUPT;
# endif

        //SA_INTERRUPT
    } else {

# if defined(SA_RESTART)
        sa.sa_flags |= SA_RESTART;
# endif

        //SA_RESTART
    }

#endif

    //SA_RESTART || SA_INTERRUPT

    if (0 > sigaction(sig_no, &sa, &osa))
        return (SIG_ERR);

    return (osa.sa_handler);
}

//static int waiting=0;
// this one is based on example for ncurses :P
//static void
//adjust(int sig)

SIGNAL_HANDLER(adjust) {
    debuglog("signal sigwinch");
    pthread_mutex_lock(&sigwinch_lock);
    //display->sigwinch(sig);
    need_resize = TRUE;
    pthread_mutex_unlock(&sigwinch_lock);
    (void) signal(SIGWINCH, adjust); // some systems need this
}

void unlinklog() {
    // FIXME later :)

#ifdef DEBUG
    char temppath[2048], temppath_bak[2048];
    snprintf(temppath, sizeof(temppath), "%spftp.debug.log", okay_dir);
    snprintf(temppath_bak, sizeof(temppath_bak), "%spftp.debug.log.bak", okay_dir);
    rename(temppath, temppath_bak);
    unlink(temppath);
#endif
}

void debuglog(char *fmt, ...) {

#ifdef DEBUG
    char linebuf[2048];
    char pidbuf[10];
    va_list argp;
    FILE *lf;
    char temppath[2048];
    time_t curtime = time(NULL);
    //uid_t oldid = geteuid();

    if (debug) {
        pthread_mutex_lock(&syscall_lock);

        va_start(argp, fmt);
        vsnprintf(linebuf, sizeof(linebuf), fmt, argp);
        va_end(argp);
        snprintf(temppath, sizeof(temppath), "%spftp.debug.log", okay_dir);
        lf = fopen(temppath, "a+");
        if (lf != NULL) {
            sprintf(pidbuf, "%u", getpid());
            fprintf(lf, "%.24s [%-6.6s] %s\n", ctime(&curtime), pidbuf,
                    linebuf);
            fclose(lf);
        }
        pthread_mutex_unlock(&syscall_lock);
    }
#endif
}

void StripANSI(char *line) {
    char *m_pos, *c = line;
    // simply search for an ESC and then wait until we read the "m" (or end)

    while (*c) {
        if (*c == 27) {
            // found the possible start of an ESC code
            // now search for the "m"
            if (*(c + 1) != '[') {
                *c = '.';
                c++;
                continue;
            }
            m_pos = c + 2;
            while (*m_pos && (*m_pos != 'm')) {
                if (((*m_pos >= '0') && (*m_pos <= '9')) || (*m_pos == ';'))
                    m_pos += 1;
                else {
                    c++;
                    continue;
                }
            }

            if (*m_pos) {
                // now copy the remaining string
                strcpy(c, m_pos + 1);
            } else {
                // else we reached '\0'
                c++;
                continue;
            }
        } else {
            if (filtermethod == 0) {
                if (((unsigned char) *c <= 31) || ((unsigned char) *c >= 127))
                    *c = '.';
            } else {
                if (((unsigned char) *c <= 31)
                    || ((unsigned char) *c == 127)
                    || ((unsigned char) *c == 155))
                    *c = '.';
            }
            c += 1;
        }
    }
    //for (m = strlen(this->log[index]), c = this->log[index]; m > 0; m--, c++) {
    //}
}

/*
bool trytls(char *site)
{
    char site1[50];
    // well shouldnt use strtok here so lets do it this way
    strcpy(site1, ",");
    strcat(site1, site);
    strcat(site1, ",");
    if (!strstr(notlslist, site1))
        return true;
    return false;
}
*/

bool FilterFilename(char *filename, char *filter) {
    char *end, *start, *pattern /*, fixed[] = {"*nuked* .*"} */ ;
    bool pat_fault = FALSE;
    //int len, fl_len;

    if (filter) {
        pattern = new(char[strlen(defaultfilter) + strlen(filter) + 1 + 1]);
        sprintf(pattern, "%s %s", defaultfilter, filter);
    } else {
        pattern = new(char[strlen(defaultfilter) + 1]);
        strcpy(pattern, defaultfilter);
    }

    // ignore patterns
    start = pattern;
    //fl_len = strlen(filename);
    do {
        end = strchr(start, ' ');
        if (end)
            *end = '\0';

        //len = strlen(start);

        if (wild_match
            ((unsigned char *) start, (unsigned char *) filename))
            pat_fault = TRUE;

        if (end) {
            start = end + 1;
            *end = ' ';
        } else
            start = NULL;

    } while (!pat_fault && start);

    delete[](pattern);
    return (!pat_fault);
}

void *DetachDisplayHandler(void *dummy) {
    int old_type;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old_type);

    if (!display->Init()) {
        displayinitdone = 1;
        delete(display);
        printf(E_DISPLAYHANDLER_INIT);
        return (dummy);
    }
    //my_signal(SIGWINCH, adjust);
    displayinitdone = 2;
    display->Loop();
    // this should be never reached since the displayhandler-thread will be killed by main thread
    return (dummy);
}

bool FireDisplayHandler(void) {
    // since pthread cant start a thread on a class member, we do it this way
    debuglog("creating display thread");
    sigset_t newmask, oldmask;
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGCHLD);
    sigaddset(&newmask, SIGTERM);
    sigaddset(&newmask, SIGQUIT);
    sigaddset(&newmask, SIGINT);
    sigaddset(&newmask, SIGHUP);
    sigaddset(&newmask, SIGWINCH);
    pthread_sigmask(SIG_BLOCK, &newmask, &oldmask);

    if (pthread_create(&(display->thread), &(display->thread_attr),
                       DetachDisplayHandler, NULL)) {
        pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
        return FALSE;
    }
    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
    return TRUE;
}

void *DetachServer(void *th_arg) {
    CServer *server = (CServer *) th_arg;

    //my_signal(SIGWINCH, SIG_IGN);
    debuglog("hi from new new server");
    int old_type;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old_type);
    server->Run();
    // this should be never reached since we kill the thread
    return (NULL);
}

void FireupLocalFilesys(void) {
    BOOKMARK bm;
    CServer *new_server;
    SERVERLIST *new_sv, *sv_temp = global_server;

    // let's create a special server-object which accesses local files
    new_sv = new(SERVERLIST);
    new_sv->next = NULL;

    new_server = new(CServer);
    new_server->SetServerType(SERVER_TYPE_LOCAL);
    new_server->SetBMMagic( -1);
    new_sv->server = new_server;

    bm.label = new(char[strlen("localhost") + 1]);
    strcpy(bm.label, "localhost");
    bm.user = bm.host = bm.pass = bm.startdir = bm.exclude_source = bm.exclude_target =
        bm.site_rules = bm.site_user = bm.site_wkup = bm.noop_cmd = bm.first_cmd = 
        bm.nuke_prefix = bm.msg_string = bm.label;
    for (int f = 0; f < NUM_SECTIONS; f++) { bm.section[f] = bm.label; }
    for (int f = 0; f < NUM_PRECOMMANDS; f++) { bm.pre_cmd[f] = bm.label; }

    bm.use_track = bm.use_jump = bm.use_noop = bm.use_refresh =
        bm.use_chaining = bm.use_sections = FALSE;
    bm.use_pasv = bm.use_ssl = bm.use_ssl_list = bm.use_ssl_data =
        bm.use_stealth_mode = bm.use_retry = bm.use_firstcmd = bm.use_rndrefr = FALSE;
    bm.file_sorting = bm.dir_sorting = 1;
    bm.use_ssl_fxp = bm.use_alt_fxp = bm.use_rev_file = bm.use_rev_dir = FALSE;
    new_server->SetServerPrefs(&bm);

    delete[](bm.label);

    if (!sv_temp) {
        global_server = new_sv;
        new_sv->magic = SERVER_MAGIC_START;
    } else {
        while (sv_temp->next)
            sv_temp = sv_temp->next;

        sv_temp->next = new_sv;
        new_sv->magic = sv_temp->magic + 1;
    }
    new_server->SetMagic(new_sv->magic);

    // we use global display-attr, keep in mind!
    debuglog("creating new localserver");
    sigset_t newmask, oldmask;
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGCHLD);
    sigaddset(&newmask, SIGTERM);
    sigaddset(&newmask, SIGQUIT);
    sigaddset(&newmask, SIGINT);
    sigaddset(&newmask, SIGHUP);
    sigaddset(&newmask, SIGWINCH);
    pthread_sigmask(SIG_BLOCK, &newmask, &oldmask);
    pthread_create(new_server->GetThreadAddress(), &(display->thread_attr),
                   DetachServer, (void *) new_server);
    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
}

void FireupRemoteServer(CServer * new_server) {
    debuglog("creating new remoteserver");

#ifndef CYGWIN
    sigset_t newmask, oldmask;
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGCHLD);
    sigaddset(&newmask, SIGTERM);
    sigaddset(&newmask, SIGQUIT);
    sigaddset(&newmask, SIGINT);
    sigaddset(&newmask, SIGHUP);
    sigaddset(&newmask, SIGWINCH);
    pthread_sigmask(SIG_BLOCK, &newmask, &oldmask);
#endif

    pthread_create(new_server->GetThreadAddress(), &(display->thread_attr),
                   DetachServer, (void *) new_server);

#ifndef CYGWIN
    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
#endif

}

void FreeBookmarks(void) {
    BOOKMARK *bm_temp1, *bm_temp = global_bookmark;
    while (bm_temp) {
        bm_temp1 = bm_temp;
        bm_temp = bm_temp->next;

        delete[](bm_temp1->label);
        delete[](bm_temp1->host);
        delete[](bm_temp1->user);
        delete[](bm_temp1->pass);
        delete[](bm_temp1->startdir);
        delete[](bm_temp1->exclude_source);
        delete[](bm_temp1->exclude_target);
	for (int f = 0; f < NUM_SECTIONS; f++) delete[](bm_temp1->section[f]);
	for (int f = 0; f < NUM_PRECOMMANDS; f++) delete[](bm_temp1->pre_cmd[f]);
        delete[](bm_temp1->site_rules);
        delete[](bm_temp1->site_user);
        delete[](bm_temp1->site_wkup);
        delete[](bm_temp1->noop_cmd);
        delete[](bm_temp1->first_cmd);
        delete(bm_temp1);
    }
    global_bookmark = NULL;
}

bool ReadConfig(char *filename) {
    FILE *in_file, *dircheck;
    char line[1024], label[1024], value[1024], *start;
    
    strcpy(uselocal_label, "");
    strcpy(defaultfilter, "");
    strcpy(firstfilesfilter, "");
    strcpy(firstdirsfilter, "");
    strcpy(own_ip, "");
    strcpy(okay_dir, "");

    if (!(in_file = fopen(filename, "r")))
        return (FALSE);

    for (int f = 0; f < NUM_SECTIONS; f++) {
       sectionlabels[f] = new char[5];
       sprintf(value, "sec%02i", f + 1);
       strcpy(sectionlabels[f], value);
    }
    for (int f = 0; f < NUM_PRECOMMANDS; f++) { 
       prelabels[f] = new char[5];
       sprintf(value, "pre%02i", f + 1);
       strcpy(prelabels[f], value);
    }

    do {
        fgets(line, 1024, in_file);
        if (!feof(in_file)) {
            if (line[0] != '#') {
                strcpy(label, line);
                start = strrchr(label, '\n');
                if (start)
                    *start = '\0';
                start = strrchr(label, '\r');
                if (start)
                    *start = '\0';
                start = strchr(label, '=');
                if (start)
                    *start = '\0';
                else
                    start = label;

                strcpy(value, start + 1);

                if ((*label != '\0') && (strlen(value) > 0)) {
                    if (!strcasecmp(label, "DEVICE")) {
                        if (!DetermineOwnIP(value)) {
                            printf("unknown network device '%s', sorry.\n",
                                   value);
                            fclose(in_file);
                            return (FALSE);
                        }
                        use_own_ip = TRUE;
                    } else if (!strcasecmp(label, "FREEOLDFILELIST")) {
                        freeoldfl = atoi(value);
                    } else if (!strcasecmp(label, "VIEWFILTER")) {
                        filtermethod = atoi(value);
                    } else if (!strcasecmp(label, "DEBUG")) {
                        debug = atoi(value);
                    } else if (!strcasecmp(label, "LOCALIP")) {
                        strncpy(own_ip, value, 254);
                        own_ip[255] = '\0';
                        use_own_ip = TRUE;
                    } else if (!strcasecmp(label, "DEFAULTFILTER")) {
                        strncpy(defaultfilter, value, 254);
                        defaultfilter[255] = '\0';
                    } else if (!strcasecmp(label, "FIRSTFILESFILTER")) {
                        strncpy(firstfilesfilter, value, 254);
                        firstfilesfilter[255] = '\0';
                    } else if (!strcasecmp(label, "FIRSTDIRSFILTER")) {
                        strncpy(firstdirsfilter, value, 254);
                        firstdirsfilter[255] = '\0';
                    } else if (!strcasecmp(label, "OKAYDIR")) {
                        struct stat s;
                        //strcpy(okay_dir, value);
                        //if (value[0] != '/') {
                        // strcpy(okay_dir, startcwd);
                        //} else {
                        strcpy(okay_dir, "");
                        //}
                        strcat(okay_dir, value);
                        strcat(okay_dir, "/");
                        if (stat(okay_dir, &s) < 0) {
                            printf("please specify a valid dir for the OKAYDIR label.\n");
                            fclose(in_file);
                            return (FALSE);
                        }
                        if (!S_ISDIR(s.st_mode)) {
                            printf("please specify a valid dir for the OKAYDIR label.\n");
                            fclose(in_file);
                            return (FALSE);
                        }
                        // check if the dir is okay
                        strcat(value, ".pftpdircheck");
                        remove(value);
                        if ((dircheck = fopen(value, "w"))) {
                            fclose(dircheck);
                        } else {
                            printf("please specify a valid dir with writing-permissions for the OKAYDIR label.\n");
                            fclose(in_file);
                            return (FALSE);
                        }
                        remove(value);
                        use_okay_dir = TRUE;
                    } else if (!strcasecmp(label, "LOCALDIR")) {
                        strcpy(localdir, value);
                    } else if (!strcasecmp(label, "USELOCAL")) {
                        strcpy(uselocal_label, value);
                    } else if (!strcasecmp(label, "MODE")) {
                        if (mode < 1)
                            mode = atoi(value);
                    } else if (!strcasecmp(label, "FILELISTLEFTFORMAT")) {
                        leftformat = atoi(value);
                    } else if (!strcasecmp(label, "FILELISTRIGHTFORMAT")) {
                        rightformat = atoi(value);

                    } else if (!strcasecmp(label, "SECTION01_LABEL")) {
                        sectionlabels[0] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[0], value);
                        //debuglog("section01: %s", sectionlabels[0]);
                    } else if (!strcasecmp(label, "SECTION02_LABEL")) {
                        sectionlabels[1] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[1], value);
                        //debuglog("section02: %s", sectionlabels[1]);
                    } else if (!strcasecmp(label, "SECTION03_LABEL")) {
                        sectionlabels[2] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[2], value);
                        //debuglog("section03: %s", sectionlabels[2]);
                    } else if (!strcasecmp(label, "SECTION04_LABEL")) {
                        sectionlabels[3] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[3], value);
                        //debuglog("section04: %s", sectionlabels[3]);
                    } else if (!strcasecmp(label, "SECTION05_LABEL")) {
                        sectionlabels[4] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[4], value);
                        //debuglog("section05: %s", sectionlabels[4]);
                    } else if (!strcasecmp(label, "SECTION06_LABEL")) {
                        sectionlabels[5] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[5], value);
                        //debuglog("section06: %s", sectionlabels[5]);
                    } else if (!strcasecmp(label, "SECTION07_LABEL")) {
                        sectionlabels[6] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[6], value);
                        //debuglog("section07: %s", sectionlabels[6]);
                    } else if (!strcasecmp(label, "SECTION08_LABEL")) {
                        sectionlabels[7] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[7], value);
                        //debuglog("section08: %s", sectionlabels[7]);
                    } else if (!strcasecmp(label, "SECTION09_LABEL")) {
                        sectionlabels[8] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[8], value);
                        //debuglog("section09: %s", sectionlabels[8]);
                    } else if (!strcasecmp(label, "SECTION10_LABEL")) {
                        sectionlabels[9] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[9], value);
                        //debuglog("section10: %s", sectionlabels[9]);
                    } else if (!strcasecmp(label, "SECTION11_LABEL")) {
                        sectionlabels[10] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[10], value);
                        //debuglog("section11: %s", sectionlabels[10]);
                    } else if (!strcasecmp(label, "SECTION12_LABEL")) {
                        sectionlabels[11] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[11], value);
                        //debuglog("section12: %s", sectionlabels[11]);
                    } else if (!strcasecmp(label, "SECTION13_LABEL")) {
                        sectionlabels[12] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[12], value);
                        //debuglog("section13: %s", sectionlabels[12]);
                    } else if (!strcasecmp(label, "SECTION14_LABEL")) {
                        sectionlabels[13] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[13], value);
                        //debuglog("section14: %s", sectionlabels[13]);
                    } else if (!strcasecmp(label, "SECTION15_LABEL")) {
                        sectionlabels[14] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[14], value);
                        //debuglog("section15: %s", sectionlabels[14]);
                    } else if (!strcasecmp(label, "SECTION16_LABEL")) {
                        sectionlabels[15] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[15], value);
                        //debuglog("section16: %s", sectionlabels[15]);
                    } else if (!strcasecmp(label, "SECTION17_LABEL")) {
                        sectionlabels[16] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[16], value);
                        //debuglog("section17: %s", sectionlabels[16]);
                    } else if (!strcasecmp(label, "SECTION18_LABEL")) {
                        sectionlabels[17] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[17], value);
                        //debuglog("section18: %s", sectionlabels[17]);
                    } else if (!strcasecmp(label, "SECTION19_LABEL")) {
                        sectionlabels[18] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[18], value);
                        //debuglog("section19: %s", sectionlabels[18]);
                    } else if (!strcasecmp(label, "SECTION20_LABEL")) {
                        sectionlabels[19] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[19], value);
                        //debuglog("section20: %s", sectionlabels[19]);
                    } else if (!strcasecmp(label, "SECTION21_LABEL")) {
                        sectionlabels[20] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[20], value);
                        //debuglog("section21: %s", sectionlabels[20]);
                    } else if (!strcasecmp(label, "SECTION22_LABEL")) {
                        sectionlabels[21] = new char [strlen(value) + 1];
                        strcpy(sectionlabels[21], value);
                        //debuglog("section22: %s", sectionlabels[21]);
                    } else if (!strcasecmp(label, "PRE01_LABEL")) { 
                        prelabels[0] = new char [strlen(value) + 1];
                        strcpy(prelabels[0], value);
                    } else if (!strcasecmp(label, "PRE02_LABEL")) { 
                        prelabels[1] = new char [strlen(value) + 1];
                        strcpy(prelabels[1], value);
                    } else if (!strcasecmp(label, "PRE03_LABEL")) { 
                        prelabels[2] = new char [strlen(value) + 1];
                        strcpy(prelabels[2], value);
                    } else if (!strcasecmp(label, "PRE04_LABEL")) { 
                        prelabels[3] = new char [strlen(value) + 1];
                        strcpy(prelabels[3], value);
                    } else if (!strcasecmp(label, "PRE05_LABEL")) { 
                        prelabels[4] = new char [strlen(value) + 1];
                        strcpy(prelabels[4], value);
                    } else if (!strcasecmp(label, "PRE06_LABEL")) { 
                        prelabels[5] = new char [strlen(value) + 1];
                        strcpy(prelabels[5], value);
                    } else if (!strcasecmp(label, "PRE07_LABEL")) { 
                        prelabels[6] = new char [strlen(value) + 1];
                        strcpy(prelabels[6], value);
                    } else if (!strcasecmp(label, "PRE08_LABEL")) { 
                        prelabels[7] = new char [strlen(value) + 1];
                        strcpy(prelabels[7], value);
                    } else if (!strcasecmp(label, "PRE09_LABEL")) { 
                        prelabels[8] = new char [strlen(value) + 1];
                        strcpy(prelabels[8], value);
                    } else if (!strcasecmp(label, "PRE10_LABEL")) { 
                        prelabels[9] = new char [strlen(value) + 1];
                        strcpy(prelabels[9], value);
                    } else if (!strcasecmp(label, "PRE11_LABEL")) { 
                        prelabels[10] = new char [strlen(value) + 1];
                        strcpy(prelabels[10], value);
                    } else if (!strcasecmp(label, "PRE12_LABEL")) { 
                        prelabels[11] = new char [strlen(value) + 1];
                        strcpy(prelabels[11], value);
                    } else {
                        printf("unknown label '%s' in configfile.\n", label);
                        fclose(in_file);
                        return (FALSE);
                    }
                }
            }
        }
    } while (!feof(in_file));

    // checking mode
    if (mode < 1 || mode > 3)
        mode = 3;

    // checking filelist format
    if (leftformat < 0 || leftformat > 4)
        leftformat = 0;
    if (rightformat < 0 || rightformat > 4)
        rightformat = 0;

    fclose(in_file);
    return (TRUE);
}

int GetDec(int prefix, char *keychar) {
    switch (prefix) {
        // functions
        case 0:
            if (strcasecmp(keychar, "SPACE") == 0) return (32);
            if (strcasecmp(keychar, "APOSTROPHE") == 0) return (39);
            if (strcasecmp(keychar, "COMMA") == 0) return (44);
            if (strcasecmp(keychar, "BACKSLASH") == 0) return (92);
            if (strcasecmp(keychar, "DEL") == 0) return (127);
            if (strcasecmp(keychar, "ESC-SPACE") == 0) return (32);
            if (strcasecmp(keychar, "ESC-APOSTROPHE") == 0) return (39);
            if (strcasecmp(keychar, "ESC-COMMA") == 0) return (44);
            if (strcasecmp(keychar, "ESC-BACKSLASH") == 0) return (92);
            if (strcasecmp(keychar, "ESC-DEL") == 0) return (127);
            if (strcasecmp(keychar, "FUNCTION_BOOKMARK_DIALOG") == 0) return (1);
            if (strcasecmp(keychar, "FUNCTION_SITEPREFS_DIALOG") == 0) return (2);
            if (strcasecmp(keychar, "FUNCTION_CLOSESITE") == 0) return (3);
            if (strcasecmp(keychar, "FUNCTION_TOGGLEMARK") == 0) return (4);
            if (strcasecmp(keychar, "FUNCTION_MARK_ALL") == 0) return (5);
            if (strcasecmp(keychar, "FUNCTION_DEMARK_ALL") == 0) return (6);
            if (strcasecmp(keychar, "FUNCTION_COMPARE") == 0) return (7);
            if (strcasecmp(keychar, "FUNCTION_REFRESH_CHAINED") == 0) return (8);
            if (strcasecmp(keychar, "FUNCTION_REFRESH") == 0) return (9);
            if (strcasecmp(keychar, "FUNCTION_DELETE") == 0) return (10);
            if (strcasecmp(keychar, "FUNCTION_TRANSFER_QUIT") == 0) return (11);
            if (strcasecmp(keychar, "FUNCTION_TRANSFER") == 0) return (12);
            if (strcasecmp(keychar, "FUNCTION_TRANSFER_OK") == 0) return (13);
            if (strcasecmp(keychar, "FUNCTION_MAGICTRADE") == 0) return (14);
            if (strcasecmp(keychar, "FUNCTION_PREPARE_INSIDE") == 0) return (15);
            if (strcasecmp(keychar, "FUNCTION_PREPARE_OUTSIDE") == 0) return (16);
            if (strcasecmp(keychar, "FUNCTION_UNDO_PREPARED") == 0) return (17);
            if (strcasecmp(keychar, "FUNCTION_MAKEDIR_DIALOG") == 0) return (18);
            if (strcasecmp(keychar, "FUNCTION_RENAME_CHAINED_DIALOG") == 0) return (19);
            if (strcasecmp(keychar, "FUNCTION_RENAME_DIALOG") == 0) return (20);
            if (strcasecmp(keychar, "FUNCTION_NUKE_DIALOG") == 0) return (21);
            if (strcasecmp(keychar, "FUNCTION_UNNUKE_DIALOG") == 0) return (22);
            if (strcasecmp(keychar, "FUNCTION_UNDUPE_DIALOG") == 0) return (23);
            if (strcasecmp(keychar, "FUNCTION_WIPE_DIALOG") == 0) return (24);
            if (strcasecmp(keychar, "FUNCTION_SITECOMMAND_CHAINED_DIALOG") == 0) return (25);
            if (strcasecmp(keychar, "FUNCTION_SITECOMMAND_DIALOG") == 0) return (26);
            if (strcasecmp(keychar, "FUNCTION_CWD_DIALOG") == 0) return (27);
            if (strcasecmp(keychar, "FUNCTION_CWD_1") == 0) return (28);
            if (strcasecmp(keychar, "FUNCTION_CWD_ROOT") == 0) return (29);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC01") == 0) return (30);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC02") == 0) return (31);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC03") == 0) return (32);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC04") == 0) return (33);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC05") == 0) return (34);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC06") == 0) return (35);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC07") == 0) return (36);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC08") == 0) return (37);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC09") == 0) return (38);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC10") == 0) return (39);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC11") == 0) return (40);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC12") == 0) return (41);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC13") == 0) return (42);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC14") == 0) return (43);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC15") == 0) return (44);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC16") == 0) return (45);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC17") == 0) return (46);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC18") == 0) return (47);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC19") == 0) return (48);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC20") == 0) return (49);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC21") == 0) return (50);
            if (strcasecmp(keychar, "FUNCTION_CWD_SEC22") == 0) return (51);
            if (strcasecmp(keychar, "FUNCTION_VIEWFILE") == 0) return (52);
            if (strcasecmp(keychar, "FUNCTION_VIEWLOG") == 0) return (53);
            if (strcasecmp(keychar, "FUNCTION_SCROLLSTATUS_UP") == 0) return (54);
            if (strcasecmp(keychar, "FUNCTION_SCROLLSTATUS_DOWN") == 0) return (55);
            if (strcasecmp(keychar, "FUNCTION_JUMP_TO_NEWEST") == 0) return (56);
            if (strcasecmp(keychar, "FUNCTION_JUMP_THRUE_NEWEST") == 0) return (57);
            if (strcasecmp(keychar, "FUNCTION_JUMP_SERVER_DIALOG") == 0) return (58);
            if (strcasecmp(keychar, "FUNCTION_JUMP_SITE_FORWARD") == 0) return (59);
            if (strcasecmp(keychar, "FUNCTION_JUMP_SITE_BACKWARD") == 0) return (60);
            if (strcasecmp(keychar, "FUNCTION_SWITCH_LISTFORMAT_FORWARD") == 0) return (61);
            if (strcasecmp(keychar, "FUNCTION_SWITCH_LISTFORMAT_BACKWARD") == 0) return (62);
            if (strcasecmp(keychar, "FUNCTION_SWITCH_DIRSORTING") == 0) return (63);
            if (strcasecmp(keychar, "FUNCTION_SWITCH_FILESORTING") == 0) return (64);
            if (strcasecmp(keychar, "FUNCTION_SWITCH_TRANSFER_MODE") == 0) return (65);
            if (strcasecmp(keychar, "FUNCTION_SWITCH_CHAIN_MODE") == 0) return (66);
            if (strcasecmp(keychar, "FUNCTION_QUIT") == 0) return (67);
            if (strcasecmp(keychar, "FUNCTION_SCROLLFILELIST_LEFT") == 0) return (68);
            if (strcasecmp(keychar, "FUNCTION_SCROLLFILELIST_RIGHT") == 0) return (69);
            if (strcasecmp(keychar, "FUNCTION_SWITCH_REVERSE_DIRSORTING") == 0) return (70);
            if (strcasecmp(keychar, "FUNCTION_SWITCH_REVERSE_FILESORTING") == 0) return (71);
            if (strcasecmp(keychar, "FUNCTION_MAGICTRADE_REPEAT") == 0) return(81);
	    	if (strcasecmp(keychar,"FUNCTION_IDLESITE_QUIT") == 0) return(98);
	    	if (strcasecmp(keychar,"FUNCTION_SITEPRE_DIALOG") == 0) return(99);
            //if (strcasecmp(keychar, "FUNCTION_CHANGE_PASSWORD") == 0) return (70);

        // normal keys
        case 1:
            switch (*keychar) {
                case '!': return (33);
                case '"': return (34);
                case '#': return (35);
                case '$': return (36);
                case '%': return (37);
                case '&': return (38);
                case '\'': return (39);
                case '(': return (40);
                case ')': return (41);
                case '*': return (42);
                case '+': return (43);
                //case ',': return(44);
                case '-': return (45);
                case '.': return (46);
                case '/': return (47);
                case '0': return (48);
                case '1': return (49);
                case '2': return (50);
                case '3': return (51);
                case '4': return (52);
                case '5': return (53);
                case '6': return (54);
                case '7': return (55);
                case '8': return (56);
                case '9': return (57);
                case ':': return (58);
                case ';': return (59);
                case '<': return (60);
                case '=': return (61);
                case '>': return (62);
                case '?': return (63);
                case '@': return (64);
                case '[': return (91);
                case '\\': return (92);
                case ']': return (93);
                case '^': return (94);
                case '_': return (95);
                case '`': return (96);
                case 'a': return (97);
                case 'b': return (98);
                case 'c': return (99);
                case 'd': return (100);
                case 'e': return (101);
                case 'f': return (102);
                case 'g': return (103);
                case 'h': return (104);
                case 'i': return (105);
                case 'j': return (106);
                case 'k': return (107);
                case 'l': return (108);
                case 'm': return (109);
                case 'n': return (110);
                case 'o': return (111);
                case 'p': return (112);
                case 'q': return (113);
                case 'r': return (114);
                case 's': return (115);
                case 't': return (116);
                case 'u': return (117);
                case 'v': return (118);
                case 'w': return (119);
                case 'x': return (120);
                case 'y': return (121);
                case 'z': return (122);
                case '{': return (123);
                case '|': return (124);
                case '}': return (125);
                case '~': return (126);
                case 'A': return (65);
                case 'B': return (66);
                case 'C': return (67);
                case 'D': return (68);
                case 'E': return (69);
                case 'F': return (70);
                case 'G': return (71);
                case 'H': return (72);
                case 'I': return (73);
                case 'J': return (74);
                case 'K': return (75);
                case 'L': return (76);
                case 'M': return (77);
                case 'N': return (78);
                case 'O': return (79);
                case 'P': return (80);
                case 'Q': return (81);
                case 'R': return (82);
                case 'S': return (83);
                case 'T': return (84);
                case 'U': return (85);
                case 'V': return (86);
                case 'W': return (87);
                case 'X': return (88);
                case 'Y': return (89);
                case 'Z': return (90);
            }

        // ctrl'ed keys
        case 2:
            switch (*keychar) {
                case '@': return (0);
                case 'a':
                case 'A': return (1);
                case 'b':
                case 'B': return (2);
                case 'c':
                case 'C': return (3);
                case 'd':
                case 'D': return (4);
                case 'e':
                case 'E': return (5);
                case 'f':
                case 'F': return (6);
                case 'g':
                case 'G': return (7);
                case 'h':
                case 'H': return (8);
                case 'i':
                case 'I': return (9);
                case 'j':
                case 'J': return (10);
                case 'k':
                case 'K': return (11);
                case 'l':
                case 'L': return (12);
                case 'm':
                case 'M': return (13);
                case 'n':
                case 'N': return (14);
                case 'o':
                case 'O': return (15);
                case 'p':
                case 'P': return (16);
                case 'q':
                case 'Q': return (17);
                case 'r':
                case 'R': return (18);
                case 's':
                case 'S': return (19);
                case 't':
                case 'T': return (20);
                case 'u':
                case 'U': return (21);
                case 'v':
                case 'V': return (22);
                case 'w':
                case 'W': return (23);
                case 'x':
                case 'X': return (24);
                case 'y':
                case 'Y': return (25);
                case 'z':
                case 'Z': return (26);
                case '[': return (27);
                //case 'FS': return(28);
                case ']': return (29);
                case '^': return (30);
                case '_': return (31);
            }
    }
    return ( -1);
}

bool ReadKeymap(char *filename) {
    FILE *in_file;
    char line[1024], label[40], value[1024], tempstr[1024],
    *start, *tempptr, *tempptr1;
    bool esc_key = FALSE;

    if (!(in_file = fopen(filename, "r")))
        return (FALSE);

    do {
        fgets(line, 1024, in_file);
        if (!feof(in_file)) {
            if (line[0] != '#') {
                strcpy(label, line);
                //debuglog("---new line---");
                //debuglog("line: %s", label);
                start = strrchr(label, '\n');
                if (start)
                    *start = '\0';
                start = strrchr(label, '\r');
                if (start)
                    *start = '\0';
                start = strchr(label, '=');
                if (start)
                    *start = '\0';
                else
                    start = label;
                //debuglog("label: %s", label);

                strcpy(value, start + 1);
                //toupper(label);
                //debuglog("value: %s", value);

                if ((*label != '\0') && (strstr(label, "FUNCTION")) && !(strlen(value) == 0)) {
                    // we sperate values to get the keys that are assigned
                    strncpy(tempstr, value, sizeof(tempstr));
                    tempptr1 = tempstr;
                    tempptr = strsep(&tempptr1, ",");
                    // now loop thrue these keys and get the dec int for each
                    while (tempptr) {
                        esc_key = FALSE;
                        //debuglog("key: %s", tempptr);
                        if (strlen(tempptr) == 0) {
                            //debuglog("was 0: %s", tempptr);
                            tempptr = strsep(&tempptr1, ",");
                            continue;
                        }
                        if (strncasecmp(tempptr, "esc-", 4) == 0)
                            esc_key = TRUE;
                        if ((strcasecmp(tempptr, "del") == 0)
                            || (strcasecmp(tempptr, "backslash") == 0)
                            || (strcasecmp(tempptr, "comma") == 0)
                            || (strcasecmp(tempptr, "apostrophe") == 0)
                            || (strcasecmp(tempptr, "space") == 0)
                            || (strcasecmp(tempptr, "esc-del") == 0)
                            || (strcasecmp(tempptr, "esc-backslash") == 0)
                            || (strcasecmp(tempptr, "esc-comma") == 0)
                            || (strcasecmp(tempptr, "esc-apostrophe") == 0)
                            || (strcasecmp(tempptr, "esc-space") == 0)) {
                            if (GetDec(0, tempptr) != -1) {
                                if (esc_key)
                                    key_list[GetDec(0, tempptr)].esc_function_code = GetDec(0, label);
                                else 
                                    key_list[GetDec(0, tempptr)].function_code = GetDec(0, label);

                               	//printf("odd key-code: %d %s-code: %d / %d",
                                //        GetDec(0, tempptr), label,
                                //        key_list[GetDec(0, tempptr)].function_code,
                                //        key_list[GetDec(0, tempptr)].esc_function_code);
                                
                            }
                        } else if ((strncasecmp(tempptr, "ctrl-", 5) == 0)
                                   || (strncasecmp(tempptr, "esc-ctrl-", 9) == 0)) {
                            char tempkey[1];
                            tempkey[0] = tempptr[strlen(tempptr) - 1];
                            //debuglog("ctrl key is: %s", tempkey);
                            if (GetDec(2, tempkey) != -1) {
                                if (esc_key)
                                    key_list[GetDec(2, tempkey)].esc_function_code = GetDec(0, label);
                                else
                                    key_list[GetDec(2, tempkey)].function_code = GetDec(0, label);

                                //debuglog("ctrl key-code: %d %s-code: %d / %d",
                                //        GetDec(2, tempkey), label,
                                //        key_list[GetDec(2, tempkey)].function_code,
                                //        key_list[GetDec(2, tempkey)].esc_function_code);
                            }
                        } else {
                            if (GetDec(1, tempptr) != -1) {
                                if (esc_key) {
                                    char tempkey[1];
                                    tempkey[0] = tempptr[strlen(tempptr) - 1];
                                    key_list[GetDec(1, tempkey)].esc_function_code = GetDec(0, label);
                                } else {
                                    key_list[GetDec(1, tempptr)].function_code = GetDec(0, label);
                                }

                                //printf("normal key-code: %d %s-code: %d / %d \n",
                                //        GetDec(1, tempptr), label,
                                //        key_list[GetDec(1, tempptr)].function_code,
                                //        key_list[GetDec(1, tempptr)].esc_function_code);                                        
                            }
                        }

                        tempptr = strsep(&tempptr1, ",");
                        continue;
                    } // end while
                } // end parsing value
            } // end if not #
        } // end !feof
    } while (!feof(in_file));
    fclose(in_file);
    return (TRUE);
}

int main(int argc, char **argv) {
    char msg[256], config_file[] = {".pftp/config"}, keymap_file[] = {".pftp/keymap"};
    int n;
    pthread_mutex_init(&syscall_lock, NULL);
    pthread_mutex_init(&sigwinch_lock, NULL);

    my_signal(SIGINT, SIG_IGN);
    my_signal(SIGPIPE, SIG_IGN);
    my_signal(SIGWINCH, adjust);
    need_resize = FALSE;

    if (argc > 1) {
        for (n = 1; n < argc; n++)
            if (!strcmp(argv[n], "-na"))
                no_autologin = TRUE;
            else if (!strncmp(argv[n], "-connect=", 9)) {
                sites2login = (char *) malloc(strlen(argv[n]) - 9 + 2);
                strcpy(sites2login, argv[n] + 9);
                strcat(sites2login, ",");
            } else if (!strncmp(argv[n], "-section=", 9)) {
                section = (char *) malloc(strlen(argv[n]) - 9 + 1);
                strcpy(section, argv[n] + 9);
            } else if (!strncmp(argv[n], "-passwd=", 8)) {
                passwdcmdline = (char *) malloc(strlen(argv[n]) - 8 + 1);
                strcpy(passwdcmdline, argv[n] + 8);
            } else if (!strncmp(argv[n], "-mode=", 6)) {
                mode = atoi(argv[n] + 6);
            } else {
                printf("pftp %s EDiTiON %s\n\n", PFTP_EDITION, PFTP_VERSION);
                printf("available command line options:\n");
                printf("\t-na\t\t\t\tomit autologin\n");
                printf("\t-connect=site1,site2,...\tautologin to specified sites\n");
                printf("\t-section=sectionA\t\tautocwd to specified section\n");
                printf("\t-passwd=mysecretpasswd\t\tcustom passwd for bookmarks\n\n");
                printf("\t-mode=[1,2,3]\t\tpftp mode (1 = FTP, 2 = Chained FTP, 3 = FXP))\n\n");
                exit( -1);
            }
    }
    getcwd(startcwd, SERVER_WORKINGDIR_SIZE);
    if (USEWORKINGPATH) {
    	if (strcmp(startcwd, WORKINGPATH) != 0) {
    		printf("FATAL system error #nnnn\nCAUSE: We should never get here!\n");
		    exit( -1);
		}    		
    }
    
    if (!ReadConfig(config_file)) {
        printf("error reading/parsing configfile '%s', bailing out.\n", config_file);
        exit( -1);
    }

    unlinklog();

    if (!ReadKeymap(keymap_file)) {
        printf("error reading/parsing keymapfile '%s', bailing out.\n", keymap_file);
        exit( -1);
    }
    debuglog("after keymap");

    if (display->ProbeBookmarkRC() == 1) {
        printf("unknown or invalid bookmark file found, delete it\n");
        exit( -1);
    }

    if (chdir(localdir) != 0) {
        printf("please specify a valid dir for the LOCALDIR label or comment it out\n");
        exit ( -1);
    }

    if (!use_own_ip) {
        printf("you need to specify a network-device in the configfile.\n");
        exit( -1);
    }

    if (!use_okay_dir) {
        printf("you need to specify a dir for the .okay and .error files in the configfile.\n");
        exit( -1);
    }

    debuglog("pftp start");
	connectSites = (char *) malloc(1);
    //CheckIP();
#ifdef TLS
    if (tls_init())
        exit(1); /* exit on TLS init error */

#endif /* TLS */

    pthread_mutex_init(&(localwindowloglock), NULL);
    for (n = 0; n < LOG_LINES; n++) {
        localwindowlog[n] = NULL;
    }
    display = new(CDisplayHandler);

    keyhandler = new(CKeyHandler);

    displayinitdone = 0;
    if (!FireDisplayHandler()) {
        delete(keyhandler);
        delete(display);
        printf(E_DISPLAYHANDLER_FIREUP);
        exit( -1);
    }
    // we will wait for display to init
    n = 0;
    while ((displayinitdone == 0) && (n < 1000)) {
        n++;
        usleep(10000);
    }
    if ((displayinitdone == 1) || (displayinitdone == 0)) {
        delete(keyhandler);
        puts("Display thread was Unable to init...");
        exit( -1);
    }
    sprintf(msg, "pftp %s edition %s",
            PFTP_EDITION, PFTP_VERSION);
    display->PostStatusLine(msg, TRUE);

    if (!keyhandler->Init()) {
        // cannot happen :)
        delete(keyhandler);
        delete(display);
        exit( -1);
    }
    // start the UDP listen thread
    int rc;
    pthread_t udpThread;
    rc = pthread_create (&udpThread, (const pthread_attr_t *)NULL, listenUdp, (void *)NULL);
    if (rc != 0) {
    	display->PostStatusLine("Unable to create UDP listener thread", TRUE);
    }    
    // done starting UDP listener
    
    keyhandler->Loop();
    delete(keyhandler);
    delete(display);

    for (n = 0; n < LOG_LINES; n++) {
        delete[](localwindowlog[n]);
        localwindowlog[n] = NULL;
    }

    FreeBookmarks();

#ifdef TLS
    tls_cleanup();
#endif
}

bool FilterDirname(char *filename, char *filter) {
    char *end, *start, *pattern, fixed [] = {"*[* *!*"};
    bool pat_fault = FALSE;
    //int len, fl_len;

    if (filter) {
        pattern = new(char[strlen(fixed ) + strlen(filter) + 1 + 1]);
        sprintf(pattern, "%s %s", fixed , filter);
    } else {
        pattern = new(char[strlen(fixed ) + 1]);
        strcpy(pattern, fixed );
    }

    // ignore patterns
    start = pattern;
    //fl_len = strlen(filename);
    do {
        end = strchr(start, ' ');
        if (end)
            *end = '\0';

        //len = strlen(start);

        if (wild_match((unsigned char *) start, (unsigned char *) filename))
            pat_fault = TRUE;

        if (end) {
            start = end + 1;
            *end = ' ';
        } else
            start = NULL;

    } while (!pat_fault && start);

    delete[](pattern);
    return (!pat_fault);
}

// void *listenUdp(void *ptr) {
void *listenUdp(void *) {
	// the UDP listener thingy here :-)
    char buf[BSIZE],*pass,*sectionUdp,*b,*sites,args[1024], *release, *release_string;
    int sd,fromlen;
    struct sockaddr_in saddr,from;
    struct hostent *hp;
    bool gothostname;
  	
	
    memset(&saddr,0,sizeof(saddr));
    saddr.sin_port = htons(PORT);
    saddr.sin_family = AF_INET;
    gothostname = false;
    if ((hp = gethostbyname(IP)) == NULL) {
		printf("\nERROR! Cannot get hostname! Binding UDP listener to 127.0.0.1.\n");
		if ((hp = gethostbyname("127.0.0.1")) == NULL) {
           printf("ERROR! Cannot get hostname at all! Edit IP setting in config.h.\n");
        } else {
           gothostname = true;
        }
    } else {
    	gothostname = true;
    }
	if (gothostname) {
	    memcpy(&saddr.sin_addr,hp->h_addr,hp->h_length);
	    if ((sd=socket(AF_INET,SOCK_DGRAM,0))==-1) {
			display->PostStatusLine("no socket listening", FALSE);
			return(NULL);
	    } else {
			display->PostStatusLine("socket listening", FALSE);
		}
	
	    if (bind (sd, (struct sockaddr *)&saddr, sizeof(saddr)) != 0) {
	    	display->PostStatusLine("unable to bind. in use?", FALSE);
	    	return(NULL);
	    }
	
	    fromlen = sizeof(struct sockaddr_in);
	
		while (sd >= 0 ) {
	        if (recvfrom(sd,buf,BSIZE,0,(struct sockaddr*)&from, (socklen_t *) &fromlen)>0) {
				memset(args,0,sizeof(args));
		
				b=strtok (buf,"\r\n");
				//display->PostStatusLine(b, FALSE);
				pass = strtok (b," ");
				sectionUdp = strtok (NULL," ");
				sites = strtok (NULL," ");
				release = strtok (NULL," ");
	            if (strcmp(pass,PASS) == 0) {
	                sites2login = (char *) malloc(strlen(sites) + 2);
	                strcpy(sites2login, sites);
	                strcat(sites2login, ",");
	                //display->PostStatusLine(sites2login, FALSE);
	                section = (char *) malloc(strlen(sectionUdp) + 1);
	                strcpy(section, sectionUdp);
	                //display->PostStatusLine(section, FALSE);
	            	connectSites = "1";
	            	if (release) {
		            	release_string = (char *) malloc(strlen(release) + strlen("Connecting to sites for ") + 1);
					    sprintf(release_string, "Connecting to sites for %s",release);					
					} else {
		            	release_string = (char *) malloc(strlen("Connecting to sites (no release given)") + 1);
					    sprintf(release_string, "Connecting to sites (no release given)");					
					}												
				    display->PostStatusLine(release_string, TRUE);
				    free(release_string);
				    release_string = NULL;
	            	
	   				//display->PostStatusLine("Msg received, connecting to sites", TRUE);
	   				is_afkish = TRUE;
	            } else {
	                 display->PostStatusLine("wrong password received", TRUE);
	            }
	        }
	    }
	}
    return(NULL);
}
