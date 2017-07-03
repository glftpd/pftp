#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <curses.h>
#include <panel.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#include "config.h"

#include "defines.h"
#include "tcp.h"
#include "server.h"
#include "displayhandler.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "keyhandler.h"
extern CKeyHandler *keyhandler;

bool FilterFilename(char *filename, char *filter);
extern char *localwindowlog[LOG_LINES];
pthread_mutex_t localwindowloglock;
//extern bool trytls(char *site);
extern void debuglog(char *fmt, ...);
extern char *section;
extern char *sectionlabels[NUM_SECTIONS];
//extern bool is_away;
//extern char notlslist[256];
extern void StripANSI(char *line);
extern CDisplayHandler *display;
extern SERVERLIST *global_server;
extern pthread_mutex_t syscall_lock;
extern char okay_dir[1024], uselocal_label[256];
//char *sectionCopy = NULL;

CServer::CServer() {
    pthread_mutex_init(&(this->cwd_lock), NULL);
    pthread_mutex_init(&(this->busy_lock), NULL);
    pthread_mutex_init(&(this->filelist_lock), NULL);
    pthread_mutex_init(&(this->displaymsg_lock), NULL);

    this->pid = (int) getpid();

    this->fl_src = NULL;
    this->actual_filelist = this->internal_filelist = NULL;
    this->display_msg_stack = NULL;
    this->param = NULL;
    this->busy = NULL;

    this->urgent = FALSE;
    this->is_busy = TRUE;
    this->is_transfering = FALSE;
    //this->alpha_sort = TRUE;
    this->file_sort_method = 1; // default file  sort
    this->dir_sort_method = 1; // default dir  sort
    this->file_sort_reverse = FALSE;
    this->dir_sort_reverse = FALSE;
    this->have_undo = FALSE;
    this->is_afk = FALSE;
    this->kill_me = FALSE;
    
    strcpy(this->working_dir, "<unknown>");

    prefs.label = NULL;
    prefs.host = NULL;
    prefs.user = NULL;
    prefs.pass = NULL;
    prefs.startdir = NULL;
    prefs.exclude_source = NULL;
    prefs.exclude_target = NULL;
    for (int f = 0; f < NUM_SECTIONS; f++) prefs.section[f] = NULL;
    for (int f = 0; f < NUM_PRECOMMANDS; f++) prefs.pre_cmd[f] = NULL;
    prefs.site_rules = NULL;
    prefs.site_user = NULL;
    prefs.site_wkup = NULL;
    prefs.noop_cmd = NULL;
    prefs.first_cmd = NULL;
    prefs.nuke_prefix = NULL;
    prefs.msg_string = NULL;

    this->use_stealth = FALSE;
    this->use_local = FALSE;
    this->noop_slept = this->refresh_slept = this->afk_slept = 0;
    this->error = E_NO_ERROR;
    this->dir_cache = NULL;
    nossl = 0;
    
    this->use_repeat = FALSE;
    this->repeat_i = 0;
    
    this->sectionCopy = NULL;
    if (section) {
	    this->sectionCopy = (char *) malloc(strlen(section) + 1);
	    strcpy(this->sectionCopy, section);
	}

    
}

CServer::~CServer() {
    FILELIST *fl_temp, *fl_temp1;
    ACTION_MSG_LIST *ac_temp, *ac_temp1;
    DIRCACHE *dc_temp, *dc_temp1;
    CACHELIST *cl_temp, *cl_temp1;

    //pthread_mutex_lock(&(this->filelist_lock));
    fl_temp = this->actual_filelist;
    while (fl_temp) {
        fl_temp1 = fl_temp;
        fl_temp = fl_temp->next;
        delete[](fl_temp1->name);
        delete(fl_temp1);
    }

    //pthread_mutex_unlock(&(this->filelist_lock));
    //pthread_mutex_lock(&(this->displaymsg_lock));
    ac_temp = this->display_msg_stack;
    while (ac_temp) {
        ac_temp1 = ac_temp;
        ac_temp = ac_temp->next;
        delete[](ac_temp1->param);
        delete(ac_temp1);
    }
    //pthread_mutex_unlock(&(this->displaymsg_lock));

    // TODO:null check?
    if (prefs.label) {
        delete[](prefs.label);
        delete[](prefs.host);
        delete[](prefs.user);
        delete[](prefs.pass);
        delete[](prefs.startdir);
        delete[](prefs.exclude_source);
        delete[](prefs.exclude_target);
	for (int f = 0; f < NUM_SECTIONS; f++) delete[](prefs.section[f]);
	for (int f = 0; f < NUM_PRECOMMANDS; f++) delete[](prefs.pre_cmd[f]);
        delete[](prefs.site_rules);
        delete[](prefs.site_user);
        delete[](prefs.site_wkup);
        delete[](prefs.noop_cmd);
        delete[](prefs.first_cmd);
        delete[](prefs.nuke_prefix);
        delete[](prefs.msg_string);
    }

    dc_temp = this->dir_cache;
    while (dc_temp) {
        dc_temp1 = dc_temp;
        dc_temp = dc_temp->next;

        // free associated cachelist
        cl_temp = dc_temp1->cachelist;
        while (cl_temp) {
            cl_temp1 = cl_temp;
            cl_temp = cl_temp->next;
            delete[](cl_temp1->name);
            delete(cl_temp1);
        }

        delete[](dc_temp1->dirname);
        delete(dc_temp1);
    }

    this->tcp.CloseControl();
}

char *CServer::GetSiteAlias(int code) {
    switch (code) {
        case 1:
            return (this->prefs.site_rules);
            break;

        case 2:
            return (this->prefs.site_user);
            break;

        default:
            return (this->prefs.site_wkup);
            break;
    }
}

char *CServer::GetFilter(void) {
    if (this->prefs.use_exclude_source)
        return (this->prefs.exclude_source);
    else
        return (NULL);
}

char *CServer::GetTargetFilter(void) {
    if (this->prefs.use_exclude_target)
        return (this->prefs.exclude_target);
    else
        return (NULL);
}

char *CServer::GetSection(int num) { 
  if (num == 0) return "1";
  else if (num == -1) return "/";
  else return this->prefs.section[num - 1];
}

void CServer::HandleMessage(int msg, char *param, int magic) {
    bool flag = 0;

    switch (msg) {
        case FOR_SERVER_MSG_RESET_TIMER:
            this->refresh_slept = 0;
            break;

        case FOR_SERVER_MSG_REFRESH:
            if (this->server_type == SERVER_TYPE_LOCAL) {
                this->LocalGetDirlist();
            } else {
                if (!this->RefreshFiles())
                    this->EvalError();
            }
            break;

        case FOR_SERVER_MSG_SECTIONS: // just remote ones get that
            if (this->prefs.use_sections) {
               if (this->ChangeWorkingDir(this->GetSection(magic))) { 
                  if (!this->GetWorkingDir()) this->EvalError();
                  if (!this->RefreshFiles()) this->EvalError();
               }
            }
            break;

        case FOR_SERVER_MSG_CWD:
            if (this->server_type == SERVER_TYPE_LOCAL) {
                if (this->LocalChangeWorkingDir(param)) {
                    this->LocalGetWorkingDir();
                    this->LocalGetDirlist();
                }
            } else {
                if (this->ChangeWorkingDir(param)) {
                    if (!this->GetWorkingDir())
                        this->EvalError();

                    if (!this->RefreshFiles())
                        this->EvalError();
                }
            }
            break;

        case FOR_SERVER_MSG_CWD_UP:
            if (this->param)
                delete[](this->param);

            this->param = new(char[3]);
            strcpy(this->param, "..");

            if (this->server_type == SERVER_TYPE_LOCAL) {
                if (this->LocalChangeWorkingDir("..")) {
                    this->LocalGetWorkingDir();
                    this->LocalGetDirlist();
                }
            } else {
                if (this->ChangeWorkingDir("..")) {
                    if (!this->GetWorkingDir())
                        this->EvalError();

                    if (!this->RefreshFiles())
                        this->EvalError();
                }
            }
            break;

        case FOR_SERVER_MSG_MKD:
            if (this->server_type == SERVER_TYPE_LOCAL) {
                this->LocalMakeDir(param, TRUE);
            } else {
                if (!this->MakeDir(param, TRUE, FALSE))
                    this->EvalError();
            }
            break;

        case FOR_SERVER_MSG_FXP_DEST_PREPARE: // will start slave mode and transfer mode
            if (!urgent) {
                this->urgent = TRUE;
                this->fxpsource = magic;
                this->fxpdest = this->magic;
                this->SetTransfering();
                display->DrawSwitchPairs();
            } else { // this one is for unurgenting
                this->ClearTransfering();
                display->DrawSwitchPairs();
                this->urgent = FALSE;
            }
            break;

        case FOR_SERVER_MSG_FXP_SRC_PREPARE_QUIT:
            flag = 1;
        case FOR_SERVER_MSG_FXP_SRC_PREPARE: // will wait for DEST to get to transfering and make a queue
            this->fxpmethod = 0;
            this->fxpsource = this->magic;
            this->fxpdest = magic;
            this->SetTransfering();
            display->DrawSwitchPairs();
            this->DoFXP(magic, FALSE, flag);
            break;

        case FOR_SERVER_MSG_FXP_SRC_PREPARE_AS_OK_QUIT:
            flag = 1;
        case FOR_SERVER_MSG_FXP_SRC_PREPARE_AS_OK: // will wait for DEST to get to transfering and make a queue
            this->fxpmethod = 0;
            this->fxpsource = this->magic;
            this->fxpdest = magic;
            this->SetTransfering();
            display->DrawSwitchPairs();
            this->DoFXP(magic, TRUE, flag);
            break;

        case FOR_SERVER_MSG_FXP_SRC_UNURGENT: // will unurgent DEST and finish transfer mode for SRC
            if (this->use_repeat) { 
		if (this->MagicTrade(magic)) { this->PostFromDisplay(FOR_SERVER_MSG_FXP_SRC_UNURGENT, "", magic); return; }
                else this->use_repeat = FALSE; 
	    }
            this->DoFXPStop(magic);
            break;

        case FOR_SERVER_MSG_FXP_DIR_SRC: // will unpack the queue
            if (!this->DoFXPDir(param, magic))
                this->EvalError();

            break;

	case FOR_SERVER_MSG_MAGICTRADE:
	    if (this->use_repeat) { 
		this->PostUrgentFromDisplay(FOR_SERVER_MSG_MAGICTRADE, "monkey", magic);
                if (!this->MagicTrade(magic)) this->RemoveFromQueue(FOR_SERVER_MSG_MAGICTRADE, "monkey");
            }
	    break;
        
        case FOR_SERVER_MSG_FXP_DIR_CWD_UP: // will put both servers to ".."
            if (!this->DoFXPDirCWDUP(param, magic))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_FXP_FILE_SRC: // will upload a file to DEST
            if (!this->DoFXPFile(param, FALSE, magic))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_FXP_FILE_SRC_AS_OK:
            if (!this->DoFXPFile(param, TRUE, magic))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_UPLOAD:
            if (!this->UploadFile(param, FALSE, FALSE))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_UPLOAD_AS_OK:
            if (!this->UploadFile(param, FALSE, TRUE))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_UPLOAD_NO_WAIT: // only remote servers get this message
            if (!this->UploadFile(param, TRUE, FALSE))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_UPLOAD_NO_WAIT_AS_OK:  // only remote servers get this message
            if (!this->UploadFile(param, TRUE, TRUE))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_VIEWFILE: // only remote gets this
            if (!this->LeechFile(param, NOTICE_TYPE_NOTICE_VIEW, FALSE, 0))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_LEECH_NO_NOTICE: // only remote servers get this message
            if (!this->LeechFile(param, NOTICE_TYPE_NO_NOTICE, FALSE, 0))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_LEECH_NOTICE_SINGLE: // only remote servers get this message
            if (!this->LeechFile(param, NOTICE_TYPE_NOTICE_SINGLE, FALSE, magic))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_LEECH_NOTICE_SINGLE_AS_OK: // only remote servers get this message
            if (!this->LeechFile(param, NOTICE_TYPE_NOTICE_SINGLE, TRUE, magic))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_LEECH_NOTICE_MULTI: // only remote servers get this message
            if (!this->LeechFile(param, NOTICE_TYPE_NOTICE_MULTI, FALSE, 0))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_EMIT_REFRESH_SINGLE:
            display->PostMessageFromServer(SERVER_MSG_EMIT_REFRESH_SINGLE,
                                           magic, param);

            break;

        case FOR_SERVER_MSG_EMIT_REFRESH_MULTI:
            display->PostMessageFromServer(SERVER_MSG_EMIT_REFRESH_MULTI,
                                           this->magic, param);

            break;

        case FOR_SERVER_MSG_UPLOAD_CWD: // this one is for the local-fs only
            if (this->LocalChangeWorkingDir(param)) {
                this->LocalGetWorkingDir();
                this->LocalGetDirlist();
                display->PostMessageFromServer(SERVER_MSG_URGENT_OKAY, magic,
                                               param);
            } else
                display->PostMessageFromServer(SERVER_MSG_URGENT_ERROR, magic,
                                               param);

            break;

        case FOR_SERVER_MSG_UPLOAD_DIR_NO_WAIT: // only remote servers get this message
            if (!urgent) {
                // we need to prepare the local server first
                this->urgent = TRUE;
                this->UploadDirStart(param);
            } else {
                this->urgent = FALSE;
                this->UploadDir(param, TRUE, FALSE);
            }
            break;

        case FOR_SERVER_MSG_UPLOAD_DIR_NO_WAIT_AS_OK: // only remote servers get this message
            if (!urgent) {
                // we need to prepare the local server first
                this->urgent = TRUE;
                this->UploadDirStart(param);
            } else {
                this->urgent = FALSE;
                this->UploadDir(param, TRUE, TRUE);
            }
            break;

        case FOR_SERVER_MSG_LEECH_DIR_NO_NOTICE: // only remote servers get this message
            this->LeechDir(param, NOTICE_TYPE_NO_NOTICE, FALSE, 0);
            break;

        case FOR_SERVER_MSG_LEECH_DIR_NOTICE_SINGLE: // only remote servers get this message
            this->LeechDir(param, NOTICE_TYPE_NOTICE_SINGLE, FALSE, magic);
            break;

        case FOR_SERVER_MSG_LEECH_DIR_NOTICE_SINGLE_AS_OK: // only remote servers get this message
            this->LeechDir(param, NOTICE_TYPE_NOTICE_SINGLE, TRUE, magic);
            break;

        case FOR_SERVER_MSG_LEECH_DIR_NOTICE_MULTI: // only remote servers get this message
            this->LeechDir(param, NOTICE_TYPE_NOTICE_MULTI, FALSE, 0);
            break;

        case FOR_SERVER_MSG_DELFILE:
            if (this->server_type == SERVER_TYPE_LOCAL)
                this->LocalDeleteFile(param);
            else {
                if (!this->DeleteFile(param))
                    this->EvalError();
            }
            break;

        case FOR_SERVER_MSG_DELDIR:
            if (this->server_type == SERVER_TYPE_LOCAL)
                this->LocalDeleteDir(param);
            else {
                if (!this->DeleteDir(param))
                    this->EvalError();
            }
            break;

        case FOR_SERVER_MSG_RENFROM:
            if (this->server_type == SERVER_TYPE_LOCAL)
                this->LocalRenFrom(param);
            else {
                this->RenFrom(param);
            }
            break;

        case FOR_SERVER_MSG_RENTO:
            if (this->server_type == SERVER_TYPE_LOCAL)
                this->LocalRenTo(param);
            else {
                if (!this->RenTo(param)) {};
                this->EvalError();
            }
            break;

        case FOR_SERVER_MSG_NUKE:
            if (this->server_type != SERVER_TYPE_LOCAL) {
                if (!this->Nuke(param)) {};
                this->EvalError();
            }
            break;

        case FOR_SERVER_MSG_UNNUKE:
            if (this->server_type != SERVER_TYPE_LOCAL) {
                if (!this->UnNuke(param)) {};
                this->EvalError();
            }
            break;

        case FOR_SERVER_MSG_WIPE:
            if (this->server_type != SERVER_TYPE_LOCAL) {
                if (!this->Wipe(param)) {};
                this->EvalError();
            }
            break;

        case FOR_SERVER_MSG_UNDUPE:
            if (this->server_type != SERVER_TYPE_LOCAL) {
                if (!this->UnDupe(param)) {};
                this->EvalError();
            }
            break;

	case FOR_SERVER_MSG_PRE:
           if (this->server_type != SERVER_TYPE_LOCAL) { 
    	      if (!this->Pre(param)) this->EvalError();
           }
           break;

        case FOR_SERVER_MSG_PREP:
            if (this->server_type == SERVER_TYPE_LOCAL) {
                this->LocalMakeDir(param, TRUE);
            } else {
                // remember undo position
                strcpy(this->undo_cwd, this->working_dir);
                if (!this->MakeDir(param, TRUE, TRUE))
                    this->EvalError();
            }
            break;

        case FOR_SERVER_MSG_UNDO:

            if (this->have_undo) {
                if (this->ChangeWorkingDir(this->undo_cwd)) {
                    if (!this->GetWorkingDir())
                        this->EvalError();

                    // try to remove dir
                    if (!this->DeleteDir(this->undo_mkd))
                        this->EvalError();

                    if (!this->RefreshFiles())
                        this->EvalError();
                }
                this->have_undo = FALSE;
            }
            break;

        case FOR_SERVER_MSG_SITE: // just remote ones get this
            if (!this->SendSITE(param))
                this->EvalError();

            break;

        case FOR_SERVER_MSG_CHANGE_FILE_SORTING: //this->alpha_sort = !this->alpha_sort;
            if (this->file_sort_method < 3)
                file_sort_method++;
            else
                file_sort_method = 1;

            this->internal_filelist = this->actual_filelist;
            this->actual_filelist = NULL;
            this->SortFilelist(FALSE);
            this->PostToDisplay(SERVER_MSG_NEW_FILELIST);
            break;

        case FOR_SERVER_MSG_CHANGE_DIR_SORTING: //this->alpha_sort = !this->alpha_sort;
            if (this->dir_sort_method < 3)
                dir_sort_method++;
            else
                dir_sort_method = 1;

            this->internal_filelist = this->actual_filelist;
            this->actual_filelist = NULL;
            this->SortFilelist(FALSE);
            this->PostToDisplay(SERVER_MSG_NEW_FILELIST);
            break;

        case FOR_SERVER_MSG_CHANGE_FILE_REVERSE:
            if (this->file_sort_reverse)
                file_sort_reverse = FALSE;
            else
                file_sort_reverse = TRUE;

            this->internal_filelist = this->actual_filelist;
            this->actual_filelist = NULL;
            this->SortFilelist(FALSE);
            this->PostToDisplay(SERVER_MSG_NEW_FILELIST);
            break;

        case FOR_SERVER_MSG_CHANGE_DIR_REVERSE:
            if (this->dir_sort_reverse)
                dir_sort_reverse = FALSE;
            else
                dir_sort_reverse = TRUE;

            this->internal_filelist = this->actual_filelist;
            this->actual_filelist = NULL;
            this->SortFilelist(FALSE);
            this->PostToDisplay(SERVER_MSG_NEW_FILELIST);
            break;

        case FOR_SERVER_MSG_CLOSE:
            this->KillMe(FALSE);
            break;
    }
}

void CServer::SetServerPrefs(BOOKMARK * bm) {
    if (prefs.label)
        delete[](prefs.label);

    if (prefs.host)
        delete[](prefs.host);

    if (prefs.user)
        delete[](prefs.user);

    if (prefs.pass)
        delete[](prefs.pass);

    if (prefs.startdir)
        delete[](prefs.startdir);

    if (prefs.exclude_source)
        delete[](prefs.exclude_source);

    if (prefs.exclude_target)
        delete[](prefs.exclude_target);
    
    for (int f = 0; f < NUM_SECTIONS; f++) { 
      if(prefs.section[f]) delete[](prefs.section[f]);
    }

    for (int f = 0; f < NUM_PRECOMMANDS; f++) { 
      if(prefs.pre_cmd[f]) delete[](prefs.pre_cmd[f]);
    }

    if (prefs.site_rules)
        delete[](prefs.site_rules);

    if (prefs.site_user)
        delete[](prefs.site_user);

    if (prefs.site_wkup)
        delete[](prefs.site_wkup);

    if (prefs.noop_cmd)
        delete[](prefs.noop_cmd);

    if (prefs.first_cmd)
        delete[](prefs.first_cmd);
        
    if (prefs.nuke_prefix)
        delete[](prefs.nuke_prefix);

    if (prefs.msg_string)
        delete[](prefs.msg_string);

    prefs.label = new(char[strlen(bm->label) + 1]);
    strcpy(prefs.label, bm->label);
    display->PostMessageFromServer(SERVER_MSG_NEWLABEL, this->magic, prefs.label);

    prefs.host = new(char[strlen(bm->host) + 1]);
    strcpy(prefs.host, bm->host);

    prefs.user = new(char[strlen(bm->user) + 1]);
    strcpy(prefs.user, bm->user);

    prefs.pass = new(char[strlen(bm->pass) + 1]);
    strcpy(prefs.pass, bm->pass);

    prefs.startdir = new(char[strlen(bm->startdir) + 1]);
    strcpy(prefs.startdir, bm->startdir);

    prefs.exclude_source = new(char[strlen(bm->exclude_source) + 1]);
    strcpy(prefs.exclude_source, bm->exclude_source);

    prefs.exclude_target = new(char[strlen(bm->exclude_target) + 1]);
    strcpy(prefs.exclude_target, bm->exclude_target);
    
    for (int f = 0; f < NUM_SECTIONS; f++) { 
      prefs.section[f] = new(char[strlen(bm->section[f]) + 1]);
      strcpy(prefs.section[f], bm->section[f]);
    }

    for (int f = 0; f < NUM_PRECOMMANDS; f++) { 
      prefs.pre_cmd[f] = new(char[strlen(bm->pre_cmd[f]) + 1]);
      strcpy(prefs.pre_cmd[f], bm->pre_cmd[f]);
    }

    prefs.site_rules = new(char[strlen(bm->site_rules) + 1]);
    strcpy(prefs.site_rules, bm->site_rules);

    prefs.site_user = new(char[strlen(bm->site_user) + 1]);
    strcpy(prefs.site_user, bm->site_user);

    prefs.site_wkup = new(char[strlen(bm->site_wkup) + 1]);
    strcpy(prefs.site_wkup, bm->site_wkup);

    prefs.noop_cmd = new(char[strlen(bm->noop_cmd) + 1]);
    strcpy(prefs.noop_cmd, bm->noop_cmd);

    prefs.first_cmd = new(char[strlen(bm->first_cmd) + 1]);
    strcpy(prefs.first_cmd, bm->first_cmd);
    
    prefs.nuke_prefix = new(char[strlen(bm->nuke_prefix) + 1]);
    strcpy(prefs.nuke_prefix, bm->nuke_prefix);

    prefs.msg_string = new(char[strlen(bm->msg_string) + 1]);
    strcpy(prefs.msg_string, bm->msg_string);
    this->tcp.SetSiteMsgString(prefs.msg_string);

    prefs.retry_counter = bm->retry_counter;
    prefs.refresh_rate = bm->refresh_rate;
    prefs.noop_rate = bm->noop_rate;
    prefs.retry_delay = bm->retry_delay;
    prefs.file_sorting = bm->file_sorting;
    prefs.dir_sorting = bm->dir_sorting;
    this->file_sort_method = prefs.file_sorting;
    this->dir_sort_method = prefs.dir_sorting;
    prefs.use_refresh = bm->use_refresh;
    prefs.use_noop = bm->use_noop;
    prefs.use_startdir = bm->use_startdir;
    prefs.use_exclude_source = bm->use_exclude_source;
    prefs.use_exclude_target = bm->use_exclude_target;
    prefs.use_jump = bm->use_jump;
    prefs.use_track = bm->use_track;
    prefs.use_autologin = bm->use_autologin;
    prefs.use_chaining = bm->use_chaining;
    prefs.use_sections = bm->use_sections;
    prefs.use_pasv = bm->use_pasv;
    prefs.use_ssl = bm->use_ssl;
    prefs.use_ssl_list = bm->use_ssl_list;
    prefs.use_ssl_data = bm->use_ssl_data;
    prefs.use_stealth_mode = bm->use_stealth_mode;
    prefs.use_retry = bm->use_retry;
    prefs.use_firstcmd = bm->use_firstcmd;
    prefs.use_rndrefr = bm->use_rndrefr;
    this->use_stealth = prefs.use_stealth_mode;
    prefs.use_ssl_fxp = bm->use_ssl_fxp;
    prefs.use_alt_fxp = bm->use_alt_fxp;
    prefs.use_rev_file = bm->use_rev_file;
    prefs.use_rev_dir = bm->use_rev_dir;
    this->file_sort_reverse = prefs.use_rev_file;
    this->dir_sort_reverse = prefs.use_rev_dir;
}

void CServer::EvalError(void) {
    char *temp_filename;
    temp_filename = new(char[26]);
    bool have_error = TRUE;

    if ((this->param) && (strlen(this->param) > 24))
        sprintf(temp_filename, "'%-12.12s~%s'", this->param,
                this->param + strlen(this->param) - 11);
    else if (this->param)
        sprintf(temp_filename, "'%s'", this->param);

    switch (this->error) {
        case E_NO_ERROR:
            have_error = FALSE;
            break;

        case E_BAD_WELCOME:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_WELCOME);
            break;

        case E_BAD_MKD:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_MKD);
            break;

        case E_BAD_TEMP_FILE:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_TEMP_FILE);
            break;

#ifdef TLS
        case E_BAD_AUTH:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_AUTH);
            break;

        case E_BAD_TLSCONN:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_TLSCONN);
            break;

        case E_BAD_PBSZ:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_AUTH);
            break;

        case E_BAD_PROT:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_AUTH);
            break;
#endif

        case E_BAD_USER:
            sprintf(temp_string, "[%-10.10s] %s (%s)", prefs.label, E_MSG_BAD_USER,
                    this->tcp.GetControlBuffer());
            break;

        case E_BAD_PASS:
            sprintf(temp_string, "[%-10.10s] %s (%s)", prefs.label, E_MSG_BAD_PASS,
                    this->tcp.GetControlBuffer());
            break;

        case E_CONTROL_RESET:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_CONTROL_RESET);
            break;

        case E_CTRL_SOCKET_CREATE:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_CTRL_SOCKET_CREATE);
            break;

        case E_CTRL_SOCKET_CONNECT:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_CTRL_SOCKET_CONNECT);
            break;

        case E_CTRL_ADDRESS_RESOLVE:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_CTRL_ADDRESS_RESOLVE);
            break;

        case E_CTRL_TIMEOUT:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_CTRL_TIMEOUT);
            break;

        case E_BAD_PWD:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_PWD);
            break;

        case E_BAD_MSG:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_MSG);
            break;

        case E_BAD_LOCALFILE:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_LOCALFILE);
            break;

        case E_BAD_TYPE:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_TYPE);
            break;

        case E_BAD_PORT:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_PORT);
            break;

        case E_BAD_LIST:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_LIST);
            break;

        case E_SOCKET_BIND:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_SOCKET_BIND);
            break;

        case E_SOCKET_LISTEN:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_SOCKET_LISTEN);
            break;

        case E_SOCKET_ACCEPT_TIMEOUT:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_SOCKET_ACCEPT_TIMEOUT);
            break;

        case E_SOCKET_ACCEPT:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_SOCKET_ACCEPT);
            break;

        case E_DATA_TIMEOUT:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_DATA_TIMEOUT);
            break;

        case E_DATA_TCPERR:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_DATA_TCPERR);
            break;

        case E_SOCKET_DATA_CREATE:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_SOCKET_DATA_CREATE);
            break;

        case E_BAD_NOOP:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_NOOP);
            break;

        case E_BAD_RETR:
            sprintf(temp_string, "[%-10.10s] refused  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_RETR);
            break;

        case E_BAD_STOR:
            sprintf(temp_string, "[%-10.10s] rejected %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_STOR);
            if (!SHOW_DUPES) have_error = FALSE;
            break;

        case E_BAD_FILESIZE:
            sprintf(temp_string, "[%-10.10s] failed   %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_FILESIZE);
            break;

        case E_WRITEFILE_ERROR:
            sprintf(temp_string, "[%-10.10s] failed   %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_WRITEFILE_ERROR);
            break;

        case E_WRITEFILE_TIMEOUT:
            sprintf(temp_string, "[%-10.10s] timeout  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_WRITEFILE_TIMEOUT);
            break;

        case E_BAD_DELE:
            sprintf(temp_string, "[%-10.10s] refused  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_DELE);
            break;

        case E_BAD_RMD:
            sprintf(temp_string, "[%-10.10s] refused  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_RMD);
            break;

        case E_BAD_RENAME:
            sprintf(temp_string, "[%-10.10s] refused  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_RENAME);
            break;

        case E_BAD_SITE:
            sprintf(temp_string, "[%-10.10s] refused  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_SITE);
            break;

        case E_BAD_STEALTH:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_STEALTH);
            break;

        case E_BAD_PASV:
            sprintf(temp_string, "[%-10.10s] %s", prefs.label, E_MSG_BAD_PASV);
            break;

        case E_PASV_FAILED:
            sprintf(temp_string, "[%-10.10s] rejected %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_PASV_FAILED);
            break;

        case E_FXP_TIMEOUT:
            sprintf(temp_string, "[%-10.10s] timeout  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_FXP_TIMEOUT);
            break;

        case E_BAD_NUKE:
            sprintf(temp_string, "[%-10.10s] refused  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_NUKE);
            break;

        case E_BAD_UNNUKE:
            sprintf(temp_string, "[%-10.10s] refused  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_UNNUKE);
            break;

        case E_BAD_WIPE:
            sprintf(temp_string, "[%-10.10s] refused  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_WIPE);
            break;

        case E_BAD_UNDUPE:
            sprintf(temp_string, "[%-10.10s] refused  %-26.26s ( %-24.24s)",
                    prefs.label, temp_filename, E_MSG_BAD_UNDUPE);
            break;
         
        case E_BAD_PRE:
           sprintf(temp_string, "[%-10.10s] pre failed",  prefs.label);
           break;

        default:
            sprintf(temp_string,
                    "[%-10.10s] undefined error - %d (?)",
                    prefs.label, this->error);
            break;
    }

    if (have_error)
        display->PostStatusLine(temp_string, FALSE);

    // deadly errors
    if (this->error == E_CONTROL_RESET)
        this->KillMe(TRUE);

    // this is recoverable, happens on big lag for example
    //if (this->error == E_CONTROL_TIMEOUT)
    //    this->KillMe(TRUE);

    this->error = E_NO_ERROR;
}

void CServer::KillMe(bool emergency) {
    int n, magic;

    // post that we had to bail out
    if (emergency)
        sprintf(this->temp_string, "[%-10.10s] bailing out", this->prefs.label);
    else
        sprintf(this->temp_string, "[%-10.10s] logging off", this->prefs.label);

    display->PostStatusLine(this->temp_string, TRUE);

    if (this->is_transfering == TRUE) {
        magic = this->fxpsource;
        if (magic == this->magic) {
            magic = this->fxpdest;
        }
    }
    // lets copy the log to localwindowlog
    pthread_mutex_lock(&(localwindowloglock));
    // free log
    for (n = 0; n < LOG_LINES; n++) {
        delete[](localwindowlog[n]);
        localwindowlog[n] = NULL;
    }
    this->GetTCP()->ObtainLog((localwindowlog));
    pthread_mutex_unlock(&(localwindowloglock));
    this->PostToDisplay(SERVER_MSG_KILLME);

    // simply wait until we get killed
    do {
        pthread_testcancel();
        sleep(1);
    } while (1);
}

void CServer::Run(void) {
    ACTION_MSG_LIST *ac_temp;
    bool no_action;
    int msg = -1, magic, random_add = 0, random_add_noop = 0;

    this->is_busy = TRUE;

    //sprintf(this->temp_string, "[%-10.10s] launching thread...", this->prefs.label);
    //display->PostStatusLine(this->temp_string, TRUE);

    if (this->server_type == SERVER_TYPE_LOCAL) {
        this->LocalGetWorkingDir();
        this->LocalGetDirlist();
        this->is_busy = FALSE;
    } else {
        if (!strcmp(this->prefs.label, uselocal_label))
            this->use_local = TRUE;

        // try to login
        if (!this->Login()) {
            this->EvalError();
            this->KillMe(TRUE);
        }

        // setting site msg detection string
        //debuglog("TESTING: %s", prefs.msg_string);
        this->tcp.SetSiteMsgString(prefs.msg_string);

        // see if we have a section we cwd too
        if (sectionCopy && this->prefs.use_sections) {
        //if (section && this->prefs.use_sections) {
            for (int f = 0; f < NUM_SECTIONS; f++) { 
              if (strcasecmp(sectionCopy, sectionlabels[f]) == 0) { 
              //if (strcasecmp(section, sectionlabels[f]) == 0) { 
		 if (!this->ChangeWorkingDir(this->prefs.section[f])) {
                    sprintf(this->temp_string, "[%-10.10s]: unable to use '%s' section, ignoring", this->prefs.label, sectionCopy);
                    //sprintf(this->temp_string, "[%-10.10s]: unable to use '%s' section, ignoring", this->prefs.label, section);
                    display->PostStatusLine(this->temp_string, TRUE);
                 }
                 break;
              }
            }
            // see if we have a nice starting dir
        } else if (this->prefs.use_startdir) {
            if (!this->ChangeWorkingDir(this->prefs.startdir)) {
                sprintf(this->temp_string,
                        "[%-10.10s] unable to use '%s' a startdir, ignoring",
                        this->prefs.label, this->prefs.startdir);
                display->PostStatusLine(this->temp_string, TRUE);
            }
        }

        if (!this->GetWorkingDir())
            this->EvalError();

        if (!this->RefreshFiles()) {
            this->EvalError();
            this->KillMe(TRUE);
        }
        this->is_busy = FALSE;
    }

    do {
        no_action = TRUE;
        pthread_testcancel();

        // look for actions
        pthread_mutex_lock(&(this->displaymsg_lock));
        ac_temp = this->display_msg_stack;

        if (ac_temp) {
            // see if we have to process an urgent message

            if ((this->urgent
                 && ((ac_temp->msg == FOR_SERVER_MSG_URGENT_OKAY)
                     || (ac_temp->msg == FOR_SERVER_MSG_URGENT_ERROR)))
                || (!this->urgent)) {
                this->display_msg_stack = ac_temp->next;
                pthread_mutex_unlock(&(this->displaymsg_lock));

                if (!this->urgent)
                    msg = ac_temp->msg;

                magic = ac_temp->magic;

                if (ac_temp->param) {
                    this->param = new(char[strlen(ac_temp->param) + 1]);
                    strcpy(this->param, ac_temp->param);
                } else
                    this->param = NULL;

                if (!this->urgent
                    || (this->urgent
                        && (ac_temp->msg == FOR_SERVER_MSG_URGENT_OKAY))) {
                    this->is_busy = TRUE;
                    pthread_testcancel();
                    if (ac_temp->param)
                        debuglog
                        ("server message msg : '%d', magic : '%d'; param : '%s'",
                         msg, magic, ac_temp->param);
                    else
                        debuglog
                        ("server message msg : '%d', magic : '%d'; param : 'NULL'",
                         msg, magic);

                    this->HandleMessage(msg, ac_temp->param, magic);
                    if (ac_temp->param)
                        debuglog
                        ("DONE server message msg : '%d', magic : '%d'; param : '%s'",
                         msg, magic, ac_temp->param);
                    else
                        debuglog
                        ("DONE server message msg : '%d', magic : '%d'; param : 'NULL'",
                         msg, magic);

                    pthread_testcancel();
                    if (!this->urgent)
                        this->is_busy = FALSE;
                } else {
                    this->urgent = FALSE;
                    this->is_busy = FALSE;
                    this->PostBusy(NULL);
                }

                if (this->param)
                    delete[](this->param);

                this->param = NULL;
                delete[](ac_temp->param);
                delete(ac_temp);
                no_action = FALSE;
            } else
                pthread_mutex_unlock(&(this->displaymsg_lock));
        } else
            pthread_mutex_unlock(&(this->displaymsg_lock));

        if (!this->is_transfering) {
            // perform automatic functions (can just be enabled with remote servers)
            // auto-refresh
            if (this->prefs.use_refresh) {
                if (this->refresh_slept >=
                    (SLEEPS_NEEDED * (this->prefs.refresh_rate + random_add))) {
                    if (prefs.use_rndrefr) {
                        random_add = (int) ( -this->prefs.refresh_rate / 2 +
                                             ((double) (this->prefs.refresh_rate) *
                                              rand() / (RAND_MAX + 1.0))) * 1;
                    } else {
                        random_add = 0;
                    }

                    this->is_busy = TRUE;
                    this->refresh_slept = 0;
                    if (!this->RefreshFiles())
                        this->EvalError();

                    this->is_busy = FALSE;
                }
            }
            // auto-noop
            if (this->prefs.use_noop && this->prefs.noop_cmd) {
                if (this->noop_slept >=
                    (SLEEPS_NEEDED * (this->prefs.noop_rate + random_add_noop))) {
                    if (prefs.use_rndrefr) {
                        random_add = (int) ( -this->prefs.noop_rate / 2 +
                                             ((double) (this->prefs.noop_rate) *
                                              rand() / (RAND_MAX + 1.0))) * 1;
                    } else {
                        random_add = 0;
                    }

                    this->is_busy = TRUE;
                    this->noop_slept = 0;
                    if (!this->Noop())
                        this->EvalError();

                    this->is_busy = FALSE;
                }
            }
//            if (is_away) { 
//              if (this->away_slept >= (SLEEPS_NEEDED * SLEEP_AWAY)){
//              	keyhandler->WantQuit(false);
//              	display->PostStatusLine("Idle timeout kicked in.", TRUE);
//              }
//            }
            if (this->is_afk) { 
              if (this->afk_slept >= (SLEEPS_NEEDED * SLEEP_AWAY)) {
              	//do ur stuff here, cause the bitch didnt do anything after autoconnecting
              	//too_afk = TRUE;
              	//display->PostStatusLine("I'm gonna DIE!!!", FALSE);
              	this->KillMe(TRUE);
              }
            }
            if (this->kill_me) {
            	this->KillMe(TRUE);
            }
            if (no_action) {
                if (this->prefs.use_noop)
                    this->noop_slept++;

                if (this->prefs.use_refresh)
                    this->refresh_slept++;
		
//   				if (is_away) this->away_slept++;
   				if (this->is_afk) this->afk_slept++;

                usleep(ACTION_MSG_SLEEP);
            }
        } else {
            usleep(ACTION_MSG_SLEEP);
        }
        if (this->tcp.SiteMessage()) {
            sprintf(this->temp_string, "[%-10.10s] You have messages to read.",
                    this->prefs.label);
            display->PostStatusLine(this->temp_string, TRUE);
            this->tcp.ResetMessage();
        }
    } while (1);
}

void CServer::PostToDisplay(int msg) {
    display->PostMessageFromServer(msg, this->magic, NULL);
}

char *CServer::ObtainBusy(void) {
    char *busy = NULL;

    pthread_mutex_lock(&(this->busy_lock));

    if (this->busy) {
        busy = new(char[strlen(this->busy) + 1]);
        strcpy(busy, this->busy);
    }

    pthread_mutex_unlock(&(this->busy_lock));

    return (busy);
}

void CServer::ObtainWorkingDir(char *cwd) {
    pthread_mutex_lock(&(this->cwd_lock));

    strcpy(cwd, this->working_dir);

    pthread_mutex_unlock(&(this->cwd_lock));

}

FILELIST *CServer::ObtainFilelist(bool * use_jump) {
    FILELIST *fl_temp, *fl_temp1 = NULL, *fl_new, *fl_start = NULL;

    pthread_mutex_lock(&(this->filelist_lock));

    fl_temp = this->actual_filelist;

    while (fl_temp) {
        fl_new = new(FILELIST);
        fl_new->next = NULL;
        fl_new->magic = fl_temp->magic;
        fl_new->is_marked = FALSE;
        fl_new->name = new(char[strlen(fl_temp->name) + 1]);
        strcpy(fl_new->name, fl_temp->name);
        strcpy(fl_new->date, fl_temp->date);
        strcpy(fl_new->owner, fl_temp->owner);
        strcpy(fl_new->group, fl_temp->group);
        strcpy(fl_new->mode, fl_temp->mode);
        fl_new->size = fl_temp->size;
        fl_new->time = fl_temp->time;
        fl_new->is_dir = fl_temp->is_dir;

        if (!fl_temp1)
            fl_start = fl_new;
        else
            fl_temp1->next = fl_new;

        fl_temp1 = fl_new;
        fl_temp = fl_temp->next;
    }

    pthread_mutex_unlock(&(this->filelist_lock));

    *use_jump = this->prefs.use_jump;
    return (fl_start);
}

void CServer::LocalRenFrom(char *from) {
    strcpy(this->rename_temp, from);
}

void CServer::LocalRenTo(char *to) {
    char *temp_filename;
    temp_filename = new(char[26]);

    if (strlen(this->rename_temp) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", this->rename_temp,
                this->rename_temp + strlen(this->rename_temp) - 11);
    else
        sprintf(temp_filename, "'%s'", this->rename_temp);

    if (rename(this->rename_temp, to) == -1) {
        sprintf(this->temp_string, "[%-10.10s] failed   %-26.26s ( rename                  )",
                this->prefs.label, temp_filename);
        display->PostStatusLine(this->temp_string, FALSE);
    }

    this->LocalGetDirlist();
}

void CServer::LocalMakeDir(char *dir, bool try_cd) {
    char *temp_filename;
    int ret;
    temp_filename = new(char[26]);

    ret = mkdir(dir, 0);
    chmod(dir, S_IRUSR | S_IWUSR | S_IXUSR);

    if (strlen(dir) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", dir, dir + strlen(dir) - 11);
    else
        sprintf(temp_filename, "'%s'", dir);

    if ((ret == 0) || try_cd) {
        if (this->LocalChangeWorkingDir(dir)) {
            this->LocalGetWorkingDir();
            this->LocalGetDirlist();
        }
    } else {
        sprintf(this->temp_string, "[%-10.10s] failed   %-26.26s ( create directory        )",
                this->prefs.label, temp_filename);
        display->PostStatusLine(this->temp_string, FALSE);
    }
}

bool CServer::LocalChangeWorkingDir(char *dir) {
    char *temp_filename;
    temp_filename = new(char[26]);

    if (!chdir(dir))
        return (TRUE);

    if (strlen(dir) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", dir, dir + strlen(dir) - 11);
    else
        sprintf(temp_filename, "'%s'", dir);

    sprintf(this->temp_string, "[%-10.10s] failed   %-26.26s ( changing working dir    )",
            this->prefs.label, temp_filename);

    this->error = E_NO_ERROR;
    display->PostStatusLine(this->temp_string, FALSE);
    return (FALSE);
}

void CServer::LocalDeleteFile(char *file) {
    char *temp_filename;
    temp_filename = new(char[26]);

    if (strlen(file) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", file, file + strlen(file) - 11);
    else
        sprintf(temp_filename, "'%s'", file);

    if (remove(file) == -1) {
        sprintf(this->temp_string, "[%-10.10s] failed   %-26.26s ( deleting file           )",
                this->prefs.label, temp_filename);
        display->PostStatusLine(this->temp_string, FALSE);
    }
}

void CServer::LocalDeleteDir(char *dir) {
    char *temp_filename;
    temp_filename = new(char[26]);

    if (strlen(dir) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", dir, dir + strlen(dir) - 11);
    else
        sprintf(temp_filename, "'%s'", dir);

    if (rmdir(dir) == -1) {
        sprintf(this->temp_string, "[%-10.10s] failed   %-26.26s ( deleting directory      )",
                this->prefs.label, temp_filename);
        display->PostStatusLine(this->temp_string, FALSE);
    }
}

void CServer::PostBusy(char *busy) {
    pthread_mutex_lock(&(this->busy_lock));

    if (this->busy)
        delete[](this->busy);

    this->busy = NULL;

    if (!busy) {
        display->PostMessageFromServer(SERVER_MSG_IDLE, this->magic, NULL);
    } else {
        this->busy = new(char[strlen(busy) + 1]);
        strcpy(this->busy, busy);
        display->PostMessageFromServer(SERVER_MSG_BUSY, this->magic, busy);
    }

    pthread_mutex_unlock(&(this->busy_lock));
}

bool CServer::SendSITE(char *site) {
    char temp[10];

    this->PostBusy("SITE");
    this->tcp.FlushStack();

    // check for QUOTE
    strncpy(temp, site, 6);
    temp[6] = '\0';

    if (!strcasecmp(temp, "QUOTE ")) {
        // quote the whole string
        sprintf(this->temp_string, "%s\r\n", site + 6);
    } else {
        sprintf(this->temp_string, "SITE %s\r\n", site);
    }

    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_SITE;
        this->PostBusy(NULL);
        return (FALSE);
    }

    this->PostBusy(NULL);
    return (TRUE);
}

void CServer::RenFrom(char *from) {
    this->PostBusy("RNFR");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "RNFR %s\r\n", from);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET; // will (not yet) be eval'd
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_RENAME; // will not be evaluated
    }
    this->PostBusy(NULL);
}

bool CServer::RenTo(char *to) {
    this->PostBusy("RNTO");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "RNTO %s\r\n", to);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_RENAME;
        this->PostBusy(NULL);
        return (FALSE);
    }

    this->AddEntryToCache(to);
    this->PostBusy(NULL);
    return (this->RefreshFiles());
}

bool CServer::Nuke(char *dir) {
    this->PostBusy("SITE");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "SITE NUKE %s\r\n", dir);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_NUKE;
        this->PostBusy(NULL);
        return (FALSE);
    }
    this->PostBusy(NULL);
    return (this->RefreshFiles());
}

bool CServer::UnNuke(char *dir) {
    this->PostBusy("SITE");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "SITE UNNUKE %s\r\n", dir);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_UNNUKE;
        this->PostBusy(NULL);
        return (FALSE);
    }
    this->PostBusy(NULL);
    return (this->RefreshFiles());
}

bool CServer::Wipe(char *dir) {
    this->PostBusy("SITE");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "SITE WIPE -r %s\r\n", dir);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_WIPE;
        this->PostBusy(NULL);
        return (FALSE);
    }
    this->PostBusy(NULL);
    return (this->RefreshFiles());
}

bool CServer::UnDupe(char *dir) {
    this->PostBusy("SITE");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "SITE UNDUPE %s\r\n", dir);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_UNDUPE;
        this->PostBusy(NULL);
        return (FALSE);
    }
    this->PostBusy(NULL);
    return (this->RefreshFiles());
}

bool CServer::Pre(char *dir) {
    char *str, tmp[50];

    this->PostBusy("PREE");
    this->tcp.FlushStack();
    
    strcpy(tmp, this->prefs.pre_cmd[display->GetPreDest()]);

    str = strtok(tmp, " ");
    strcpy(this->temp_string, "SITE");
    while (str) { 
       debuglog(str);
       strcat(this->temp_string, " ");
       if (!strcmp(str, "%R")) strcat(this->temp_string, dir); 
       else strcat(this->temp_string, (char *)str);
       str = strtok(NULL, " ");
    }
    strcat(this->temp_string, "\r\n");

    if (!this->tcp.SendData(this->temp_string)) {
       this->error = E_CONTROL_RESET;                
       this->PostBusy(NULL);                
       return (FALSE);        
    }
    if (!this->tcp.WaitForMessage()) { 
       display->PostStatusLine("pre failed", FALSE);
       this->error = E_BAD_PRE;                
       this->PostBusy(NULL);                
       return (FALSE); 
    }
    sprintf(this->temp_string, "[%-8.8s ->      PRE] %s ", this->prefs.label, dir);
    display->PostStatusLine(this->temp_string, FALSE);

    this->PostBusy(NULL);
    return (this->RefreshFiles());
}

bool CServer::DeleteFile(char *file) {
    this->PostBusy("DELE");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "DELE %s\r\n", file);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_DELE;
        this->PostBusy(NULL);
        return (FALSE);
    }
    // okiez, all worked fine. we can post a msg to deselect the entry
    display->PostMessageFromServer(SERVER_MSG_DEMARK, this->magic, file);
    this->PostBusy(NULL);
    return (TRUE);
}

bool CServer::DeleteDir(char *dir) {
    this->PostBusy(" RMD");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "RMD %s\r\n", dir);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_RMD;
        this->PostBusy(NULL);
        return (FALSE);
    }
    // okiez, all worked fine. we can post a msg to deselect the entry
    display->PostMessageFromServer(SERVER_MSG_DEMARK, this->magic, dir);
    this->PostBusy(NULL);
    return (TRUE);
}

bool CServer::MakeDir(char *dir, bool try_cd, bool use_undo) {
    char *start, *end, *buffer, *real_dir = NULL, *current_dir = NULL, *creation_dir = NULL;
    bool fail = FALSE, use_real_name = FALSE;

	
	// Check if the dir-to-make is the same as the parent dir
	//display->PostStatusLine(this->working_dir, TRUE);    
    start = strrchr(this->working_dir, '/');
    
    if (start) {    	
        // extract last part
        start = start + 1;
        current_dir = new(char[strlen(start) + 1]);
        strcpy(current_dir, start);
    
	    if ((strstr(dir, "/")) != NULL) {    		    
	    	// okay absolute path given, manual create will be allowed, so if a manual create is done
	    	// the dirprotection part will not be done
	    	
	        // extract last part
	        start = strrchr(dir, '/');
	        start = start + 1;
	        creation_dir = new(char[strlen(start) + 1]);
	        strcpy(creation_dir, start);

		    if (strcmp(current_dir,creation_dir) == 0) {
				// uh oh, same dir 
				display->PostStatusLine("unable to create the same dir in dir", TRUE);
				this->PostBusy(NULL);
				return(FALSE);
			}
	        
		}
    } 

    start = NULL;

    this->PostBusy(" MKD");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "MKD %s\r\n", dir);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        if (!try_cd) {
            this->error = E_BAD_MKD;
            this->PostBusy(NULL);
            return (FALSE);
        } else
            use_real_name = TRUE;
    }
    // now get the real name of the created dir
    if (!use_real_name) {
        buffer = this->tcp.GetControlBuffer();
        start = strchr(buffer, '"');
        if (start) {
            start += 1;
            end = strchr(start, '"');

            if (end) {
                *end = '\0';
                strcpy(this->temp_string, start);
                *end = '"';

                // extract the last part of it
                start = strrchr(this->temp_string, '/');
                if (start) {
                    // extract last part
                    start = start + 1;
                    real_dir = new(char[strlen(start) + 1]);
                    strcpy(real_dir, start);
                } else {
                    // seems like we have no preceding path, use the full length
                    real_dir = new(char[strlen(this->temp_string) + 1]);
                    strcpy(real_dir, this->temp_string);
                }
            } else
                fail = TRUE;
        } else
            fail = TRUE;
    } else {
        real_dir = new(char[strlen(dir) + 1]);
        strcpy(real_dir, dir);
    }

    if (fail) {
        // our last chance, we take the original name... hmmrz
        real_dir = new(char[strlen(dir) + 1]);
        strcpy(real_dir, dir);
    }

    if (!use_real_name) {
        if (use_undo) {
            strcpy(this->undo_mkd, real_dir);
            this->have_undo = TRUE;
        }
        this->AddEntryToCache(real_dir);
        delete[](real_dir);
    }

    if (this->ChangeWorkingDir(dir)) {
        if (this->GetWorkingDir()) {
            this->PostBusy(NULL);
            return (this->RefreshFiles());
        } else {
            this->PostBusy(NULL);
            return (FALSE);
        }
    } else {
        this->PostBusy(NULL);
        return (FALSE);
    }
}

bool CServer::Noop(void) {
    this->PostBusy("IDLE");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "%s\r\n", prefs.noop_cmd);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_NOOP;
        this->PostBusy(NULL);
        return (FALSE);
    }
    this->PostBusy(NULL);
    return (TRUE);
}


bool CServer::ChangeWorkingDir(char *dir) {
    char *temp_filename;
    temp_filename = new(char[26]);

    this->PostBusy(" CWD");
    this->tcp.FlushStack();

    sprintf(this->temp_string, "CWD %s\r\n", dir);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->EvalError();
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (strlen(dir) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", dir, dir + strlen(dir) - 11);
    else
        sprintf(temp_filename, "'%s'", dir);

    if (!this->tcp.WaitForMessage()) {
        sprintf(this->temp_string, "[%-10.10s] failed   %-26.26s ( changing working dir    )",
                this->prefs.label, temp_filename);
        display->PostStatusLine(this->temp_string, FALSE);
        this->error = E_NO_ERROR;
        this->PostBusy(NULL);
        return (FALSE);
    }

    this->PostBusy(NULL);
    return (TRUE);
}

void CServer::LocalGetWorkingDir(void) {
    pthread_mutex_lock(&(this->cwd_lock));

    getcwd(this->working_dir, SERVER_WORKINGDIR_SIZE);

    pthread_mutex_unlock(&(this->cwd_lock));

    this->PostToDisplay(SERVER_MSG_NEW_CWD);
}

void CServer::LocalGetDirlist(void) {
    DIR *dir = NULL;
    struct dirent *dirent = NULL;
    int magic = 0;
    FILELIST *fl_temp = NULL, *fl_new = NULL;
    struct stat status;
    struct passwd *pw_ent = NULL;
    struct group *gr_ent = NULL;
    bool invalid;
    char *end = NULL, *start = NULL;

    dir = opendir(this->working_dir);

    if (dir) {
        // build up new list
        fl_temp = NULL;

        while ((dirent = readdir(dir))) {
            if (strcmp(dirent->d_name, ".")
                && strcmp(dirent->d_name, "..")) {

                fl_new = new(FILELIST);
                fl_new->next = NULL;
                fl_new->magic = magic;
                magic++;

                fl_new->name = new(char[strlen(dirent->d_name) + 1]);
                strcpy(fl_new->name, dirent->d_name);

                // now get additional information about file/dir
                sprintf(this->temp_string, "%s/%s", this->working_dir,
                        fl_new->name);
                stat(this->temp_string, &status);

                fl_new->is_dir = S_ISDIR(status.st_mode);
                fl_new->mode[0] = fl_new->is_dir ? 'd' : '-';
                fl_new->mode[1] = status.st_mode & S_IRUSR ? 'r' : '-';
                fl_new->mode[2] = status.st_mode & S_IWUSR ? 'w' : '-';
                fl_new->mode[3] = status.st_mode & S_IXUSR ? 'x' : '-';
                fl_new->mode[4] = status.st_mode & S_IRGRP ? 'r' : '-';
                fl_new->mode[5] = status.st_mode & S_IWGRP ? 'w' : '-';
                fl_new->mode[6] = status.st_mode & S_IXGRP ? 'x' : '-';
                fl_new->mode[7] = status.st_mode & S_IROTH ? 'r' : '-';
                fl_new->mode[8] = status.st_mode & S_IWOTH ? 'w' : '-';
                fl_new->mode[9] = status.st_mode & S_IXOTH ? 'x' : '-';
                fl_new->mode[10] = '\0';

                pw_ent = getpwuid(status.st_uid);
                if (pw_ent) {
                    strncpy(fl_new->owner, pw_ent->pw_name, 8);
                    fl_new->owner[8] = '\0';
                } else
                    strcpy(fl_new->owner, "<INVALD>");

                gr_ent = getgrgid(status.st_gid);
                if (gr_ent) {
                    strncpy(fl_new->group, gr_ent->gr_name, 8);
                    fl_new->group[8] = '\0';
                } else
                    strcpy(fl_new->group, "<INVALD>");

                fl_new->size = status.st_size;
                sprintf(this->temp_string, ctime(&status.st_mtime));

                invalid = FALSE;
                end = strrchr(this->temp_string, ':');
                if (end) {
                    *end = '\0';
                    start = strchr(this->temp_string, ' ');
                    if (start)
                        start += 1;
                    else
                        invalid = TRUE;
                } else
                    invalid = TRUE;

                if (!invalid)
                    strcpy(fl_new->date, start);
                else
                    strcpy(fl_new->date, "INVALID");

                if (fl_temp)
                    fl_temp->next = fl_new;
                else
                    this->internal_filelist = fl_new;

                fl_temp = fl_new;
            }
        }

        closedir(dir);

        this->SortFilelist(TRUE);
        this->PostToDisplay(SERVER_MSG_NEW_FILELIST);
    }
}

void CServer::PostFromDisplay(int msg, char *param) {
    this->PostFromDisplay(msg, param, 0);
}

void CServer::PostFromDisplay(int msg, char *param, int magic) {
    ACTION_MSG_LIST *msg_temp, *msg_new;

    pthread_mutex_lock(&(this->displaymsg_lock));

    msg_temp = this->display_msg_stack;
    msg_new = new(ACTION_MSG_LIST);
    msg_new->next = NULL;
    msg_new->msg = msg;
    msg_new->param = new(char[strlen(param) + 1]);
    strcpy(msg_new->param, param);
    msg_new->magic = magic;

    if (msg_temp) {
        while (msg_temp->next)
            msg_temp = msg_temp->next;

        msg_temp->next = msg_new;
    } else
        this->display_msg_stack = msg_new;

    pthread_mutex_unlock(&(this->displaymsg_lock));
}

void CServer::PostUrgentFromDisplay(int msg, char *param) { 
   this->PostUrgentFromDisplay(msg, param, 0);
}

void CServer::PostUrgentFromDisplay(int msg, char *param, int magic) {
    ACTION_MSG_LIST *msg_temp, *msg_new;

    pthread_mutex_lock(&(this->displaymsg_lock));

    msg_temp = this->display_msg_stack;
    msg_new = new(ACTION_MSG_LIST);
    msg_new->next = NULL;
    msg_new->msg = msg;
    msg_new->param = new(char[strlen(param) + 1]);
    strcpy(msg_new->param, param);
    msg_new->magic = magic;

    this->display_msg_stack = msg_new;
    msg_new->next = msg_temp;

    pthread_mutex_unlock(&(this->displaymsg_lock));
}

bool CServer::SetProt(int i) {
#ifdef TLS
    if (nossl == 1)
        return (TRUE);

    if (i == 1 && tcp.prot == 0) {
        sprintf(this->temp_string, "PROT P\r\n");
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_PROT;
            this->PostBusy(NULL);
            return (FALSE);
        }
        tcp.prot = 1;
    } else if (i == 0 && tcp.prot == 1) {
        sprintf(this->temp_string, "PROT C\r\n");
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_PROT;
            this->PostBusy(NULL);
            return (FALSE);
        }
        tcp.prot = 0;
    }
#endif

    return (TRUE);
}

bool CServer::Login(void) {
    bool no_login = FALSE;
    int n;

    for (n = 0; n < prefs.retry_counter + 1; n++) {
        this->ClearRetrying();
        this->PostBusy("CONN");
        if (prefs.use_ssl)
            sprintf(this->temp_string, "[%-10.10s] logging in ... (TLS mode)",
                    prefs.label);
        else
            sprintf(this->temp_string, "[%-10.10s] logging in ... (NO-TLS mode)",
                    prefs.label);

        display->PostStatusLine(this->temp_string, FALSE);
        if (!this->tcp.OpenControl(&(this->prefs))) {
            this->error = this->tcp.GetError();
            this->PostBusy(NULL);
            return (FALSE);
        }
        this->PostBusy("WLCM");

        // wait for welcome message
        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_WELCOME;
            this->PostBusy(NULL);
            return (FALSE);
        }

#ifdef TLS
        if (prefs.use_ssl) {
            this->PostBusy("AUTH");
            if (!this->tcp.SendData("AUTH TLS\r\n")) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                return (FALSE);
            }

            if (!this->tcp.WaitForMessage()) {
                this->error = E_BAD_AUTH;
                this->EvalError();
                nossl = 1;
            } else {
                if (!this->tcp.SecureControl()) {
                    //this->error = E_BAD_TLSCONN;
                    sprintf(this->temp_string, "[%-10.10s] %s", this->prefs.label,
                            E_MSG_BAD_TLSCONN);
                    display->PostStatusLine(this->temp_string, TRUE);
                } else {
                    /*
                    sprintf(this->temp_string,
                    "[%-10.10s] switched to TLS mode",
                    this->prefs.label);
                    */

                    display->PostMessageFromServer(SERVER_MSG_NEWLABEL,
                                                   this->magic, prefs.label);
                    //display->PostStatusLine(temp_string, FALSE);
                }
            }
        } else {
            /*
            sprintf(this->temp_string, "[%-10.10s] NO-TLS forced",
            this->prefs.label);
            display->PostStatusLine(temp_string, FALSE);
            */
            nossl = 1;
        }
#endif

        // send USER
        this->PostBusy("USER");
        sprintf(this->temp_string, "USER %s\r\n", this->prefs.user);
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_USER;
            this->PostBusy(NULL);
            return (FALSE);
        }
        // send PASS
        this->PostBusy("PASS");
        no_login = FALSE;

        sprintf(this->temp_string, "PASS %s\r\n", this->prefs.pass);
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->PostBusy(NULL);
            if (!prefs.use_retry || (n == prefs.retry_counter)) {
                this->error = E_BAD_PASS;
                return (FALSE);
            } else {
                no_login = TRUE;
            }
        } else {
            break;
        }

        if (prefs.use_retry && no_login) {
            sprintf(this->temp_string, "[%-10.10s] login failed, retry in %ds ... (%s)",
                    prefs.label, prefs.retry_delay, this->tcp.GetControlBuffer());
            display->PostStatusLine(this->temp_string, FALSE);
            this->tcp.CloseControl();
            this->SetRetrying();
            sleep(prefs.retry_delay);
        }
    }

#ifdef TLS
    if (nossl != 1) {
        this->PostBusy("PBSZ");
        /* now i dunno if this is needed but lets do it anyway : */
        sprintf(this->temp_string, "PBSZ 0\r\n");
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_PBSZ;
            this->PostBusy(NULL);
            return (FALSE);
        }
        /*
        this->PostBusy("PROT");

        if (prefs.use_ssl_list)
            if (!SetProt(1)) return (FALSE); //secure data
        else if (!SetProt(0)) return (FALSE); //secure data
        */
    }
#endif

    this->PostBusy("XDUP");
    sprintf(this->temp_string, "SITE XDUPE 3\r\n");
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    /*
    if (this->tcp.WaitForMessage()) {
    sprintf(this->temp_string, "[%-10.10s] XDUPE turned on",
    this->prefs.label);
    display->PostStatusLine(temp_string, FALSE);
    } else {
    */

    if (!this->tcp.WaitForMessage()) {
        sprintf(this->temp_string, "[%-10.10s] XDUPE not supported",
                this->prefs.label);
        display->PostStatusLine(temp_string, FALSE);
    }

    this->PostBusy("TYPE");
    // TYPE with error checking
    sprintf(this->temp_string, "TYPE I\r\n");
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        sprintf(this->temp_string, "[%-10.10s] failed   - TYPE command",
                this->prefs.label);
        display->PostStatusLine(temp_string, TRUE);
        this->PostBusy(NULL);
        return (FALSE); //this is major error, we bail out
    }
    this->type = TYPE_I;

    // first command
    if (prefs.use_firstcmd && prefs.first_cmd) {
        this->PostBusy("1CMD");
        sprintf(this->temp_string, "%s\r\n", prefs.first_cmd);
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            sprintf(this->temp_string, "[%-10.10s] failed   - 1ST CMD",
                    this->prefs.label);
            display->PostStatusLine(temp_string, TRUE);
            this->PostBusy(NULL);
        }
    }
    this->PostBusy(NULL);
    return (TRUE);
}

bool CServer::GetWorkingDir(void) {
    char *buffer;

    this->PostBusy(" PWD");
    this->tcp.FlushStack();

    if (!this->tcp.SendData("PWD\r\n")) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->error = E_BAD_PWD;
        this->PostBusy(NULL);
        return (FALSE);
    }
    buffer = this->tcp.GetControlBuffer();
    buffer = strchr(buffer, '"') + 1;

    pthread_mutex_lock(&(this->cwd_lock));
    if (!buffer || !strchr(buffer, '"')) {
        sprintf(this->temp_string,
                "[%-10.10s] failed   - getting current working dir",
                this->prefs.label);
        display->PostStatusLine(this->temp_string, TRUE);
        strcpy(this->working_dir, "INVALID");
    } else {
        *(strchr(buffer, '"')) = '\0';
        strcpy(this->working_dir, buffer);
    }
    pthread_mutex_unlock(&(this->cwd_lock));

    this->PostToDisplay(SERVER_MSG_NEW_CWD);
    this->PostBusy(NULL);
    return (TRUE);
}

bool CServer::DoFXP(int dest_magic, bool as_ok, bool quit) {
    // we have to start the queue
    // free the filelist
    // and unurgent the DEST
    FILELIST *fl_temp, *fl_temp1;

    // wait for dest to get in transfer mode
    int try_transfer = 0;
    //SERVERLIST *sv_temp = global_server;
    // we have to get the SRC server to start uploading
    // after that, we have to stop URGENT mode on this server :)

    this->stop_transfer = FALSE;

    while ((try_transfer < 80) && (destsrv->IsTransfering() == FALSE)) {
        try_transfer++;
        usleep(250000);
    }
    if (try_transfer == 80) {
        this->error = E_FXP_TIMEOUT;
        return (FALSE);
    }
    //display->PostStatusLine("FXP TRANSFER STARTING", FALSE);
    fl_temp = this->fl_src;
    while (fl_temp) {
        if (!fl_temp->is_dir) {
            if (as_ok == FALSE)
                this->PostFromDisplay(FOR_SERVER_MSG_FXP_FILE_SRC,
                                      fl_temp->name, dest_magic);
            else
                this->PostFromDisplay(FOR_SERVER_MSG_FXP_FILE_SRC_AS_OK,
                                      fl_temp->name, dest_magic);
        }
        fl_temp = fl_temp->next;
    }

    if (as_ok == FALSE) {
        fl_temp = this->fl_src;
        while (fl_temp) {
            if (fl_temp->is_dir) {
                this->PostFromDisplay(FOR_SERVER_MSG_FXP_DIR_SRC,
                                      fl_temp->name, dest_magic);
            }
            fl_temp = fl_temp->next;
        }
    }
    //ok added to queue, now free it
    fl_temp = this->fl_src;
    while (fl_temp) {
        fl_temp1 = fl_temp;
        fl_temp = fl_temp->next;
        delete[](fl_temp1->name);
        delete(fl_temp1);
    }
    this->fl_src = NULL;
    if (quit)
        this->PostFromDisplay(FOR_SERVER_MSG_CLOSE, "", 0);
    else
        this->PostFromDisplay(FOR_SERVER_MSG_FXP_SRC_UNURGENT, "", dest_magic);

    return (TRUE);
}

bool isin_fl(char *name, FILELIST *fl) {
	FILELIST *fl_temp;
	fl_temp = fl;
	while(fl_temp) { 
		if(strcmp(name, fl_temp->name) == 0) return TRUE;
		fl_temp = fl_temp->next;
	}
	return FALSE;
}

bool CServer::MagicTrade(int dest_magic) { 
	SERVERLIST *sv_temp;
	CServer *dest = NULL;
	FILELIST *fl_src = NULL, *fl_dest = NULL, *fl_temp;
	bool found = FALSE, magictrade = TRUE;
	bool filter;

	this->ClearTransfering();
	display->DrawSwitchPairs();

	sv_temp = global_server;
	while(sv_temp) { 
		if(sv_temp->server->GetMagic() == dest_magic) {
			dest = sv_temp->server;
			break;
		}
		sv_temp = sv_temp->next;
	}
	if (!dest) return FALSE; //wth?
	this->repeat_i = 0;
	while (this->use_repeat && magictrade) {
                if(!this->RefreshFiles()) { this->EvalError(); }
		fl_src = this->actual_filelist;
		if(!dest->RefreshFiles()) { dest->EvalError(); }
		fl_dest = dest->actual_filelist;
		fl_temp = fl_src;
		while (fl_temp) {
			//do the skiplist thingy here
	        if (this->prefs.use_exclude_source)
	            filter = FilterFilename(fl_temp->name, this->prefs.exclude_source);
	        else
	            filter = FilterFilename(fl_temp->name, NULL);
	            
	        if (filter) {		
				if(!isin_fl(fl_temp->name, fl_dest) && !fl_temp->is_dir && strncmp(".", fl_temp->name, 1) != 0 && fl_temp->size != 0) {
					if (!found) { 
						this->fxpsource = this->magic;
						this->fxpdest = dest_magic;
						this->SetTransfering();
						display->DrawSwitchPairs();
					}
	
					this->PostUrgentFromDisplay(FOR_SERVER_MSG_FXP_FILE_SRC, fl_temp->name, dest_magic);
					found = TRUE;
				} 
			}
			fl_temp = fl_temp->next;
		}


		if(!found) {
			this->ClearTransfering();
			display->DrawSwitchPairs();
			sleep(REPEAT_SLEEP);
			if(this->repeat_i++ > REPEAT_MAX - 2) magictrade = FALSE;
		} else { 
			this->repeat_i = 0; 
			break;
		}
		found = FALSE;
	} 
/*	if (!found) {
           if(!this->RefreshFiles()) { this->EvalError(); }
	   if(!dest->RefreshFiles()) { dest->EvalError(); }
        }*/
	return found;
}


bool CServer::DoFXPStop(int dest_magic) {
    display->PostStatusLine("FXP TRANSFER DONE", TRUE);
    this->ClearTransfering();
    display->DrawSwitchPairs();    
    display->PostMessageFromServer(SERVER_MSG_URGENT_OKAY, dest_magic, "");
    return (TRUE);
}

extern int
    DoCompare(FILELIST * src, FILELIST * dest, char *excl, int oldcurs);

bool CServer::DoFXPDir(char *ddirn, int dest_magic) {
    // uhm now the hard part
    // we CWD to the dir on both servers, get filelist
    // add to begining of a queue all the files in a dir (and also dirs)
    // and add CWD_UP to queue after the files

    FILELIST *fl_temp, *fl_head, *fl_head2;
    bool okay, filter;
    //SERVERLIST *sv_temp = global_server;
    bool dummy;
    ACTION_MSG_LIST *ac_temp, *ac_new, *ac_temp2;
    char fdir[SERVER_WORKINGDIR_SIZE];
    char sdirn[SERVER_WORKINGDIR_SIZE], dir[SERVER_WORKINGDIR_SIZE];
    char sdir[SERVER_WORKINGDIR_SIZE], ddir[SERVER_WORKINGDIR_SIZE];
    char *tmp, *temp_filename;
    temp_filename = new(char[26]);

    if (this->stop_transfer == TRUE)
        return (TRUE);

    okay = FALSE;
    this->tcp.FlushStack();
    destsrv->tcp.FlushStack();

    tmp = strrchr(ddirn, '/');
    if (!tmp)
        return (FALSE);

    *tmp = '\0';
    strcpy(ddir, ddirn);
    strcpy(dir, tmp + 1);
    *tmp = '/';

    this->ObtainWorkingDir(sdir);
    snprintf(sdirn, SERVER_WORKINGDIR_SIZE, "%s/%s", sdir, dir);

    // now CWD into dir and get filelist
    if (!this->ChangeWorkingDir(sdirn)) {
        this->EvalError();
        return (FALSE);
    }

    if (!this->GetWorkingDir()) {
        this->EvalError();
        //lets try to get back
        this->ChangeWorkingDir(sdir);
        this->GetWorkingDir();
        this->RefreshFiles();
        return (FALSE);
    }

    this->ObtainWorkingDir(fdir);

    // not following links !
    /*
    if (strcmp(fdir,sdirn)!=0) {
        this->ChangeWorkingDir(sdir);
        this->GetWorkingDir();
        this->RefreshFiles();
        return(FALSE);
    }
    */

    if (!this->RefreshFiles()) {
        this->EvalError();
        this->ChangeWorkingDir(sdir);
        this->GetWorkingDir();
        this->RefreshFiles();
        return (FALSE);
    }

    // lets check if the dir isnt empty first...
    // we dont really want to move empty dirs around sites, do we ?
    okay = FALSE;

    //resort the filelist to special mode where firstfiles are first
    this->internal_filelist = this->actual_filelist;
    this->actual_filelist = NULL;
    this->SortFilelist(FALSE, TRUE);

	//check if there is something interesting in the fl
    fl_head = this->ObtainFilelist(&dummy);
    fl_temp = fl_head;
    while ((okay == FALSE) && (fl_temp != NULL)) {
        if (this->prefs.use_exclude_source) {
            filter = FilterFilename(fl_temp->name, this->prefs.exclude_source);
        } else {
            filter = FilterFilename(fl_temp->name, NULL);
        }

        if (filter) {
            if (fl_temp->is_dir) {
                if (strcmp("..", fl_temp->name))
                    okay = TRUE;
            } else {
                if (fl_temp->size)
                    okay = TRUE;
            }
        }
        fl_temp = fl_temp->next;
    }

    if (okay == FALSE) {
        // found nothing enteresting... lets destroy the fl and cwd back
        while (fl_head) {
            fl_temp = fl_head;
            fl_head = fl_head->next;
            delete[](fl_temp->name);
            delete(fl_temp);
        }
        this->ChangeWorkingDir(sdir);
        this->GetWorkingDir();
        this->RefreshFiles();
        return (TRUE);
    }

    //sv_temp->server->ObtainWorkingDir(ddir);
    //snprintf(ddirn,SERVER_WORKINGDIR_SIZE,"%s/%s",ddir,dir);

    //ok lets try to create a dir and get in
    if (!destsrv->MakeDir(ddirn, TRUE, FALSE)) {
        while (fl_head) {
            fl_temp = fl_head;
            fl_head = fl_head->next;
            delete[](fl_temp->name);
            delete(fl_temp);
        }
        destsrv->EvalError();
        this->ChangeWorkingDir(sdir);
        this->GetWorkingDir();
        this->RefreshFiles();
        destsrv->ChangeWorkingDir(ddir);
        destsrv->GetWorkingDir();
        destsrv->RefreshFiles();
        return (FALSE);
    }
    destsrv->ObtainWorkingDir(fdir);

    fl_head2 = destsrv->ObtainFilelist(&dummy);
    DoCompare(fl_head, fl_head2, this->GetFilter(), -1);

    // now we are going to create the queue
    // grrr this is a bitch to do...

    if (strlen(dir) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", dir, dir + strlen(dir) - 11);
    else
        sprintf(temp_filename, "'%s'", dir);

    sprintf(this->temp_string, "[%-10.10s] XFERING  %-26.26s  ...",
            this->prefs.label, temp_filename);

    display->PostStatusLine(this->temp_string, FALSE);

    ac_temp = NULL;
    ac_new = NULL;
    ac_temp2 = NULL;

    // adding to queue
    //xcept skiplist files
    fl_temp = fl_head;
    while (fl_temp) {
        if (this->prefs.use_exclude_source)
            filter = FilterFilename(fl_temp->name, this->prefs.exclude_source);
        else
            filter = FilterFilename(fl_temp->name, NULL);

        if (filter) {
            if (!fl_temp->is_dir) {
                if (fl_temp->is_marked) {
                    display->PostMessageFromServer(SERVER_MSG_MARK, this->magic,
                                                   fl_temp->name);
                    // add a file to our new queue
                    ac_temp = new(ACTION_MSG_LIST);
                    ac_temp->next = NULL;
                    ac_temp->msg = FOR_SERVER_MSG_FXP_FILE_SRC;
                    ac_temp->param = new(char[strlen(fl_temp->name) + 1]);
                    strcpy(ac_temp->param, fl_temp->name);
                    ac_temp->magic = dest_magic;
                    if (ac_new == NULL) {
                        ac_new = ac_temp;
                    } else {
                        ac_temp2->next = ac_temp;
                    }
                    ac_temp2 = ac_temp;
                }
            } else {
                if (strcmp("..", fl_temp->name)) {
                    // add a dir to our new queue
                    ac_temp = new(ACTION_MSG_LIST);
                    ac_temp->next = NULL;
                    ac_temp->msg = FOR_SERVER_MSG_FXP_DIR_SRC;
                    ac_temp->param = new(char[strlen(fl_temp->name) + strlen(ddirn) + 2]);
                    strcpy(ac_temp->param, ddirn);
                    strcat(ac_temp->param, "/");
                    strcat(ac_temp->param, fl_temp->name);
                    ac_temp->magic = dest_magic;
                    if (ac_new == NULL) {
                        ac_new = ac_temp;
                    } else {
                        ac_temp2->next = ac_temp;
                    }
                    ac_temp2 = ac_temp;
                }
            }
        }
        fl_temp = fl_temp->next;
    }

    // free the fl
    while (fl_head) {
        fl_temp = fl_head;
        fl_head = fl_head->next;
        delete[](fl_temp->name);
        delete(fl_temp);
    }

    while (fl_head2) {
        fl_temp = fl_head2;
        fl_head2 = fl_head2->next;
        delete[](fl_temp->name);
        delete(fl_temp);
    }

    //add to the end of new queue FXP_CWD_UP
    if (ac_new != NULL) {
        pthread_mutex_lock(&(this->displaymsg_lock));
		if (this->use_repeat) { 
			//here should be the skiplist thingy for magictrade
			ac_temp = new(ACTION_MSG_LIST);
			ac_temp->msg = FOR_SERVER_MSG_MAGICTRADE;
			ac_temp->param = NULL;
			ac_temp->magic = dest_magic;
			ac_temp2->next = ac_temp;
			ac_temp2 = ac_temp2->next;
		}

        ac_temp = new(ACTION_MSG_LIST);
        //ac_temp->next = this->display_msg_stack;
        ac_temp->msg = FOR_SERVER_MSG_FXP_DIR_CWD_UP;
        ac_temp->param = new(char[strlen(sdir) + 1]);
        strcpy(ac_temp->param, sdir);
        ac_temp->magic = this->magic;
        ac_temp2->next = ac_temp;
        ac_temp2 = ac_temp2->next;

        ac_temp = new(ACTION_MSG_LIST);
        ac_temp->next = this->display_msg_stack;
        ac_temp->msg = FOR_SERVER_MSG_FXP_DIR_CWD_UP;
        ac_temp->param = new(char[strlen(ddir) + 1]);
        strcpy(ac_temp->param, ddir);
        ac_temp->magic = dest_magic;
        ac_temp2->next = ac_temp;

        this->display_msg_stack = ac_new;
        pthread_mutex_unlock(&(this->displaymsg_lock));
    } else {
        // we have made a dir and cwded to a dir and there is nothing to transfer
        // wtf :) hm nevermind lets go back, but not delete the dir
        // note : this happens usually when all files are already uploaded
        destsrv->ChangeWorkingDir(ddir);
        destsrv->GetWorkingDir();
        destsrv->RefreshFiles();
        this->ChangeWorkingDir(sdir);
        this->GetWorkingDir();
        this->RefreshFiles();
        return (FALSE);
    }
    return (TRUE);
}


bool CServer::DoFXPDirCWDUP(char *dir, int dest_magic) {
    char *temp_filename;
    temp_filename = new(char[26]);
    //SERVERLIST *sv_temp = global_server;

    if (dest_magic == this->magic) {
        debuglog("CWDUP for source");
        if (strlen(dir) > 24)
            sprintf(temp_filename, "'%-12.12s~%s'", dir, dir + strlen(dir) - 11);
        else
            sprintf(temp_filename, "'%s'", dir);

        sprintf(this->temp_string, "[%-10.10s] CWD UP   %-26.26s  ...",
                this->prefs.label, temp_filename);
        display->PostStatusLine(this->temp_string, FALSE);
        this->tcp.FlushStack();
        this->ChangeWorkingDir(dir);
        this->GetWorkingDir();
        this->RefreshFiles();
        return (TRUE);
    }

    debuglog("CWDUP for destination");

    destsrv->tcp.FlushStack();
    destsrv->ChangeWorkingDir(dir);
    destsrv->GetWorkingDir();
    destsrv->RefreshFiles();
    return (TRUE);
}

bool CServer::LeechDir(char *dir, int notice_type, bool as_ok,
                       int dest_magic) {
    FILELIST *fl_temp;
    bool okay, filter;

    // CWD into dir and get filelist

    if (!this->ChangeWorkingDir(dir))
        return (FALSE);

    if (!this->GetWorkingDir()) {
        this->EvalError();
        return (FALSE);
    }

    if (!this->RefreshFiles()) {
        this->EvalError();
        return (FALSE);
    }

    // post MKD notice
    switch (notice_type) {
        case NOTICE_TYPE_NOTICE_SINGLE:
            display->PostMessageFromServer(SERVER_MSG_NOTICE_LEECH_SINGLE_MKD,
                                           dest_magic, dir);
            break;

        case NOTICE_TYPE_NOTICE_MULTI:
            display->PostMessageFromServer(SERVER_MSG_NOTICE_LEECH_MULTI_MKD,
                                           this->magic, dir);
            break;
    }

    // now leech all files in the dir (and post correct notices)
    fl_temp = this->actual_filelist;
    okay = TRUE;
    while (okay && fl_temp) {
        if (!fl_temp->is_dir) {
            if (this->prefs.use_exclude_source)
                filter =
                    FilterFilename(fl_temp->name, this->prefs.exclude_source);
            else
                filter = FilterFilename(fl_temp->name, NULL);

            if (filter) {
                if (this->param)
                    delete[](this->param);

                this->param = new(char[strlen(fl_temp->name) + 1]);
                strcpy(this->param, fl_temp->name);

                if (!
                    (okay =
                         this->LeechFile(fl_temp->name, notice_type, as_ok,
                                         dest_magic))) {
                    // continue in case of DUPE
                    if (this->error == E_BAD_STOR)
                        okay = TRUE;

                    this->EvalError();
                }
            }
        }
        fl_temp = fl_temp->next;
    }

    if (!okay)
        return (FALSE);

    // now CWD back on this host (and all noticed)
    if (!this->ChangeWorkingDir(".."))
        return (FALSE);

    if (!this->GetWorkingDir()) {
        this->EvalError();
        return (FALSE);
    }

    if (!this->RefreshFiles()) {
        this->EvalError();
        return (FALSE);
    }
    // post CWD notice
    switch (notice_type) {
        case NOTICE_TYPE_NOTICE_SINGLE:
            display->PostMessageFromServer(SERVER_MSG_NOTICE_LEECH_SINGLE_CWD,
                                           dest_magic, "..");
            break;

        case NOTICE_TYPE_NOTICE_MULTI:
            display->PostMessageFromServer(SERVER_MSG_NOTICE_LEECH_MULTI_CWD,
                                           this->magic, "..");
            break;
    }
    return (TRUE);
}

bool CServer::LeechFile(char *file, int notice_type, bool as_ok,
                        int dest_magic) {
    char *end, *start, *buffer, name[SERVER_WORKINGDIR_SIZE], *temp_filename,
        *temp_size;
    char port_str[256], unit[5];
    long size;
    struct timeval before, after;
    float speed;
    FILE *localskip_test;
    temp_size = new(char[4]);
    temp_filename = new(char[26]);

    this->PostBusy("RETR");
    this->tcp.FlushStack();
    
    if (strlen(file) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", file, file + strlen(file) - 11);
    else
        sprintf(temp_filename, "'%s'", file);
    /*
    sprintf(this->temp_string,
            "[%-10.10s] getting  %-26.26s  ...", this->prefs.label,
            temp_filename);
    display->PostStatusLine(this->temp_string, FALSE);
    */
    // test if file already exists locally
    // so that we don't lose race to some aod lamer

    global_server->server->ObtainWorkingDir(name);
    sprintf(this->temp_string, "%s/%s", name, file);

    localskip_test = fopen(this->temp_string, "rb");
    if (localskip_test != NULL) {
        fclose(localskip_test);
        display->PostMessageFromServer(SERVER_MSG_DEMARK, this->magic, file);
        this->error = E_BAD_STOR;
        this->PostBusy(NULL);
        return (FALSE);
    }
    // so comment out the above if you trade 0day or something.. bleh

    //I is now default
    if (this->type != TYPE_I) {
        if (!this->tcp.SendData("TYPE I\r\n")) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_TYPE;
            this->PostBusy(NULL);
            return (FALSE);
        }
    }
    this->type = TYPE_I;

    /*
    if(!this->tcp.SendData("MODE S\r\n")) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return(FALSE);
     }

     this->tcp.WaitForMessage();
    */

   if (!SetProt(prefs.use_ssl_data ? 1 : 0)) {
        return (FALSE);
    }

    if (prefs.use_pasv == FALSE) {
        if (!this->tcp.OpenData(port_str, this->use_local)) {
            this->error = this->tcp.GetError();
            this->PostBusy(NULL);
            return (FALSE);
        }

        sprintf(this->temp_string, "PORT %s\r\n", port_str);
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->tcp.CloseData();
            this->error = E_BAD_PORT;
            this->PostBusy(NULL);
            return (FALSE);
        }
    } else {
        sprintf(this->temp_string, "PRET RETR %s\r\n", file);
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }
        this->tcp.WaitForMessage();
        if (!this->tcp.SendData("PASV\r\n")) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_PASV;
            this->PostBusy(NULL);
            return (FALSE);
        }

        /* now get the PASV string */

        buffer = this->tcp.GetControlBuffer();
        start = strrchr(buffer, '(');
        end = strrchr(buffer, ')');
        if ((start == NULL) || (end == NULL)) {
            this->error = E_BAD_PASV;
            this->PostBusy(NULL);
            return (FALSE);
        }
        start = start + 1;
        end = end - 1;
        strncpy(this->temp_string, start, end - start + 1);
        this->temp_string[end + 1 - start] = '\0';

        if (!this->tcp.OpenDataPASV(this->temp_string)) {
            this->error = this->tcp.GetError();
            this->PostBusy(NULL);
            return (FALSE);
        }
    }

    sprintf(this->temp_string, "RETR %s\r\n", file);
    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->tcp.CloseData();
        this->error = E_BAD_RETR;
        this->PostBusy(NULL);
        return (FALSE);
    }

    // welp, determine filesize
    size = -1;
    buffer = this->tcp.GetControlBuffer();
    start = strrchr(buffer, '(');
    if (start) {
        end = strchr(start, ' ');
        if (end) {
            start += 1;
            *end = '\0';
            size = atol(start);
            *end = ' ';
        }
    }

    if (!this->tcp.AcceptData()) {
        this->error = this->tcp.GetError();
        this->PostBusy(NULL);
        return (FALSE);
    }
    // build full dirname from local filesys
    global_server->server->ObtainWorkingDir(name);

    // now remove old .okay and .error files for this one
    sprintf(this->temp_string, "%s%s.okay", okay_dir, file);
    remove(this->temp_string);
    sprintf(this->temp_string, "%s%s.error", okay_dir, file);
    remove(this->temp_string);

    sprintf(this->temp_string, "%s/%s", name, file);

    // if we should notice the dest(s), do it now
    switch (notice_type) {
        case NOTICE_TYPE_NOTICE_SINGLE:
            if (!as_ok)
                display->PostMessageFromServer(SERVER_MSG_NOTICE_UPLOAD_SINGLE,
                                               dest_magic, file);
            else
                display->
                PostMessageFromServer
                (SERVER_MSG_NOTICE_UPLOAD_SINGLE_AS_OK, dest_magic, file);

            break;

        case NOTICE_TYPE_NOTICE_MULTI:
            display->PostMessageFromServer(SERVER_MSG_NOTICE_UPLOAD_MULTI,
                                           this->magic, file);
            break;
    }

    if (!this->tcp.ReadFile(this->temp_string, size)) {
        this->error = this->tcp.GetError();
        this->tcp.CloseData();
        this->tcp.WaitForMessage();
        this->PostBusy(NULL);
        return (FALSE);
    }

    this->tcp.CloseData();

    if (!this->tcp.WaitForMessage()) {
        this->error = this->tcp.GetError();
        this->PostBusy(NULL);
        return (FALSE);
    }
    // okiez, all worked fine. we can post a msg to deselect the entry
    display->PostMessageFromServer(SERVER_MSG_DEMARK, this->magic, file);

    this->tcp.GetTimevals(&before, &after, &size);

    if (size > 1000000000) {
        sprintf(temp_size, "%7.3fG", (float)size / 1024.0 / 1024.0 / 1024.0);
    } else if (size > 1048576) {
        sprintf(temp_size, "%5.1f mb", (float)size / 1024.0 / 1024.0);
    } else {
        sprintf(temp_size, "%8ld", size);
    }

    if (after.tv_sec >= before.tv_sec) {
        speed = (after.tv_sec - before.tv_sec) * 1000000 +
                (after.tv_usec - before.tv_usec) + 1;
        speed = ((float) size / ((float) (speed) / 1000000.0)) / 1024;
        if ( speed > 1024 ) { 	       
           speed = (float) speed / 1024.0;	       
           strcpy(unit,"mb/s"); 	    
        } else {	      
           strcpy(unit,"kb/s");	    
        }
	sprintf(this->temp_string, "[%-8.8s ->    local] got      %-26.26s %8.8s %7.01fl%s %*lds",
			           this->prefs.label, temp_filename, temp_size, speed, unit, 4,
				   (long) (after.tv_sec - before.tv_sec));
	display->PostStatusLine(this->temp_string, FALSE);
    }
    this->PostBusy(NULL);

    if (notice_type == NOTICE_TYPE_NOTICE_VIEW)
        display->PostMessageFromServer(SERVER_MSG_NOTICE_VIEW, this->magic,
                                       file);

    return (TRUE);
}

void CServer::UploadDirStart(char *dir) {
    SERVERLIST *local = global_server;

    // we have to get the LOCAL server to change dir and refresh the files
    // after that, he should notice us that he is finished, and we can continue

    local->server->PostFromDisplay(FOR_SERVER_MSG_UPLOAD_CWD, dir, this->magic);
}

bool CServer::UploadDir(char *dir, bool no_wait, bool as_ok) {
    SERVERLIST *local = global_server;
    FILELIST *fl_start, *fl_temp, *fl_temp1;
    bool dummy, okay, filter;

    // at this point, the LOCAL server already noticed us, that he has changed into the to-be-copied dir
    // now we need to create the dir on this server, CWD into and refresh files
    if (!this->MakeDir(dir, TRUE, FALSE)) {
        this->EvalError();
        return (FALSE);
    }

    // now obtain a filelist from the LOCAL server, and upload every file
    fl_start = local->server->ObtainFilelist(&dummy);

    fl_temp = fl_start;
    okay = TRUE;
    while (okay && fl_temp) {
        if (!fl_temp->is_dir) {
            if (this->prefs.use_exclude_source)
                filter =
                    FilterFilename(fl_temp->name, this->prefs.exclude_source);
            else
                filter = FilterFilename(fl_temp->name, NULL);

            if (filter) {
                if (this->param)
                    delete[](this->param);

                this->param = new(char[strlen(fl_temp->name) + 1]);
                strcpy(this->param, fl_temp->name);

                if (!
                    (okay =
                         this->UploadFile(fl_temp->name, no_wait, as_ok))) {
                    // continue in case of DUPE
                    if (this->error == E_BAD_STOR)
                        okay = TRUE;

                    this->EvalError();
                }
            }
        } else {
        	//Do the subfoldersfix here
        }
        fl_temp = fl_temp->next;
    }

    // free the obtained filelist
    fl_temp = fl_start;
    while (fl_temp) {
        fl_temp1 = fl_temp;
        fl_temp = fl_temp->next;
        delete[](fl_temp1->name);
        delete(fl_temp1);
    }

    if (!okay)
        return (FALSE);

    // now CWD back on this host and on the LOCAL filesys
    local->server->PostFromDisplay(FOR_SERVER_MSG_CWD, "..");

    if (!this->ChangeWorkingDir(".."))
        return (FALSE);

    if (!this->GetWorkingDir()) {
        this->EvalError();
        return (FALSE);
    }

    if (!this->RefreshFiles()) {
        this->EvalError();
        return (FALSE);
    }

    return (TRUE);
}

void CServer::PostStatusFile(char *filename, char *suffix) {
    FILE *file;
    char temp[256];

    sprintf(temp, "%s%d.%s.%s", okay_dir, this->pid, filename, suffix);

    file = fopen(temp, "w");
    fclose(file);
}

void CServer::PostStatusFile(char *filename, char *suffix, char *text) {
    FILE *file;
    char temp[256];

    sprintf(temp, "%s%d.%s.%s", okay_dir, this->pid, filename, suffix);

    file = fopen(temp, "w");
    fprintf(file, "%s", text);
    fclose(file);
}

bool CServer::WaitForStatusFile(int dpid, char *filename, char *suffix) {
    struct stat stat_out;
    char temp[256], temp2[256];
    int try_file = 0;

    sprintf(temp, "%s%d.%s.%s", okay_dir, dpid, filename, suffix);
    sprintf(temp2, "%s%d.%s.ABORT", okay_dir, dpid, filename);

    while ((try_file < 80) && (stat(temp, &stat_out) == -1)
           && (stat(temp2, &stat_out) == -1)) {
        try_file++;
        usleep(250000);
    }

    if ((stat(temp, &stat_out) == -1)) {
        remove(temp2);
        return (FALSE);
    } else
        remove(temp);

    return (TRUE);
}

bool CServer::WaitForStatusFile(int dpid, char *filename, char *suffix,
                                char *out) {
    struct stat stat_out;
    FILE *file;
    char temp[256], temp2[256];
    int try_file = 0;

    sprintf(temp, "%s%d.%s.%s", okay_dir, dpid, filename, suffix);
    sprintf(temp2, "%s%d.%s.ABORT", okay_dir, dpid, filename);

    while ((try_file < 80) && (stat(temp, &stat_out) == -1)
           && (stat(temp2, &stat_out) == -1)) {
        try_file++;
        usleep(250000);
    }

    if ((stat(temp, &stat_out) == -1)) {
        remove(temp2);
        return (FALSE);
    } else {
        file = fopen(temp, "r");
        fgets(out, 31, file);
        fclose(file);
        remove(temp);
    }

    return (TRUE);
}

void CServer::RemoveFromQueue(int msg, char *name) {
    ACTION_MSG_LIST *atmp, *atmp2;

    pthread_mutex_lock(&(this->displaymsg_lock));

    atmp = this->display_msg_stack;

    if ((atmp->msg == msg) && (strcasecmp(atmp->param, name) == 0)) {
        this->display_msg_stack = atmp->next;
        delete(atmp);
        //display->PostMessageFromServer(SERVER_MSG_DEMARK, this->magic, name);
    } else {
        while (atmp->next != NULL) {
            if ((atmp->next->msg == msg)
                && (strcasecmp(atmp->next->param, name) == 0)) {
                atmp2 = atmp->next;
                atmp->next = atmp2->next;
                delete(atmp2);
                //display->PostMessageFromServer(SERVER_MSG_DEMARK, this->magic, name);
                break;
            }
            atmp = atmp->next;
        }
    }

    display->PostMessageFromServer(SERVER_MSG_DEMARK, this->magic, name);

    pthread_mutex_unlock(&(this->displaymsg_lock));
}


bool CServer::DoFXPFile(char *file, bool as_ok, int destmagic) {
    char *buffer, *start, *end, port_msg[32], temp[256], *temp_filename, *temp_size;
    int len, dest_pid;
    //SERVERLIST *sv_temp = global_server;
    CServer *dest;
    struct timeval before, after;
    long size = -1;
    temp_size = new(char[4]);
    temp_filename = new(char[26]);
    float speed;
    XDUPELIST *xtmp;
    char unit[5];

    if (this->stop_transfer == TRUE)
        return (TRUE);

    /*
    while (sv_temp) {
        if (sv_temp->server->GetMagic() == destmagic) {
            break;
        }
            sv_temp = sv_temp->next;
    }
    if (!sv_temp)
        return (FALSE);
    */

    dest_pid = destsrv->GetPID();
    dest = destsrv;

    this->PostBusy("HSHK");
    this->tcp.FlushStack();

    dest->PostBusy("HSHK");
    dest->tcp.FlushStack();

    // TYPE without error checking (shrugs)
    if (this->type != TYPE_I) {
        this->tcp.SendData("TYPE I\r\n");
        this->tcp.WaitForMessage();
        this->type = TYPE_I;
    }
    // TYPE without error checking (shrugs)
    if (dest->type != TYPE_I) {
        dest->tcp.SendData("TYPE I\r\n");
        dest->tcp.WaitForMessage();
        dest->type = TYPE_I;
    }
    debuglog("after TYPE I");

    if (nossl != 1 && dest->nossl != 1 && prefs.use_ssl_fxp && dest->prefs.use_ssl_fxp) {
        //dest & src support ssl and want ssl_fxp
        if (!SetProt(1))
            return (FALSE); // secure data ON for fxp

        if (!dest->SetProt(1))
            return (FALSE); // secure data ON for fxp
    } else {
        if (!SetProt(0))
            return (FALSE); // secure data OFF for fxp

        if (!dest->SetProt(0))
            return (FALSE); // secure data OFF for fxp
    }

    if (strlen(this->param) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", this->param, this->param + strlen(this->param) - 11);
    else
        sprintf(temp_filename, "'%s'", this->param);

    if (fxpmethod == 0 && prefs.use_alt_fxp) {
        fxpmethod = 1;
    }

    if (fxpmethod == 0) {
        sprintf(this->temp_string, "PRET RETR %s\r\n", file);
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            dest->PostBusy(NULL);
            return (FALSE);
        }
        this->tcp.WaitForMessage();
        if (nossl != 1 && dest->nossl != 1 && prefs.use_ssl_fxp && dest->prefs.use_ssl_fxp) {
            // dest & src support ssl and want ssl_fxp
            // send CPSV, extract PORT info and post file, then wait (until DEST sent PORT and then STOR)
            if (!this->tcp.SendData("CPSV\r\n")) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                dest->PostBusy(NULL);
                //this->PostStatusFile(file, "ABORT");
                return (FALSE);
            }
        } else {
            // send PASV, extract PORT info and post file, then wait (until DEST sent PORT and then STOR)
            if (!this->tcp.SendData("PASV\r\n")) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                dest->PostBusy(NULL);
                //this->PostStatusFile(file, "ABORT");
                return (FALSE);
            }
        }

        if (!this->tcp.WaitForMessage()) {
            //ok pasv not accepted lets try to switch the fxp method to reverse
            fxpmethod = 1;
            sprintf(this->temp_string,
                    "[%-10.10s] rejected - PASV command, trying reversed FXP",
                    this->prefs.label);
            display->PostStatusLine(this->temp_string, TRUE);
            goto fxpmeth1;
        }
        buffer = this->tcp.GetControlBuffer();

        // extract PORT info
        start = strrchr(buffer, '(');
        end = strrchr(buffer, ')');
        if ((start == NULL) || (end == NULL)) {
            // bad pasv reply
            this->error = E_BAD_PASV; // error printed by SRC
            this->PostBusy(NULL);
            dest->PostBusy(NULL);
            //this->PostStatusFile(file, "ABORT");
            return (FALSE);
        }

        start = start + 1;
        end = end - 1;
        strncpy(port_msg, start, end - start + 1);
        len = end - start;
        *(port_msg + len + 1) = '\0';

        debuglog("got pasv reply");

        // send PORT str
        sprintf(temp, "PORT %s\r\n", port_msg);
        if (!dest->tcp.SendData(temp)) {
            this->error = E_CONTROL_RESET; // error printed by SRC
            dest->PostBusy(NULL);
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!dest->tcp.WaitForMessage()) {
            fxpmethod = 1;
            sprintf(this->temp_string,
                    "[%-10.10s] rejected - PORT command, trying reversed FXP",
                    dest->prefs.label);
            display->PostStatusLine(this->temp_string, TRUE);
            goto fxpmeth1;
            //sprintf(this->temp_string, "[%-10.10s] '%s': %s", sv_temp->server->prefs.label, this->param, E_MSG_BAD_PORT);
            //display->PostStatusLine(this->temp_string, TRUE);
            //this->error = E_NO_ERROR; // error printed by SRC
            //sv_temp->server->PostBusy(NULL);
            //this->PostBusy(NULL);
            //return(FALSE);
        }
        debuglog("PORT command sent");

        // try to up the file, issue STOR
        if (!as_ok)
            strcpy(this->temp_string, file);
        else {
            strcpy(temp, file);
            start = strrchr(temp, '.');
            if (start) {
                *start = '\0';
                sprintf(this->temp_string, "%s.%s.ok", temp, start + 1);
            } else
                sprintf(this->temp_string, "%s.ok", temp);
        }

        sprintf(temp, "STOR %s\r\n", this->temp_string);

        dest->tcp.PrepareXDupe();
        if (!dest->tcp.SendData(temp)) {
            dest->tcp.StopXDupe();
            this->error = E_CONTROL_RESET; // error printed by SRC
            dest->PostBusy(NULL);
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!dest->tcp.WaitForMessage()) {
            xtmp = dest->tcp.GetXDupe();
            // remove dupes from queue
            while (xtmp != NULL) {
                this->RemoveFromQueue(FOR_SERVER_MSG_FXP_FILE_SRC,
                                      xtmp->name);
                xtmp = xtmp->next;
            }
            // flush queue and stop scanning for xdupe
            dest->tcp.StopXDupe();

            //display->PostMessageFromServer(SERVER_MSG_DEMARK, this->magic, this->param);

            sprintf(this->temp_string, "[%-10.10s] rejected %-26.26s ( %-24.24s)", dest->prefs.label,
                    temp_filename, E_MSG_BAD_STOR);
            display->PostStatusLine(this->temp_string, TRUE);
            this->error = E_NO_ERROR; // error printed by SRC
            dest->PostBusy(NULL);
            this->PostBusy(NULL);
            return (FALSE);
        }
        dest->tcp.StopXDupe();

        debuglog("sent STOR");

        // okay, dest had a successful PORT, and STOR accepted the file. lets try a RETR
        sprintf(temp, "RETR %s\r\n", file);
        if (!this->tcp.SendData(temp)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            dest->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            // retr failed (not enuf creds or rights...)
            dest->PostBusy(NULL);
            // we do pasv to make stor stop waiting (nasty trick)
            if (!this->tcp.SendData("PASV\r\n")) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                //this->PostStatusFile(file, "ABORT");
                return (FALSE);
            }
            // we get pasv reply
            if (!this->tcp.WaitForMessage()) {
                this->error = E_BAD_PASV;
                this->PostBusy(NULL);
                //this->PostStatusFile(file, "ABORT");
                return (FALSE);
            }
            //and we get stor reply
            dest->tcp.WaitForMessage();
            // TODO :glbug
            // there is ugly glftpd bug in here
            // so give glftpd chance to give us its bad reply
            // and we just flush it
            // this is no big deal, coz this error shouldnt happen so much
            sleep(4);
            dest->tcp.FlushStack();

            this->error = E_BAD_RETR;
            this->PostBusy(NULL);
            //this->PostStatusFile(file, "ABORT");
            return (FALSE);
        }
        // extract file size (not correct if file is upped *shugs*)

        buffer = this->tcp.GetControlBuffer();

        debuglog("RETR sent");

        size = -1;
        start = strrchr(buffer, '(');
        if (start != NULL) {
            end = strchr(start, ' ');
            if (end != NULL) {
                start = start + 1;
                strncpy(port_msg, start, end - start + 1);
                len = end - start;
                *(port_msg + len + 1) = '\0';
                size = atol(port_msg);
            }
        }

        this->PostBusy("FXP>");
        dest->PostBusy("FXP<");
        this->StartTime();
        dest->StartTime();
        gettimeofday(&before, NULL);

        if (!this->tcp.WaitForMessage(0)) {
            // maybe disconect if this happens ?
            dest->tcp.WaitForMessage();
            this->error = E_PASV_FAILED; // PASV transfer failed
            dest->PostBusy(NULL);
            this->PostBusy(NULL);
            return (FALSE);
        }

        dest->tcp.PrepareXDupe();
        if (!dest->tcp.WaitForMessage(0)) {
            sprintf(this->temp_string, "[%-10.10s] rejected %-26.26s ( %-24.24s)",
                    dest->prefs.label, temp_filename, E_MSG_PASV_FAILED);
            display->PostStatusLine(this->temp_string, TRUE);
            this->error = E_NO_ERROR;   // error printed by SRC
            dest->PostBusy(NULL);
            this->PostBusy(NULL);
            return (FALSE);
        }
        xtmp = dest->tcp.GetXDupe();
        while (xtmp != NULL) {
            this->RemoveFromQueue(FOR_SERVER_MSG_FXP_FILE_SRC, xtmp->name);
            xtmp = xtmp->next;
        }
        dest->tcp.StopXDupe();
    }

fxpmeth1:
    if (fxpmethod == 1) {
        debuglog("normal fxp rejected trying alternative method");
        sprintf(this->temp_string, "PRET STOR %s\r\n", file);
        if (!dest->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            dest->PostBusy(NULL);
            return (FALSE);
        }
        dest->tcp.WaitForMessage();
        if (nossl != 1 && dest->nossl != 1 && prefs.use_ssl_fxp && dest->prefs.use_ssl_fxp) {
            // dest & src support ssl and want ssl_fxp
            // send CPSV, extract PORT info and post file, then wait (until DEST sent PORT and then RETR)
            if (!dest->tcp.SendData("CPSV\r\n")) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                dest->PostBusy(NULL);
                //this->PostStatusFile(file, "ABORT");
                return (FALSE);
            }
        } else {
            sprintf(this->temp_string, "PRET STOR %s\r\n", file);
            if (!this->tcp.SendData(this->temp_string)) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                return (FALSE);
            }
            this->tcp.WaitForMessage();
            // send PASV, extract PORT info and post file, then wait (until DEST sent PORT and then RETR)
            if (!dest->tcp.SendData("PASV\r\n")) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                dest->PostBusy(NULL);
                //this->PostStatusFile(file, "ABORT");
                return (FALSE);
            }
        }

        if (!dest->tcp.WaitForMessage()) {
            sprintf(this->temp_string, "[%-10.10s] rejected %-26.26s ( %-24.24s)",
                    dest->prefs.label, temp_filename, E_MSG_BAD_PASV);
            display->PostStatusLine(this->temp_string, TRUE);
            this->error = E_NO_ERROR; // error printed by SRC
            this->PostBusy(NULL);
            dest->PostBusy(NULL);
            //this->PostStatusFile(file, "ABORT");
            return (FALSE);
        }

        buffer = dest->tcp.GetControlBuffer();

        debuglog("got PASV reply2");

        // extract PORT info
        start = strrchr(buffer, '(');
        end = strrchr(buffer, ')');
        if ((start == NULL) || (end == NULL)) {
            sprintf(this->temp_string, "[%-10.10s] rejected %-26.26s ( %-24.24s)",
                    dest->prefs.label, temp_filename, E_MSG_BAD_PASV);
            display->PostStatusLine(this->temp_string, TRUE);
            this->error = E_NO_ERROR; // error printed by SRC
            this->PostBusy(NULL);
            dest->PostBusy(NULL);
            //this->PostStatusFile(file, "ABORT");
            return (FALSE);
        }
        end = end - 1;
        start = start + 1;
        strncpy(port_msg, start, end - start + 1);
        len = end - start;
        *(port_msg + len + 1) = '\0';

        // send PORT str
        sprintf(temp, "PORT %s\r\n", port_msg);
        if (!this->tcp.SendData(temp)) {
            this->error = E_CONTROL_RESET; // error printed by SRC
            dest->PostBusy(NULL);
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_PORT; // error printed by SRC
            dest->PostBusy(NULL);
            this->PostBusy(NULL);
            return (FALSE);
        }
        // try to up the file, issue STOR
        if (!as_ok)
            strcpy(this->temp_string, file);
        else {
            strcpy(temp, file);
            start = strrchr(temp, '.');
            if (start) {
                *start = '\0';
                sprintf(this->temp_string, "%s.%s.ok", temp, start + 1);
            } else
                sprintf(this->temp_string, "%s.ok", temp);
        }
        debuglog("before RETR");

        sprintf(temp, "RETR %s\r\n", file);
        if (!this->tcp.SendData(temp)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            dest->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            dest->PostBusy(NULL);
            this->error = E_BAD_RETR;
            this->PostBusy(NULL);
            return (FALSE);
        }

        debuglog("before STOR");
        sprintf(temp, "STOR %s\r\n", this->temp_string);
        dest->tcp.PrepareXDupe();
        if (!dest->tcp.SendData(temp)) {
            dest->tcp.StopXDupe();
            this->error = E_CONTROL_RESET; // error printed by SRC
            dest->PostBusy(NULL);
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!dest->tcp.WaitForMessage()) {
            xtmp = dest->tcp.GetXDupe();
            // remove dupes from queue

            while (xtmp != NULL) {
                this->RemoveFromQueue(FOR_SERVER_MSG_FXP_FILE_SRC, xtmp->name);
                xtmp = xtmp->next;
            }
            // flush queue and stop scanning for xdupe
            dest->tcp.StopXDupe();

            dest->PostBusy(NULL);
            this->PostBusy(NULL);

            // so we do pasv to make retr stop waiting

            // we do pasv to make retr stop waiting (nasty trick)
            if (!dest->tcp.SendData("PASV\r\n")) {
                this->error = E_CONTROL_RESET;
                return (FALSE);
            }
            // we get pasv reply
            if (!dest->tcp.WaitForMessage()) {
                sprintf(this->temp_string, "[%-10.10s] rejected %-26.26s ( %-24.24s)",
                        dest->prefs.label, temp_filename, E_MSG_BAD_PASV);
                display->PostStatusLine(this->temp_string, TRUE);
                return (FALSE);
            }
            // nd we get retr reply
            this->tcp.WaitForMessage();

            // TODO :glbug
            sleep(4);
            this->tcp.FlushStack();

            sprintf(this->temp_string, "[%-10.10s] rejected %-26.26s ( %-24.24s)",
                    dest->prefs.label, temp_filename, E_MSG_BAD_STOR);
            display->PostStatusLine(this->temp_string, FALSE);

            this->error = E_NO_ERROR; // error printed by SRC
            return (FALSE);
        }
        dest->tcp.StopXDupe();

        // extract file size (not correct if file is upped *shugs*)
        buffer = this->tcp.GetControlBuffer();

        size = -1;
        start = strrchr(buffer, '(');

        if (start != NULL) {
            end = strchr(start, ' ');
            if (end != NULL) {
                start = start + 1;
                strncpy(port_msg, start, end - start + 1);
                len = end - start;
                *(port_msg + len + 1) = '\0';
                size = atol(port_msg);
            }
        }
        //this->PostStatusFile(file, "RETR_OK", port_msg); // notify dest and GO!
        this->PostBusy("FXP>");
        dest->PostBusy("FXP<");
        this->StartTime();
        dest->StartTime();
        gettimeofday(&before, NULL);

        if (!this->tcp.WaitForMessage(0)) {
            this->error = E_PASV_FAILED; // PASV transfer failed
            dest->PostBusy(NULL);
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!dest->tcp.WaitForMessage(0)) {
            sprintf(this->temp_string, "[%-10.10s] rejected %-26.26s ( %-24.24s)",
                    dest->prefs.label, temp_filename, E_MSG_PASV_FAILED);
            display->PostStatusLine(this->temp_string, TRUE);
            this->error = E_NO_ERROR; // error printed by SRC
            dest->PostBusy(NULL);
            this->PostBusy(NULL);
            return (FALSE);
        }
    }
    debuglog("alternate fxp success");

    gettimeofday(&after, NULL);

    display->PostMessageFromServer(SERVER_MSG_DEMARK, this->magic, file);
    dest->AddEntryToCache(this->temp_string);

    temp_filename = new(char[26]);

    if (strlen(file) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", file, file + strlen(file) - 11);
    else
        sprintf(temp_filename, "'%s'", file);

    if (size > 1000000000) {
        sprintf(temp_size, "%7.3fG", (float)size / 1024.0 / 1024.0 / 1024.0);
    } else if (size > 1048576) {
        sprintf(temp_size, "%5.1f mb", (float)size / 1024.0 / 1024.0);
    } else {
        sprintf(temp_size, "%8ld", size);
    }

    if (size == -1) {
        sprintf(this->temp_string,
                "[%-8.8s -> %8.8s]  %-26.26s      n/a    n/a kb/s %*lds", this->prefs.label,
                dest->prefs.label, temp_filename, 4, (long) (after.tv_sec - before.tv_sec));
    } else if (before.tv_sec <= after.tv_sec) {
        speed = (after.tv_sec - before.tv_sec) * 1000000.0 +
                (after.tv_usec - before.tv_usec) + 1;
        speed = ((float) size / ((float) (speed) / 1000000.0)) / 1024;
        if ( speed > 1024 ) { 
	   speed = (float) speed / 1024.0;
	   strcpy(unit,"mb/s"); 
	} else {
	   strcpy(unit,"kb/s");
	}
        sprintf(this->temp_string,
                "[%-8.8s -> %8.8s]  %-26.26s %8.8s %7.01f %s %*lds", 
		this->prefs.label, dest->prefs.label, temp_filename, temp_size, speed, unit, 4,
                (long) (after.tv_sec - before.tv_sec));
    } else {
        sprintf(this->temp_string,
                "[%-8.8s -> %8.8s]  %-26.26s %8.8s    n/a kb/s %*lds", 
		this->prefs.label, dest->prefs.label, temp_filename, temp_size, 4,
                (long) (after.tv_sec - before.tv_sec));
    }
    display->PostStatusLine(this->temp_string, TRUE);

    this->PostBusy(NULL);
    dest->PostBusy(NULL);
    return (TRUE);
}

void CServer::StartTime(void) {
    gettimeofday(&(this->fxptime), NULL);
}

long CServer::GetTime() {
    struct timeval now;
    gettimeofday(&now, NULL);

    if (now.tv_sec < this->fxptime.tv_sec) {
        return 0;
    }
    return (now.tv_sec - this->fxptime.tv_sec);
}

bool CServer::UploadFile(char *file, bool no_wait, bool as_ok) {
    char *start, temp_name[256], name[SERVER_WORKINGDIR_SIZE], *temp_filename,
        *temp_size;
    char *end, *buffer;
    char port_str[256];
    temp_size = new(char[4]);
    temp_filename = new(char[26]);
    struct timeval before, after;
    long size;
    float speed;

    this->PostBusy("STOR");
    this->tcp.FlushStack();

    if (strlen(file) > 24)
        sprintf(temp_filename, "'%-12.12s~%s'", file, file + strlen(file) - 11);
    else
        sprintf(temp_filename, "'%s'", file);

    sprintf(this->temp_string, "[%7.7s]< putting  %-26.26s  ...", this->prefs.label,
            temp_filename);

    display->PostStatusLine(this->temp_string, FALSE);

    if (this->type != TYPE_I) {
        if (!this->tcp.SendData("TYPE I\r\n")) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_TYPE;
            this->PostBusy(NULL);
            return (FALSE);
        }

        this->type = TYPE_I;
    }

    /*
    if(!this->tcp.SendData("MODE S\r\n")) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return(FALSE);
    }

    this->tcp.WaitForMessage();
    */

   if (!SetProt(prefs.use_ssl_data ? 1 : 0)) {
        return (FALSE);
    }

    if (prefs.use_pasv == FALSE) {
        if (!this->tcp.OpenData(port_str, this->use_local)) {
            this->error = this->tcp.GetError();
            this->PostBusy(NULL);
            return (FALSE);
        }

        sprintf(this->temp_string, "PORT %s\r\n", port_str);
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->tcp.CloseData();
            this->error = E_BAD_PORT;
            this->PostBusy(NULL);
            return (FALSE);
        }
    } else {
        if (!this->tcp.SendData("PASV\r\n")) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_PASV;
            this->PostBusy(NULL);
            return (FALSE);
        }

        /* now get the PASV string */

        buffer = this->tcp.GetControlBuffer();
        start = strrchr(buffer, '(');
        end = strrchr(buffer, ')');
        if ((start == NULL) || (end == NULL)) {
            this->error = E_BAD_PASV;
            this->PostBusy(NULL);
            return (FALSE);
        }
        start = start + 1;
        end = end - 1;
        strncpy(this->temp_string, start, end - start + 1);
        this->temp_string[end + 1 - start] = '\0';

        if (!this->tcp.OpenDataPASV(this->temp_string)) {
            this->error = this->tcp.GetError();
            this->PostBusy(NULL);
            return (FALSE);
        }
    }

    if (!as_ok) {
        sprintf(this->temp_string, "STOR %s\r\n", file);
        this->AddEntryToCache(file);
    } else {
        strcpy(temp_name, file);
        start = strrchr(temp_name, '.');
        if (start) {
            *start = '\0';
            sprintf(this->temp_string, "%s.%s.ok", temp_name, start + 1);
            this->AddEntryToCache(this->temp_string);
            sprintf(this->temp_string, "STOR %s.%s.ok\r\n", temp_name,
                    start + 1);
        } else {
            sprintf(this->temp_string, "%s.ok", temp_name);
            this->AddEntryToCache(this->temp_string);
            sprintf(this->temp_string, "STOR %s.ok\r\n", temp_name);
        }
    }

    if (!this->tcp.SendData(this->temp_string)) {
        this->error = E_CONTROL_RESET;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.WaitForMessage()) {
        this->tcp.CloseData();
        this->error = E_BAD_STOR;
        this->PostBusy(NULL);
        return (FALSE);
    }

    if (!this->tcp.AcceptData()) {
        this->error = this->tcp.GetError();
        this->PostBusy(NULL);
        return (FALSE);
    }

    // build full dirname from local filesys
    global_server->server->ObtainWorkingDir(name);
    sprintf(this->temp_string, "%s/%s", name, file);

    if (!this->tcp.WriteFile(this->temp_string, no_wait)) {
        this->error = this->tcp.GetError();
        this->tcp.CloseData();
        this->tcp.WaitForMessage();
        this->PostBusy(NULL);
        return (FALSE);
    }

    this->tcp.CloseData();

    if (!this->tcp.WaitForMessage()) {
        this->error = this->tcp.GetError();
        this->PostBusy(NULL);
        return (FALSE);
    }

    this->tcp.GetTimevals(&before, &after, &size);

    if (size > 1000000000) {
        sprintf(temp_size, "%7.3fG", (float)size / 1024.0 / 1024.0 / 1024.0);
    } else if (size > 100000000) {
        sprintf(temp_size, "%7.1fM", (float)size / 1024.0 / 1024.0);
    } else if (size > 10000000) {
        sprintf(temp_size, "%7.3fM", (float)size / 1024.0 / 1024.0);
    } else {
        sprintf(temp_size, "%8ld", size);
    }

    speed = (after.tv_sec - before.tv_sec) * 1000000.0 +
            (after.tv_usec - before.tv_usec) + 1;
    speed = ((float) size / ((float) (speed) / 1000000.0)) / 1024;
    sprintf(this->temp_string,
            "[%7.7s]< put      %-26.26s |%8.8s %7.01fKbs %*lds|",
            this->prefs.label, temp_filename, temp_size, speed, 4,
            (long) (after.tv_sec - before.tv_sec));
    display->PostStatusLine(this->temp_string, FALSE);

    this->PostBusy(NULL);
    return (TRUE);
}

bool CServer::CheckStealth(char *filename) {
    char buffer[256];
    FILE *file;

    if ((file = fopen(filename, "r"))) {
        fgets(buffer, 255, file);
        fclose(file);
        if (strstr(buffer, "total "))
            return (TRUE);
        else {
            sprintf(this->temp_string,
                    "[%-10.10s] failed   - to get dirlist / stealth mode (wrong data)",
                    this->prefs.label);
            display->PostStatusLine(this->temp_string, TRUE);
            remove(filename);
            return (FALSE);
        }
    }

    sprintf(this->temp_string,
            "[%-10.10s] failed   - to get dirlist / stealth mode (file not found)",
            this->prefs.label);
    display->PostStatusLine(this->temp_string, TRUE);
    return (FALSE);
}

bool CServer::RefreshFiles(void) {
    char port_str[256];
    char *buffer, *start, *end;
    char temp_file[1024];
    //int file;
    FILE *file;
    extern int freeoldfl;

    // free old filelist
    if (freeoldfl) {
        FILELIST *fl_temp, *fl_temp1;

        pthread_mutex_lock(&(this->filelist_lock));
        fl_temp = this->actual_filelist;
        while (fl_temp) {
            fl_temp1 = fl_temp;
            fl_temp = fl_temp->next;
            delete[](fl_temp1->name);
            delete(fl_temp1);
        }
        fl_temp = this->actual_filelist = NULL;
        pthread_mutex_unlock(&(this->filelist_lock));
        this->PostToDisplay(SERVER_MSG_NEW_FILELIST);
    }

    if (!this->use_stealth) {
        this->PostBusy("LIST");
    } else
        this->PostBusy("STLT");

    this->tcp.FlushStack();

    //pthread_mutex_lock(&syscall_lock);
    //snprintf(temp_file,1023, "%spftpfxpXXXXXX", okay_dir);
    // crappy mkstemp doesnt seems to work right
    snprintf(temp_file, 1023, "%spftpfxp-%d-%d", okay_dir, getpid(), this->magic);

    //file = mkstemp(temp_file);
    file = fopen(temp_file, "w+");
    //pthread_mutex_unlock(&syscall_lock);
    debuglog("temp_file %s file %d errno %d", temp_file, file, errno);
    if (file == NULL) {
        this->error = E_BAD_TEMP_FILE;
        this->PostBusy(NULL);
        return (FALSE);
    }
    fclose(file);
    //remove(temp_file);

    if (this->use_stealth) {
        this->tcp.SetStealth(TRUE, temp_file);
        // stealth mode
        if (!this->tcp.SendData("STAT -la\r\n")) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }
        if (!this->tcp.WaitForMessage()) {
            this->tcp.SetStealth(FALSE, temp_file);
            this->error = E_BAD_STEALTH;
            this->PostBusy(NULL);
            return (FALSE);
        }

        this->tcp.SetStealth(FALSE, temp_file);
        if (this->CheckStealth(temp_file) == FALSE) return FALSE;
    }

    if (!this->use_stealth) {
        if (prefs.use_ssl_list) {
            if (!SetProt(1))
                return (FALSE); // secure data
        } else if (!SetProt(0))
            return (FALSE); // secure data

        if (this->type != TYPE_A) {
            if (!this->tcp.SendData("TYPE A\r\n")) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                return (FALSE);
            }

            if (!this->tcp.WaitForMessage()) {
                this->error = E_BAD_TYPE;
                this->PostBusy(NULL);
                return (FALSE);
            }
            this->type = TYPE_A;
        }

        if (prefs.use_pasv == FALSE) {
            if (!this->tcp.OpenData(port_str, this->use_local)) {
                this->error = this->tcp.GetError();
                this->PostBusy(NULL);
                return (FALSE);
            }
            sprintf(this->temp_string, "PORT %s\r\n", port_str);
            if (!this->tcp.SendData(this->temp_string)) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                return (FALSE);
            }

            if (!this->tcp.WaitForMessage()) {
                this->tcp.CloseData();
                this->error = E_BAD_PORT;
                this->PostBusy(NULL);
                return (FALSE);
            }
        } else { /* PASV MODE */
            if(!this->tcp.SendData("PRET LIST\r\n")) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                return (FALSE);
            }
            this->tcp.WaitForMessage();
            if (!this->tcp.SendData("PASV\r\n")) {
                this->error = E_CONTROL_RESET;
                this->PostBusy(NULL);
                return (FALSE);
            }

            if (!this->tcp.WaitForMessage()) {
                this->error = E_BAD_PASV;
                this->PostBusy(NULL);
                return (FALSE);
            }

            /* now get the PASV string */
            buffer = this->tcp.GetControlBuffer();
            start = strrchr(buffer, '(');
            end = strrchr(buffer, ')');
            if ((start == NULL) || (end == NULL)) {
                this->error = E_BAD_PASV;
                this->PostBusy(NULL);
                return (FALSE);
            }
            start = start + 1;
            end = end - 1;

            strncpy(this->temp_string, start, end - start + 1);
            this->temp_string[end + 1 - start] = '\0';

            if (!this->tcp.OpenDataPASV(this->temp_string)) {
                this->error = this->tcp.GetError();
                this->PostBusy(NULL);
                return (FALSE);
            }
        }
        debuglog("list");
        strcpy(this->temp_string, "LIST -la\r\n");
        if (!this->tcp.SendData(this->temp_string)) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->tcp.CloseData();
            this->error = E_BAD_LIST;
            this->PostBusy(NULL);
            return (FALSE);
        }
        debuglog("list done ");

        if (!this->tcp.AcceptData()) {
            this->error = this->tcp.GetError();
            this->PostBusy(NULL);
            return (FALSE);
        }
    }
    debuglog("recieved filelist");

    if (!this->use_stealth) {
        if (!this->tcp.ReadFile(temp_file, -1)) {
            this->tcp.CloseData();
            this->error = this->tcp.GetError();
            remove(temp_file);
            this->PostBusy(NULL);
            return (FALSE);
        }

        this->tcp.CloseData();

        if (!this->tcp.WaitForMessage()) {
            this->error = this->tcp.GetError();
            remove(temp_file);
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.SendData("TYPE I\r\n")) {
            this->error = E_CONTROL_RESET;
            this->PostBusy(NULL);
            return (FALSE);
        }

        if (!this->tcp.WaitForMessage()) {
            this->error = E_BAD_TYPE;
            this->PostBusy(NULL);
            return (FALSE);
        }
    }

    this->FormatFilelist(temp_file);
    this->PostBusy(NULL);
    return (TRUE);
}

void CServer::FormatFilelist(char *filename) {
    FILE *file_in;
    FILELIST *fl_temp, *fl_new;
    char *start, name[PATHLEN], dummy[12], date1[10], date2[10], date3[10];
    char owntemp[9], grptemp[9];
    int magic = 0, n, blocks;
    char *start_fix, *end_fix;
    char month_name[][5] = {"Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ",
                            "Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec "};
    bool use_fix, space;

    if ((file_in = fopen(filename, "r"))) {
        // build up new list
        fl_temp = NULL;

        do {
            fgets(this->temp_string, PATHLEN, file_in);
            if (!feof(file_in)) {
                if (strrchr(temp_string, '\r'))
                    *strrchr(temp_string, '\r') = '\0';

                if (strrchr(temp_string, '\n'))
                    *strrchr(temp_string, '\n') = '\0';

                StripANSI(this->temp_string);

                if (!strncmp(this->temp_string, "total", 5)) continue;

                fl_new = new(FILELIST);
                fl_new->next = NULL;

                // determine if we have all the needed fields
                // this one is tricky. first we need to determine the beginning of the date, since this is
                // the only stop-point that keeps beeing the same for all sites/dirs/dates/...
                // we cant rely on counting spaces since it could be a symlink or a file with a space
                n = 0;
                end_fix = NULL;
                use_fix = 0;
                while (!end_fix && (n < 12)) {
                    end_fix = strstr(this->temp_string, month_name[n]);
                    n++;
                }

                if (end_fix) {
                    // now we have the beginning of the date
                    // here we can count blocks to see if we have groups or not
                    start_fix = this->temp_string;
                    space = TRUE;
                    blocks = 0;
                    while (start_fix < end_fix) {
                        if (space) {
                            // look for a space
                            if (*start_fix == ' ') {
                                blocks++;
                                space = !space;
                            }
                        } else {
                            // look for a non-space
                            if (*start_fix != ' ') {
                                space = !space;
                            }
                        }
                        start_fix += 1;
                    }

                    switch (blocks) {
                        // normal
                        case 5:
                            use_fix = 0;
                            break;

                        // just no user
                        case 4:
                            use_fix = 1;
                            break;

                        // not even groups
                        case 3:
                            use_fix = 2;
                            break;

                        default:
                            // unknown dirlist line
                            debuglog("weird dirlist line, cant parse");
                            debuglog("line : '%s'", this->temp_string);
                            sprintf(this->temp_string, "[%-10.10s] error in dirlist, couldnt parse",
                                    this->prefs.label);
                            display->PostStatusLine(this->temp_string, TRUE);
                            continue;
                            break;
                    }
                } else {
                    debuglog("uhm, no month entry found in the dirlist line");
                    debuglog("line : '%s'", this->temp_string);
                    sprintf(this->temp_string, "[%-10.10s] no month entry found in dirlist line",
                            this->prefs.label);
                    display->PostStatusLine(this->temp_string, TRUE);
                    continue;
                }

                // break line into correct parts
                if (!use_fix) {
                    if (this->temp_string[0] != 'l') {
                        sscanf(this->temp_string,
                               "%s %s %s %s %ld %s %s %s",
                               fl_new->mode, dummy, owntemp, grptemp,
                               &(fl_new->size), date1, date2, date3);
                        strncpy(fl_new->owner, owntemp, 8);
                        fl_new->owner[8] = '\0';
                        strncpy(fl_new->group, grptemp, 8);
                        fl_new->group[8] = '\0';
                        start = strstr(this->temp_string, date3);
                        start += strlen(date3) + 1;
                        strcpy(name, start);
                        /*
                        if (strrchr(name, '\r'))
                            *strrchr(name, '\r') = '\0';
                        if (strrchr(name, '\n'))
                            *strrchr(name, '\n') = '\0';
                        */
                        fl_new->name = new(char[strlen(name) + 1]);
                        strcpy(fl_new->name, name);

                        if (this->temp_string[0] == 'd')
                            fl_new->is_dir = TRUE;
                        else
                            fl_new->is_dir = FALSE;

                        if (strlen(date2) > 1)
                            sprintf(fl_new->date, "%s %s %s", date1, date2, date3);
                        else
                            sprintf(fl_new->date, "%s  %s %s", date1, date2, date3);

                        fl_new->magic = magic;
                    } else {
                        sscanf(this->temp_string,
                               "%s %s %s %s %ld %s %s %s",
                               fl_new->mode, dummy, owntemp, grptemp,
                               &(fl_new->size), date1, date2, date3);
                        strncpy(fl_new->owner, owntemp, 8);
                        fl_new->owner[8] = '\0';
                        strncpy(fl_new->group, grptemp, 8);
                        fl_new->group[8] = '\0';
                        start = strstr(this->temp_string, " -> ");
                        if (start)
                            *start = '\0';

                        start = strstr(this->temp_string, date3);
                        start += strlen(date3) + 1;
                        strcpy(name, start);
                        if (strrchr(name, '\r'))
                            *strrchr(name, '\r') = '\0';

                        if (strrchr(name, '\n'))
                            *strrchr(name, '\n') = '\0';

                        fl_new->name = new(char[strlen(name) + 1]);
                        strcpy(fl_new->name, name);

                        if (this->temp_string[0] == 'l')
                            fl_new->is_dir = TRUE;
                        else
                            fl_new->is_dir = FALSE;

                        if (strlen(date2) > 1)
                            sprintf(fl_new->date, "%s %s %s", date1, date2, date3);
                        else
                            sprintf(fl_new->date, "%s  %s %s", date1, date2, date3);

                        fl_new->magic = magic;
                    }
                } else if (use_fix == 1) {
                    // use fix for sitez like STH (gftpd)
                    if (this->temp_string[0] != 'l') {
                        sscanf(this->temp_string,
                               "%s %s %s %ld %s %s %s", fl_new->mode,
                               dummy, owntemp, &(fl_new->size), date1,
                               date2, date3);
                        strncpy(fl_new->owner, owntemp, 8);
                        fl_new->owner[8] = '\0';
                        strcpy(fl_new->group, "none");
                        start = strstr(this->temp_string, date3);
                        start += strlen(date3) + 1;
                        strcpy(name, start);
                        if (strrchr(name, '\r'))
                            *strrchr(name, '\r') = '\0';

                        if (strrchr(name, '\n'))
                            *strrchr(name, '\n') = '\0';

                        fl_new->name = new(char[strlen(name) + 1]);
                        strcpy(fl_new->name, name);

                        if (this->temp_string[0] == 'd')
                            fl_new->is_dir = TRUE;
                        else
                            fl_new->is_dir = FALSE;

                        if (strlen(date2) > 1)
                            sprintf(fl_new->date, "%s %s %s", date1, date2, date3);
                        else
                            sprintf(fl_new->date, "%s  %s %s", date1, date2, date3);

                        fl_new->magic = magic;
                    } else {
                        sscanf(this->temp_string,
                               "%s %s %s %ld %s %s %s", fl_new->mode,
                               dummy, owntemp, &(fl_new->size), dummy,
                               dummy, dummy);
                        strncpy(fl_new->owner, owntemp, 8);
                        fl_new->owner[8] = '\0';
                        strcpy(fl_new->group, "none");

                        start = strstr(this->temp_string, " -> ");
                        if (start)
                            *start = '\0';

                        start = strstr(this->temp_string, dummy);

                        start += strlen(dummy) + 1;
                        strcpy(name, start);
                        if (strrchr(name, '\r'))
                            *strrchr(name, '\r') = '\0';

                        if (strrchr(name, '\n'))
                            *strrchr(name, '\n') = '\0';

                        fl_new->name = new(char[strlen(name) + 1]);
                        strcpy(fl_new->name, name);

                        if (this->temp_string[0] == 'l')
                            fl_new->is_dir = TRUE;
                        else
                            fl_new->is_dir = FALSE;

                        if (strlen(date2) > 1)
                            sprintf(fl_new->date, "%s %s %s", date1, date2, date3);
                        else
                            sprintf(fl_new->date, "%s  %s %s", date1, date2, date3);

                        fl_new->magic = magic;
                    }
                } else {
                    // use fix for entries with no user, no group (FUCK YOU)
                    strcpy(fl_new->owner, "none");
                    strcpy(fl_new->group, "none");

                    if (this->temp_string[0] != 'l') {
                        sscanf(this->temp_string, "%s %s %ld %s %s %s",
                               fl_new->mode, dummy, &(fl_new->size),
                               date1, date2, date3);
                        start = strstr(this->temp_string, date3);
                        start += strlen(date3) + 1;
                        strcpy(name, start);
                        if (strrchr(name, '\r'))
                            *strrchr(name, '\r') = '\0';

                        if (strrchr(name, '\n'))
                            *strrchr(name, '\n') = '\0';

                        fl_new->name = new(char[strlen(name) + 1]);
                        strcpy(fl_new->name, name);

                        if (this->temp_string[0] == 'd')
                            fl_new->is_dir = TRUE;
                        else
                            fl_new->is_dir = FALSE;

                        if (strlen(date2) > 1)
                            sprintf(fl_new->date, "%s %s %s", date1, date2, date3);
                        else
                            sprintf(fl_new->date, "%s  %s %s", date1, date2, date3);

                        fl_new->magic = magic;
                    } else {
                        sscanf(this->temp_string, "%s %s %ld %s %s %s",
                               fl_new->mode, dummy, &(fl_new->size),
                               dummy, dummy, dummy);
                        start = strstr(this->temp_string, " -> ");
                        if (start)
                            *start = '\0';

                        start = strstr(this->temp_string, dummy);
                        start += strlen(dummy) + 1;
                        strcpy(name, start);
                        if (strrchr(name, '\r'))
                            *strrchr(name, '\r') = '\0';

                        if (strrchr(name, '\n'))
                            *strrchr(name, '\n') = '\0';

                        fl_new->name = new(char[strlen(name) + 1]);
                        strcpy(fl_new->name, name);

                        if (this->temp_string[0] == 'l')
                            fl_new->is_dir = TRUE;
                        else
                            fl_new->is_dir = FALSE;

                        if (strlen(date2) > 1)
                            sprintf(fl_new->date, "%s %s %s", date1, date2, date3);
                        else
                            sprintf(fl_new->date, "%s  %s %s", date1, date2, date3);

                        fl_new->magic = magic;
                    }
                }

                if (strcmp(fl_new->name, ".")
                    && strcmp(fl_new->name, "..")) {
                    magic++;

                    if (fl_temp)
                        fl_temp->next = fl_new;
                    else
                        this->internal_filelist = fl_new;

                    fl_temp = fl_new;
                } else {
                    delete[](fl_new->name);
                    delete(fl_new);
                }
            }
        } while (!feof(file_in));

        fclose(file_in);
        // determine changings
        if (this->prefs.use_track)
            this->UseDirCache();

        this->SortFilelist(TRUE);
        this->PostToDisplay(SERVER_MSG_NEW_FILELIST);
    }
    remove(filename);
}

void CServer::AddEntryToCache(char *entry) {
    DIRCACHE *dc_temp = this->dir_cache;
    CACHELIST *cl_new;
    bool found = FALSE;

    // now lets see if we already have a list corresponding to our actual working dir
    while (!found && dc_temp) {
        if (!strcmp(dc_temp->dirname, this->working_dir))
            found = TRUE;
        else
            dc_temp = dc_temp->next;
    }

    if (found) {
        // add our entry
        cl_new = new(CACHELIST);
        cl_new->next = dc_temp->cachelist;
        cl_new->name = new(char[strlen(entry) + 1]);
        strcpy(cl_new->name, entry);
        dc_temp->cachelist = cl_new;
    }
}

void CServer::UseDirCache(void) {
    FILELIST *fl_temp;
    DIRCACHE *dc_temp1 = NULL, *dc_temp = this->dir_cache;
    CACHELIST *cl_temp, *cl_temp1;
    bool found = FALSE;
    char *temp_filename;
    int n;
    temp_filename = new(char[26]);

    // now lets see if we already have a list corresponding to our actual working dir
    while (!found && dc_temp) {
        if (!strcmp(dc_temp->dirname, this->working_dir))
            found = TRUE;
        else {
            dc_temp1 = dc_temp;
            dc_temp = dc_temp->next;
        }
    }

    if (found) {
        // okiez, let's see if we have new files/dirs
        fl_temp = this->internal_filelist;
        while (fl_temp) {
            cl_temp = dc_temp->cachelist;
            found = FALSE;
            while (!found && cl_temp) {
                if (!strcmp(fl_temp->name, cl_temp->name))
                    found = TRUE;
                else
                    cl_temp = cl_temp->next;
            }
            if (!found) {
                if (this->prefs.exclude_source)
                    found =
                        FilterFilename(fl_temp->name, this->prefs.exclude_source);
                else
                    found = FilterFilename(fl_temp->name, NULL);

                if (found) {
                    if (strlen(fl_temp->name) > 24)
                        sprintf(temp_filename, "'%-12.12s~%s'", fl_temp->name,
                                fl_temp->name + strlen(fl_temp->name) - 11);
                    else
                        sprintf(temp_filename, "'%s'", fl_temp->name);

                    if (fl_temp->is_dir) {
                        sprintf(this->temp_string, "[%-8.8s ->   NEWDIR] %-26.26s ( %11.11s / %-10.10s)",
                                this->prefs.label, temp_filename,
                                fl_temp->owner, fl_temp->group);

                    	display->PostStatusLine(this->temp_string, TRUE);
	            }
                    beep();
                }
            }
            fl_temp = fl_temp->next;
        }

        // free cached list
        if (!dc_temp1)
            this->dir_cache = dc_temp->next;
        else
            dc_temp1->next = dc_temp->next;

        cl_temp = dc_temp->cachelist;
        while (cl_temp) {
            cl_temp1 = cl_temp;
            cl_temp = cl_temp->next;
            delete[](cl_temp1->name);
            delete(cl_temp1);
        }

        delete[](dc_temp->dirname);
        delete(dc_temp);
    }
    // make our list as the new one (if we are allowed, depending on cache size)
    dc_temp1 = new(DIRCACHE);
    dc_temp1->dirname = new(char[strlen(this->working_dir) + 1]);
    strcpy(dc_temp1->dirname, this->working_dir);
    dc_temp1->cachelist = NULL;
    dc_temp1->next = this->dir_cache;

    fl_temp = this->internal_filelist;
    cl_temp1 = NULL;
    while (fl_temp) {
        cl_temp = new(CACHELIST);
        cl_temp->name = new(char[strlen(fl_temp->name) + 1]);
        strcpy(cl_temp->name, fl_temp->name);
        cl_temp->next = NULL;

        if (cl_temp1)
            cl_temp1->next = cl_temp;
        else
            dc_temp1->cachelist = cl_temp;

        cl_temp1 = cl_temp;
        fl_temp = fl_temp->next;
    }

    this->dir_cache = dc_temp1;

    // see if we should strip the oldest dircache
    dc_temp = this->dir_cache;
    n = 0;
    while (dc_temp) {
        n++;
        dc_temp = dc_temp->next;
    }

    if (n >= SERVER_DIR_CACHE) { // okay, kill the last list
        dc_temp = this->dir_cache;
        dc_temp1 = NULL;
        while (dc_temp->next) {
            dc_temp1 = dc_temp;
            dc_temp = dc_temp->next;
        }

        dc_temp1->next = NULL;
        delete[](dc_temp->dirname);
        cl_temp = dc_temp->cachelist;
        while (cl_temp) {
            cl_temp1 = cl_temp;
            cl_temp = cl_temp->next;
            delete[](cl_temp1->name);
            delete(cl_temp1);
        }
    }
}

/* The quoting character -- what overrides wildcards (do not undef)    */
#define QUOTE '\\'

/* The "matches ANYTHING" wildcard (do not undef)                      */
#define WILDS '*'

/* The "matches ANY NUMBER OF NON-SPACE CHARS" wildcard (do not undef) */
#define WILDP '%'

/* The "matches EXACTLY ONE CHARACTER" wildcard (do not undef)         */
#define WILDQ '?'
#define NOMATCH 0
#define UNQUOTED (0x7FFF)
#define QUOTED   (0x8000)
#define MATCH ((match+sofar)&UNQUOTED)

/*=======================================================================*
 * wild_match(char *ma, char *na)                                        *
 *                                                                       *
 * Features:  Backwards, case-insensitive, ?, *                          *
 * Best use:  Matching of hostmasks (since they are likely to begin with *
 *             a * rather than end with one).                            *
 *=======================================================================*/
int wild_match(register unsigned char *m, register unsigned char *n) {
    unsigned char *ma = m, *na = n, *lsm = 0, *lsn = 0;
    int match = 1;
    register int sofar = 0;

    /* take care of null strings (should never match) */
    if ((ma == 0) || (na == 0) || (!*ma) || (!*na))
        return NOMATCH;

    /* find the end of each string */
    while (*(++m));
    m--;
    while (*(++n));
    n--;

    while (n >= na) {
        if ((m <= ma) || (m[ -1] != QUOTE)) { /* Only look if no quote */
            switch (*m) {
                case WILDS: /* Matches anything */
                    do
                        m--; /* Zap redundant wilds */
                    while ((m >= ma) && ((*m == WILDS) || (*m == WILDP)));

                    if ((m >= ma) && (*m == '\\'))
                        m++; /* Keep quoted wildcard! */

                    lsm = m;
                    lsn = n;
                    match += sofar;
                    sofar = 0; /* Update fallback pos */
                    continue; /* Next char, please */

                case WILDQ:
                    m--;
                    n--;
                    continue; /* ? always matches */
            }
            sofar &= UNQUOTED; /* Remember not quoted */
        } else
            sofar |= QUOTED; /* Remember quoted */

        if (tolower(*m) == tolower(*n)) { /* If matching char */
            m--;
            n--;
            sofar++; /* Tally the match */
            if (sofar & QUOTED)
                m--; /* Skip the quote char */

            continue; /* Next char, please */
        }
        if (lsm) { /* To to fallback on * */
            n = --lsn;
            m = lsm;
            if (n < na)
                lsm = 0; /* Rewind to saved pos */

            sofar = 0;
            continue; /* Next char, please */
        }
        return NOMATCH; /* No fallback=No match */
    }
    while ((m >= ma) && ((*m == WILDS) || (*m == WILDP)))
        m--; /* Zap leftover %s & *s */

    return (m >= ma) ? NOMATCH : MATCH; /* Start of both = match */
}

int compare_alpha(const void *a, const void *b) {
    const FILELIST *aa, *bb;

    aa = (FILELIST *)(*((FILELIST **)a));
    bb = (FILELIST *)(*((FILELIST **)b));
    return strcasecmp(aa->name, bb->name);
}

int compare_alpha_rev(const void *a, const void *b) {
    const FILELIST *aa, *bb;

    aa = (FILELIST *)(*((FILELIST **)a));
    bb = (FILELIST *)(*((FILELIST **)b));
    return strcasecmp(bb->name, aa->name);
}

int compare_size(const void *a, const void *b) {
    const FILELIST *aa, *bb;

    aa = (FILELIST *)(*((FILELIST **)a));
    bb = (FILELIST *)(*((FILELIST **)b));

    if (aa->size > bb->size) {
        return -1;
    } else {
        if (aa->size == bb->size) {
            return strcasecmp(aa->name, bb->name);
        } else
            return 1;
    }
}

int compare_size_rev(const void *a, const void *b) {
    const FILELIST *aa, *bb;

    aa = (FILELIST *)(*((FILELIST **)a));
    bb = (FILELIST *)(*((FILELIST **)b));

    if (aa->size < bb->size) {
        return -1;
    } else {
        if (aa->size == bb->size) {
            return strcasecmp(bb->name, aa->name);
        } else
            return 1;
    }
}

int compare_time(const void *a, const void *b) {
    const FILELIST *aa, *bb;

    aa = (FILELIST *)(*((FILELIST **)a));
    bb = (FILELIST *)(*((FILELIST **)b));

    if (aa->time > bb->time) {
        return -1;
    } else {
        if (aa->time == bb->time) {
            return strcasecmp(aa->name, bb->name);
        } else
            return 1;
    }
}

int compare_time_rev(const void *a, const void *b) {
    const FILELIST *aa, *bb;

    aa = (FILELIST *)(*((FILELIST **)a));
    bb = (FILELIST *)(*((FILELIST **)b));

    if (aa->time < bb->time) {
        return -1;
    } else {
        if (aa->time == bb->time) {
            return strcasecmp(bb->name, aa->name);
        } else
            return 1;
    }
}

void CServer::SortFilelist(bool add_root, bool transfer_sort) {
    bool found, finished = FALSE;
    int m, year, mon;
    time_t elapsed_time;
    struct tm file_time;
    char tmp[32], month_name[][4] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                     "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
    FILELIST *fl_temp, *fl_temp1, *fl_temp2, *fl_new;
    int magic;
    extern int freeoldfl;

    pthread_mutex_lock(&(this->filelist_lock));

    // free old filelist
    fl_temp = this->actual_filelist;
    while (fl_temp) {
        fl_temp1 = fl_temp;
        fl_temp = fl_temp->next;
        delete[](fl_temp1->name);
        delete(fl_temp1);
    }
    fl_temp = this->actual_filelist = NULL;
    int dir_count = 0;
    int file_count = 0;

    // first calc the times for all entries
    // (used even if no chronosort is needed for newest files/dirs)
    elapsed_time = ::time(NULL);
    year = localtime(&elapsed_time)->tm_year;
    mon = localtime(&elapsed_time)->tm_mon;
    fl_temp = this->internal_filelist;
    while (fl_temp) {
        file_time.tm_sec = file_time.tm_min = file_time.tm_hour =
                                                  file_time.tm_isdst = 0;
        file_time.tm_year = year;

        // month
        strncpy(tmp, fl_temp->date, 3);
        tmp[3] = '\0';
        m = 0;
        found = FALSE;

        while (!found && (m < 12)) {
            if (!strcasecmp(tmp, month_name[m]))
                found = TRUE;
            else
                m++;
        }
        file_time.tm_mon = m;

        // day
        strncpy(tmp, fl_temp->date + 4, 2);
        tmp[2] = '\0';
        file_time.tm_mday = atoi(tmp);

        // hour & min or year
        strncpy(tmp, fl_temp->date + 7, 5);
        tmp[5] = '\0';
        if (strchr(tmp, ':')) {
            // k, was a valid time
            tmp[2] = '\0';
            file_time.tm_hour = atoi(tmp);
            file_time.tm_min = atoi(tmp + 3);
            if (file_time.tm_mon > mon)
                file_time.tm_year = year - 1;
        } else
            file_time.tm_year = atoi(tmp) - 1900;

        // generate overall seconds
        fl_temp->time = mktime(&file_time);
        if (fl_temp->is_dir)
            dir_count++;
        else
            file_count++;

        fl_temp = fl_temp->next;
    }

    // now to the real sorting
    fl_new = new(FILELIST);
    fl_new->next = NULL;
    fl_new->name = new(char[3]);
    strcpy(fl_new->name, "..");
    fl_new->is_dir = TRUE;
    fl_new->size = 0;
    strcpy(fl_new->owner, "<none>");
    strcpy(fl_new->group, "<none>");
    strcpy(fl_new->mode, "<none>    ");
    strcpy(fl_new->date, "            ");
    fl_new->time = 0;
    this->actual_filelist = fl_temp = fl_new;

    if (freeoldfl) {
        if (this->internal_filelist == NULL) {
            pthread_mutex_unlock(&(this->filelist_lock));
            return ;
        }
    }

    if (!add_root) {
        fl_temp1 = this->internal_filelist;
        this->internal_filelist = this->internal_filelist->next;
        delete[](fl_temp1->name);
        delete(fl_temp1);
        dir_count--;
    }

    if ((dir_count > 0) || (file_count > 0)) {
        // nifty quicksort attempt :P
        finished = FALSE;
        int f, d;
        FILELIST **dir_list, **file_list;
        dir_list = new FILELIST *[dir_count];
        file_list = new FILELIST *[file_count];

        //make arrays for the sorting
        fl_temp = this->internal_filelist;
        d = 0;
        f = 0;
        while (fl_temp != NULL) {
            if (fl_temp->is_dir) {
                dir_list[d] = fl_temp;
                d++;
            } else {
                file_list[f] = fl_temp;
                f++;
            }
            fl_temp = fl_temp->next;
        }

        // sort the arrays
        int (*func_dir_compare)(const void *, const void *);
        int (*func_file_compare)(const void *, const void *);

        if (dir_sort_reverse) {
            switch (dir_sort_method) {
                case SORTING_METHOD_ALPHA :
                    func_dir_compare = compare_alpha_rev;
                    break;

                case SORTING_METHOD_SIZE :
                    func_dir_compare = compare_size_rev;
                    break;

                default :
                    func_dir_compare = compare_time_rev;
                    break;
            }
        } else {
            switch (dir_sort_method) {
                case SORTING_METHOD_ALPHA :
                    func_dir_compare = compare_alpha;
                    break;

                case SORTING_METHOD_SIZE :
                    func_dir_compare = compare_size;
                    break;

                default :
                    func_dir_compare = compare_time;
                    break;
            }
        }

        if (file_sort_reverse) {
            switch (file_sort_method) {
                case SORTING_METHOD_ALPHA :
                    func_file_compare = compare_alpha_rev;
                    break;

                case SORTING_METHOD_SIZE :
                    func_file_compare = compare_size_rev;
                    break;

                default :
                    func_file_compare = compare_time_rev;
                    break;
            }
        } else {
            switch (file_sort_method) {
                case SORTING_METHOD_ALPHA :
                    func_file_compare = compare_alpha;
                    break;

                case SORTING_METHOD_SIZE :
                    func_file_compare = compare_size;
                    break;

                default :
                    func_file_compare = compare_time;
                    break;
            }
        }

        if (dir_count > 0) qsort((void *)dir_list, dir_count, sizeof(FILELIST *), func_dir_compare);
        if (file_count > 0) qsort((void *)file_list, file_count, sizeof(FILELIST *), func_file_compare);

        // fix the linked lists
        if (dir_count > 0) {
            //debuglog("sortfilelist - dir_count > 0 - %d", dir_count);
            for (d = 0; d < dir_count - 1; d++) {
                //debuglog("sortfilelist - dir_list loop %d", d);
                dir_list[d]->next = dir_list[d + 1];
            }
        }
        //debuglog("sortfilelist - after fix dir_list");

        if (file_count > 0) {
            //debuglog("sortfilelist - file_count > 0 - %d", file_count);
            for (f = 0; f < file_count - 1; f++) {
                //debuglog("sortfilelist - file_list loop %d", f);
                file_list[f]->next = file_list[f + 1];
            }
            //debuglog("sortfilelist - after file_list loop");
            file_list[file_count - 1]->next = NULL;
            if (dir_count > 0)
                dir_list[dir_count - 1]->next = file_list[0];
        } else {
            //debuglog("sortfilelist - file_count !> 0");
            if (dir_count > 0)
                dir_list[dir_count - 1]->next = NULL;
        }
        //debuglog("sortfilelist - after fix file_list");

        if (dir_count > 0)
            this->internal_filelist = dir_list[0];
        else
            this->internal_filelist = file_list[0];

        // delete the arrays
        delete[](dir_list);
        delete[](file_list);

        // done
        this->actual_filelist->next = this->internal_filelist;
        this->internal_filelist = NULL;

        // we start by moving non-nuked dirs to the front
        // pretty biased filter, should be removed oneday (disabled for now)
        /*
        fl_temp = this->actual_filelist->next; // first entry should be ".." !
        fl_temp1 = this->actual_filelist;
        fl_temp2 = this->actual_filelist;
        while (fl_temp) {
            if (fl_temp->is_dir) {
                if ((fl_temp->name[0] != '[') && (fl_temp->name[0] != '!')) {
                    // k, we found a dir. unlink the entry from it's original position
                    fl_temp1->next = fl_temp->next;
                    // now link after the last known directory
                    fl_new = fl_temp2->next;
                    fl_temp2->next = fl_temp;
                    fl_temp->next = fl_new;
                    // remember that position
                    fl_temp2 = fl_temp;
                }
            }
            fl_temp1 = fl_temp;
            fl_temp = fl_temp->next;
        }
        */

        char *dirs_end, *dirs_start; // *dirs_pattern;
        extern char firstdirsfilter[256];

        char *files_end, *files_start; // *files_pattern;
        extern char firstfilesfilter[256];

        fl_temp2 = this->actual_filelist;

        // starting sorting for building xfer queue
        if (transfer_sort == TRUE) {
            // now the patterns from FIRSTFILESFILTER should be first
            // lets pick them and move them to front
            files_start = firstfilesfilter;
            do {
                files_end = strchr(files_start, ' ');
                if (files_end)
                    *files_end = '\0';

                fl_temp = fl_temp2->next;
                fl_temp1 = fl_temp2;
                while (fl_temp) {
                    if (!fl_temp->is_dir
                        && (wild_match
                            ((unsigned char *) files_start,
                             (unsigned char *) fl_temp->name))) {
                        fl_temp1->next = fl_temp->next;
                        fl_new = fl_temp2->next;
                        fl_temp2->next = fl_temp;
                        fl_temp->next = fl_new;
                        fl_temp2 = fl_temp;
                    }
                    fl_temp1 = fl_temp;
                    fl_temp = fl_temp->next;
                }
                if (files_end) {
                    files_start = files_end + 1;
                    *files_end = ' ';
                } else
                    files_start = NULL;

            } while (files_start);

            // now the patterns from FIRSTDIRSFILTER should be first
            // lets pick them and move them to front
            dirs_start = firstdirsfilter;
            do {
                dirs_end = strchr(dirs_start, ' ');
                if (dirs_end)
                    *dirs_end = '\0';

                fl_temp = fl_temp2->next;
                fl_temp1 = fl_temp2;
                while (fl_temp) {
                    if (fl_temp->is_dir
                        && (wild_match
                            ((unsigned char *) dirs_start,
                             (unsigned char *) fl_temp->name))) {
                        fl_temp1->next = fl_temp->next;
                        fl_new = fl_temp2->next;
                        fl_temp2->next = fl_temp;
                        fl_temp->next = fl_new;
                        fl_temp2 = fl_temp;
                    }
                    fl_temp1 = fl_temp;
                    fl_temp = fl_temp->next;
                }

                if (dirs_end) {
                    dirs_start = dirs_end + 1;
                    *dirs_end = ' ';
                } else
                    dirs_start = NULL;

            } while (dirs_start);

            // lets pick the rest of the dirs and move them to front
            // right behind the FIRSTDIRS
            fl_temp = fl_temp2->next;
            fl_temp1 = fl_temp2;
            while (fl_temp) {
                if (fl_temp->is_dir) {
                    // k, we found a dir. unlink the entry from it's original position
                    fl_temp1->next = fl_temp->next;
                    // now link after the last known directory
                    fl_new = fl_temp2->next;
                    fl_temp2->next = fl_temp;
                    fl_temp->next = fl_new;
                    // remember that position
                    fl_temp2 = fl_temp;
                }
                fl_temp1 = fl_temp;
                fl_temp = fl_temp->next;
            }
        } else { // end of sorting for xfer queue, now general sorting

            // now the patterns from FIRSTDIRSFILTER should be first
            // lets pick them and move them to front
            dirs_start = firstdirsfilter;
            do {
                dirs_end = strchr(dirs_start, ' ');
                if (dirs_end)
                    *dirs_end = '\0';

                fl_temp = fl_temp2->next;
                fl_temp1 = fl_temp2;
                while (fl_temp) {
                    if (fl_temp->is_dir
                        && (wild_match
                            ((unsigned char *) dirs_start,
                             (unsigned char *) fl_temp->name))) {
                        fl_temp1->next = fl_temp->next;
                        fl_new = fl_temp2->next;
                        fl_temp2->next = fl_temp;
                        fl_temp->next = fl_new;
                        fl_temp2 = fl_temp;
                    }
                    fl_temp1 = fl_temp;
                    fl_temp = fl_temp->next;
                }
                if (dirs_end) {
                    dirs_start = dirs_end + 1;
                    *dirs_end = ' ';
                } else
                    dirs_start = NULL;

            } while (dirs_start);

            // lets pick the rest of the dirs and move them to front
            // right behind the FIRSTDIRS
            fl_temp = fl_temp2->next;
            fl_temp1 = fl_temp2;
            while (fl_temp) {
                if (fl_temp->is_dir) {
                    // k, we found a dir. unlink the entry from it's original position
                    fl_temp1->next = fl_temp->next;
                    // now link after the last known directory
                    fl_new = fl_temp2->next;
                    fl_temp2->next = fl_temp;
                    fl_temp->next = fl_new;
                    // remember that position
                    fl_temp2 = fl_temp;
                }
                fl_temp1 = fl_temp;
                fl_temp = fl_temp->next;
            }

            // now the patterns from FIRSTFILESFILTER should be first
            // lets pick them and move them to front
            files_start = firstfilesfilter;
            do {
                files_end = strchr(files_start, ' ');
                if (files_end)
                    *files_end = '\0';

                fl_temp = fl_temp2->next;
                fl_temp1 = fl_temp2;
                while (fl_temp) {
                    if (!fl_temp->is_dir
                        && (wild_match
                            ((unsigned char *) files_start,
                             (unsigned char *) fl_temp->name))) {
                        fl_temp1->next = fl_temp->next;
                        fl_new = fl_temp2->next;
                        fl_temp2->next = fl_temp;
                        fl_temp->next = fl_new;
                        fl_temp2 = fl_temp;
                    }
                    fl_temp1 = fl_temp;
                    fl_temp = fl_temp->next;
                }
                if (files_end) {
                    files_start = files_end + 1;
                    *files_end = ' ';
                } else
                    files_start = NULL;

            } while (files_start);
        }
    }

    // fixup magics
    fl_temp = this->actual_filelist;
    magic = 0;
    while (fl_temp) {
        fl_temp->magic = magic;
        magic++;
        fl_temp = fl_temp->next;
    }

    // delete internal list
    fl_temp = this->internal_filelist;
    while (fl_temp) {
        fl_temp1 = fl_temp;
        fl_temp = fl_temp->next;
        delete[](fl_temp1->name);
        delete(fl_temp1);
    }
    this->internal_filelist = NULL;

    pthread_mutex_unlock(&(this->filelist_lock));
}

char *CServer::GetAlias(void) {
    return (prefs.label);
}

void CServer::StopTransfer(void) {
    //SERVERLIST *sv_temp = global_server;

    // crash here plz
    //destsrv=NULL;
    //destsrv->GetMagic();

    if (this->GetMagic() == this->GetFXPSourceMagic()) {
        this->stop_transfer = TRUE;
    } else {
        assert(destsrv != NULL);
        destsrv->StopTransfer();
    }
}

