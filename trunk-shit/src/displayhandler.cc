#include <assert.h>
#include <string.h>
#include <curses.h>
#include <panel.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include "config.h"

#include "defines.h"
#include "tcp.h"
#include "server.h"
#include "displayhandler.h"
#include "keyhandler.h"

//extern bool is_away;
extern char okay_dir[1024];
extern void debuglog(char *fmt, ...);
extern CDisplayHandler *display;
extern CKeyHandler *keyhandler;
extern SERVERLIST *global_server;
extern BOOKMARK *global_bookmark;
extern bool no_autologin;
extern bool need_resize;
extern int mode, leftformat, rightformat;
extern char own_ip[256];
extern pthread_mutex_t sigwinch_lock;
extern char *localwindowlog[LOG_LINES];
extern pthread_mutex_t localwindowloglock;
extern char *sites2login;
extern char *section;
extern char *passwdcmdline;
extern char *connectSites;
#define RETSIGTYPE void
typedef RETSIGTYPE sigfunc(int);
#define SIGNAL_HANDLER(x) \
    RETSIGTYPE x (int sig)
extern SIGNAL_HANDLER(adjust);
extern sigfunc *my_signal(int sig_no, sigfunc * sig_handler);
/* Set if the window has changed it's size */
int winch_flag = 0;
unsigned int view_lrpos;
int old_ftp_mode = MODE_FTP_NOCHAIN, old_fxp_mode = MODE_FXP_NOCHAIN;
bool FilterFilename(char *filename, char *filter);
bool FilterDirname(char *filename, char *filter);
void FireupRemoteServer(CServer * server);

void flag_winch(int) {
    winch_flag = 1;
}

CDisplayHandler::CDisplayHandler() {
    int n;

    this->speed_left = this->speed_right = this->speed_left_old =
                                               this->speed_right_old = 0.0;
    this->time_left = this->time_right = 0;
    for (n = 0; n < STATUS_LOG; n++)
        this->statuslog[n] = NULL;

    for (n = 0; n < LOG_LINES; n++)
        this->log[n] = NULL;

    for (n = 0; n < JUMP_STACK; n++) {
        this->jump_stack_left[n] = -1;
//         this->jump_stack_right[n] = -1;
    }

    for (n = 0; n < 19; n++) {
        switch_list[n].used = FALSE;
    }

    this->jump_stack_left_pos = 0;
    this->jump_stack_right_pos = 0;

    this->leftwindow_busy = this->rightwindow_busy = NULL;

    debuglog("FILELIST L: %d, R: %d", leftformat, rightformat);
    this->filelist_left_format = leftformat;
    this->filelist_right_format = rightformat;
    this->filelist_left_soffset = this->filelist_right_soffset = 0;

    strcpy(this->window_left_cwd, "");
    strcpy(this->window_right_cwd, "");

    strcpy(this->input_temp, "");

    this->screen_too_small = FALSE;
    this->waiting = FALSE;
    this->status_line = 0;
    this->thread_attr_init = FALSE;
    this->thread_running = FALSE;
    this->default_windows = FALSE;
    this->status_win_size = 9;
    this->message_stack = NULL;
    this->server_message_stack = NULL;
    this->status_msg_stack = NULL;
    this->internal_state = DISPLAY_STATE_NORMAL;
    this->window_dialog = this->window_notice = this->window_input = NULL;
    this->bm_save = FALSE;
    this->leftwindow_magic = this->rightwindow_magic = -1;
    this->filelist_left = this->filelist_right = NULL;
    this->window_tab = this->window_prefs = NULL;

    pthread_mutex_init(&(this->message_lock), NULL);
    pthread_mutex_init(&(this->server_message_lock), NULL);
    pthread_mutex_init(&(this->status_lock), NULL);

    /*
    if(pthread_attr_init(&(display->thread_attr)))
        return(FALSE);
    else
        display->thread_attr_init = TRUE;

    if(pthread_attr_setdetachstate(&(display->thread_attr), PTHREAD_CREATE_DETACHED))
        return(FALSE);
    */

    pthread_attr_init(&(this->thread_attr));
    this->thread_attr_init = TRUE;

    //pthread_attr_setdetachstate(&(this->thread_attr), PTHREAD_CREATE_DETACHED);

    // init curses
    initscr();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    nonl();
    noecho();
    //cbreak();
    //timeout(0);
    //nodelay(stdscr,TRUE);
    raw();
    halfdelay(KEYBOARD_DELAY);
    //timeout(KEYBOARD_DELAY*1);
    //notimeout(stdscr,TRUE);
    curs_set(0);
}

CDisplayHandler::~CDisplayHandler() {
    MSG_LIST *msg_temp, *msg_temp1;
    SERVER_MSG_LIST *server_msg_temp, *server_msg_temp1;
    STATUS_MSG_LIST *status_msg_temp, *status_msg_temp1;
    int n;

    // following will never be execute by the displayhandler thread itself
    if (this->thread_running) {
        this->thread_running = FALSE;
        pthread_cancel(this->thread);
        pthread_join(this->thread, NULL);
    }

    if (this->thread_attr_init) {
        this->thread_attr_init = FALSE;
        pthread_attr_destroy(&(this->thread_attr));
    }

    if (this->default_windows) {
        this->default_windows = FALSE;

        wattrset(this->window_command, COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
        wbkgdset(this->window_command, ' ' | COLOR_PAIR(STYLE_WHITE));
        werase(this->window_command);
        //wbkgdset(this->window_command, ' ');

        wattrset(this->window_status, COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
        wbkgdset(this->window_status, ' ' | COLOR_PAIR(STYLE_WHITE));
        werase(this->window_status);
        //wbkgdset(this->window_status, ' ');

        wattrset(this->window_left, COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
        wbkgdset(this->window_left, ' ' | COLOR_PAIR(STYLE_WHITE));
        werase(this->window_left);
        //wbkgdset(this->window_left, ' ');

        wattrset(this->window_right, COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
        wbkgdset(this->window_right, ' ' | COLOR_PAIR(STYLE_WHITE));
        werase(this->window_right);
        //wbkgdset(this->window_right, ' ');

        update_panels();
        doupdate();

        del_panel(this->panel_command);
        del_panel(this->panel_status);
        del_panel(this->panel_left);
        del_panel(this->panel_right);

        delwin(this->window_command);
        delwin(this->window_status);
        delwin(this->window_left);
        delwin(this->window_right);
    }

    // shutdown curses
    curs_set(1);
    noraw();
    //nocbreak();
    echo();
    nl();
    keypad(stdscr, FALSE);
    erase();
    endwin();

    for (n = (STATUS_LOG - 1); n >= 0; n--) {
        delete[](this->statuslog[n]);
    }

    FILELIST *fl_temp, *fl_temp1;
    fl_temp = this->filelist_right;
    while (fl_temp) {
        fl_temp1 = fl_temp;
        fl_temp = fl_temp->next;
        delete[](fl_temp1->name);
        delete(fl_temp1);
    }
    fl_temp = this->filelist_left;
    while (fl_temp) {
        fl_temp1 = fl_temp;
        fl_temp = fl_temp->next;
        delete[](fl_temp1->name);
        delete(fl_temp1);
    }

    pthread_mutex_lock(&(this->message_lock));
    msg_temp = this->message_stack;
    while (msg_temp) {
        msg_temp1 = msg_temp;
        msg_temp = msg_temp->next;
        delete(msg_temp1);
    }
    pthread_mutex_unlock(&(this->message_lock));

    pthread_mutex_lock(&(this->server_message_lock));
    server_msg_temp = this->server_message_stack;
    while (server_msg_temp) {
        server_msg_temp1 = server_msg_temp;
        server_msg_temp = server_msg_temp->next;
        delete(server_msg_temp1);
    }
    pthread_mutex_unlock(&(this->server_message_lock));

    pthread_mutex_lock(&(this->status_lock));
    status_msg_temp = this->status_msg_stack;
    while (status_msg_temp) {
        status_msg_temp1 = status_msg_temp;
        status_msg_temp = status_msg_temp->next;
        delete[](status_msg_temp1->line);
        delete(status_msg_temp1);
    }
    pthread_mutex_unlock(&(this->status_lock));
}

bool CDisplayHandler::Init(void) {
    this->InitColors();
    this->DetermineScreenSize();
    if ((this->terminal_max_y < 30) || (this->terminal_max_x < 80)) {
        return FALSE;
    }
    this->OpenDefaultWindows();
    this->default_windows = TRUE;
    this->BuildScreen();
    return (TRUE);
}

void CDisplayHandler::DialogPre(void) { 
    this->window_pre = newwin(5, 60, this->terminal_max_y / 2 - 3, this->terminal_max_x / 2 - 30);
    this->panel_pre = new_panel(this->window_pre);

    wattrset(this->window_pre, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_pre, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_pre);
    wborder(this->window_pre, 0, 0, 0, 0, 0, 0, 0, 0);

    sprintf(this->temp_string, "pre   %-43.43s", this->input_temp);
    mvwaddnstr(this->window_pre, 1, 2, this->temp_string, 56);
    mvwaddnstr(this->window_pre, 2, 2, "to", 2);

    this->pre_buttonstate = 0;
    this->DialogPreUpdate();

}

void CDisplayHandler::DialogPreUpdate(void) { 
    extern char *prelabels[NUM_PRECOMMANDS];
    int r = 2, i = 0;
    for (int f = 0; f < NUM_PRECOMMANDS; f++) {
        if (f == this->pre_buttonstate)
            wattrset(this->window_pre,
                     COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);
        else
            wattrset(this->window_pre,
                     COLOR_PAIR(STYLE_MARKED) | A_BOLD);

        snprintf(this->temp_string, 8, "%s", prelabels[f]);
        if (NUM_PRECOMMANDS / 2 == f) { r++; i = 0; }
        mvwaddstr(this->window_pre, r, 8 + (i * 8), this->temp_string);
        i++;
    }

    update_panels();
    doupdate();

}

void CDisplayHandler::DialogPreClose(void) { 
    del_panel(this->panel_pre);    
    delwin(this->window_pre);    
    this->window_pre = NULL;    
    this->panel_pre = NULL;    
    update_panels();    
    doupdate();
}

void CDisplayHandler::HandleMessage(int msg, int extended, int function_key) {
    CServer *server;
    FILELIST *filelist, *fl_checkmark;
    int temp = 0, magic, ret;
    bool flag = FALSE;
    bool flag2 = FALSE;
    bool found = FALSE;
    extern char *sectionlabels[NUM_SECTIONS];
    extern char *prelabels[NUM_PRECOMMANDS];
 
    SERVERLIST *sv_temp = global_server;

    switch (this->internal_state) {
        case DISPLAY_STATE_NORMAL:

            switch (msg) {
                case MSG_KEY_UP:
                    this->UpdateFilelistScroll(FALSE, FALSE);
                    this->ResetTimer();
                    break;

                case MSG_KEY_DOWN:
                    this->UpdateFilelistScroll(TRUE, FALSE);
                    this->ResetTimer();
                    break;

                case MSG_KEY_END:
                    this->UpdateFilelistPageEnd(FALSE);
                    this->ResetTimer();
                    break;

                case MSG_KEY_HOME:
                    this->UpdateFilelistPageEnd(TRUE);
                    this->ResetTimer();
                    break;

                case MSG_KEY_PGUP:
                    this->UpdateFilelistPageMove(TRUE);
                    this->ResetTimer();
                    break;

                case MSG_KEY_PGDN:
                    this->UpdateFilelistPageMove(FALSE);
                    this->ResetTimer();
                    break;

                case MSG_KEY_LEFT:
                    if (this->window_tab == this->window_left)
                        this->filelist_left_soffset = 0;
                    else
                        this->filelist_right_soffset = 0;

                    this->ChangeDir(FOR_SERVER_MSG_CWD_UP);
                    this->ResetTimer();
                    break;

                case MSG_KEY_RIGHT:
                case MSG_KEY_RETURN:
                    if (this->window_tab == this->window_left)
                        this->filelist_left_soffset = 0;
                    else
                        this->filelist_right_soffset = 0;

                    this->ChangeDir(FOR_SERVER_MSG_CWD);
                    this->ResetTimer();
                    break;

                case MSG_KEY_TAB:
                    if (this->window_tab == this->window_left) {
                        //if(this->filelist_right)
                        this->window_tab = this->window_right;
                    } else {
                        //if(this->filelist_left)
                        this->window_tab = this->window_left;
                    }
                    this->UpdateTabBar(FALSE);
                    break;

                case MSG_KEY_STATUS_UP:
                    this->ScrollStatusUp();
                    break;

                case MSG_KEY_STATUS_DOWN:
                    this->ScrollStatusDown();
                    break;

                case MSG_DISPLAY_CHPASSWORD:
                    this->internal_state = DISPLAY_STATE_CHPASSWORD;
                    break;

                case MSG_DISPLAY_PASSWORD:
                    if (passwdcmdline) {
                        strcpy(this->custom_password, passwdcmdline);
                        if (this->ProbeBookmarkRC() != 2) {
                            if (!this->ReadBookmarks()) {
                                this->internal_state = DISPLAY_STATE_BADPASS;
                                this->NoticeNoMatch();
                            }
                        }
                        if (this->internal_state == DISPLAY_STATE_NORMAL) {
                            this->leftwindow_magic = SERVER_MAGIC_START;
                            this->window_tab = this->window_left;
                            strcpy(this->window_left_label, "localhost");
                            this->FireupLocalFilesys();
                            this->internal_state = DISPLAY_STATE_WELCOME;
                            this->AutoLogin();
                            this->CommandLineLogin();
                            this->DrawSwitchPairs();
                            //this->ShowWelcome();
                        }
                        } else {
                        this->internal_state = DISPLAY_STATE_PASSWORD;
                        this->OpenPasswordInput(TRUE);
                    }
                    break;

                case MSG_DISPLAY_NOBOOKMRK:
                    this->internal_state = DISPLAY_STATE_NOBKMRK;
                    this->bm_save = TRUE;
                    this->NoticeNoBookmark();
                    break;

                case MSG_KEY_ESCAPE:
                    switch ((char) extended) {
                        default:
                            if ((temp = this->IsOkSwitchKey(extended)) != -1) {   //fast server switch
                                this->AssignSwitch(temp);
                                this->DrawSwitchPairs();
                                break;
                            }
                            if ((extended >= 'a') && (extended <= 'z'))
                                this->UpdateFilelistByChar(extended);

                            //else if (extended==407)  // in case of suspend=CTRL-Z
                            //this->UpdateFilelistByChar('z');
                            break;
                    }
                    break;

                //case MSG_KEY_FUNCTION:
                //switch ((char) extended) {
                case MSG_KEY_EXTENDED:
                    switch ((char) function_key) {
                        // FUNCTION_BOOKMARK_DIALOG
                        case 1:
                            this->internal_state = DISPLAY_STATE_OPENSITE;
                            this->OpenSiteDialog();
                            break;

                        // FUNCTION_SITEPREFS_DIALOG
                        case 2:
                            if ((server = TryPrefs(&(this->siteopen_bm_realmagic)))) {
                                this->internal_state = DISPLAY_STATE_PREFS;
                                this->internal_state_previous = DISPLAY_STATE_NORMAL;
                                this->FillInfoForPrefs();
                                this->OpenPrefsDialog();
                                this->siteprefs_server = server;
                            } else {
                                this->internal_state = DISPLAY_STATE_NOTICE;
                                this->DialogNotice(NOTICE_NO_PREFS, DEFAULT_OKAY);
                            }
                            break;

                        // FUNCTION_CLOSESITE
                        case 3:
                            this->CloseSite();
                            break;

                        // FUNCTION_TOGGLEMARK
                        case 4:
                            this->FileToggleMark();
                            this->ResetTimer();
                            break;

                        // FUNCTION_MARK_ALL
                        case 5:
                            flag = TRUE;
                        // FUNCTION_DEMARK_ALL
                        case 6:
                            this->MarkFiles(flag);
                            this->ResetTimer();
                            break;

                        // FUNCTION_COMPARE
                        case 7:
                            CompareFiles();
                            this->ResetTimer();
                            break;

                        // FUNCTION_REFRESH_CHAINED
                        case 8:
                            flag = TRUE;
                        // FUNCTION_REFRESH
                        case 9:
                            this->RefreshSite(flag);
                            this->ResetTimer();
                            break;

                        // FUNCTION_DELETE
                        case 10:
                            this->DeleteFile();
                            this->ResetTimer();
                            break;

                        // FUNCTION_TRANSFER_QUIT
                        case 11:
                            flag2 = TRUE;
                        // FUNCTION_TRANSFER
                        case 12:
                            this->AutoAssignSwitch();
                            display->DrawSwitchPairs();
                            this->TransferFile(flag, flag2);
                            this->ResetTimer();
                            break;

                        // FUNCTION_TRANSFER_OK
                        case 13:
                            this->AutoAssignSwitch();
                            this->DrawSwitchPairs();
                            flag = TRUE;
                            this->TransferFile(flag, flag2);
                            this->ResetTimer();
                            break;

                        // FUNCTION_MAGICTRADE
                        case 14:
                            if (SAFE_MAGIC) {
			                   char slashstr1[3];
			                   char slashstr2[3];
			                   strcpy (slashstr1, "");
			                   strcpy (slashstr2, "");
			                   if (strcmp(this->window_left_cwd + strlen(window_left_cwd) - 1, "/") == 0) {
			                   	 strncat(slashstr1, this->window_left_cwd + strlen(window_left_cwd) - 4, 3);
			                   } 
			                   else {
			                   	 strncat(slashstr1, this->window_left_cwd + strlen(window_left_cwd) - 3, 3);
							   }                   	
			                   if (strcmp(this->window_right_cwd + strlen(window_right_cwd) - 1, "/") == 0) {
			                   	 strncat(slashstr2, this->window_right_cwd + strlen(window_right_cwd) - 4, 3);
			                   } 
			                   else {
			                   	 strncat(slashstr2, this->window_right_cwd + strlen(window_right_cwd) - 3, 3);
			                   }
						       if (strcmp(slashstr1, slashstr2) != 0) { 
						  	     this->PostStatusLine("you are not in the right dir do that", TRUE);
					             break;
					           }
			                }
                            this->AutoAssignSwitch();
                            this->DrawSwitchPairs();
                            RefreshSiteS();
                            CompareFiles();
                            if (this->window_tab == this->window_left) {
                                fl_checkmark = this->filelist_left;
                            } else {
                                fl_checkmark = filelist_right;
                            }
                            while (!found && fl_checkmark) {
                                if (fl_checkmark->is_marked)
                                    found = TRUE;
                                else
                                    fl_checkmark = fl_checkmark->next;
                            }
                            if (found) {
                                this->TransferFile(flag, flag2);
                            }
                            this->ResetTimer();
                            break;

                        // FUNCTION_PREPARE_INSIDE
                        case 15:
                            flag = TRUE;
                        // FUNCTION_PREPARE_OUTSIDE
                        case 16:
                            if (flag)
                                this->PrepDir(FOR_SERVER_MSG_PREP_IN);
                            else
                                this->PrepDir(FOR_SERVER_MSG_PREP);

                            this->ResetTimer();
                            break;

                        // FUNCTION_UNDO_PREPARED
                        case 17:
                            this->WipePrep();
                            this->ResetTimer();
                            break;

                        // FUNCTION_MAKEDIR_DIALOG
                        case 18:
                            strcpy(this->input_temp, "");
                            this->internal_state_previous = INPUT_DO_MKD;
                            this->internal_state = DISPLAY_STATE_INPUT;
                            this->DialogInput(DIALOG_ENTERMKD, this->input_temp, INPUT_TEMP_MAX, FALSE);
                            break;

                        // FUNCTION_RENAME_DIALOG_CHAINED
                        case 19:
                            flag = TRUE;
                        // FUNCTION_RENAME_DIALOG
                        case 20:
                            if (this->window_tab == this->window_left) {
                                filelist = this->filelist_left;
                                magic = this->filelist_left_magic;
                            } else {
                                filelist = this->filelist_right;
                                magic = this->filelist_right_magic;
                            }
                            if (filelist == NULL) {
                                break;
                            }
                            filelist = this->GetFilelistEntry(filelist, magic);
                            strcpy(this->old_input_temp, filelist->name);
                            strcpy(this->input_temp, filelist->name);

                            if (flag)
                                this->internal_state_previous = INPUT_DO_RENAME_CHAINED;
                            else
                                this->internal_state_previous = INPUT_DO_RENAME;

                            this->internal_state = DISPLAY_STATE_INPUT;
                            this->DialogInput(DIALOG_ENTERRENAME, this->input_temp, INPUT_TEMP_MAX, FALSE);
                            break;

                        // FUNCTION_NUKE_DIALOG
                        case 21:
                            if (this->window_tab == this->window_left) {
                                filelist = this->filelist_left;
                                magic = this->filelist_left_magic;
                            } else {
                                filelist = this->filelist_right;
                                magic = this->filelist_right_magic;
                            }
                            if (filelist == NULL) {
                                break;
                            }
                            filelist = this->GetFilelistEntry(filelist, magic);
                            strcpy(this->old_input_temp, filelist->name);
                            strcpy(this->input_temp, filelist->name);
                            strcat(this->input_temp, " 1");
                            this->internal_state_previous = INPUT_DO_NUKE;
                            this->internal_state = DISPLAY_STATE_INPUT;
                            this->DialogInput("NUKE - enter directory + factor + reason:", this->input_temp, INPUT_TEMP_MAX, FALSE);
                            break;

                        // FUNCTION_UNNUKE_DIALAOG
                        case 22:
                            if (this->window_tab == this->window_left) {
                                filelist = this->filelist_left;
                                magic = this->filelist_left_magic;
                            } else {
                                filelist = this->filelist_right;
                                magic = this->filelist_right_magic;
                            }
                            if (filelist == NULL) {
                                break;
                            }
                            filelist = this->GetFilelistEntry(filelist, magic);
                            strcpy(this->old_input_temp, filelist->name);
                            //temp = 0;
                            if (strncmp(filelist->name, this->nuke_prefix, strlen(this->nuke_prefix))) {
                                sprintf(this->temp_string, "[%7.7s]: not a NUKED dir", this->alias);
                                this->PostStatusLine(this->temp_string, TRUE);
                                break;
                            }
                            strcpy(this->input_temp, filelist->name + strlen(this->nuke_prefix));
                            /*
                            while ((filelist->name[temp] != '-') && (filelist->name[temp] != 0))
                            temp++;
                            while ((filelist->name[temp] == '-') && (filelist->name[temp] != 0))
                            temp++;
                            strcpy(this->input_temp, &filelist->name[temp]);
                            */
                            this->internal_state_previous = INPUT_DO_UNNUKE;
                            this->internal_state = DISPLAY_STATE_INPUT;
                            this->DialogInput("UNNUKE - enter directory + reason:", this->input_temp, INPUT_TEMP_MAX, FALSE);
                            break;

                        // FUNCTION_UNDUPE_DIALOG
                        case 23:
                            if (this->window_tab == this->window_left) {
                                filelist = this->filelist_left;
                                magic = this->filelist_left_magic;
                            } else {
                                filelist = this->filelist_right;
                                magic = this->filelist_right_magic;
                            }
                            if (filelist == NULL) {
                                break;
                            }
                            filelist = this->GetFilelistEntry(filelist, magic);
                            strcpy(this->old_input_temp, filelist->name);
                            strcpy(this->input_temp, filelist->name);
                            this->internal_state_previous = INPUT_DO_UNDUPE;
                            this->internal_state = DISPLAY_STATE_INPUT;
                            this->DialogInput("UNDUPE - enter filename or *mask:", this->input_temp, INPUT_TEMP_MAX, FALSE);
                            break;

                        // FUNCTION_WIPE_DIALOG
                        case 24:
                            if (this->window_tab == this->window_left) {
                                filelist = this->filelist_left;
                                magic = this->filelist_left_magic;
                            } else {
                                filelist = this->filelist_right;
                                magic = this->filelist_right_magic;
                            }
                            if (filelist == NULL) {
                                break;
                            }
                            filelist = this->GetFilelistEntry(filelist, magic);
                            strcpy(this->old_input_temp, filelist->name);
                            strcpy(this->input_temp, filelist->name);
                            this->internal_state_previous = INPUT_DO_WIPE;
                            this->internal_state = DISPLAY_STATE_INPUT;
                            this->DialogInput("WIPE - enter directory or filename:", this->input_temp, INPUT_TEMP_MAX, FALSE);
                            break;

                        // FUNCTION_SITECOMMAND_CHAINED_DIALOG
                        case 25:
                            flag = TRUE;
                        // FUNCTION_SITECOMMAND_DIALOG
                        case 26:
                            if ((ret = TrySite(this->window_tab)) == 2) {
                                this->internal_state = DISPLAY_STATE_XSITE;
                                this->DialogXSite(flag);
                            } else if (ret == 0) {
                                this->internal_state = DISPLAY_STATE_NOTICE;
                                this->DialogNotice(NOTICE_NO_XSITE, DEFAULT_OKAY);
                            }
                            break;

                        // FUNCTION_CWD_DIALOG
                        case 27:
                            if (this->window_tab == this->window_left)
                                strcpy(this->input_temp, this->window_left_cwd);
                            else
                                strcpy(this->input_temp, this->window_right_cwd);

                            this->internal_state_previous = INPUT_DO_CWD;
                            this->internal_state = DISPLAY_STATE_INPUT;
                            this->DialogInput(DIALOG_ENTERCWD, this->input_temp, INPUT_TEMP_MAX, FALSE);
                            break;

                        // FUNCTION_CWD_1
                        case 28:
                            this->Sections( -1);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_ROOT
                        case 29:
                            this->Sections(0);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC01
                        case 30:
                            this->Sections(1);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC02
                        case 31:
                            this->Sections(2);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC03
                        case 32:
                            this->Sections(3);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC04
                        case 33:
                            this->Sections(4);
                            this->ResetTimer();
                            break;

                        // FUNTION_CWD_SEC05
                        case 34:
                            this->Sections(5);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC06
                        case 35:
                            this->Sections(6);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC07
                        case 36:
                            this->Sections(7);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC08
                        case 37:
                            this->Sections(8);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC09
                        case 38:
                            this->Sections(9);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC10
                        case 39:
                            this->Sections(10);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC11
                        case 40:
                            this->Sections(11);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC12
                        case 41:
                            this->Sections(12);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC13
                        case 42:
                            this->Sections(13);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC14
                        case 43:
                            this->Sections(14);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC15
                        case 44:
                            this->Sections(15);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC16
                        case 45:
                            this->Sections(16);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC17
                        case 46:
                            this->Sections(17);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC18
                        case 47:
                            this->Sections(18);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC19
                        case 48:
                            this->Sections(19);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC20
                        case 49:
                            this->Sections(20);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC21
                        case 50:
                            this->Sections(21);
                            this->ResetTimer();
                            break;

                        // FUNCTION_CWD_SEC22
                        case 51:
                            this->Sections(22);
                            this->ResetTimer();
                            break;

                        // FUNCTION_VIEWFILE
                        case 52:
                            this->ViewFile();
                            break;

                        // FUNCTION_VIEWLOG
                        case 53:
                            if ((server = TryOpenLog())) {
                                this->internal_state = DISPLAY_STATE_LOG;
                                this->OpenLog(server);
                            } else {
                                this->internal_state = DISPLAY_STATE_NOTICE;
                                this->DialogNotice(NOTICE_NO_LOG, DEFAULT_OKAY);
                            }
                            break;

                        // FUNCTION_SCROLLSTATUS_UP
                        case 54:
                            this->ScrollStatusUp();
                            break;

                        // FUNCTION_SCROLLSTATUS_DOWN
                        case 55:
                            this->ScrollStatusDown();
                            break;

                        // FUNCTION_JUMP_TO_NEWEST
                        case 56:
                            flag = TRUE;
                        // FUNCTION_JUMP_THRUE_NEWEST
                        case 57:
                            if (this->window_tab == this->window_left) {
                                if (!flag)
                                    this->jump_stack_left_pos++;
                                else
                                    this->jump_stack_left_pos = 0;

                                if (this->jump_stack_left_pos >= JUMP_STACK)
                                    this->jump_stack_left_pos = 0;

                                if (this->jump_stack_left[this->jump_stack_left_pos] == -1)
                                    this->jump_stack_left_pos = 0;
                                else
                                    this->filelist_left_magic = this->jump_stack_left[this->jump_stack_left_pos];
                            } else {
                                if (!flag)
                                    this->jump_stack_right_pos++;
                                else
                                    this->jump_stack_right_pos = 0;

                                if (this->jump_stack_right_pos >= JUMP_STACK)
                                    this->jump_stack_right_pos = 0;

                                if (this->
                                    jump_stack_right[this->jump_stack_right_pos] == -1)
                                    this->jump_stack_right_pos = 0;
                                else
                                    this->filelist_right_magic = this->jump_stack_right[this->jump_stack_right_pos];
                            }
                            this->UpdateFilelistNewPosition(this->window_tab);
                            this->UpdateTabBar(FALSE);
                            this->ResetTimer();
                            break;

                        // FUNCTION_JUMP_SERVER_DIALOG
                        case 58:
                            this->internal_state = DISPLAY_STATE_SWITCH;
                            if (this->window_tab == this->window_left) {
                                this->OpenSwitchWindow(this->leftwindow_magic);
                            } else {
                                this->OpenSwitchWindow(this->rightwindow_magic);
                            }
                            break;

                        // FUNCTION_JUMP_SITE_FORWARD
                        case 59:
                            this->NextSwitch(TRUE);
                            break;

                        // FUNCTION_JUMP_SITE_BACKWARD
                        case 60:
                            this->NextSwitch(FALSE);
                            break;

                        // FUNCTION_SWITCH_LISTFORMAT_BACKWARD
                        case 61:
                            if (this->window_tab == this->window_left) {
                                if (this->filelist_left == NULL)
                                    break;

                                if (this->filelist_left_format == 0)
                                    this->filelist_left_format = 5;

                                this->filelist_left_format--;
                            } else {
                                if (this->filelist_right == NULL)
                                    break;

                                if (this->filelist_right_format == 0)
                                    this->filelist_right_format = 5;

                                this->filelist_right_format--;
                            }
                            this->UpdateFilelistNewPosition(this->window_tab);
                            this->UpdateTabBar(FALSE);
                            this->ResetTimer();
                            break;

                        // FUNCTION_SWITCH_LISTFORMAT_FORWARD
                        case 62:
                            if (this->window_tab == this->window_left) {
                                if (this->filelist_left == NULL)
                                    break;

                                this->filelist_left_format++;
                                if (this->filelist_left_format == 5)
                                    this->filelist_left_format = 0;
                            } else {
                                if (this->filelist_right == NULL)
                                    break;

                                this->filelist_right_format++;
                                if (this->filelist_right_format == 5)
                                    this->filelist_right_format = 0;
                            }
                            this->UpdateFilelistNewPosition(this->window_tab);
                            this->UpdateTabBar(FALSE);
                            this->ResetTimer();
                            break;

                        // FUNCTION_SWITCH_DIRSORTING
                        case 63:
                            this->ChangeDirSorting();
                            break;

                        // FUNCTION_SWITCH_FILESORTING
                        case 64:
                            this->ChangeFileSorting();
                            break;

                        // FUNCTION_SWITCH_TRANSFER_MODE
                        case 65:
                            if ((mode == MODE_FTP_CHAIN) || (mode == MODE_FTP_NOCHAIN)) {
                                old_ftp_mode = mode;
                                mode = old_fxp_mode;
                            } else {
                                old_fxp_mode = mode;
                                mode = old_ftp_mode;
                            }
                            this->UpdateMode();
                            break;

                        // FUNCTION_SWITCH_CHAIN_MODE
                        case 66:
                            if (mode == MODE_FTP_CHAIN)
                                mode = MODE_FTP_NOCHAIN;
                            else if (mode == MODE_FTP_NOCHAIN)
                                mode = MODE_FTP_CHAIN;

                            //else if(mode == MODE_FXP_CHAIN)
                            //    mode = MODE_FXP_NOCHAIN;
                            //else
                            //    mode = MODE_FXP_CHAIN;

                            this->UpdateMode();
                            break;

                        // FUNCTION_QUIT
                        case 67:
                            keyhandler->WantQuit(this->bm_save);
                            break;

                        // FUNCTION_SCROLLFILELIST_LEFT
                        case 68:
                            if (this->window_tab == this->window_left) {
                                this->filelist_left_soffset--;
                                if (this->filelist_left_soffset < 0)
                                    this->filelist_left_soffset = 0;
                            } else {
                                this->filelist_right_soffset--;
                                if (this->filelist_right_soffset < 0)
                                    this->filelist_right_soffset = 0;
                            }
                            this->UpdateFilelistNewPosition(this->window_tab);
                            this->UpdateTabBar(FALSE);
                            break;

                        // FUNCTION_SCROLLFILELIST_RIGHT
                        case 69:
                            if (this->window_tab == this->window_left) {
                                this->filelist_left_soffset++;
                                if (this->filelist_left_soffset > PATHLEN)
                                    this->filelist_left_soffset = PATHLEN;
                            } else {
                                this->filelist_right_soffset++;
                                if (this->filelist_right_soffset > PATHLEN)
                                    this->filelist_right_soffset = PATHLEN;
                            }
                            this->UpdateFilelistNewPosition(this->window_tab);
                            this->UpdateTabBar(FALSE);
                            break;

                        // FUNCTION_SWITCH_REVERSE_DIRSORTING
                        case 70:
                            this->ChangeReverseSorting(TRUE);
                            break;

                        // FUNCTION_SWITCH_REVERSE_FILESORTING
                        case 71:
                            this->ChangeReverseSorting(FALSE);
                            break;
                            
                        					       //FUNCTION_MAGICTRADE_REPEAT
			case 81:
			    sv_temp = global_server;
			    if (this->window_tab == this->window_left) temp = this->leftwindow_magic;
			    else temp = this->rightwindow_magic;
			    while (sv_temp) {
			    	if (sv_temp->magic == temp) {
			    		sv_temp->server->use_repeat = !sv_temp->server->use_repeat;
			    		break;
			    	}
            		    	sv_temp = sv_temp->next;
			    }
			    sprintf(this->temp_string, "[%-10s] repeated magic trade = ", sv_temp->server->GetAlias());
                            if (sv_temp->server->use_repeat)
			    	strcat(this->temp_string, "ON");
			    else 
			    	strcat(this->temp_string, "OFF");
			    this->PostStatusLine(this->temp_string, TRUE);
			    this->DrawSwitchPairs();
			    break;
			//FUNCTION_IDLESITE_QUIT
			case 98:
			    //while (sv_temp) {
			   //	if (sv_temp->server->GetAfk()) {
			//		//disconnect the site here
			//		this->KillServer(sv_temp->server);
			//	}
			//	sv_temp = sv_temp->next;
			//    }
			    break;				

			//FUNCTION_SITEPRE_DIALOG
			case 99:
			    if (this->window_tab == this->window_left) {
			        filelist = this->filelist_left;
			        magic = this->filelist_left_magic;
			    } else {
			        filelist = this->filelist_right;
			        magic = this->filelist_right_magic;
			    }
			    if (filelist == NULL) {
			        break;
			    }
			    filelist = this->GetFilelistEntry(filelist, magic);
			    strcpy(this->input_temp, filelist->name);
			    this->internal_state = DISPLAY_STATE_PRE;
                            this->DialogPre();
			    break;
                        
                        // FUNCTION_CHANGE_PASSWORD (we have passwd change in bookmarks)
                        /*
                        case 71:
                            this->internal_state = DISPLAY_STATE_CHPASSWORD;
                            strcpy(this->password_confirm, "");
                            this->OpenChangePasswordInput(FALSE);
                            break;
                        */
                        
                        default:
                            if ((temp = this->IsOkSwitchKey(extended)) != -1) {   //fast server switch
                                this->JumpSwitch(temp);
                            }
                            break;
                    } // end switch ((char) extended)
                    break; // end MSK_KEY_EXTENDED
                        
                /*      
                case MSG_KEY_EXTENDED:
                    switch ((char) extended) {
                        case 'Q':
                            keyhandler->WantQuit(this->bm_save);
                            break;
                        
                        default:
                            if ((extended >= '1') && (extended <= '9')) { //fast server switch
                                this->JumpSwitch(extended - '1');
                            }
                            break;
                    }   
                    break;*/
            }           
            break;      
                        
        case DISPLAY_STATE_INPUT:
                        
            switch (msg) {
                case MSG_KEY_EXTENDED:
                    this->InputAppendChar((char) extended);
                    break;

                case MSG_KEY_BACKSPACE:
                    this->InputBackspace();
                    break;

                case MSG_KEY_RIGHT:
                    this->InputRight();
                    break;

                case MSG_KEY_LEFT:
                    this->InputLeft();
                    break;

                case MSG_KEY_HOME:
                    this->InputHome();
                    break;

                case MSG_KEY_END:
                    this->InputEnd();
                    break;

                case MSG_KEY_DELLINE:
                    this->InputDelline();
                    break;

                case MSG_KEY_RETURN:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    this->CloseInput();

                    if (strlen(this->input_temp) > 0) {
                        switch (this->internal_state_previous) {
                            case INPUT_DO_MKD:
                                this->PostToServer(FOR_SERVER_MSG_MKD, TRUE,
                                                   MSG_TYPE_INPUT);
                                break;

                            case INPUT_DO_CWD:
                                this->PostToServer(FOR_SERVER_MSG_CWD, TRUE,
                                                   MSG_TYPE_INPUT);
                                break;

                            case INPUT_DO_RENAME:
                                this->PostToServer(FOR_SERVER_MSG_RENFROM, TRUE,
                                                   MSG_TYPE_OLDINPUT);
                                this->PostToServer(FOR_SERVER_MSG_RENTO, TRUE,
                                                   MSG_TYPE_INPUT);
                                break;

                            case INPUT_DO_NUKE:
                                this->PostToServer(FOR_SERVER_MSG_NUKE, TRUE,
                                                   MSG_TYPE_INPUT);
                                break;

                            case INPUT_DO_UNNUKE:
                                this->PostToServer(FOR_SERVER_MSG_UNNUKE, TRUE,
                                                   MSG_TYPE_INPUT);
                                break;

                            case INPUT_DO_UNDUPE:
                                this->PostToServer(FOR_SERVER_MSG_UNDUPE, TRUE,
                                                   MSG_TYPE_INPUT);
                                break;

                            case INPUT_DO_WIPE:
                                this->PostToServer(FOR_SERVER_MSG_WIPE, TRUE,
                                                   MSG_TYPE_INPUT);
                                break;

                            case INPUT_DO_RENAME_CHAINED:
                                this->PostToServers(FOR_SERVER_MSG_RENFROM,
                                                    this->old_input_temp);
                                this->PostToServers(FOR_SERVER_MSG_RENTO,
                                                    this->input_temp);
                                break;
                        }
                    }
                    break;

                case MSG_KEY_ESC:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    this->CloseInput();
                    break;
            }
            break;

	case DISPLAY_STATE_PRE:
	    switch(msg) { 
                case MSG_KEY_TAB:        
                case MSG_KEY_RIGHT:
                case MSG_KEY_DOWN:            
                    this->pre_buttonstate++;            
                    if (this->pre_buttonstate > NUM_PRECOMMANDS - 1) this->pre_buttonstate = 0;            
                    this->DialogPreUpdate();           
                 break;

		case MSG_KEY_LEFT:
                case MSG_KEY_UP:
                    this->pre_buttonstate--;            
                    if (this->pre_buttonstate < 0) this->pre_buttonstate = NUM_PRECOMMANDS - 1;
                    this->DialogPreUpdate();            
                 break;

                case MSG_KEY_RETURN:
                    this->internal_state = DISPLAY_STATE_NORMAL;
		    this->DialogPreClose();
		    this->PostToServers(FOR_SERVER_MSG_PRE, this->input_temp);
                 break;

                case MSG_KEY_ESC:
                    this->internal_state = DISPLAY_STATE_NORMAL;
		    this->DialogPreClose();
                 break;
            }
	    break;

        case DISPLAY_STATE_XSITE:

            switch (msg) {
                case MSG_KEY_TAB:
                case MSG_KEY_DOWN:
                    this->xsite_buttonstate++;
                    if (this->xsite_buttonstate > 3)
                        this->xsite_buttonstate = 0;

                    this->UpdateXSiteButtons();
                    break;

                case MSG_KEY_HOME:
                    this->InputHome();
                    break;

                case MSG_KEY_END:
                    this->InputEnd();
                    break;

                case MSG_KEY_RIGHT:
                    this->InputRight();
                    break;

                case MSG_KEY_LEFT:
                    this->InputLeft();
                    break;

                case MSG_KEY_UP:
                    this->xsite_buttonstate--;
                    if (this->xsite_buttonstate < 0)
                        this->xsite_buttonstate = 3;

                    this->UpdateXSiteButtons();
                    break;

                case MSG_KEY_EXTENDED:
                    this->InputAppendChar((char) extended);
                    break;

                case MSG_KEY_BACKSPACE:
                    this->InputBackspace();
                    break;

                case MSG_KEY_DELLINE:
                    this->InputDelline();
                    break;

                case MSG_KEY_RETURN:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    flag = this->CloseSiteInput();

                    switch (this->xsite_buttonstate) {
                        case 0:
                            if (strlen(this->input_temp) > 0) {
                                if (flag == TRUE)
                                    this->PostToServers(FOR_SERVER_MSG_SITE,
                                                        this->input_temp);
                                else
                                    this->PostToServer(FOR_SERVER_MSG_SITE, TRUE,
                                                       MSG_TYPE_INPUT);
                            }
                            break;

                        default:
                            this->GetSiteAlias(this->xsite_buttonstate);
                            this->PostToServer(FOR_SERVER_MSG_SITE, TRUE,
                                               MSG_TYPE_INPUT);
                    }
                    break;

                case MSG_KEY_ESC:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    this->CloseSiteInput();
                    break;
            }
            break;

        case DISPLAY_STATE_VIEW:

            switch (msg) {
                case MSG_KEY_EXTENDED:

                    switch ((char) extended) {
                        case '9':
                            this->PageMoveView(FALSE);
                            break;

                        case '3':
                            this->PageMoveView(TRUE);
                            break;
                    }
                    break;

                case MSG_KEY_UP:
                    this->ScrollView(FALSE);
                    break;

                case MSG_KEY_DOWN:
                    this->ScrollView(TRUE);
                    break;

                case MSG_KEY_RIGHT:
                    view_lrpos++;
                    this->RefreshView();
                    break;

                case MSG_KEY_LEFT:
                    if (view_lrpos) {
                        view_lrpos--;
                        this->RefreshView();
                    }
                    break;

                case MSG_KEY_HOME:
                    this->PageMoveViewEnd(TRUE);
                    break;

                case MSG_KEY_END:
                    this->PageMoveViewEnd(FALSE);
                    break;

                case MSG_KEY_PGUP:
                    this->PageMoveView(TRUE);
                    break;

                case MSG_KEY_PGDN:
                    this->PageMoveView(FALSE);
                    break;

                case MSG_KEY_ESC:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    this->CloseView();
                    break;
            }
            break;

        case DISPLAY_STATE_LOG:

            switch (msg) {
                case MSG_KEY_EXTENDED:

                    switch ((char) extended) {
                        case '9':
                            this->PageMoveLog(TRUE);
                            break;

                        case '3':
                            this->PageMoveLog(FALSE);
                            break;

                    }
                    break;

                case MSG_KEY_RIGHT:
                    view_lrpos++;
                    this->RefreshLog();
                    break;

                case MSG_KEY_LEFT:
                    if (view_lrpos) {
                        view_lrpos--;
                        this->RefreshLog();
                    }
                    break;

                case MSG_KEY_UP:
                    this->ScrollLog(TRUE);
                    break;

                case MSG_KEY_DOWN:
                    this->ScrollLog(FALSE);
                    break;

                case MSG_KEY_PGDN:
                    this->PageMoveLog(TRUE);
                    break;

                case MSG_KEY_PGUP:
                    this->PageMoveLog(FALSE);
                    break;

                //log has no top so home doesnt work
                case MSG_KEY_HOME:
                    this->PageMoveLogEnd(FALSE);
                    break;

                case MSG_KEY_END:
                    this->PageMoveLogEnd(FALSE);
                    break;

                case MSG_KEY_ESC:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    this->CloseLog();
                    break;
            }
            break;

        case DISPLAY_STATE_SWITCH:

            switch (msg) {
                case MSG_KEY_UP:
                    this->ScrollSwitch(TRUE);
                    break;

                case MSG_KEY_DOWN:
                    this->ScrollSwitch(FALSE);
                    break;

                case MSG_KEY_HOME:
                    this->PageMoveSwitchEnd(TRUE);
                    break;

                case MSG_KEY_END:
                    this->PageMoveSwitchEnd(FALSE);
                    break;

                case MSG_KEY_PGUP:
                    this->PageMoveSwitch(TRUE);
                    break;

                case MSG_KEY_PGDN:
                    this->PageMoveSwitch(FALSE);
                    break;

                case MSG_KEY_RETURN:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    this->CloseSwitchWindow();
                    this->PerformSwitch();
                    break;

                case MSG_KEY_ESC:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    this->CloseSwitchWindow();
                    break;

                case MSG_KEY_EXTENDED:

                    switch ((char) extended) {
                        case '3':
                            this->PageMoveSwitch(TRUE);
                            break;

                        case '9':
                            this->PageMoveSwitch(FALSE);
                            break;

                        case 'C':
                        case 'c':
                            this->internal_state = DISPLAY_STATE_NORMAL;
                            this->CloseSwitchWindow();
                            this->PerformSwitchClose();
                            break;

                        case 'K':
                        case 'k':
                            this->PerformSwitchClose();
                            this->RedrawSwitchWindow();
                            break;
                    }
                    break;
            }
            break;

        case DISPLAY_STATE_NOBKMRK:

            switch (msg) {
                case MSG_KEY_ESC:
                case MSG_KEY_RETURN:
                    this->internal_state = DISPLAY_STATE_PASSWORD;
                    this->CloseNotice();
                    this->OpenPasswordInput(TRUE);
                    break;
            }
            break;

        case DISPLAY_STATE_CHNOPASSWD:

            switch (msg) {
                case MSG_KEY_ESC:
                case MSG_KEY_RETURN:
                    this->internal_state = DISPLAY_STATE_CHPASSWORD;
                    this->CloseNotice();
                    break;
            }
            break;

        case DISPLAY_STATE_NOPASSWD:

            switch (msg) {
                case MSG_KEY_ESC:
                case MSG_KEY_RETURN:
                    this->internal_state = DISPLAY_STATE_PASSWORD;
                    this->CloseNotice();
                    break;
            }
            break;

        case DISPLAY_STATE_CHBADPASS:

            switch (msg) {
                case MSG_KEY_ESC:
                case MSG_KEY_RETURN:
                    this->CloseNotice();
                    this->internal_state = DISPLAY_STATE_OPENSITE;
                    break;
            }
            break;

        case DISPLAY_STATE_BADPASS:

            switch (msg) {
                case MSG_KEY_ESC:
                case MSG_KEY_RETURN:
                    this->CloseNotice();
                    keyhandler->WantQuit(FALSE);
                    break;
            }
            break;

        case DISPLAY_STATE_PASSWORD:

            switch (msg) {
                case MSG_KEY_EXTENDED:
                    this->InputAppendChar((char) extended);
                    break;

                case MSG_KEY_BACKSPACE:
                    this->InputBackspace();
                    break;

                case MSG_KEY_HOME:
                    this->InputHome();
                    break;

                case MSG_KEY_END:
                    this->InputEnd();
                    break;

                case MSG_KEY_RIGHT:
                    this->InputRight();
                    break;

                case MSG_KEY_LEFT:
                    this->InputLeft();
                    break;

                case MSG_KEY_DELLINE:
                    this->InputDelline();
                    break;

                case MSG_KEY_RETURN:
                    if (strlen(this->password) < 6) {
                        this->internal_state = DISPLAY_STATE_NOPASSWD;
                        this->NoticeBadPasswd();
                    } else {
                        this->internal_state = DISPLAY_STATE_NORMAL;
                        this->CloseInput();
                        strcpy(this->custom_password, this->password);
                        if (this->ProbeBookmarkRC() != 2) {
                            if (!this->ReadBookmarks()) {
                                this->internal_state = DISPLAY_STATE_BADPASS;
                                this->NoticeNoMatch();
                            }
                        }
                        if (this->internal_state == DISPLAY_STATE_NORMAL) {
                            this->leftwindow_magic = SERVER_MAGIC_START;
                            this->window_tab = this->window_left;
                            strcpy(this->window_left_label, "localhost");
                            this->FireupLocalFilesys();
                            this->internal_state = DISPLAY_STATE_WELCOME;
                            this->AutoLogin();
                            this->CommandLineLogin();
                            this->DrawSwitchPairs();
                            //this->ShowWelcome();
                        }
                    }
                    break;

                case MSG_KEY_ESC:
                    this->internal_state = DISPLAY_STATE_NOPASSWD;
                    this->NoticeNoPasswd();
                    break;
            }
            break;

        case DISPLAY_STATE_CHPASSWORD:

            switch (msg) {
                case MSG_KEY_EXTENDED:
                    this->InputAppendChar((char) extended);
                    break;

                case MSG_KEY_BACKSPACE:
                    this->InputBackspace();
                    break;

                case MSG_KEY_HOME:
                    this->InputHome();
                    break;

                case MSG_KEY_END:
                    this->InputEnd();
                    break;

                case MSG_KEY_RIGHT:
                    this->InputRight();
                    break;

                case MSG_KEY_LEFT:
                    this->InputLeft();
                    break;

                case MSG_KEY_DELLINE:
                    this->InputDelline();
                    break;

                case MSG_KEY_RETURN:
                    if (strlen(this->password_new) < 6) {
                        this->internal_state = DISPLAY_STATE_CHNOPASSWD;
                        this->NoticeBadPasswd();
                    } else {
                        if (strlen(this->password_confirm) == 0) {
                            this->CloseInput();
                            this->OpenChangePasswordInput(TRUE);
                        } else if (strlen(this->password_confirm) < 6) {
                            this->internal_state = DISPLAY_STATE_CHNOPASSWD;
                            this->NoticeBadPasswd();
                        } else {
                            if (strcmp(this->password_new, this->password_confirm) == 0) {
                                this->CloseInput();
                                this->internal_state = DISPLAY_STATE_OPENSITE;
                                strcpy(this->custom_password, this->password_new);
                                this->RedrawBookmarkSites();
                                this->bm_save = TRUE;
                                //this->PostStatusLine("saving bookmarks, with new password ...",TRUE);
                                //this->SaveBookmarks();
                            } else {
                                this->CloseInput();
                                this->internal_state = DISPLAY_STATE_CHBADPASS;
                                this->NoticeNoMatch();
                            }
                        }
                    }
                    break;

                case MSG_KEY_ESC:
                    this->internal_state = DISPLAY_STATE_OPENSITE;
                    this->CloseInput();
                    this->RedrawBookmarkSites();
                    //this->PostStatusLine("bookmark password change aborted.",TRUE);
                    break;
            }
            break;

        case DISPLAY_STATE_OS_NOTICE:

            switch (msg) {
                case MSG_KEY_ESC:
                case MSG_KEY_RETURN:
                    this->internal_state = DISPLAY_STATE_OPENSITE;
                    this->CloseNotice();
                    break;
            }
            break;

        case DISPLAY_STATE_NOTICE:

            switch (msg) {
                case MSG_KEY_ESC:
                case MSG_KEY_RETURN:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    this->CloseNotice();
                    break;
            }
            break;

        case DISPLAY_STATE_OPENSITE:

            switch (msg) {
                case MSG_KEY_UP:
                    this->ScrollBookmarkSites(TRUE);
                    break;

                case MSG_KEY_DOWN:
                    this->ScrollBookmarkSites(FALSE);
                    break;

                case MSG_KEY_HOME:
                    this->PageMoveBookmarkSitesEnd(FALSE);
                    break;

                case MSG_KEY_END:
                    this->PageMoveBookmarkSitesEnd(TRUE);
                    break;

                case MSG_KEY_PGUP:
                    this->PageMoveBookmarkSites(FALSE);
                    break;

                case MSG_KEY_PGDN:
                    this->PageMoveBookmarkSites(TRUE);
                    break;

                case MSG_KEY_TAB:
                case MSG_KEY_RIGHT:
                    this->siteopen_buttonstate++;

                    if (this->siteopen_buttonstate > 5)
                        this->siteopen_buttonstate = 0;

                    /*
                    if((this->siteopen_buttonstate == 0) && !this->filelist_right)
                        this->siteopen_buttonstate = 1;
                    else if((this->siteopen_buttonstate == 1) && !this->filelist_left)
                        this->siteopen_buttonstate = 2;
                    */

                    this->UpdateSiteOpenButtons();
                    break;

                case MSG_KEY_LEFT:
                    this->siteopen_buttonstate--;
                    if (this->siteopen_buttonstate < 0)
                        this->siteopen_buttonstate = 5;

                    this->UpdateSiteOpenButtons();
                    break;

                case MSG_KEY_RETURN:

                    switch (this->siteopen_buttonstate) {
                        case 0:
                            if (global_bookmark) {
                                this->internal_state = DISPLAY_STATE_NORMAL;
                                this->CloseSiteDialog();
                                if (this->bm_save) {
                                    this->PostStatusLine("saving bookmarks ...", TRUE);
                                    this->SaveBookmarks();
                                    this->bm_save = FALSE;
                                }
                                this->FreeWindow(this->window_left);
                                this->filelist_left_format = leftformat;
                                if (this->leftwindow_busy) {
                                    delete[](this->leftwindow_busy);
                                    this->leftwindow_busy = NULL;
                                }
                                this->leftwindow_magic =
                                    this->FireupServer(this->window_left_label);
                            } else {
                                this->internal_state = DISPLAY_STATE_OS_NOTICE;
                                this->DialogNotice(NOTICE_NO_SITE, DEFAULT_OKAY);
                            }
                            break;

                        case 1:
                            if (global_bookmark) {
                                this->internal_state = DISPLAY_STATE_NORMAL;
                                this->CloseSiteDialog();
                                this->FreeWindow(this->window_right);
                                if (this->bm_save) {
                                    this->PostStatusLine("saving bookmarks ...", TRUE);
                                    this->SaveBookmarks();
                                    this->bm_save = FALSE;
                                }
                                this->filelist_right_format = rightformat;
                                if (this->rightwindow_busy) {
                                    delete[](this->rightwindow_busy);
                                    this->rightwindow_busy = NULL;
                                }
                                this->rightwindow_magic =
                                    this->FireupServer(this->window_right_label);
                            } else {
                                this->internal_state = DISPLAY_STATE_OS_NOTICE;
                                this->DialogNotice(NOTICE_NO_SITE, DEFAULT_OKAY);
                            }
                            break;

                        case 2:
                            extern char *sectionlabels[NUM_SECTIONS];
			    extern char *prelabels[NUM_PRECOMMANDS];
                        addsite_label:
                            this->internal_state = DISPLAY_STATE_PREFS;
                            this->internal_state_previous = DISPLAY_STATE_OPENSITE_ADD;
                            strcpy(this->alias, "");
                            strcpy(this->hostname, "");
                            this->retry_counter = DEFAULT_RETRY_COUNTER;
                            strcpy(this->username, DEFAULT_USERNAME);
                            strcpy(this->password, DEFAULT_PASSWORD);
                            strcpy(this->startdir, DEFAULT_STARTDIR);
                            strcpy(this->exclude_source, DEFAULT_EXCLUDE_SOURCE);
                            strcpy(this->exclude_target, DEFAULT_EXCLUDE_TARGET);
                            for (int f = 0; f < NUM_SECTIONS; f++) strcpy(this->sectionList[f], sectionlabels[f]);
			    for (int f = 0; f < NUM_PRECOMMANDS; f++) sprintf(this->pre_cmd[f], "PRE %%R %s", prelabels[f]);
                            strcpy(this->site_rules, "RULES");
                            strcpy(this->site_user, "USER");
                            strcpy(this->site_wkup, "WKUP");
                            strcpy(this->noop_cmd, DEFAULT_NOOP_CMD);
                            strcpy(this->first_cmd, DEFAULT_FIRST_CMD);
                            strcpy(this->nuke_prefix, "NUKED-");
                            strcpy(this->msg_string, "250- You have messages");
			    this->refresh_rate = DEFAULT_REFRESH_RATE;
			    this->noop_rate = DEFAULT_NOOP_RATE;
			    this->retry_delay = DEFAULT_RETRY_DELAY;
			    this->file_sorting = DEFAULT_FILE_SORTING;
			    this->dir_sorting = DEFAULT_DIR_SORTING;
			    this->retry_counter = DEFAULT_RETRY_COUNTER;
			    this->use_autologin = DEFAULT_USE_AUTOLOGIN;
			    this->use_chaining = DEFAULT_USE_CHAINING;
			    this->use_exclude_source = DEFAULT_USE_EXCLUDE_SOURCE;
			    this->use_exclude_target = DEFAULT_USE_EXCLUDE_TARGET;
			    this->use_firstcmd = DEFAULT_USE_FIRSTCMD;
			    this->use_jump = DEFAULT_USE_JUMP;
			    this->use_noop = DEFAULT_USE_NOOP;
			    this->use_pasv = DEFAULT_USE_PASV;
			    this->use_refresh = DEFAULT_USE_REFRESH;
			    this->use_retry = DEFAULT_USE_RETRY;
			    this->use_rndrefr = DEFAULT_USE_RNDREFR;
			    this->use_sections = DEFAULT_USE_SECTIONS;
			    this->use_ssl = DEFAULT_USE_SSL;
			    this->use_ssl_data = DEFAULT_USE_SSL_DATA;
			    this->use_ssl_list = DEFAULT_USE_SSL_LIST;
			    this->use_startdir = DEFAULT_USE_STARTDIR;
			    this->use_stealth_mode = DEFAULT_USE_STEALTH_MODE;
			    this->use_track = DEFAULT_USE_TRACK;
                            this->OpenPrefsDialog();
                            break;

                        case 3:
                            if (global_bookmark) {
                                this->internal_state = DISPLAY_STATE_OPENSITE;
                                this->WipeBookmark();
                                this->bm_save = TRUE;
                            } else {
                                this->internal_state = DISPLAY_STATE_OS_NOTICE;
                                this->DialogNotice(NOTICE_NO_SITE, DEFAULT_OKAY);
                            }
                            break;

                        case 4:
                            if (global_bookmark) {
                                this->internal_state = DISPLAY_STATE_PREFS;
                                this->internal_state_previous =
                                    DISPLAY_STATE_OPENSITE_MODIFY;
                                this->FillInfoForPrefs();
                                this->OpenPrefsDialog();
                            } else {
                                this->internal_state = DISPLAY_STATE_OS_NOTICE;
                                this->DialogNotice(NOTICE_NO_SITE, DEFAULT_OKAY);
                            }
                            break;

                        case 5:
                            this->internal_state = DISPLAY_STATE_CHPASSWORD;
                            strcpy(this->password_confirm, "");
                            this->OpenChangePasswordInput(FALSE);
                            break;
                    }
                    break;

                case MSG_KEY_ESCAPE:
                    if ((extended >= 'a') && (extended <= 'z'))
                        this->MoveBookmarkByChar(extended);

                    break;

                case MSG_KEY_EXTENDED:

                    switch ((char) extended) {
                        case '3':
                            this->PageMoveBookmarkSites(FALSE);
                            break;

                        case '9':
                            this->PageMoveBookmarkSites(TRUE);
                            break;

                        case 'e':
                            if (global_bookmark) {
                                this->internal_state = DISPLAY_STATE_NORMAL;
                                this->CloseSiteDialog();
                                if (this->bm_save) {
                                    this->PostStatusLine("saving bookmarks ...", TRUE);
                                    this->SaveBookmarks();
                                    this->bm_save = FALSE;
                                }
                                this->FreeWindow(this->window_left);
                                this->filelist_left_format = leftformat;
                                this->filelist_left_soffset = 0; 
                                this->leftwindow_magic =
                                    this->FireupServer(this->window_left_label);
                            } else {
                                this->internal_state = DISPLAY_STATE_OS_NOTICE;
                                this->DialogNotice(NOTICE_NO_SITE, DEFAULT_OKAY);
                            }
                            break;

                        case 'r':
                            if (global_bookmark) {
                                this->internal_state = DISPLAY_STATE_NORMAL;
                                this->CloseSiteDialog();
                                if (this->bm_save) {
                                    this->PostStatusLine("saving bookmarks ...", TRUE);
                                    this->SaveBookmarks();
                                    this->bm_save = FALSE;
                                }
                                this->FreeWindow(this->window_right);
                                this->filelist_right_format = rightformat;
                                this->filelist_right_soffset = 0;
                                this->rightwindow_magic =
                                    this->FireupServer(this->window_right_label);
                            } else {
                                this->internal_state = DISPLAY_STATE_OS_NOTICE;
                                this->DialogNotice(NOTICE_NO_SITE, DEFAULT_OKAY);
                            }
                            break;

                        case 'a':
                            goto addsite_label;
                            break;

                        case 'p':
                            this->internal_state = DISPLAY_STATE_CHPASSWORD;
                            strcpy(this->password_confirm, "");
                            this->OpenChangePasswordInput(FALSE);
                            break;

                        case 'd':
                            if (global_bookmark) {
                                this->internal_state = DISPLAY_STATE_OPENSITE;
                                this->WipeBookmark();
                                this->bm_save = TRUE;
                            } else {
                                this->internal_state = DISPLAY_STATE_OS_NOTICE;
                                this->DialogNotice(NOTICE_NO_SITE, DEFAULT_OKAY);
                            }
                            break;

                        case 'm':
                            if (global_bookmark) {
                                this->internal_state = DISPLAY_STATE_PREFS;
                                this->internal_state_previous =
                                    DISPLAY_STATE_OPENSITE_MODIFY;
                                this->FillInfoForPrefs();
                                this->OpenPrefsDialog();
                            } else {
                                this->internal_state = DISPLAY_STATE_OS_NOTICE;
                                this->DialogNotice(NOTICE_NO_SITE, DEFAULT_OKAY);
                            }
                            break;

                        //default :
                            //break;
                    }
                    break;

                case MSG_KEY_ESC:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    this->CloseSiteDialog();
                    if (this->bm_save) {
                        this->PostStatusLine("saving bookmarks ...", TRUE);
                        this->SaveBookmarks();
                        this->bm_save = FALSE;
                    }
                    break;
            }
            break;

        case DISPLAY_STATE_PREFS:

            switch (msg) {
                case MSG_KEY_TAB:
                case MSG_KEY_DOWN:
                case MSG_KEY_RIGHT:
                    this->prefsdialog_buttonstate++;
                    if (this->prefsdialog_buttonstate > 74)
                        this->prefsdialog_buttonstate = 0;

                    this->UpdatePrefsItems();
                    break;

                case MSG_KEY_UP:
                case MSG_KEY_LEFT:
                    this->prefsdialog_buttonstate--;
                    if (this->prefsdialog_buttonstate < 0)
                        this->prefsdialog_buttonstate = 74;

                    this->UpdatePrefsItems();
                    break;

                case MSG_KEY_EXTENDED:
                    if (extended == 32) {
                        flag = FALSE;

                        switch (this->prefsdialog_buttonstate) {
                            case 4:
                                this->use_retry = !this->use_retry;
                                flag = TRUE;
                                break;

                            case 6:
                                this->use_ssl = !this->use_ssl;
                                if (use_ssl == FALSE) {
                                    use_ssl_list = use_ssl_data = use_ssl_fxp = FALSE;
                                }
                                flag = TRUE;
                                break;

                            case 7:
                                this->use_ssl_list = !this->use_ssl_list;
                                if (use_ssl == FALSE) {
                                    use_ssl_list = use_ssl_data = use_ssl_fxp = FALSE;
                                }
                                flag = TRUE;
                                break;

                            case 8:
                                this->use_ssl_data = !this->use_ssl_data;
                                if (use_ssl == FALSE) {
                                    use_ssl_list = use_ssl_data = use_ssl_fxp = FALSE;
                                }
                                flag = TRUE;
                                break;

                            case 9:
                                this->use_ssl_fxp = !this->use_ssl_fxp;
                                if (use_ssl == FALSE) {
                                    use_ssl_list = use_ssl_data = use_ssl_fxp = FALSE;
                                }
                                flag = TRUE;
                                break;

                            case 10:
                                this->use_pasv = !this->use_pasv;
                                flag = TRUE;
                                break;

                            case 11:
                                this->use_alt_fxp = !this->use_alt_fxp;
                                flag = TRUE;
                                break;

                            case 12:
                                this->use_stealth_mode = !this->use_stealth_mode;
                                flag = TRUE;
                                break;

                            case 13:
                                this->use_firstcmd = !this->use_firstcmd;
                                flag = TRUE;
                                break;

                            case 14:
                                this->use_startdir = !this->use_startdir;
                                flag = TRUE;
                                break;

                            case 15:
                                this->use_exclude_source = !this->use_exclude_source;
                                flag = TRUE;
                                break;

                            case 16:
                                this->use_exclude_target = !this->use_exclude_target;
                                flag = TRUE;
                                break;

                            case 17:
                                this->use_refresh = !this->use_refresh;
                                flag = TRUE;
                                break;

                            case 18:
                                this->use_noop = !this->use_noop;
                                flag = TRUE;
                                break;

                            case 20:
                                this->use_rndrefr = !this->use_rndrefr;
                                flag = TRUE;
                                break;

                            case 21:
                                this->use_jump = !this->use_jump;
                                flag = TRUE;
                                break;

                            case 23:
                                this->use_track = !this->use_track;
                                flag = TRUE;
                                break;

                            case 25:
                                this->use_autologin = !this->use_autologin;
                                flag = TRUE;
                                break;

                            case 26:
                                this->use_chaining = !this->use_chaining;
                                flag = TRUE;
                                break;

                            case 27:
                                this->use_sections = !this->use_sections;
                                flag = TRUE;
                                break;

                            case 49:
                                this->file_sorting = 1;
                                flag = TRUE;
                                break;

                            case 50:
                                this->file_sorting = 2;
                                flag = TRUE;
                                break;

                            case 51:
                                this->file_sorting = 3;
                                flag = TRUE;
                                break;

                            case 52:
                                this->use_rev_file = !this->use_rev_file;
                                flag = TRUE;
                                break;

                            case 53:
                                this->dir_sorting = 1;
                                flag = TRUE;
                                break;

                            case 54:
                                this->dir_sorting = 2;
                                flag = TRUE;
                                break;

                            case 55:
                                this->dir_sorting = 3;
                                flag = TRUE;
                                break;

                            case 56:
                                this->use_rev_dir = !this->use_rev_dir;
                                flag = TRUE;
                                break;
                        }

                        if (flag) {
                            this->UpdatePrefsItems();
                        }
                    } else if ((extended == 'O') || (extended == 'o')) {
                        if (this->internal_state_previous ==
                            DISPLAY_STATE_OPENSITE_ADD) {
                            // add site
                            this->PrefsAddSite();
                            this->internal_state = DISPLAY_STATE_OPENSITE;
                            this->ClosePrefsDialog();
                            this->RedrawBookmarkSites();
                            this->bm_save = TRUE;
                        } else if (this->internal_state_previous ==
                                   DISPLAY_STATE_OPENSITE_MODIFY) {
                            // just modify prefs
                            this->PrefsModifySite();
                            this->internal_state = DISPLAY_STATE_OPENSITE;
                            this->ClosePrefsDialog();
                            this->RedrawBookmarkSites();
                            this->bm_save = TRUE;
                        } else {
                            // prefs change from main screen
                            this->siteopen_bm_realmagic =
                                this->siteprefs_server->GetBMMagic();
                            this->PrefsModifySite();
                            this->UpdateSitePrefs(this->siteprefs_server);
                            this->internal_state = this->internal_state_previous;
                            this->ClosePrefsDialog();
                            this->PostStatusLine("saving bookmarks ...", TRUE);
                            this->SaveBookmarks();
                            this->bm_save = FALSE;
                        }
                    } else if ((extended == 'C') || (extended == 'c')) {
                        if (this->internal_state_previous ==
                            DISPLAY_STATE_OPENSITE_MODIFY
                            || this->internal_state_previous ==
                            DISPLAY_STATE_OPENSITE_ADD) {
                            this->internal_state = DISPLAY_STATE_OPENSITE;
                            this->ClosePrefsDialog();
                        } else {
                            this->internal_state = this->internal_state_previous;
                            this->ClosePrefsDialog();
                        }
                    }
                    break;

                case MSG_KEY_RETURN:

                    switch (this->prefsdialog_buttonstate) {
                        case 0:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter site ALIAS:", this->alias,
                                              ALIAS_MAX, FALSE);
                            break;

                        case 1:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter site HOSTNAME/IP:PORT:",
                                              this->hostname, HOSTNAME_MAX, FALSE);
                            break;

                        case 2:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter USERNAME (login):", this->username,
                                              USERNAME_MAX, FALSE);
                            break;

                        case 3:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter PASSWORD (login):", this->password,
                                              PASSWORD_MAX, TRUE);
                            break;

                        case 4:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = 0;
                            sprintf(this->num_string, "%d", this->retry_counter);
                            this->DialogInput("enter how often to RETRY:",
                                              this->num_string, 4, FALSE);
                            break;

                        case 5:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = 1;
                            sprintf(this->num_string, "%d", this->retry_delay);
                            this->DialogInput("enter RETRY DELAY in seconds:",
                                              this->num_string, 4, FALSE);
                            break;

                        case 13:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter FIRST CMD after login:",
                                              this->first_cmd, INPUT_TEMP_MAX, FALSE);
                            break;

                        case 14:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter STARTING DIRECTORY:",
                                              this->startdir, STARTDIR_MAX, FALSE);
                            break;

                        case 15:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter patterns to EXCLUDE from copying/detecting:",
                                              this->exclude_source, EXCLUDE_MAX, FALSE);
                            break;

                        case 16:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter patterns to EXCLUDE in chaining:",
                                              this->exclude_target, EXCLUDE_MAX, FALSE);
                            break;

                        case 17:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = 2;
                            sprintf(this->num_string, "%d", this->refresh_rate);
                            this->DialogInput("enter REFRESH RATE in seconds:",
                                              this->num_string, 4, FALSE);
                            break;

                        case 18:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter NOOP COMMAND:",
                                              this->noop_cmd, SITE_MAX, FALSE);
                            break;

                        case 19:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = 3;
                            sprintf(this->num_string, "%d", this->noop_rate);
                            this->DialogInput("enter NOOP RATE in seconds:",
                                              this->num_string, 4, FALSE);
                            break;

                        case 22:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter NUKED dir prefix:",
                                              this->nuke_prefix, 16, FALSE);
                            break;

                        case 24:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter SITE MSG detection string:",
                                              this->msg_string, 256, FALSE);
                            break;

                        case 27:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION01 (%s) directory:", sectionlabels[0]);
                            this->DialogInput(temp_string, this->sectionList[0], GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 28:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION02 (%s) directory:",
                                    sectionlabels[1]);
                            this->DialogInput(temp_string, this->sectionList[1],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 29:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION03 (%s) directory:",
                                    sectionlabels[2]);
                            this->DialogInput(temp_string, this->sectionList[2],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 30:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION04 (%s) directory:",
                                    sectionlabels[3]);
                            this->DialogInput(temp_string, this->sectionList[3],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 31:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION05 (%s) directory:",
                                    sectionlabels[4]);
                            this->DialogInput(temp_string, this->sectionList[4],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 32:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION06 (%s) directory:",
                                    sectionlabels[5]);
                            this->DialogInput(temp_string, this->sectionList[5],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 33:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION07 (%s) directory:",
                                    sectionlabels[6]);
                            this->DialogInput(temp_string, this->sectionList[6],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 34:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION08 (%s) directory:",
                                    sectionlabels[7]);
                            this->DialogInput(temp_string, this->sectionList[7],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 35:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION09 (%s) directory:",
                                    sectionlabels[8]);
                            this->DialogInput(temp_string, this->sectionList[8],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 36:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION10 (%s) directory:",
                                    sectionlabels[9]);
                            this->DialogInput(temp_string, this->sectionList[9],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 37:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION11 (%s) directory:",
                                    sectionlabels[10]);
                            this->DialogInput(temp_string, this->sectionList[10],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 38:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION12 (%s) directory:",
                                    sectionlabels[11]);
                            this->DialogInput(temp_string, this->sectionList[11],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 39:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION13 (%s) directory:",
                                    sectionlabels[12]);
                            this->DialogInput(temp_string, this->sectionList[12],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 40:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION14 (%s) directory:",
                                    sectionlabels[13]);
                            this->DialogInput(temp_string, this->sectionList[13],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 41:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION15 (%s) directory:",
                                    sectionlabels[14]);
                            this->DialogInput(temp_string, this->sectionList[14],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 42:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION16 (%s) directory:",
                                    sectionlabels[15]);
                            this->DialogInput(temp_string, this->sectionList[15],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 43:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION17 (%s) directory:",
                                    sectionlabels[16]);
                            this->DialogInput(temp_string, this->sectionList[16],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 44:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION18 (%s) directory:",
                                    sectionlabels[17]);
                            this->DialogInput(temp_string, this->sectionList[17],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 45:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION19 (%s) directory:",
                                    sectionlabels[18]);
                            this->DialogInput(temp_string, this->sectionList[18],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 46:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION20 (%s) directory:",
                                    sectionlabels[19]);
                            this->DialogInput(temp_string, this->sectionList[19],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 47:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION21 (%s) directory:",
                                    sectionlabels[20]);
                            this->DialogInput(temp_string, this->sectionList[20],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 48:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter SECTION22 (%s) directory:",
                                    sectionlabels[21]);
                            this->DialogInput(temp_string, this->sectionList[21],
                                              GAMEUTILDIR_MAX, FALSE);
                            break;

                        case 57:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter SITE RULES shortcut:",
                                              this->site_rules, SITE_MAX, FALSE);
                            break;

                        case 58:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter SITE USER shortcut:",
                                              this->site_user, SITE_MAX, FALSE);
                            break;

                        case 59:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            this->DialogInput("enter SITE WKUP shortcut:",
                                              this->site_wkup, SITE_MAX, FALSE);
                            break;

                        case 60:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD01 (%s)", prelabels[0]);
                            this->DialogInput(temp_string, this->pre_cmd[0], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 61:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD02 (%s)", prelabels[1]);
                            this->DialogInput(temp_string, this->pre_cmd[1], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 62:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD03 (%s)", prelabels[2]);
                            this->DialogInput(temp_string, this->pre_cmd[2], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 63:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD04 (%s)", prelabels[3]);
                            this->DialogInput(temp_string, this->pre_cmd[3], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 64:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD05 (%s)", prelabels[4]);
                            this->DialogInput(temp_string, this->pre_cmd[4], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 65:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD06 (%s)", prelabels[5]);
                            this->DialogInput(temp_string, this->pre_cmd[5], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 66:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD07 (%s)", prelabels[6]);
                            this->DialogInput(temp_string, this->pre_cmd[6], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 67:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD08 (%s)", prelabels[7]);
                            this->DialogInput(temp_string, this->pre_cmd[7], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 68:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD09 (%s)", prelabels[8]);
                            this->DialogInput(temp_string, this->pre_cmd[8], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 69:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD10 (%s)", prelabels[9]);
                            this->DialogInput(temp_string, this->pre_cmd[9], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 70:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD11 (%s)", prelabels[10]);
                            this->DialogInput(temp_string, this->pre_cmd[10], GAMEUTILDIR_MAX, FALSE);
                            break;
                        case 71:
                            this->internal_state = DISPLAY_STATE_PREFSINPUT;
                            this->prefs_inputtype = -1;
                            sprintf(temp_string, "enter PRE_CMD12 (%s)", prelabels[11]);
                            this->DialogInput(temp_string, this->pre_cmd[11], GAMEUTILDIR_MAX, FALSE);
                            break;
                   
	                case 72:
                            if (this->internal_state_previous ==
                                DISPLAY_STATE_OPENSITE_ADD) {
                                // add site
                                this->PrefsAddSite();
                                this->internal_state = DISPLAY_STATE_OPENSITE;
                                this->ClosePrefsDialog();
                                this->RedrawBookmarkSites();
                                this->bm_save = TRUE;
                            } else if (this->internal_state_previous ==
                                       DISPLAY_STATE_OPENSITE_MODIFY) {
                                // just modify prefs
                                this->PrefsModifySite();
                                this->internal_state = DISPLAY_STATE_OPENSITE;
                                this->ClosePrefsDialog();
                                this->RedrawBookmarkSites();
                                this->bm_save = TRUE;
                            } else {
                                // prefs change from main screen
                                this->siteopen_bm_realmagic =
                                    this->siteprefs_server->GetBMMagic();
                                this->PrefsModifySite();
                                this->UpdateSitePrefs(this->siteprefs_server);
                                this->internal_state = this->internal_state_previous;
                                this->ClosePrefsDialog();
                                this->bm_save = TRUE;
                            }
                            break;

                        case 73:
                            if (this->internal_state_previous ==
                                DISPLAY_STATE_OPENSITE_MODIFY
                                || this->internal_state_previous ==
                                DISPLAY_STATE_OPENSITE_ADD) {
                                this->internal_state = DISPLAY_STATE_OPENSITE;
                                this->ClosePrefsDialog();
                            } else {
                                this->internal_state = this->internal_state_previous;
                                this->ClosePrefsDialog();
                            }
                            break;
                    }
                    break;

                case MSG_KEY_ESC:
                    if (this->internal_state_previous ==
                        DISPLAY_STATE_OPENSITE_MODIFY
                        || this->internal_state_previous ==
                        DISPLAY_STATE_OPENSITE_ADD)
                        this->internal_state = DISPLAY_STATE_OPENSITE;
                    else
                        this->internal_state = this->internal_state_previous;

                    this->ClosePrefsDialog();
                    break;
            }
            break;

        case DISPLAY_STATE_PREFSINPUT:

            switch (msg) {
                case MSG_KEY_EXTENDED:
                    this->InputAppendChar((char) extended);
                    break;

                case MSG_KEY_BACKSPACE:
                    this->InputBackspace();
                    break;

                case MSG_KEY_RIGHT:
                    this->InputRight();
                    break;

                case MSG_KEY_LEFT:
                    this->InputLeft();
                    break;

                case MSG_KEY_HOME:
                    this->InputHome();
                    break;

                case MSG_KEY_END:
                    this->InputEnd();
                    break;

                case MSG_KEY_DELLINE:
                    this->InputDelline();
                    break;

                case MSG_KEY_RETURN:
                case MSG_KEY_ESC:
                    this->CloseInput();

                    switch (this->prefs_inputtype) {
                        case 0:
                            temp = atoi(this->num_string);
                            if (temp <= 0)
                                temp = 1;
                            else if (temp > 99)
                                temp = 99;

                            this->retry_counter = temp;
                            break;

                        case 1:
                            temp = atoi(this->num_string);
                            if (temp <= 1)
                                temp = 1;
                            else if (temp > 999)
                                temp = 999;

                            this->retry_delay = temp;
                            break;

                        case 2:
                            temp = atoi(this->num_string);
                            if (temp <= 3)
                                temp = 4;
                            else if (temp > 999)
                                temp = 999;

                            this->refresh_rate = temp;
                            break;

                        case 3:
                            temp = atoi(this->num_string);
                            if (temp <= 0)
                                temp = 60;
                            else if (temp > 999)
                                temp = 999;

                            this->noop_rate = temp;
                            break;
                    }
                    this->internal_state = DISPLAY_STATE_PREFS;
                    this->UpdatePrefsItems();
                    break;
            }
            break;

        case DISPLAY_STATE_WELCOME:

            switch (msg) {
                case MSG_KEY_ESC:
                case MSG_KEY_RETURN:
                    this->internal_state = DISPLAY_STATE_NORMAL;
                    //this->HideWelcome();
                    break;
            }
            break;
    }
}

void CDisplayHandler::NoticeViewFile(char *file) {
    char dir[SERVER_WORKINGDIR_SIZE];

    global_server->server->ObtainWorkingDir(dir);
    sprintf(this->view_filename, "%s/%s", dir, file);
    strcpy(this->view_fname, file);
    this->internal_state = DISPLAY_STATE_VIEW;
    this->OpenView();
    global_server->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH, "");
}

void CDisplayHandler::ViewFile(void) {
    SERVERLIST *sv_temp = global_server;
    FILELIST *fl_temp;
    int magic;
    bool found = FALSE;
    char dir[SERVER_WORKINGDIR_SIZE];

    if (this->window_tab == this->window_left) {
        if (this->filelist_left == NULL)
            return ;

        magic = this->leftwindow_magic;
        fl_temp = this->GetFilelistEntry(this->filelist_left,
                                         this->filelist_left_magic);
    } else {
        if (this->filelist_right == NULL)
            return ;

        magic = this->rightwindow_magic;

        fl_temp = this->GetFilelistEntry(this->filelist_right,
                                         this->filelist_right_magic);
    }

    if (!fl_temp->is_dir && strcmp(fl_temp->name, "..")) {
        // stupid users tend to try everything to segfault
        while (!found && sv_temp) {
            if (sv_temp->server->GetMagic() == magic)
                found = TRUE;
            else
                sv_temp = sv_temp->next;
        }
        if (found) {
            // check if we have to retrieve the file first
            if (sv_temp->server->GetServerType() == SERVER_TYPE_LOCAL) {
                // file is local, open viewer
                this->internal_state = DISPLAY_STATE_VIEW;
                sv_temp->server->ObtainWorkingDir(dir);
                sprintf(this->view_filename, "%s/%s", dir, fl_temp->name);
                this->OpenView();
            } else if (!sv_temp->server->IsBusy()) {
                // retrieve file first. server will notice us when the file is on our shell
                sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_VIEWFILE,
                                                 fl_temp->name);
            }
        }
    }
}

void CDisplayHandler::ChangeReverseSorting(bool dirlist) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    if (this->window_tab == this->window_left)
        magic = this->leftwindow_magic;
    else
        magic = this->rightwindow_magic;

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found) {
        if (dirlist)
            sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_CHANGE_DIR_REVERSE, "");
        else
            sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_CHANGE_FILE_REVERSE, "");
    }
}


void CDisplayHandler::ChangeFileSorting(void) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    if (this->window_tab == this->window_left)
        magic = this->leftwindow_magic;
    else
        magic = this->rightwindow_magic;

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found)
        sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_CHANGE_FILE_SORTING, "");
}

void CDisplayHandler::ChangeDirSorting(void) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    if (this->window_tab == this->window_left)
        magic = this->leftwindow_magic;
    else
        magic = this->rightwindow_magic;

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found)
        sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_CHANGE_DIR_SORTING, "");
}


void CDisplayHandler::AutoLogin(void) {
    BOOKMARK *bm_temp = global_bookmark;

    if (!no_autologin) {
        // probe all bookmarks for auto-login flag
        while (bm_temp) {
            if (bm_temp->use_autologin) {
                // we simply open all threads in the left window
                // a bit of a lame design here, but who really carez on system startup
                this->siteopen_bm_realmagic = bm_temp->magic;
                this->FreeWindow(this->window_right);
                this->filelist_right_format = rightformat;
                this->rightwindow_magic =
                    this->FireupServer(this->window_right_label);
            }
            bm_temp = bm_temp->next;
        }
    }
}

void CDisplayHandler::Sections(int goto_section) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    // determine active site
    if (this->window_tab == this->window_left)
        magic = this->leftwindow_magic;
    else
        magic = this->rightwindow_magic;

    while (((!found
             && ((mode != MODE_FTP_CHAIN) && (mode != MODE_FXP_CHAIN)))
            || ((mode == MODE_FTP_CHAIN) || (mode == MODE_FXP_CHAIN)))
           && sv_temp) {
        if ((((mode != MODE_FTP_CHAIN) && (mode != MODE_FXP_CHAIN))
             && (sv_temp->server->GetMagic() == magic))
            || (((mode == MODE_FTP_CHAIN) || (mode == MODE_FXP_CHAIN))
                && sv_temp->server->GetChaining()
                && (sv_temp->server->GetServerType() != SERVER_TYPE_LOCAL))) {

            found = TRUE;
            if (((mode == MODE_FTP_CHAIN) || (mode == MODE_FXP_CHAIN))
                || !sv_temp->server->IsBusy())

                sv_temp->server->
                PostFromDisplay(FOR_SERVER_MSG_SECTIONS, "", goto_section);
        }
        sv_temp = sv_temp->next;
    }
}

void CDisplayHandler::ResetTimer(void) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    if (this->window_tab == this->window_left)
        magic = this->leftwindow_magic;
    else
        magic = this->rightwindow_magic;

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found)
        sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_RESET_TIMER, "");
}

void CDisplayHandler::PostToServers(int msg, char *message) {
    SERVERLIST *sv_temp = global_server;

    while (sv_temp) {
        if ((sv_temp->server->GetServerType() == SERVER_TYPE_REMOTE) ||
            (msg == FOR_SERVER_MSG_REFRESH))
            sv_temp->server->PostFromDisplay(msg, message);

        sv_temp = sv_temp->next;
    }
}

void CDisplayHandler::RefreshSite(bool refresh_all) {
    SERVERLIST *sv_temp = global_server;
    int magic = -1;
    bool found = FALSE;

    if (refresh_all) {
        this->PostToServers(FOR_SERVER_MSG_REFRESH, "");
    } else {
        // determine active site
        if (this->window_tab == this->window_left)
            magic = this->leftwindow_magic;
        else
            magic = this->rightwindow_magic;

        while (!found && sv_temp) {
            // find server
            if (sv_temp->server->GetMagic() == magic) {
                sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH, "");
                found = TRUE;
            }
            sv_temp = sv_temp->next;
        }
    }
}

void CDisplayHandler::CloseSite(void) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    if (this->window_tab == this->window_left)
        magic = this->leftwindow_magic;
    else
        magic = this->rightwindow_magic;

    if (magic == SERVER_MAGIC_START) {
        this->internal_state = DISPLAY_STATE_NOTICE;
        this->DialogNotice(NOTICE_NO_CLOSE, DEFAULT_OKAY);
    } else {
        while (!found && sv_temp) {
            if (sv_temp->server->GetMagic() == magic)
                found = TRUE;
            else
                sv_temp = sv_temp->next;
        }

        if (found && sv_temp->server->IsRetrying())
            this->KillServer(sv_temp->server);

        //sv_temp->server->PostFromDisplay(SERVER_MSG_KILLME, "");
        if (found && !sv_temp->server->IsBusy())
            sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_CLOSE, "");
    }
}

void CDisplayHandler::WipePrep(void) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    if (this->window_tab == this->window_left) {
        magic = this->leftwindow_magic;
    } else {
        magic = this->rightwindow_magic;
    }

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found) {
        if ((mode == MODE_FTP_NOCHAIN) || (mode == MODE_FXP_NOCHAIN)) {
            // just undo on the actual server
            if (sv_temp->server->GetServerType() != SERVER_TYPE_LOCAL)
                sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_UNDO, "");
        } else {
            // post to all cahined hosts
            sv_temp = global_server;
            while (sv_temp) {
                if (sv_temp->server->GetChaining()
                    && (sv_temp->server->GetServerType() !=
                        SERVER_TYPE_LOCAL))
                    sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_UNDO,
                                                     "");

                sv_temp = sv_temp->next;
            }
        }
    }
}

void CDisplayHandler::PrepDir(int msg) {
    SERVERLIST *sv_save, *sv_temp = global_server;
    FILELIST *fl_temp;
    int dest_magic, magic, file_magic;
    char *start, *cwd, inside_name[512];
    bool found = FALSE;

    if (this->window_tab == this->window_left) {
        magic = this->leftwindow_magic;
        dest_magic = this->rightwindow_magic;
        file_magic = this->filelist_left_magic;
        fl_temp = this->filelist_left;
        cwd = this->window_left_cwd;
    } else {
        magic = this->rightwindow_magic;
        dest_magic = this->leftwindow_magic;
        file_magic = this->filelist_right_magic;
        fl_temp = filelist_right;
        cwd = this->window_right_cwd;
    }

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (fl_temp == NULL)
        found = FALSE;

    if (found && ((!sv_temp->server->IsBusy()
                   && ((mode != MODE_FTP_CHAIN)
                       && (mode != MODE_FXP_CHAIN)))
                  || ((mode == MODE_FTP_CHAIN)
                      || (mode == MODE_FXP_CHAIN)))) {
        fl_temp = this->GetFilelistEntry(fl_temp, file_magic);

        if (fl_temp->is_dir || (msg != FOR_SERVER_MSG_PREP)) {
            // determine last part of dirname for inside-preps
            strcpy(this->temp_string, cwd);
            start = strrchr(this->temp_string, '/');
            if (start)
                strcpy(inside_name, start + 1);
            else
                strcpy(inside_name, this->temp_string); // take full name, since no slash was there (?)

            if ((mode == MODE_FTP_CHAIN) || (mode == MODE_FXP_CHAIN)) {
                // okay. if we are inside already, do nothing on the source.
                // if we aren't, CD into at the source
                // prep&cd on all chained hosts
                sv_save = sv_temp;
                sv_temp = global_server;
                while (sv_temp) {
                    // possible source
                    if (msg == FOR_SERVER_MSG_PREP) {
                        // see if this is the source, and CD into
                        if (sv_temp == sv_save)
                            sv_temp->server->
                            PostFromDisplay(FOR_SERVER_MSG_CWD, fl_temp->name);
                    }
                    // now for possible targets (just remote ones can be target in ftp)
                    if ((sv_temp != sv_save)
                        && (sv_temp->server->GetServerType() ==
                            SERVER_TYPE_REMOTE)
                        && (sv_temp->server->GetChaining())) {

                        // ok, this one is no local filesys and has chaining on, and is not the source
                        sv_temp->server->ClearAllowChaining();
                        if (msg == FOR_SERVER_MSG_PREP) {
                            if (FilterDirname(fl_temp->name, sv_temp->server->GetTargetFilter())) {
                                sv_temp->server->SetAllowChaining();
                                sv_temp->server->PostFromDisplay(msg, fl_temp->name);
                            }
                        } else {
                            if (FilterDirname(inside_name, sv_temp->server->GetTargetFilter())) {
                                sv_temp->server->SetAllowChaining();
                                sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_PREP, inside_name);
                            }
                        }
                    }
                    sv_temp = sv_temp->next;
                }
            } else {
                // ftp no chaining
                // if we are outside, CD in on the source and create prep the dir on the target
                // if we are in, do nothing on src
                // first determine dest server
                sv_save = sv_temp;
                sv_temp = global_server;
                found = FALSE;
                while (!found && sv_temp) {
                    if (sv_temp->server->GetMagic() == dest_magic)
                        found = TRUE;
                    else
                        sv_temp = sv_temp->next;
                }
                if (found && !sv_temp->server->IsBusy()) {
                    if (msg == FOR_SERVER_MSG_PREP) {
                        // CD in on src, prep on dest
                        sv_save->server->
                            PostFromDisplay(FOR_SERVER_MSG_CWD, fl_temp->name);
                        sv_temp->server->PostFromDisplay(msg, fl_temp->name);
                    } else {
                        // prep inside-name on dest only
                        sv_temp->server->
                            PostFromDisplay(FOR_SERVER_MSG_PREP, inside_name);
                    }
                }
            }
        }
    }
}

void CDisplayHandler::ChangeDir(int msg) {
    SERVERLIST *sv_temp1, *sv_temp = global_server;
    FILELIST *fl_temp;
    int magic, file_magic;
    char *start, *cwd, inside_name[512];
    bool found = FALSE;

    if (this->window_tab == this->window_left) {
        magic = this->leftwindow_magic;
        file_magic = this->filelist_left_magic;
        fl_temp = this->filelist_left;
        cwd = this->window_left_cwd;
    } else {
        magic = this->rightwindow_magic;
        file_magic = this->filelist_right_magic;
        fl_temp = filelist_right;
        cwd = this->window_right_cwd;
    }

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }
    if (fl_temp == NULL) {
        found = FALSE;
    } // segf protection

    if (found && ((!sv_temp->server->IsBusy() && ((mode != MODE_FTP_CHAIN)
                    && (mode != MODE_FXP_CHAIN)))
                  || ((mode == MODE_FTP_CHAIN) || (mode == MODE_FXP_CHAIN)))) {
        fl_temp = this->GetFilelistEntry(fl_temp, file_magic);

        if (fl_temp->is_dir || (msg == FOR_SERVER_MSG_CWD_UP)) {
            // lets see if the poster is a local one
            if (sv_temp->server->GetServerType() == SERVER_TYPE_LOCAL) {
                // ok, no chaining or whatever.
                if (msg == FOR_SERVER_MSG_CWD_UP)
                    sv_temp->server->PostFromDisplay(msg, "");
                else
                    sv_temp->server->PostFromDisplay(msg, fl_temp->name);
            } else {
                if ((mode == MODE_FTP_CHAIN) || (mode == MODE_FXP_CHAIN)) {
                    // okay, spread msg to all servers expect local and those who have disabled chaining
                    sv_temp1 = sv_temp;
                    sv_temp = global_server;
                    while (sv_temp) {
                        if (((sv_temp->server->GetServerType() ==
                              SERVER_TYPE_REMOTE)
                             && (sv_temp->server->GetChaining()))
                            || (sv_temp == sv_temp1)) {

                            // ok, this one is no local filesys and has chaining on (or is source)
                            // do we have to determine the entry first?
                            if (msg == FOR_SERVER_MSG_CWD_UP) {
                                // checking if we are in unwanted dir on src
                                // disabled in navigating or multidir xfer screwes
                                strcpy(this->temp_string, cwd);
                                start = strrchr(this->temp_string, '/');
                                if (start)
                                    strcpy(inside_name, start + 1);
                                else
                                    strcpy(inside_name, this->temp_string);

                                // if (sv_temp->server->AllowChaining())
                                if (FilterDirname(inside_name,
                                                  sv_temp->server->GetTargetFilter()))
                                    sv_temp->server->PostFromDisplay(msg, "");
                            } else {
                                // if (sv_temp->server->AllowChaining())
                                if (FilterDirname(fl_temp->name,
                                                  sv_temp->server->GetTargetFilter()))
                                    // use name of actual cursor selection
                                    sv_temp->server->PostFromDisplay(msg, fl_temp->name);
                            }
                        }
                        sv_temp = sv_temp->next;
                    }
                } else {
                    // ftp nochain
                    // okay, send the msg to this server only
                    if (msg == FOR_SERVER_MSG_CWD_UP) {
                        sv_temp->server->PostFromDisplay(msg, "");
                    } else {
                        // use name of actual cursor selection
                        sv_temp->server->PostFromDisplay(msg, fl_temp->name);
                    }
                }
            }
        }
    }
}

void CDisplayHandler::MarkFiles(bool mark) {
    FILELIST *fl_temp;
    int magic;

    if (this->window_tab == this->window_left) {
        fl_temp = this->filelist_left;
        magic = this->jump_stack_left[this->jump_stack_left_pos];
        if (magic == -1)
            magic = 0;

        this->filelist_left_magic = magic;
    } else {
        fl_temp = this->filelist_right;
        magic = this->jump_stack_right[this->jump_stack_right_pos];
        if (magic == -1)
            magic = 0;

        this->filelist_right_magic = magic;
    }

    if (fl_temp) {
        while (fl_temp) {
            fl_temp->is_marked = mark;
            fl_temp = fl_temp->next;
        }

        this->UpdateFilelistNewPosition(this->window_tab);
        this->UpdateTabBar(FALSE);
    }
}

bool NamesEqual(char *name1, char *name2) {
    char *c1 = name1, *c2 = name2;
    bool equal = TRUE;

    if (strlen(name1) != strlen(name2)) {
        equal = FALSE;
    }
    while (equal && *c1) {
        if ((*c1 == '.') || (*c1 == '_') || (*c1 == '-') || (*c1 == ' ')) {
            // special case
            if ((*c2 != '.') && (*c2 != '_') && (*c2 != '-') && (*c2 != ' '))
                equal = FALSE;
        } else {
            if (toupper(*c1) != toupper(*c2))
                equal = FALSE;
        }

        c1 += 1;
        c2 += 1;
    }
    return (equal);
}

int DoCompare(FILELIST * src, FILELIST * dest, char *excl, int oldcurs) {
    FILELIST *dest_start = dest;
    int first = -1;
    bool found;

    // mark entries in source, if they aren't in dest (stupid search algo, who carez here)
    while (src) {
        // should we use this entry?
        if ((FilterFilename(src->name, excl)) && ((src->is_dir == TRUE) ||
//            ((src->is_dir != TRUE) && (src->size != 0i)))) {
            ((src->is_dir != TRUE) && (src->size != 0)))) {

            dest = dest_start;
            found = FALSE;

            while (!found && dest) {
                // now make an intelligent compare. "-", "." and "_" are equal
                if (NamesEqual(src->name, dest->name))
                    found = TRUE;
                else
                    dest = dest->next;
            }

            if (!found) {
                if (first == -1)
                    first = src->magic;
            }
        } else
            found = TRUE;

        src->is_marked = !found; // mark or demark (!)
        src = src->next;
    }

    if (first != -1)
        return (first);
    else
        return (oldcurs); // first entry
}

void CDisplayHandler::CompareFiles(void) {
    FILELIST *fl_l, *fl_r;
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    fl_l = this->filelist_left;
    fl_r = this->filelist_right;

    if (fl_r && fl_l) {
        magic = this->leftwindow_magic;
        while (!found && sv_temp) {
            if (sv_temp->server->GetMagic() == magic)
                found = TRUE;
            else
                sv_temp = sv_temp->next;
        }
        if (found) {
            this->filelist_left_magic =
                DoCompare(fl_l, fl_r, sv_temp->server->GetFilter(),
                          this->filelist_left_magic);
        } else
            /* local ? */
            this->filelist_left_magic =
                DoCompare(fl_l, fl_r, NULL, this->filelist_left_magic);

        found = FALSE;
        sv_temp = global_server;
        magic = this->rightwindow_magic;
        while (!found && sv_temp) {
            if (sv_temp->server->GetMagic() == magic)
                found = TRUE;
            else
                sv_temp = sv_temp->next;
        }
        if (found) {
            this->filelist_right_magic =
                DoCompare(fl_r, fl_l, sv_temp->server->GetFilter(),
                          this->filelist_right_magic);
        } else
            /* local ? */
            this->filelist_right_magic =
                DoCompare(fl_r, fl_l, NULL, this->filelist_right_magic);

        // refresh windows
        this->UpdateFilelistNewPosition(this->window_left);
        this->UpdateFilelistNewPosition(this->window_right);
        this->UpdateTabBar(FALSE);
    }
}

void CDisplayHandler::DeleteFile(void) {
    SERVERLIST *sv_temp = global_server;
    FILELIST *fl_temp, *fl_start;
    int magic, file_magic, done = 0;
    bool found = FALSE;

    if (this->window_tab == this->window_left) {
        magic = this->leftwindow_magic;
        file_magic = this->filelist_left_magic;
        fl_start = this->filelist_left;
    } else {
        magic = this->rightwindow_magic;
        file_magic = this->filelist_right_magic;
        fl_start = filelist_right;
    }

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found && !sv_temp->server->IsBusy()) {
        // walk through list and see if we have marked entries. otherwise use the actual cursor-position
        fl_temp = fl_start;
        while (fl_temp) {
            if (fl_temp->is_marked) {
                done++;
                if (fl_temp->is_dir)
                    sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_DELDIR,
                                                     fl_temp->name);
                else
                    sv_temp->server->
                    PostFromDisplay(FOR_SERVER_MSG_DELFILE, fl_temp->name);
            }
            fl_temp = fl_temp->next;
        }

        if (done == 0) {
            fl_temp = fl_start;
            found = FALSE;
            while (!found && fl_temp) {
                if (fl_temp->magic == file_magic)
                    found = TRUE;
                else
                    fl_temp = fl_temp->next;
            }

            if (found) {
                if (fl_temp->is_dir)
                    sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_DELDIR,
                                                     fl_temp->name);
                else
                    sv_temp->server->
                    PostFromDisplay(FOR_SERVER_MSG_DELFILE, fl_temp->name);
            }
        }
        // now issue a refresh
        sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH, "");
    }
}

void CDisplayHandler::TransferFile(bool use_ok, bool after_quit) {
    SERVERLIST *sv_temp = global_server, *sv_src = NULL, *sv_dest = NULL;
    FILELIST *fl_temp, *fl_start, *fl_new, *fl_src, *fl_temp1;
    int dest_magic, src_magic, file_magic, msg, file_msg, dir_msg,
        refresh_msg = -1;
    bool found = FALSE, to_src, all_dests = FALSE;
    char dir[SERVER_WORKINGDIR_SIZE];

    if (this->window_tab == this->window_left) {
        src_magic = this->leftwindow_magic;
        dest_magic = this->rightwindow_magic;
        file_magic = this->filelist_left_magic;
        fl_start = this->filelist_left;
    } else {
        src_magic = this->rightwindow_magic;
        dest_magic = this->leftwindow_magic;
        file_magic = this->filelist_right_magic;
        fl_start = filelist_right;
    }

    while ((!sv_src || !sv_dest) && sv_temp) {
        if (sv_temp->server->GetMagic() == src_magic)
            sv_src = sv_temp;
        else if (sv_temp->server->GetMagic() == dest_magic)
            sv_dest = sv_temp;

        sv_temp = sv_temp->next;
    }
    if (sv_src == NULL || sv_dest == NULL)
        return ;

    if ((mode == MODE_FXP_CHAIN) || (mode == MODE_FXP_NOCHAIN)) {
        // check if we have found our servers
        if (!sv_src->server->IsTransfering()
            && !sv_dest->server->IsTransfering()) {
            if ((sv_src->server->GetServerType() != SERVER_TYPE_LOCAL)
                && (sv_dest->server->GetServerType() != SERVER_TYPE_LOCAL)) {
                // good. hosts are idle, none of them is local (else do nothing!)
                // walk through the filelist, look for marks
                fl_temp = fl_start;
                found = FALSE;
                while (!found && fl_temp) {
                    if (fl_temp->is_marked)
                        found = TRUE;
                    else
                        fl_temp = fl_temp->next;
                }

                fl_temp = fl_start;
                fl_temp1 = NULL; //avoid warning :)
                fl_src = NULL; //we create a filelist of the transfered files
                // and pass it to SRC which will be waiting in
                // urgent mode for the signal from DEST
                sv_dest->server->ObtainWorkingDir(dir);
                while (fl_temp) {
                    // search for a marked entry
                    if (((found && fl_temp->is_marked)
                         || (!found && fl_temp->magic == file_magic))
                        && (strcmp(fl_temp->name, "..") != 0)) {
                        fl_new = new(FILELIST);
                        fl_new->next = NULL;
                        fl_new->magic = fl_temp->magic;
                        if (fl_temp->is_dir) {
                            fl_new->name = new(char
                                               [strlen(fl_temp->name) +
                                                strlen(dir) + 2]);
                            strcpy(fl_new->name, dir);
                            strcat(fl_new->name, "/");
                            strcat(fl_new->name, fl_temp->name);
                        } else {
                            fl_new->name =
                                new(char[strlen(fl_temp->name) + 1]);
                            strcpy(fl_new->name, fl_temp->name);
                        }
                        fl_new->is_dir = fl_temp->is_dir;

                        if (fl_src == NULL) {
                            fl_src = fl_new;
                        } else {
                            fl_temp1->next = fl_new;
                        }
                        fl_temp1 = fl_new;
                    }
                    fl_temp = fl_temp->next;
                }
                if (fl_src) {
                    //now if anything is going to be transferred, lets tell the servers
                    sv_dest->server->
                    PostFromDisplay(FOR_SERVER_MSG_FXP_DEST_PREPARE,
                                    "", sv_src->server->GetMagic());
                    sv_src->server->SetTransferFilelist(fl_src);
                    sv_src->server->SetDestServer(sv_dest->server);
                    sv_dest->server->SetSrcServer(sv_src->server);

                    int todo;

                    if (use_ok == FALSE) {
                        if (after_quit == FALSE) {
                            todo = FOR_SERVER_MSG_FXP_SRC_PREPARE;
                        } else {
                            todo = FOR_SERVER_MSG_FXP_SRC_PREPARE_QUIT;
                        }
                    } else {
                        if (after_quit == FALSE) {
                            todo = FOR_SERVER_MSG_FXP_SRC_PREPARE_AS_OK;
                        } else {
                            todo = FOR_SERVER_MSG_FXP_SRC_PREPARE_AS_OK_QUIT;
                        }
                    }

                    sv_src->server->PostFromDisplay(todo, "",
                                                    sv_dest->server->GetMagic());
                }

                sv_dest->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH,
                                                 "");
                // we dont want src to refresh, coz those unmarked files left show what files was not transferred ok
                // and also coz src should not be touched from now on !!!
                // however i do think nothing bad should happen with it on :)
                //sv_src->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH, "");
            }
        } else {
            if (sv_src->server->IsTransfering()) { 
		if (sv_src->server->use_repeat) sv_src->server->use_repeat = FALSE;
                sv_src->server->StopTransfer();
            }
        }

    } else // standard ftp goes here....
        if (((mode != MODE_FTP_CHAIN) && !sv_src->server->IsBusy()
             && !sv_dest->server->IsBusy()) || (mode == MODE_FTP_CHAIN)) {
            // determine msg to post to src/local
            // FTP_NOCHAIN: src local:   upload (as_ok) to destination
            //              dest local:  leech to src with flag NO_NOTICE_WHEN_SUCCESSFUL
            //              both remote: leech (as_ok) to src with flag NOTICE_SINGLE_DEST and magic of dest
            //
            // FTP_CHAIN:   src local:   upload to destinations
            //              (there is no dest_local)
            //              all remote:  leech to src with flag NOTICE_ALL_DESTS
            //

            if (mode == MODE_FTP_NOCHAIN) {
                // no chaining
                if (sv_src->server->GetServerType() == SERVER_TYPE_LOCAL) {
                    // src local, dest remote
                    if (!use_ok) {
                        file_msg = FOR_SERVER_MSG_UPLOAD_NO_WAIT;
                        dir_msg = FOR_SERVER_MSG_UPLOAD_DIR_NO_WAIT;
                    } else {
                        file_msg = FOR_SERVER_MSG_UPLOAD_NO_WAIT_AS_OK;
                        dir_msg = FOR_SERVER_MSG_UPLOAD_DIR_NO_WAIT_AS_OK;
                    }

                    to_src = FALSE;
                } else if (sv_dest->server->GetServerType() ==
                           SERVER_TYPE_LOCAL) {
                    // src remote, dest local
                    file_msg = FOR_SERVER_MSG_LEECH_NO_NOTICE;
                    dir_msg = FOR_SERVER_MSG_LEECH_DIR_NO_NOTICE;
                    refresh_msg = FOR_SERVER_MSG_REFRESH;
                    to_src = TRUE;
                } else {
                    // both remote
                    if (!use_ok) {
                        file_msg = FOR_SERVER_MSG_LEECH_NOTICE_SINGLE;
                        dir_msg = FOR_SERVER_MSG_LEECH_DIR_NOTICE_SINGLE;
                    } else {
                        file_msg = FOR_SERVER_MSG_LEECH_NOTICE_SINGLE_AS_OK;
                        dir_msg = FOR_SERVER_MSG_LEECH_DIR_NOTICE_SINGLE_AS_OK;
                    }

                    refresh_msg = FOR_SERVER_MSG_EMIT_REFRESH_SINGLE;
                    to_src = TRUE;
                }
            } else {
                // chaining
                if (sv_src->server->GetServerType() == SERVER_TYPE_LOCAL) {
                    // src local, dest remote
                    file_msg = FOR_SERVER_MSG_UPLOAD_NO_WAIT;
                    dir_msg = FOR_SERVER_MSG_UPLOAD_DIR_NO_WAIT;
                    to_src = FALSE;
                    all_dests = TRUE;
                } else {
                    // all remote
                    file_msg = FOR_SERVER_MSG_LEECH_NOTICE_MULTI;
                    dir_msg = FOR_SERVER_MSG_LEECH_DIR_NOTICE_MULTI;
                    refresh_msg = FOR_SERVER_MSG_EMIT_REFRESH_MULTI;
                    to_src = TRUE;
                }
            }

            // walk through the filelist, look for marks
            fl_temp = fl_start;
            found = FALSE;
            while (!found && fl_temp) {
                if (fl_temp->is_marked)
                    found = TRUE;
                else
                    fl_temp = fl_temp->next;
            }

            fl_temp = fl_start;
            while (fl_temp) {
                // search for a marked entry
                if (((found && fl_temp->is_marked)
                     || (!found && fl_temp->magic == file_magic))
                    && (strcmp(fl_temp->name, "..") != 0)) {

                    // we have either a marked entry or we reached the cursor-position
                    if (fl_temp->is_dir)
                        msg = dir_msg;
                    else
                        msg = file_msg;

                    if (all_dests) {
                        sv_temp = global_server;
                        while (sv_temp) {
                            if ((sv_temp != sv_src)
                                && (sv_temp->server->GetServerType() !=
                                    SERVER_TYPE_LOCAL)) {
                                if (to_src)
                                    sv_temp->server->PostFromDisplay(msg,
                                                                     fl_temp->name,
                                                                     dest_magic);
                                else
                                    sv_temp->server->PostFromDisplay(msg,
                                                                     fl_temp->name);
                            }
                            sv_temp = sv_temp->next;
                        }
                    } else {
                        if (to_src)
                            sv_src->server->PostFromDisplay(msg, fl_temp->name,
                                                            dest_magic);
                        else
                            sv_dest->server->PostFromDisplay(msg,
                                                             fl_temp->name);
                    }
                }
                fl_temp = fl_temp->next;
            }

            // see if we can post a refresh rite now
            if (!to_src) {
                if (all_dests) {
                    sv_temp = global_server;
                    while (sv_temp) {
                        if ((sv_temp != sv_src)
                            && (sv_temp->server->GetServerType() !=
                                SERVER_TYPE_LOCAL)) {
                            sv_temp->server->
                            PostFromDisplay(FOR_SERVER_MSG_REFRESH, "");
                        }
                        sv_temp = sv_temp->next;
                    }
                } else
                    sv_dest->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH,
                                                     "");
            } else {
                if (refresh_msg == FOR_SERVER_MSG_EMIT_REFRESH_SINGLE)
                    sv_src->server->PostFromDisplay(refresh_msg, "",
                                                    dest_magic);
                else
                    sv_src->server->PostFromDisplay(refresh_msg, "");
            }
        }
}

void CDisplayHandler::UpdateSitePrefs(CServer * server) {
    BOOKMARK *bm_temp = global_bookmark;
    bool found = FALSE;

    while (!found && bm_temp) {
        if (bm_temp->magic == this->siteopen_bm_realmagic)
            found = TRUE;
        else
            bm_temp = bm_temp->next;
    }

    if (found)
        server->SetServerPrefs(bm_temp);
}

int CDisplayHandler::TrySite(WINDOW * window) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    if (window == this->window_left)
        magic = this->leftwindow_magic;
    else
        magic = this->rightwindow_magic;

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found) {
        if (!sv_temp->server->IsBusy()) {
            if (sv_temp->server->GetServerType() == SERVER_TYPE_REMOTE)
                return (2);
            else
                return (0);
        } else {
            if (sv_temp->server->GetServerType() == SERVER_TYPE_REMOTE)
                return (1);
            else
                return (0);
        }
    }

    return (0);
}

void CDisplayHandler::GetSiteAlias(int code) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    if (this->window_tab == this->window_left)
        magic = this->leftwindow_magic;
    else
        magic = this->rightwindow_magic;

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found) {
        if (sv_temp->server->GetServerType() == SERVER_TYPE_REMOTE) {
            switch (code) {
                case 1:
                case 3:
                    strcpy(this->input_temp,
                           sv_temp->server->GetSiteAlias(code));
                    break;

                default:
                    sprintf(this->input_temp, "%s %s",
                            sv_temp->server->GetSiteAlias(code),
                            sv_temp->server->GetPrefs()->user);
                    break;
            }
        } else
            strcpy(this->input_temp, "<INTERNAL ERROR>");
    } else
        strcpy(this->input_temp, "<INTERNAL ERROR>");
}

CServer *CDisplayHandler::TryPrefs(int *bm_magic) {
    SERVERLIST *sv_temp = global_server;
    BOOKMARK *bm_temp = global_bookmark;
    int magic;
    bool found = FALSE;

    if (this->window_tab == this->window_left)
        magic = this->leftwindow_magic;
    else
        magic = this->rightwindow_magic;

    if (magic == SERVER_MAGIC_START)
        return (NULL);
    else {
        while (!found && sv_temp) {
            if (sv_temp->server->GetMagic() == magic)
                found = TRUE;
            else
                sv_temp = sv_temp->next;
        }

        if (found == FALSE)
            return (NULL);

        *bm_magic = sv_temp->server->GetBMMagic();
        found = FALSE;
        while (!found && bm_temp) {
            if (bm_temp->magic == *bm_magic)
                found = TRUE;
            else
                bm_temp = bm_temp->next;
        }
        if (found)
            return (sv_temp->server);
        else
            return (NULL);
    }
}

CServer *CDisplayHandler::TryOpenLog(void) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    bool found = FALSE;

    if (this->window_tab == this->window_left)
        magic = this->leftwindow_magic;
    else
        magic = this->rightwindow_magic;

    /*
    if((magic == SERVER_MAGIC_START)&&(localwindowlog[0]==NULL))
        return(NULL);
    else {
    */

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }
    if (found == FALSE)
        return (NULL);

    return (sv_temp->server);

    //}
}

int CDisplayHandler::IsOkSwitchKey(int i) { 
  if (9 > (i - 49) && (i - 49) >= 0) {
    return(i - 49);
  } else if (i == '!') {
    return 9;
  } else if (i == '@') {
    return 10;
  } else if (i == '#') {
    return 11;
  } else if (i == '$') {
    return 12;
  } else if (i == '%') {
    return 13;
  } else if (i == '^') {
    return 14;
  } else if (i == '&') {
    return 15;
  } else if (i == '*') {
    return 16;
  } else if (i == '(') {
    return 17;
  } else if (i == ')') {
    return 18;
  } else { return -1; } 
}

void CDisplayHandler::AssignSwitch(int i) {
    char temp_leftalias[5], temp_rightalias[5];
    SERVERLIST *sv_temp = global_server, *sv_temp2 = global_server;

    assert(i >= 0);
    assert(i < 19);

    // setting switch pair i
    switch_list[i].used = TRUE;
    switch_list[i].magic_left = this->leftwindow_magic;
    switch_list[i].magic_right = this->rightwindow_magic;

    // getting site aliases
    while (sv_temp) {
        if (sv_temp->server->GetMagic() == this->leftwindow_magic) {
            sprintf(temp_leftalias, "%-3.3s", sv_temp->server->GetAlias());
            break;
        }
        sv_temp = sv_temp->next;
    }
    while (sv_temp2) {
        if (sv_temp2->server->GetMagic() == this->rightwindow_magic) {
            sprintf(temp_rightalias, "%3.3s", sv_temp2->server->GetAlias());
            break;
        }
        sv_temp2 = sv_temp2->next;
    }

    // notify statusline
    sprintf(temp_string, "[%-8.8s <> %8.8s] assigned fast switch to key '%d'", temp_leftalias, temp_rightalias,i + 1);
    this->PostStatusLine(temp_string, FALSE);
}

void CDisplayHandler::AutoAssignSwitch(void) {
    char temp_leftalias[4], temp_rightalias[4];
    SERVERLIST *sv_temp = global_server, *sv_temp2 = global_server;
    int i, free_switch = 20;
    bool need_switch = TRUE;

    //debuglog("AAS start for left_magic: %d, right_magic: %d", this->leftwindow_magic, this->rightwindow_magic);

    for (i = 18; i >= 0; i--) {
        debuglog("AAS loop thrue switches - %d", i);
        if (switch_list[i].used != TRUE && need_switch) {
            debuglog("AAS found free switch %d", i);
            free_switch = i;
        }
        // checking if this switch is already assign to the site-pair
        if (switch_list[i].magic_left == this->leftwindow_magic) {
            if (switch_list[i].magic_right == this->rightwindow_magic) {
                //free_switch = 10;
                need_switch = FALSE;
                debuglog("AAS dont need switch 1");
            }
        } else {
            // checking if current window-site-pair aint assign reversed already
            if (switch_list[i].magic_left == this->rightwindow_magic) {
                if (switch_list[i].magic_right == this->leftwindow_magic) {
                    //free_switch = 10;
                    need_switch = FALSE;
                    debuglog("AAS dont need switch 2");
                }
            }
        }
    }
    debuglog("AAS free_switch is %d", free_switch);

    if (need_switch && free_switch != 20) {
        debuglog("AAS assign free_switch start - %d", free_switch);
        // setting switch pair i
        switch_list[free_switch].used = TRUE;
        switch_list[free_switch].magic_left = this->leftwindow_magic;
        switch_list[free_switch].magic_right = this->rightwindow_magic;

        // getting site labels
        while (sv_temp) {
            if (sv_temp->server->GetMagic() == this->leftwindow_magic) {
                sprintf(temp_leftalias, "%-3.3s", sv_temp->server->GetAlias());
                break;
            }
            sv_temp = sv_temp->next;
        }
        while (sv_temp2) {
            if (sv_temp2->server->GetMagic() == this->rightwindow_magic) {
                sprintf(temp_rightalias, "%3.3s", sv_temp2->server->GetAlias());
                //free_switch = 10;
                break;
            }
            sv_temp2 = sv_temp2->next;
        }
        debuglog("AAS after free_switch assign");
        // notify statusline
        sprintf(temp_string, "[%-8.8s <> %8.8s] auto-assigned fast switch to key '%d'", temp_leftalias, temp_rightalias, free_switch + 1);
        this->PostStatusLine(temp_string, FALSE);
    }
    debuglog("AAS after post status line");
}

void CDisplayHandler::JumpSwitch(int i) {
    SERVERLIST *sv_temp = global_server, *sv_temp2 = global_server;

    if (switch_list[i].used != TRUE)
        return ;

    while (sv_temp) {
        if (sv_temp->server->GetMagic() == switch_list[i].magic_left)
            break;

        sv_temp = sv_temp->next;
    }

    if (!sv_temp)
        return ;

    while (sv_temp2) {
        if (sv_temp2->server->GetMagic() == switch_list[i].magic_right)
            break;

        sv_temp2 = sv_temp2->next;
    }

    if (!sv_temp2)
        return ;

    this->leftwindow_magic = switch_list[i].magic_left;
    if (this->leftwindow_busy) {
        delete[](this->leftwindow_busy);
        this->leftwindow_busy = NULL;
    }
    // determine label
    this->UpdateServerLabel(switch_list[i].magic_left);
    this->UpdateServerCWD(sv_temp->server, this->window_left);
    this->UpdateServerFilelist(sv_temp->server, this->window_left);
    this->FetchBusy(sv_temp->server, this->window_left);

    this->rightwindow_magic = switch_list[i].magic_right;
    if (this->rightwindow_busy) {
        delete[](this->rightwindow_busy);
        this->rightwindow_busy = NULL;
    }
    // determine label
    this->UpdateServerLabel(switch_list[i].magic_right);
    this->UpdateServerCWD(sv_temp2->server, this->window_right);
    this->UpdateServerFilelist(sv_temp2->server, this->window_right);
    this->FetchBusy(sv_temp2->server, this->window_right);
}

void CDisplayHandler::NextSwitch(bool b) {
    SERVERLIST *sv_temp = global_server, *sv_temp2;
    int magic, magic2, newmagic;

    this->switch_count = 0;
    if (this->window_tab == this->window_left) {
        magic = this->leftwindow_magic;
        magic2 = this->rightwindow_magic;
    } else {
        magic2 = this->leftwindow_magic;
        magic = this->rightwindow_magic;
    }
    while (sv_temp) {
        if (sv_temp->server->GetMagic() == magic) {
            break;
        }
        sv_temp = sv_temp->next;
    }

    if (!sv_temp)
        return ;

    if (b) {
        sv_temp = sv_temp->next;
        if (sv_temp == NULL)
            return ;

        newmagic = sv_temp->server->GetMagic();
        if (newmagic == magic2) {
            sv_temp = sv_temp->next;
            if (sv_temp == NULL)
                return ;

            newmagic = sv_temp->server->GetMagic();
        }
    } else {
        newmagic = 0;
        sv_temp2 = global_server;
        while (sv_temp2->next) {
            if (sv_temp2->next == sv_temp) {
                sv_temp = sv_temp2;
                newmagic = sv_temp->server->GetMagic();
                break;
            }
            sv_temp2 = sv_temp2->next;
        }
        if (sv_temp2->next == NULL)
            return ;

        if (newmagic == magic2) {
            sv_temp2 = global_server;
            while (sv_temp2->next) {
                if (sv_temp2->next == sv_temp) {
                    sv_temp = sv_temp2;
                    newmagic = sv_temp->server->GetMagic();
                    break;
                }
                sv_temp2 = sv_temp2->next;
            }
            if (sv_temp2->next == NULL)
                return ;
        }
    }

    if (this->window_tab == this->window_left) {
        this->leftwindow_magic = newmagic;
        if (this->leftwindow_busy) {
            delete[](this->leftwindow_busy);
            this->leftwindow_busy = NULL;
        }
        // determine label
        this->UpdateServerLabel(newmagic);
        this->UpdateServerCWD(sv_temp->server, this->window_left);
        this->UpdateServerFilelist(sv_temp->server, this->window_left);
        this->FetchBusy(sv_temp->server, this->window_left);
    } else {
        this->rightwindow_magic = newmagic;
        if (this->rightwindow_busy) {
            delete[](this->rightwindow_busy);
            this->rightwindow_busy = NULL;
        }
        // determine label
        this->UpdateServerLabel(newmagic);
        this->UpdateServerCWD(sv_temp->server, this->window_right);
        this->UpdateServerFilelist(sv_temp->server, this->window_right);
        this->FetchBusy(sv_temp->server, this->window_right);
    }
}

void CDisplayHandler::PerformSwitch(void) {
    //BOOKMARK *bm_temp;
    SERVERLIST *sv_temp = global_server, *sv_temp2, *sv_tmp;
    int magic, magic2, magtmp, n = 0;
    //bool found = FALSE;

    while (sv_temp && (n < this->switch_pos)) {
        sv_temp = sv_temp->next;
        n++;
    }

    if (sv_temp) {
        magic = sv_temp->server->GetMagic();
        if (sv_temp->server->IsTransfering() == TRUE) {
            magic2 = sv_temp->server->GetFXPSourceMagic();
            if (magic2 == magic) {
                magic2 = sv_temp->server->GetFXPDestMagic();
            }
            sv_temp2 = global_server;
            while (sv_temp2) {
                if (sv_temp2->server->GetMagic() == magic2)
                    break;
                sv_temp2 = sv_temp2->next;
            }
            if (!sv_temp2)
                return ; //important

            if (this->window_tab != this->window_left) {
                magtmp = magic2;
                magic2 = magic;
                magic = magtmp;
                sv_tmp = sv_temp2;
                sv_temp2 = sv_temp;
                sv_temp = sv_tmp;
            }
            if (this->leftwindow_magic != magic) {
                this->leftwindow_magic = magic;
                if (this->leftwindow_busy) {
                    delete[](this->leftwindow_busy);
                    this->leftwindow_busy = NULL;
                }
                // determine label
                this->filelist_left_soffset = 0;
                this->UpdateServerLabel(magic);
                this->UpdateServerCWD(sv_temp->server, this->window_left);
                this->UpdateServerFilelist(sv_temp->server, this->window_left);
                this->FetchBusy(sv_temp->server, this->window_left);
            }
            if (this->rightwindow_magic != magic2) {
                this->rightwindow_magic = magic2;
                if (this->rightwindow_busy) {
                    delete[](this->rightwindow_busy);
                    this->rightwindow_busy = NULL;
                }
                // determine label
                this->filelist_right_soffset = 0;
                this->UpdateServerLabel(magic2);
                this->UpdateServerCWD(sv_temp2->server, this->window_right);
                this->UpdateServerFilelist(sv_temp2->server, this->window_right);
                this->FetchBusy(sv_temp2->server, this->window_right);
            }
        } else if ((this->leftwindow_magic != magic)
                   && (this->rightwindow_magic != magic)) {
            if (this->window_tab == this->window_left) {
                this->leftwindow_magic = sv_temp->server->GetMagic();
                this->filelist_left_soffset = 0;
                if (this->leftwindow_busy) {
                    delete[](this->leftwindow_busy);
                    this->leftwindow_busy = NULL;
                }
            } else {
                this->rightwindow_magic = sv_temp->server->GetMagic();
                this->filelist_right_soffset = 0;
                if (this->rightwindow_busy) {
                    delete[](this->rightwindow_busy);
                    this->rightwindow_busy = NULL;
                }
            }
            // determine label
            this->UpdateServerLabel(sv_temp->server->GetMagic());
            this->UpdateServerCWD(sv_temp->server, this->window_tab);
            this->UpdateServerFilelist(sv_temp->server, this->window_tab);
            this->FetchBusy(sv_temp->server, this->window_tab);
        }
    }
    // else.. uhm, cant happen
}

void CDisplayHandler::PerformSwitchClose(void) {
    SERVERLIST *sv_temp = global_server, *sv_temp2;
    int magic, issrc;
    int n = 0;

    while (sv_temp && (n < this->switch_pos)) {
        sv_temp = sv_temp->next;
        n++;
    }

    if (sv_temp) {
        if (sv_temp->server->GetMagic() != SERVER_MAGIC_START) {
            if (sv_temp->server->IsTransfering() == TRUE) {
                magic = sv_temp->server->GetFXPSourceMagic();
                if (magic == sv_temp->server->GetMagic()) {
                    issrc = 1;
                    magic = sv_temp->server->GetFXPDestMagic();
                } else
                    issrc = 0;

                sv_temp2 = global_server;
                while (sv_temp2) {
                    if (sv_temp2->server->GetMagic() == magic)
                        break;

                    sv_temp2 = sv_temp2->next;
                }
                if (!sv_temp2)
                    return ;

                if (issrc) {
                    this->KillServer(sv_temp->server);
                    this->KillServer(sv_temp2->server);
                } else {
                    this->KillServer(sv_temp2->server);
                    this->KillServer(sv_temp->server);
                }
                this->switch_count -= 2;
                this->switch_pos -= 2;
            } else {
                this->KillServer(sv_temp->server);
                this->switch_count--;
                this->switch_pos--;
            }
            if (this->switch_pos < 0)
                this->switch_pos = 0;
        }
    }
}

void CDisplayHandler::KillServer(CServer * server) {
    SERVERLIST *sv_temp1 = NULL, *sv_temp = global_server;
    bool found = FALSE;
    int magic, n;
    WINDOW *window_to_fill = NULL;

    magic = server->GetMagic();
    if (this->leftwindow_magic == magic)
        window_to_fill = this->window_left;
    else if (this->rightwindow_magic == magic)
        window_to_fill = this->window_right;

    // first kill the thread
    pthread_cancel(server->GetThread());
    pthread_join(server->GetThread(), NULL);

    for (n = 0; n < 19; n++) {
        if ((switch_list[n].used == TRUE))
            if ((switch_list[n].magic_left == magic)
                || (switch_list[n].magic_right == magic))
                switch_list[n].used = FALSE;
    }
    this->DrawSwitchPairs();

    // now it's associated object and list-entry
    // no segfault protection, must work!
    assert(sv_temp->server != NULL);
    while (sv_temp->server != server) {
        sv_temp1 = sv_temp;
        sv_temp = sv_temp->next;
    }

    assert(sv_temp1 != NULL);
    if (sv_temp1) {
        sv_temp1->next = sv_temp->next;
        delete(sv_temp->server); // free up all allocated resources
        delete(sv_temp);
    } else { // should never happen since local "server" is always open
        global_server = sv_temp->next;
        delete(sv_temp->server);
        delete(sv_temp);
    }

    if (window_to_fill) {
        // now let the find-server-for-emtpy-window algorithm take place...
        // simply find a server which isn't in leftwindow_magic or rightwindow_magic
        // if none is available, simply clear it
        // but avoid switching local filesys in front if remote is available!
        sv_temp = global_server->next;
        while (!found && sv_temp) {
            magic = sv_temp->server->GetMagic();
            if ((magic != this->leftwindow_magic)
                && (magic != this->rightwindow_magic))
                found = TRUE;
            else
                sv_temp = sv_temp->next;
        }

        if (!found) {
            // okiez, none appropriate. see if we can choose the local-filesys
            magic = global_server->server->GetMagic();
            if ((magic != this->leftwindow_magic)
                && (magic != this->rightwindow_magic)) {
                found = TRUE;
                sv_temp = global_server;
            }
        }

        if (found) {
            // welp, do all necessary things to "link" the server to our view
            if (window_to_fill == this->window_left) {
                this->leftwindow_magic = magic;

#ifdef TLS
                if (sv_temp->server->TLS_Status()) {
                    this->window_left_label[0] = '*';
                    strcpy(this->window_left_label + 1, sv_temp->server->GetAlias());
                } else
#endif

                    strcpy(this->window_left_label, sv_temp->server->GetAlias());
            } else {

#ifdef TLS
                if (sv_temp->server->TLS_Status()) {
                    this->window_right_label[0] = '*';
                    strcpy(this->window_right_label + 1, sv_temp->server->GetAlias());
                } else
#endif

                    strcpy(this->window_right_label, sv_temp->server->GetAlias());

                this->rightwindow_magic = magic;
            }

            this->UpdateServerCWD(sv_temp->server, window_to_fill);
            this->UpdateServerFilelist(sv_temp->server, window_to_fill);
            this->FetchBusy(sv_temp->server, window_to_fill);
        } else {
            // cleanup occupied view
            this->FreeWindow(window_to_fill);
        }
    }
    // else do nothing since a server bailed out but wasn't in our view
}

void CDisplayHandler::DeMark(WINDOW * window, char *name) {
    FILELIST *fl_temp;
    bool found = FALSE;

    if (window == this->window_left)
        fl_temp = this->filelist_left;
    else
        fl_temp = this->filelist_right;

    while (!found && fl_temp) {
        if (!strcmp(fl_temp->name, name))
            found = TRUE;
        else
            fl_temp = fl_temp->next;
    }

    if (found) {
        if (fl_temp->is_marked == TRUE) {
            fl_temp->is_marked = FALSE;
            this->UpdateFilelistNewPosition(window);
            this->UpdateTabBar(FALSE);
        }
    }
}

void CDisplayHandler::Mark(WINDOW * window, char *name) {
    FILELIST *fl_temp;
    bool found = FALSE;

    if (window == this->window_left)
        fl_temp = this->filelist_left;
    else
        fl_temp = this->filelist_right;

    while (!found && fl_temp) {
        if (!strcmp(fl_temp->name, name))
            found = TRUE;
        else
            fl_temp = fl_temp->next;
    }

    if (found) {
        if (fl_temp->is_marked == FALSE) {
            fl_temp->is_marked = TRUE;
            this->UpdateFilelistNewPosition(window);
            this->UpdateTabBar(FALSE);
        }
    }
}

void CDisplayHandler::FreeWindow(WINDOW * window) {
    FILELIST *fl_temp, *fl_temp1;

    wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(window, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(window);
    //wbkgdset(window, ' ');
    mywborder(window);

    //wnoutrefresh(window);
    update_panels();
    doupdate();

    // correct position of tab-bar
    if (window == this->window_left) {
        if (this->filelist_right)
            this->window_tab = this->window_right;

        fl_temp = this->filelist_left;
        this->leftwindow_magic = -1;
        this->filelist_left = NULL;
    } else {
        if (this->filelist_left)
            this->window_tab = this->window_left;

        fl_temp = this->filelist_right;
        this->rightwindow_magic = -1;
        this->filelist_right = NULL;
    }

    // free filelist to prevent tab-bar switching
    while (fl_temp) {
        fl_temp1 = fl_temp;
        fl_temp = fl_temp->next;
        delete[](fl_temp1->name);
        delete(fl_temp1);
    }
    this->UpdateTabBar(FALSE);
}

void CDisplayHandler::LeechOkayNowUpload(int magic, char *file,
                                         bool single, bool use_ok) {
    SERVERLIST *sv_temp = global_server;

    // this one gets called everytime a server successfully started a leech. now we have to notice the destination(s) that a file is available to upload
    // if single = TRUE, post to magic. else to all REMOTE ones except magic
    while (sv_temp) {
        if (single) {
            if (sv_temp->server->GetMagic() == magic) {
                if (!use_ok)
                    sv_temp->server->
                    PostFromDisplay(FOR_SERVER_MSG_UPLOAD, file);
                else
                    sv_temp->server->
                    PostFromDisplay(FOR_SERVER_MSG_UPLOAD_AS_OK, file);
            }
        } else {
            // broadcast, see if the server is chained (NO mode check, because we were in chaining as this was issued)
            if ((sv_temp->server->GetMagic() != magic)
                && (sv_temp->server->GetChaining())
                && (sv_temp->server->GetServerType() == SERVER_TYPE_REMOTE)
                && (sv_temp->server->AllowChaining()))
                sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_UPLOAD, file); // there is no '.ok' in multi !
        }
        sv_temp = sv_temp->next;
    }
}

void CDisplayHandler::DirOkayNowMKD(int magic, char *dir, bool single) {
    SERVERLIST *sv_temp = global_server;

    // if single = TRUE, post to magic. else to all REMOTE ones except magic
    while (sv_temp) {
        if (single) {
            if (sv_temp->server->GetMagic() == magic) {
                sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_PREP, dir);
            }
        } else {
            // broadcast, see if the server is chained (NO mode check, because we were in chaining as this was issued)
            sv_temp->server->ClearAllowChaining();
            if ((sv_temp->server->GetMagic() != magic)
                && (sv_temp->server->GetChaining())
                && (sv_temp->server->GetServerType() == SERVER_TYPE_REMOTE)) {
                if (FilterDirname(dir, sv_temp->server->GetTargetFilter())) {
                    sv_temp->server->SetAllowChaining();
                    sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_PREP, dir);
                }
            }
        }
        sv_temp = sv_temp->next;
    }
}

void CDisplayHandler::DirOkayNowCWD(int magic, char *dir, bool single) {
    SERVERLIST *sv_temp = global_server;

    // if single = TRUE, post to magic. else to all REMOTE ones except magic
    while (sv_temp) {
        if (single) {
            if (sv_temp->server->GetMagic() == magic) {
                sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_CWD, dir);
            }
        } else {
            // broadcast, see if the server is chained (NO mode check, because we were in chaining as this was issued)
            if ((sv_temp->server->GetMagic() != magic)
                && (sv_temp->server->GetChaining())
                && (sv_temp->server->GetServerType() == SERVER_TYPE_REMOTE)) {
                if (sv_temp->server->AllowChaining())
                    // if (FilterDirname(dir, sv_temp->server->GetTargetFilter()))
                    sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_CWD, dir);
            }
        }
        sv_temp = sv_temp->next;
    }
}

void CDisplayHandler::TransferOkayNowRefresh(int magic, bool single) {
    SERVERLIST *sv_temp = global_server;

    // if single = TRUE, post to magic. else to all REMOTE ones except magic
    while (sv_temp) {
        if (single) {
            if ((sv_temp->server->GetMagic() == magic)
                || (sv_temp->server->GetServerType() == SERVER_TYPE_LOCAL)) {
                sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH,
                                                 "");
            }
        } else {
            // broadcast, see if the server is chained (NO mode check, because we were in chaining as this was issued)
            if ((sv_temp->server->GetMagic() != magic)
                && (sv_temp->server->GetChaining()))
                sv_temp->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH,
                                                 "");
        }
        sv_temp = sv_temp->next;
    }
}

void CDisplayHandler::PostUrgent(int msg, int magic, char *param) {
    SERVERLIST *sv_temp = global_server;
    bool found = FALSE;

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;

    }

    if (found) {
        sv_temp->server->PostUrgentFromDisplay(msg, param);
    }
    // this one is critical... hmm.. server WAITS for an urgent message :(
}

void CDisplayHandler::HandleServerMessage(int msg, int magic, char *data) {
    SERVERLIST *sv_temp = global_server;
    bool valid = FALSE;
    WINDOW *window = NULL;

    // see if the msg is usable, i.e. if the server still exists
    // no lock on serverlist since we are the only thread which could add/del entries
    while (!valid && sv_temp) {
        if (sv_temp->magic == magic)
            valid = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    // some msgs dont have a from-server-magic, but another magic... let them fall through
    if (!valid
        && (msg == SERVER_MSG_NOTICE_UPLOAD_SINGLE
            || msg == SERVER_MSG_NOTICE_UPLOAD_SINGLE_AS_OK))
        valid = TRUE;

    // okiez, handle message
    if (valid) {
        if (magic == this->leftwindow_magic)
            window = this->window_left;
        else if (magic == this->rightwindow_magic)
            window = this->window_right;

        if (window) {
            switch (msg) {
                case SERVER_MSG_BUSY:
                    this->UpdateServerBusy(window, data);
                    break;

                case SERVER_MSG_IDLE:
                    this->UpdateServerBusy(window, data);
                    break;

                case SERVER_MSG_NEW_CWD:
                    this->UpdateServerCWD(sv_temp->server, window);
                    break;

                case SERVER_MSG_NEW_FILELIST:
                    this->UpdateServerFilelist(sv_temp->server, window);
                    break;

                case SERVER_MSG_DEMARK:
                    this->DeMark(window, data);
                    break;

                case SERVER_MSG_MARK:
                    this->Mark(window, data);
                    break;

                case SERVER_MSG_KILLME:
                    this->KillServer(sv_temp->server);
                    break;
                case SERVER_MSG_NOTICE_UPLOAD_SINGLE:
                    this->LeechOkayNowUpload(magic, data, TRUE, FALSE);
                    break;

                case SERVER_MSG_NOTICE_UPLOAD_SINGLE_AS_OK:
                    this->LeechOkayNowUpload(magic, data, TRUE, TRUE);
                    break;

                case SERVER_MSG_NOTICE_UPLOAD_MULTI:
                    this->LeechOkayNowUpload(magic, data, FALSE, FALSE);
                    break;

                case SERVER_MSG_NOTICE_LEECH_SINGLE_MKD:
                    this->DirOkayNowMKD(magic, data, TRUE);
                    break;

                case SERVER_MSG_NOTICE_LEECH_MULTI_MKD:
                    this->DirOkayNowMKD(magic, data, FALSE);
                    break;

                case SERVER_MSG_NOTICE_LEECH_SINGLE_CWD:
                    this->DirOkayNowCWD(magic, data, TRUE);
                    break;

                case SERVER_MSG_NOTICE_LEECH_MULTI_CWD:
                    this->DirOkayNowCWD(magic, data, FALSE);
                    break;

                case SERVER_MSG_URGENT_OKAY:
                    this->PostUrgent(FOR_SERVER_MSG_URGENT_OKAY, magic, data);
                    break;

                case SERVER_MSG_URGENT_ERROR:
                    this->PostUrgent(FOR_SERVER_MSG_URGENT_ERROR, magic, data);
                    break;

                case SERVER_MSG_EMIT_REFRESH_SINGLE:
                    this->TransferOkayNowRefresh(magic, TRUE);
                    break;

                case SERVER_MSG_NEWLABEL:
                    this->UpdateServerLabel(magic);
                    break;

                case SERVER_MSG_EMIT_REFRESH_MULTI:
                    this->TransferOkayNowRefresh(magic, FALSE);
                    break;

                case SERVER_MSG_NOTICE_VIEW:
                    this->NoticeViewFile(data);
                    break;
            }
        } else {
            // non window-related things (don't depend on a window)
            switch (msg) {
                case SERVER_MSG_KILLME:
                    this->KillServer(sv_temp->server);
                    break;

                case SERVER_MSG_NEWLABEL:
                    this->UpdateServerLabel(magic);
                    break;

                case SERVER_MSG_NOTICE_UPLOAD_SINGLE:
                    this->LeechOkayNowUpload(magic, data, TRUE, FALSE);
                    break;

                case SERVER_MSG_NOTICE_UPLOAD_SINGLE_AS_OK:
                    this->LeechOkayNowUpload(magic, data, TRUE, TRUE);
                    break;

                case SERVER_MSG_NOTICE_UPLOAD_MULTI:
                    this->LeechOkayNowUpload(magic, data, FALSE, FALSE);
                    break;

                case SERVER_MSG_NOTICE_LEECH_SINGLE_MKD:
                    this->DirOkayNowMKD(magic, data, TRUE);
                    break;

                case SERVER_MSG_NOTICE_LEECH_MULTI_MKD:
                    this->DirOkayNowMKD(magic, data, FALSE);
                    break;

                case SERVER_MSG_NOTICE_LEECH_SINGLE_CWD:
                    this->DirOkayNowCWD(magic, data, TRUE);
                    break;

                case SERVER_MSG_NOTICE_LEECH_MULTI_CWD:
                    this->DirOkayNowCWD(magic, data, FALSE);
                    break;

                case SERVER_MSG_URGENT_OKAY:
                    this->PostUrgent(FOR_SERVER_MSG_URGENT_OKAY, magic, data);
                    break;

                case SERVER_MSG_URGENT_ERROR:
                    this->PostUrgent(FOR_SERVER_MSG_URGENT_ERROR, magic, data);
                    break;

                case SERVER_MSG_EMIT_REFRESH_SINGLE:
                    this->TransferOkayNowRefresh(magic, TRUE);
                    break;

                case SERVER_MSG_EMIT_REFRESH_MULTI:
                    this->TransferOkayNowRefresh(magic, FALSE);
                    break;

                case SERVER_MSG_NOTICE_VIEW:
                    this->NoticeViewFile(data);
                    break;
            }
        }
    }
}

void CDisplayHandler::Loop(void) {
    MSG_LIST *msg_temp;
    SERVER_MSG_LIST *server_msg_temp;
    STATUS_MSG_LIST *status_msg_temp;
    int msg, extended, function_key, magic, speed_wait = 0;
    bool no_action, force_speed;
    char *data;
    this->thread_running = TRUE;

    do {
		if (strcmp(connectSites,"1") == 0) {
			//this->PostStatusLine("YEH YEH YEH YEH RACE RACE RACE!!!", TRUE);
			this->UdpLogin();
			connectSites = "0";
		}
        pthread_testcancel();
        no_action = TRUE;
        if (this->screen_too_small == FALSE) {
            // look for keyhandler msgs
            pthread_mutex_lock(&(this->message_lock));
            msg_temp = this->message_stack;

            if (msg_temp) {
                this->message_stack = msg_temp->next;
                msg = msg_temp->msg;
                extended = msg_temp->extended;
                function_key = msg_temp->function_key;
                delete(msg_temp);
                pthread_mutex_unlock(&(this->message_lock));
                pthread_testcancel();
                this->HandleMessage(msg, extended, function_key);
                pthread_testcancel();
                pthread_mutex_lock(&sigwinch_lock);
                if (need_resize) {
                    debuglog("sigwinch(0) from Loop(), after handle message");
                    this->sigwinch(0);
                }
                pthread_mutex_unlock(&sigwinch_lock);
                no_action = FALSE;
            } else
                pthread_mutex_unlock(&(this->message_lock));

            if ((this->internal_state == DISPLAY_STATE_NORMAL)
                || (this->internal_state == DISPLAY_STATE_WELCOME)
                || (this->internal_state == DISPLAY_STATE_PASSWORD)) {
                // no windows which clutter up desktop

                // look for new status_log lines
                pthread_mutex_lock(&(this->status_lock));
                status_msg_temp = this->status_msg_stack;

                if (status_msg_temp) {
                    this->status_msg_stack = status_msg_temp->next;
                    this->AddStatusLine(status_msg_temp->line,
                                        status_msg_temp->highlight);
                    delete[](status_msg_temp->line);
                    delete(status_msg_temp);
                    no_action = FALSE;
                }
                pthread_mutex_unlock(&(this->status_lock));

                if (this->internal_state == DISPLAY_STATE_NORMAL) {
                    // look for server msgs
                    pthread_mutex_lock(&(this->server_message_lock));
                    server_msg_temp = this->server_message_stack;
                    if (server_msg_temp) {
                        this->server_message_stack = server_msg_temp->next;
                        msg = server_msg_temp->msg;
                        magic = server_msg_temp->magic;

                        if (server_msg_temp->data) {
                            data = new(char
                                       [strlen(server_msg_temp->data) + 1]);
                            strcpy(data, server_msg_temp->data);
                        } else
                            data = NULL;

                        delete[](server_msg_temp->data);
                        delete(server_msg_temp);

                        // now remove identical msg's which came from the same server (useless msgs)
                        // this was causing not removing status in filesystem win sometimes
                        // maybe could be fixed by leaving last and remove those before
                        /*
                        server_msg_temp = this->server_message_stack;
                        server_msg_temp1 = NULL;
                        while (server_msg_temp) {
                            if ((server_msg_temp->magic == magic)
                                && (server_msg_temp->msg == msg)
                                && (server_msg_temp->data == NULL)) {

                                // remove this one
                                if (server_msg_temp1) {
                                    server_msg_temp1->next = server_msg_temp->next;
                                    delete(server_msg_temp);
                                    server_msg_temp = server_msg_temp1->next;
                                } else {
                                    this->server_message_stack =
                                        server_msg_temp->next;
                                    delete(server_msg_temp);
                                    server_msg_temp = this->server_message_stack;
                                }
                            } else {
                                server_msg_temp1 = server_msg_temp;
                                server_msg_temp = server_msg_temp->next;
                            }
                        }
                        */

                        pthread_mutex_unlock(&(this->server_message_lock));
                        pthread_testcancel();
                        if (data)
                            debuglog
                            ("display from server message msg : '%d', magic : '%d'; param : '%s'",
                             msg, magic, data);
                        else
                            debuglog
                            ("display from server message msg : '%d', magic : '%d'; param : 'NULL'",
                             msg, magic);

                        this->HandleServerMessage(msg, magic, data);
                        if (data)
                            debuglog
                            ("DONE display from server message msg : '%d', magic : '%d'; param : '%s'",
                             msg, magic, data);
                        else
                            debuglog
                            ("DONE display from server message msg : '%d', magic : '%d'; param : 'NULL'",
                             msg, magic);

                        pthread_testcancel();
                        pthread_mutex_lock(&sigwinch_lock);

                        if (need_resize) {
                            debuglog("sigwinch(0) from Loop(), after handle server message");
                            this->sigwinch(0);
                        }
                        pthread_mutex_unlock(&sigwinch_lock);

                        if (data)
                            delete[](data);

                        no_action = FALSE;
                    } else
                        pthread_mutex_unlock(&(this->server_message_lock));
                }
            }
            pthread_mutex_lock(&sigwinch_lock);
            if (need_resize) {
                debuglog("sigwinch(0) from Loop(), before noaction");
                this->sigwinch(0);
            }
            pthread_mutex_unlock(&sigwinch_lock);
            if (no_action) {
                speed_wait++;
                if (speed_wait > SPEED_UPDATE_SLEEP) {
                    speed_wait = 0;
                    force_speed = TRUE;
                } else
                    force_speed = FALSE;

                // update speed-information
                if (this->leftwindow_busy
                    && (internal_state == DISPLAY_STATE_NORMAL))
                    this->UpdateSpeed(TRUE, force_speed);

                if (this->rightwindow_busy
                    && (internal_state == DISPLAY_STATE_NORMAL))
                    this->UpdateSpeed(FALSE, force_speed);

                this->waiting = TRUE;
                usleep(DISPLAY_MSG_SLEEP);
                this->waiting = FALSE;
            }
        } else {
            usleep(DISPLAY_MSG_SLEEP);
            pthread_mutex_lock(&sigwinch_lock);
            if (need_resize) {
                debuglog("sigwinch(0) from Loop(), in sleep");
                this->sigwinch(0);
            }
            pthread_mutex_unlock(&sigwinch_lock);
        }
    } while (TRUE); // thread will be killed by main thread
}

void CDisplayHandler::InitColors(void) {
    start_color();
//    init_pair(STYLE_NORMAL, COLOR_WHITE, COLOR_BLUE);
    init_pair(STYLE_NORMAL, COLOR_WHITE, COLOR_BLACK);
//    init_pair(STYLE_MARKED, COLOR_YELLOW, COLOR_BLUE);
    init_pair(STYLE_MARKED, COLOR_YELLOW, COLOR_BLACK);
//    init_pair(STYLE_INVERSE, COLOR_BLACK, COLOR_CYAN);
    init_pair(STYLE_INVERSE, COLOR_BLACK, COLOR_WHITE);
//    init_pair(STYLE_MARKED_INVERSE, COLOR_WHITE, COLOR_CYAN);
    init_pair(STYLE_MARKED_INVERSE, COLOR_BLUE, COLOR_WHITE);
    init_pair(STYLE_WHITE, COLOR_WHITE, COLOR_BLACK);
    init_pair(STYLE_MARKED_STATUS, COLOR_GREEN, COLOR_BLACK);
//    init_pair(STYLE_EXCLUDED, COLOR_RED, COLOR_BLUE);
    init_pair(STYLE_EXCLUDED, COLOR_RED, COLOR_BLACK);
//    init_pair(STYLE_EXCLUDED_INVERSE, COLOR_RED, COLOR_CYAN);
    init_pair(STYLE_EXCLUDED_INVERSE, COLOR_WHITE, COLOR_RED);
    init_pair(STYLE_REPEATED, COLOR_RED, COLOR_CYAN);
    if (has_colors())
        this->inverse_mono = 0;
    else
        this->inverse_mono = A_REVERSE;
}

void CDisplayHandler::DetermineScreenSize(void) {
    this->window_probe = newwin(0, 0, 0, 0);
    getmaxyx(this->window_probe, (this->terminal_max_y),
             (this->terminal_max_x));
    delwin(this->window_probe);
}

void CDisplayHandler::OpenDefaultWindows(void) {
    // create default windows based on actual screen-size
    this->window_left_width = this->terminal_max_x / 2;
    this->window_right_width = this->terminal_max_x - this->window_left_width;

    this->window_command =
        newwin(4, this->terminal_max_x, this->terminal_max_y - 4, 0);
    this->window_status =
        newwin(this->status_win_size, this->terminal_max_x,
               this->terminal_max_y - this->status_win_size - 4, 0);
    this->window_left =
        newwin(this->terminal_max_y - this->status_win_size - 4,
               this->window_left_width, 0, 0);
    this->window_right =
        newwin(this->terminal_max_y - this->status_win_size - 4,
               this->window_right_width, 0, this->window_left_width);

    leaveok(window_command, TRUE);
    leaveok(window_status, TRUE);
    leaveok(window_left, TRUE);
    leaveok(window_right, TRUE);

    scrollok(window_left, TRUE);
    scrollok(window_right, TRUE);

    this->panel_command = new_panel(this->window_command);
    this->panel_status = new_panel(this->window_status);
    this->panel_left = new_panel(this->window_left);
    this->panel_right = new_panel(this->window_right);

    /*
    this->window_command = panel_window(this->panel_command);
    this->window_status = panel_window(this->panel_status);
    this->window_left = panel_window(this->panel_left);
    this->window_right = panel_window(this->panel_right);
    */
}

void CDisplayHandler::DrawSwitchPairs(void) {
    char temp_leftalias[5], temp_rightalias[5], temp_mode[5];
    int i, magic, issrc;
    bool repeat;

    debuglog("drawswitchpairs start");
    for (i = 18; i >= 0; i--) {
        SERVERLIST *sv_temp = global_server, *sv_temp2 = global_server;

        // setting default aliases
        sprintf(temp_leftalias, " -- ");
        sprintf(temp_rightalias, " -- ");
        sprintf(temp_mode, " :: ");
        repeat = FALSE;

        if (switch_list[i].used != FALSE) {
            // finding left server for pair i
            while (sv_temp) {
                if (sv_temp->server->GetMagic() == switch_list[i].magic_left) {
                    sprintf(temp_leftalias, "%4.4s", sv_temp->server->GetAlias());
                    break;
                }
                sv_temp = sv_temp->next;
            }
            // finding right server for pair i
            while (sv_temp2) {
                if (sv_temp2->server->GetMagic() == switch_list[i].magic_right) {
                    sprintf(temp_rightalias, "%-4.4s", sv_temp2->server->GetAlias());
                    break;
                }
                sv_temp2 = sv_temp2->next;
            }

            // figuring mode for server pair
			if (sv_temp->server->use_repeat) {
				repeat = TRUE;
			}

            if (sv_temp && sv_temp2 && (sv_temp->server->IsTransfering() == TRUE)) {
                magic = sv_temp->server->GetFXPSourceMagic();

                // left server is source
                if (magic == sv_temp->server->GetMagic()) {
                    issrc = 1;
                    magic = sv_temp->server->GetFXPDestMagic();
                } else { // left server is target
                    issrc = 0;
                }
				
                if ((sv_temp2->server->GetMagic() == magic) && (issrc == 1)) {
                	if (sv_temp->server->use_repeat) {
                		sprintf(temp_mode, " => ");
                	} else {
	                    sprintf(temp_mode, " -> ");
	                }
                } else if (sv_temp2->server->GetMagic() == magic) {
                	if (sv_temp->server->use_repeat) {
                		sprintf(temp_mode, " <= ");
                	} else {
	                    sprintf(temp_mode, " <- ");
	                }
                } else {
                    sprintf(temp_mode, " :: ");
                }
            } else if (sv_temp->server->use_repeat) {
	        sprintf(temp_mode, " ** ");
	    } else {
                sprintf(temp_mode, " :: ");
            }
        }
        // drawing site aliases+mode for key i
		if (repeat) {
        	wattrset(this->window_command, COLOR_PAIR(STYLE_REPEATED) | inverse_mono);
        } else {
//        	wattrset(this->window_command, COLOR_PAIR(STYLE_INVERSE) | inverse_mono);
        	wattrset(this->window_command, COLOR_PAIR(STYLE_WHITE) | inverse_mono);
        }

        if (i <= 4) {
            mvwaddnstr(this->window_command, 0, 2 + (i * 16), temp_leftalias, 4);
            mvwaddnstr(this->window_command, 0, 6 + (i * 16), temp_mode, 4);
            mvwaddnstr(this->window_command, 0, 10 + (i * 16), temp_rightalias, 4);
        } else if (i <= 8) {
	    mvwaddnstr(this->window_command, 1, 2 + ((i - 5) * 16), temp_leftalias, 4);
	    mvwaddnstr(this->window_command, 1, 6 + ((i - 5) * 16), temp_mode, 4);
	    mvwaddnstr(this->window_command, 1, 10 + ((i - 5) * 16), temp_rightalias, 4);
	} else if (i <= 13) {
	    mvwaddnstr(this->window_command, 2, 2 + ((i - 9) * 16), temp_leftalias, 4);
	    mvwaddnstr(this->window_command, 2, 6 + ((i - 9) * 16), temp_mode, 4);
	    mvwaddnstr(this->window_command, 2, 10 + ((i - 9) * 16), temp_rightalias, 4);
	} else {
	    mvwaddnstr(this->window_command, 3, 2 + ((i - 14) * 16), temp_leftalias, 4);
	    mvwaddnstr(this->window_command, 3, 6 + ((i - 14) * 16), temp_mode, 4);
	    mvwaddnstr(this->window_command, 3, 10 + ((i - 14) * 16), temp_rightalias, 4);
		
	}
    }
    debuglog("drawswitchpairs end");
}

void CDisplayHandler::DrawCommandKeys(char *command, int y) {
    char *c = command;
    int x = 0;

    while (*c) {
        if (*c != '.')
            mvwaddch(this->window_command, y, x, (unsigned long int) (*c));

        c += 1;
        x++;
    }
}

void CDisplayHandler::BuildScreen(void) {
    // command-lines on bottom of screen
    wattrset(this->window_command, COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
    wbkgdset(this->window_command, ' ' | COLOR_PAIR(STYLE_WHITE));
    werase(this->window_command);
    //wbkgdset(this->window_command, ' ');

    wattrset(this->window_command, COLOR_PAIR(STYLE_INVERSE) | inverse_mono);
    mvwaddnstr(window_command, 0, 0, CONTROL_PANEL_1, this->terminal_max_x);
    mvwaddnstr(window_command, 1, 0, CONTROL_PANEL_2, this->terminal_max_x);
    mvwaddnstr(window_command, 2, 0, CONTROL_PANEL_3, this->terminal_max_x);
    mvwaddnstr(window_command, 3, 0, CONTROL_PANEL_4, this->terminal_max_x);

    wattrset(this->window_command, COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
    this->DrawCommandKeys(CONTROL_PANEL_1_KEYS, 0);
    this->DrawCommandKeys(CONTROL_PANEL_2_KEYS, 1);
    this->DrawCommandKeys(CONTROL_PANEL_3_KEYS, 2);
    this->DrawCommandKeys(CONTROL_PANEL_4_KEYS, 3);
    this->UpdateMode();
    //wnoutrefresh(window_command);

    // status window
    wattrset(this->window_status, COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
    wbkgdset(this->window_status, ' ' | COLOR_PAIR(STYLE_WHITE));
    werase(this->window_status);
    //wbkgdset(this->window_status, ' ');
    //wnoutrefresh(window_status);

    // the 2 filelister
    wattrset(this->window_left, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_left, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_left);
    //wbkgdset(this->window_left, ' ');
    mywborder(this->window_left);
    //wnoutrefresh(window_left);

    wattrset(this->window_right, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_right, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_right);
    //wbkgdset(this->window_right, ' ');
    mywborder(this->window_right);
    //wnoutrefresh(window_right);
    update_panels();
    doupdate();
}

void CDisplayHandler::sigwinch(int sig) {
    int cols1, lines1;
    struct winsize size;

    debuglog("sigwinch sig : '%d' need resize : '%d'", sig, need_resize);

    assert(sig == 0);
    //beware in endwin subwins should be deleted first it seems
    /*
    debuglog("sigwinch(0) before endwin");
    endwin();
    debuglog("sigwinch(0) before refresh");
    refresh();
    debuglog("sigwinch(0) after refresh");
    */

    /*
    debuglog("sigwinch(0) before newwin");
    this->window_probe = newwin(0, 0, 0, 0);
    debuglog("sigwinch(0) before getmaxxy");
    getmaxyx(this->window_probe, lines1, cols1);
    debuglog("sigwinch(0) before delwin, new lines : %d, new cols : %d",lines1,cols1);
    delwin(this->window_probe);
    */
    debuglog("sigwinch(0) before ioctl");

    ioctl(fileno(stdout), TIOCGWINSZ, &size);
    lines1 = size.ws_row;
    cols1 = size.ws_col;

    debuglog("sigwinch(0) after ioctl, new lines : %d, new cols : %d", lines1, cols1);
    debuglog("sigwinch(0) before resizeterm");

    resizeterm(lines1, cols1);

    debuglog("sigwinch(0) after resizeterm");

    //resize if possible, no need to try if terminal is small
    if ((lines1 > 22) && (cols1 > 50)) {
        debuglog("sigwinch(0) before rebuild");
        this->RebuildScreen(lines1, cols1);
        debuglog("sigwinch(0) before poststatus");
        this->PostStatusLine("resizing terminal ...", FALSE);
        this->DrawSwitchPairs();
        this->screen_too_small = FALSE;
    } else {
        debuglog("sigwinch(0) screen too small");
        this->screen_too_small = TRUE;
    }
    debuglog("DONE sigwinch need resize : '%d'", need_resize);
    need_resize = FALSE;
}

void CDisplayHandler::RebuildScreen(int y, int x) {
    clearok(stdscr, TRUE);
    this->terminal_max_x = x;
    this->terminal_max_y = y;
    //resize and move default windows...
    this->window_left_width = this->terminal_max_x / 2;
    this->window_right_width = this->terminal_max_x - this->window_left_width;
    wresize(this->window_command, 4, this->terminal_max_x);
    wresize(this->window_status, this->status_win_size, this->terminal_max_x);
    wresize(this->window_left,
            this->terminal_max_y - this->status_win_size - 4,
            this->window_left_width);
    wresize(this->window_right, this->terminal_max_y - this->status_win_size - 4,
            this->window_right_width);
    move_panel(this->panel_command, this->terminal_max_y - 4, 0);
    move_panel(this->panel_status,
               this->terminal_max_y - this->status_win_size - 4, 0);
    move_panel(this->panel_left, 0, 0);
    move_panel(this->panel_right, 0, this->window_left_width);

    //now other windows
    if (window_log) {
        wresize(this->window_log, this->terminal_max_y, this->terminal_max_x);
        //move_panel(this->panel_log, 0, 0);
        // this window should be refreshed, but we dunno if its view win or log...
        // so it will be refreshed when u press up/down or something to scroll it
    }
    /* we dont show that
    if (window_welcome) {
        //wresize(this->window_log,14,60);
        move_panel(this->panel_welcome,
                   (this->terminal_max_y - this->status_win_size - 2) / 2 -
                   7, this->terminal_max_x / 2 - 30);
    }
    */
    if (window_notice) {
        move_panel(this->panel_notice, this->terminal_max_y / 2 - 2,
                   this->terminal_max_x / 2 - 30);
    }
    if (window_input) {
        move_panel(this->panel_input, this->terminal_max_y / 2 - 2,
                   this->terminal_max_x / 2 - 30);
    }
    if (window_prefs) {
        move_panel(this->panel_prefs, this->terminal_max_y / 2 - 11,
                   this->terminal_max_x / 2 - 30);
    }
    if (window_dialog) {
        move_panel(this->panel_dialog, this->terminal_max_y / 2 - 10,
                   this->terminal_max_x / 2 - 30);
    }
    if (window_switch) {
        move_panel(this->panel_switch, this->terminal_max_y / 2 - 10,
                   this->terminal_max_x / 2 - 30);
    }
    update_panels();

    this->UpdateFilelistNewPosition(this->window_left);
    this->UpdateFilelistNewPosition(this->window_right);
    this->DisplayStatusLog();

    // command-lines on bottom of screen
    wattrset(this->window_command, COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
    wbkgdset(this->window_command, ' ' | COLOR_PAIR(STYLE_WHITE));
    werase(this->window_command);
    //wbkgdset(this->window_command, ' ');

    wattrset(this->window_command, COLOR_PAIR(STYLE_INVERSE) | inverse_mono);
    mvwaddnstr(window_command, 0, 0, CONTROL_PANEL_1, this->terminal_max_x);
    mvwaddnstr(window_command, 1, 0, CONTROL_PANEL_2, this->terminal_max_x);
    mvwaddnstr(window_command, 2, 0, CONTROL_PANEL_3, this->terminal_max_x);
    mvwaddnstr(window_command, 3, 0, CONTROL_PANEL_4, this->terminal_max_x);

    wattrset(this->window_command, COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
    this->DrawCommandKeys(CONTROL_PANEL_1_KEYS, 0);
    this->DrawCommandKeys(CONTROL_PANEL_2_KEYS, 1);
    this->DrawCommandKeys(CONTROL_PANEL_3_KEYS, 2);
    this->DrawCommandKeys(CONTROL_PANEL_4_KEYS, 3);
    this->UpdateMode();
    this->UpdateTabBar(FALSE);
    //wnoutrefresh(window_command);
    //update_panels();doupdate();
}

void CDisplayHandler::PostMessage(int msg) {
    this->PostMessage(msg, 0, 0);
}

void CDisplayHandler::PostMessage(int msg, int) {
    this->PostMessage(msg, 0, 0);
}

void CDisplayHandler::PostMessage(int msg, int extended, int function_key) {
    MSG_LIST *msg_temp, *msg_new;

    pthread_mutex_lock(&(this->message_lock));

    msg_temp = this->message_stack;
    msg_new = new(MSG_LIST);
    msg_new->next = NULL;

    if (msg_temp) {
        while (msg_temp->next)
            msg_temp = msg_temp->next;

        msg_temp->next = msg_new;
    } else
        this->message_stack = msg_new;

    msg_new->msg = msg;
    msg_new->extended = extended;
    msg_new->function_key = function_key;
    pthread_mutex_unlock(&(this->message_lock));
}

void CDisplayHandler::PostMessageFromServer(int msg, int magic, char *data) {
    SERVER_MSG_LIST *msg_temp, *msg_new;

    pthread_mutex_lock(&(this->server_message_lock));

    msg_temp = this->server_message_stack;
    msg_new = new(SERVER_MSG_LIST);
    msg_new->next = NULL;

    if (msg_temp) {
        while (msg_temp->next)
            msg_temp = msg_temp->next;

        msg_temp->next = msg_new;
    } else
        this->server_message_stack = msg_new;

    msg_new->msg = msg;
    msg_new->magic = magic;
    if (data) {
        msg_new->data = new(char[strlen(data) + 1]);
        strcpy(msg_new->data, data);
    } else
        msg_new->data = NULL;

    pthread_mutex_unlock(&(this->server_message_lock));
}

void CDisplayHandler::PostStatusLine(char *line, bool highlight) {
    STATUS_MSG_LIST *msg_temp, *msg_new;
    time_t timenow = time(NULL);

    struct tm *ptr;

    pthread_mutex_lock(&(this->status_lock));

    debuglog("POST STATUS LINE : %s", line);
    msg_temp = this->status_msg_stack;
    msg_new = new(STATUS_MSG_LIST);
    msg_new->next = NULL;
    msg_new->line = new(char[strlen(line) + 1 + 9]);
    ptr = (struct tm *) localtime(&timenow);
    strftime(msg_new->line, 10, "%T ", ptr);

    if (msg_temp) {
        while (msg_temp->next)
            msg_temp = msg_temp->next;

        msg_temp->next = msg_new;
    } else
        this->status_msg_stack = msg_new;

    strcpy(msg_new->line + 9, line);
    msg_new->highlight = highlight;
    pthread_mutex_unlock(&(this->status_lock));
}

void CDisplayHandler::FireupLocalFilesys(void) {
    // let the main task create thread for local filesys-window
    keyhandler->WantFireupLocal();

    while (!global_server)
        usleep(200000);// we need to wait until the server got registered!
}

int CDisplayHandler::FireupServer(char *label) {
    CServer *new_server;
    SERVERLIST *new_sv, *sv_temp = global_server;
    BOOKMARK *bm_temp = global_bookmark;
    bool found = FALSE;

    while (!found && bm_temp) {
        if (bm_temp->magic == this->siteopen_bm_realmagic)
            found = TRUE;
        else
            bm_temp = bm_temp->next;
    }
    // no check if valid !

    new_sv = new(SERVERLIST);
    new_sv->next = NULL;

    new_server = new(CServer);
    new_server->SetServerType(SERVER_TYPE_REMOTE);
    new_server->SetBMMagic(bm_temp->magic);
    new_server->SetServerPrefs(bm_temp);
    new_sv->server = new_server;
    strcpy(label, bm_temp->label);

    // local filesys exists atleast !
    while (sv_temp->next)
        sv_temp = sv_temp->next;

    sv_temp->next = new_sv;
    new_sv->magic = sv_temp->magic + 1;

    new_server->SetMagic(new_sv->magic);

    this->FillInfoForPrefs();

    // we create the new thread
    FireupRemoteServer(new_server);
    return (new_sv->magic);
}

void CDisplayHandler::PostToServer(int msg, bool mustbe_dir, int type) {
    SERVERLIST *sv_temp = global_server; // will just be modified by this thead
    FILELIST *filelist, *file_entry;
    int magic, entry_magic;
    bool found = FALSE;

    if (this->window_tab == this->window_left) {
        magic = this->leftwindow_magic;
        filelist = this->filelist_left;
        entry_magic = this->filelist_left_magic;
    } else {
        magic = this->rightwindow_magic;
        filelist = this->filelist_right;
        entry_magic = this->filelist_right_magic;
    }

    // see if it's a valid magic for an existing server thread
    while (!found && sv_temp) {
        if (sv_temp->magic == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found) {
        switch (type) {
            case MSG_TYPE_STD:
                file_entry = this->GetFilelistEntry(filelist, entry_magic);

                if (mustbe_dir) {
                    if (file_entry->is_dir)
                        sv_temp->server->PostFromDisplay(msg, file_entry->name);
                } else
                    sv_temp->server->PostFromDisplay(msg, file_entry->name);

                break;

            case MSG_TYPE_INPUT:
                sv_temp->server->PostFromDisplay(msg, this->input_temp);
                break;

            case MSG_TYPE_OLDINPUT:
                sv_temp->server->PostFromDisplay(msg, this->old_input_temp);
                break;
        }
    }
    // else drop the message (?)
}

void CDisplayHandler::CommandLineLogin(void) {
    BOOKMARK *bm_temp;
    //char t[512];
    char *site;
    int i = 0, sk = 0;

    if (sites2login) {
        site = strtok(sites2login, ",");
        while (site) {

            bm_temp = global_bookmark;
            while (bm_temp) {
                //snprintf(t, 512, "%s,", bm_temp->label);
                if (strcmp(site, bm_temp->label) == 0) {
                    this->siteopen_bm_realmagic = bm_temp->magic;

                    if (i % 2 == 1) {
                        this->FreeWindow(this->window_right);
                        this->filelist_right_format = rightformat;
                        this->rightwindow_magic =
                            this->FireupServer(this->window_right_label);

                        if (sk < 20) {
                            switch_list[sk].used = TRUE;
                            switch_list[sk].magic_left = this->leftwindow_magic;
                            switch_list[sk].magic_right = this->rightwindow_magic;
                            sk++;
                        }
                    } else {
                        this->FreeWindow(this->window_left);
                        this->filelist_left_format = leftformat;
                        this->leftwindow_magic =
                            this->FireupServer(this->window_left_label);
                    }
                    i++;
                }
                bm_temp = bm_temp->next;
            }
            site = strtok(NULL, ",");
        }
    }
    this->JumpSwitch(0);
    free(sites2login);
    sites2login = NULL;
}

void CDisplayHandler::RefreshSiteS(void) {
    SERVERLIST *sv_temp = global_server, *sv_src = NULL, *sv_dest = NULL;
    int dest_magic, src_magic;

    if (this->window_tab == this->window_left) {
        src_magic = this->leftwindow_magic;
        dest_magic = this->rightwindow_magic;
    } else {
        src_magic = this->rightwindow_magic;
        dest_magic = this->leftwindow_magic;
    }
    while ((!sv_src || !sv_dest) && sv_temp) {
        if (sv_temp->server->GetMagic() == src_magic)
            sv_src = sv_temp;
        else if (sv_temp->server->GetMagic() == dest_magic)
            sv_dest = sv_temp;

        sv_temp = sv_temp->next;
    }
    if (sv_src == NULL || sv_dest == NULL)
        return ;

    sv_dest->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH, "");
    sv_src->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH, "");
}

void CDisplayHandler::UdpLogin(void) {
    BOOKMARK *bm_temp;
    //char t[512];
    char *site;
    int i = 0, sk = 0, magic;

    if (sites2login) {
        site = strtok(sites2login, ",");
        while (site) {

            bm_temp = global_bookmark;
            while (bm_temp) {
                //snprintf(t, 512, "%s,", bm_temp->label);
                if (strcmp(site, bm_temp->label) == 0) {
                    this->siteopen_bm_realmagic = bm_temp->magic;
					//display->PostStatusLine("wassah wassah", TRUE);
					//set the loop to set afk per server here
                    if (i % 2 == 1) {
                        this->FreeWindow(this->window_right);
                        this->filelist_right_format = rightformat;
                        this->rightwindow_magic = this->FireupServer(this->window_right_label);
                        magic = this->rightwindow_magic;
                        if (sk < 20) {
							while ((switch_list[sk].used == TRUE) && (sk < 19)) {
								sk++;
							}
                            switch_list[sk].used = TRUE;
                            switch_list[sk].magic_left = this->leftwindow_magic;
                            switch_list[sk].magic_right = this->rightwindow_magic;
                            sk++;
                        }
                    } else {
                        this->FreeWindow(this->window_left);
                        this->filelist_left_format = leftformat;
                        this->leftwindow_magic = this->FireupServer(this->window_left_label);
                        magic = this->leftwindow_magic;
                    }
                    i++;
                   	SERVERLIST *sv_temp = global_server;
                    while (sv_temp && (sv_temp->server->GetMagic() != magic)) {
        				sv_temp = sv_temp->next;
				    }
				
				    if (sv_temp) {
				    	sv_temp->server->SetAfk(TRUE);
				    }
				    

                    
                }
                bm_temp = bm_temp->next;
            }
            site = strtok(NULL, ",");
        }
    }
    this->JumpSwitch(0);
    free(sites2login);
    sites2login = NULL;
	free(section);
	section = NULL;    
    this->DrawSwitchPairs();
}
