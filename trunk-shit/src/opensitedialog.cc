#include <assert.h>
#include <curses.h>
#include <ctype.h>
#include <pthread.h>
#include <panel.h>
#include <string.h>

#include "defines.h"
#include "tcp.h"
#include "server.h"
#include "displayhandler.h"

extern BOOKMARK *global_bookmark;
extern int bm_magic_max;
extern char *sectionlabels[NUM_SECTIONS];
extern char *prelabels[NUM_PRECOMMANDS];

void CDisplayHandler::OpenPrefsDialog(void) {
    this->window_prefs =
        newwin(26, 78, this->terminal_max_y / 2 - 15,
               this->terminal_max_x / 2 - 39);
    this->panel_prefs = new_panel(this->window_prefs);
    leaveok(window_prefs, TRUE);

    wattrset(this->window_prefs, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_prefs, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_prefs);
    //wbkgdset(this->window_prefs, ' ');
    mywborder(this->window_prefs);
    mvwaddstr(this->window_prefs, 0, 2, "[ bookmark properties ]");

    this->prefsdialog_buttonstate = 0;
    this->UpdatePrefsItems();
}

void CDisplayHandler::WipeBookmark(void) {
    BOOKMARK *bm_temp1 = NULL, *bm_temp = global_bookmark;
    bool found = FALSE;

    while (!found && bm_temp) {
        if (bm_temp->magic == this->siteopen_bm_realmagic)
            found = TRUE;
        else {
            bm_temp1 = bm_temp;
            bm_temp = bm_temp->next;
        }
    }

    if (found) {
        if (bm_temp1)
            bm_temp1->next = bm_temp->next;
        else
            global_bookmark = bm_temp->next;

        delete[](bm_temp->label);
        delete[](bm_temp->host);
        delete[](bm_temp->user);
        delete[](bm_temp->pass);
        delete[](bm_temp->startdir);
        delete[](bm_temp->exclude_source);
        delete[](bm_temp->exclude_target);
        for (int f = 0; f < NUM_SECTIONS; f++) delete[](bm_temp->section[f]);
        for (int f = 0; f < NUM_PRECOMMANDS; f++) delete[](bm_temp->pre_cmd[f]);
        delete[](bm_temp->site_rules);
        delete[](bm_temp->site_user);
        delete[](bm_temp->site_wkup);
        delete[](bm_temp->noop_cmd);
        delete[](bm_temp->first_cmd);
        delete[](bm_temp->nuke_prefix);
        delete[](bm_temp->msg_string);
        delete(bm_temp);

        this->siteopen_bm_magic = this->siteopen_bm_startmagic = 0;
        this->RedrawBookmarkSites();
    }
}

void CDisplayHandler::FillInfoForPrefs(void) {
    BOOKMARK *bm_temp = global_bookmark;
    bool found = FALSE;

    // find matching magic
    while (!found && bm_temp) {
        if (bm_temp->magic == this->siteopen_bm_realmagic)
            found = TRUE;
        else
            bm_temp = bm_temp->next;
    }

    // if not found... well, ignore (?)
    if (found) {
        strcpy(this->alias, bm_temp->label);
        strcpy(this->hostname, bm_temp->host);
        strcpy(this->password, bm_temp->pass);
        strcpy(this->username, bm_temp->user);
        strcpy(this->startdir, bm_temp->startdir);
        strcpy(this->exclude_source, bm_temp->exclude_source);
        strcpy(this->exclude_target, bm_temp->exclude_target);
	for (int f = 0; f < NUM_SECTIONS; f++) strcpy(this->sectionList[f], bm_temp->section[f]);
        for (int f = 0; f < NUM_PRECOMMANDS; f++) strcpy(this->pre_cmd[f], bm_temp->pre_cmd[f]);
        strcpy(this->site_rules, bm_temp->site_rules);
        strcpy(this->site_user, bm_temp->site_user);
        strcpy(this->site_wkup, bm_temp->site_wkup);
        strcpy(this->noop_cmd, bm_temp->noop_cmd);
        strcpy(this->first_cmd, bm_temp->first_cmd);
        strcpy(this->nuke_prefix, bm_temp->nuke_prefix);
        strcpy(this->msg_string, bm_temp->msg_string);
        this->refresh_rate = bm_temp->refresh_rate;
        this->noop_rate = bm_temp->noop_rate;
        this->retry_delay = bm_temp->retry_delay;
        this->file_sorting = bm_temp->file_sorting;
        this->dir_sorting = bm_temp->dir_sorting;
        this->retry_counter = bm_temp->retry_counter;
        this->use_refresh = bm_temp->use_refresh;
        this->use_noop = bm_temp->use_noop;
        this->use_jump = bm_temp->use_jump;
        this->use_track = bm_temp->use_track;
        this->use_startdir = bm_temp->use_startdir;
        this->use_exclude_source = bm_temp->use_exclude_source;
        this->use_exclude_target = bm_temp->use_exclude_target;
        this->use_autologin = bm_temp->use_autologin;
        this->use_chaining = bm_temp->use_chaining;
        this->use_sections = bm_temp->use_sections;
        this->use_pasv = bm_temp->use_pasv;
        this->use_ssl = bm_temp->use_ssl;
        this->use_ssl_list = bm_temp->use_ssl_list;
        this->use_ssl_data = bm_temp->use_ssl_data;
        this->use_stealth_mode = bm_temp->use_stealth_mode;
        this->use_retry = bm_temp->use_retry;
        this->use_firstcmd = bm_temp->use_firstcmd;
        this->use_rndrefr = bm_temp->use_rndrefr;
        this->use_ssl_fxp = bm_temp->use_ssl_fxp;
        this->use_alt_fxp = bm_temp->use_alt_fxp;
        this->use_rev_file = bm_temp->use_rev_file;
        this->use_rev_dir = bm_temp->use_rev_dir;
    }
}

void CDisplayHandler::PrefsAddSite(void) {
    BOOKMARK *bm_new, *bm_temp1 = NULL, *bm_temp = global_bookmark;
    bool found = FALSE;

    bm_new = new(BOOKMARK);
    bm_new->next = NULL;
    bm_magic_max++;
    bm_new->magic = bm_magic_max;

    bm_new->label = new(char[strlen(this->alias) + 1]);
    strcpy(bm_new->label, this->alias);
    bm_new->host = new(char[strlen(this->hostname) + 1]);
    strcpy(bm_new->host, this->hostname);
    bm_new->user = new(char[strlen(this->username) + 1]);
    strcpy(bm_new->user, this->username);
    bm_new->pass = new(char[strlen(this->password) + 1]);
    strcpy(bm_new->pass, this->password);
    bm_new->startdir = new(char[strlen(this->startdir) + 1]);
    strcpy(bm_new->startdir, this->startdir);
    bm_new->exclude_source = new(char[strlen(this->exclude_source) + 1]);
    strcpy(bm_new->exclude_source, this->exclude_source);
    bm_new->exclude_target = new(char[strlen(this->exclude_target) + 1]);
    strcpy(bm_new->exclude_target, this->exclude_target);
    for (int f = 0; f < NUM_SECTIONS; f++) {
      bm_new->section[f] = new(char[strlen(this->sectionList[f]) + 1]);
      strcpy(bm_new->section[f], this->sectionList[f]);
    }
    for (int f = 0; f < NUM_PRECOMMANDS; f++) {
      bm_new->pre_cmd[f] = new(char[strlen(this->pre_cmd[f]) + 1]);
      strcpy(bm_new->pre_cmd[f], this->pre_cmd[f]);
    }
    bm_new->site_rules = new(char[strlen(this->site_rules) + 1]);
    strcpy(bm_new->site_rules, this->site_rules);
    bm_new->site_user = new(char[strlen(this->site_user) + 1]);
    strcpy(bm_new->site_user, this->site_user);
    bm_new->site_wkup = new(char[strlen(this->site_wkup) + 1]);
    strcpy(bm_new->site_wkup, this->site_wkup);
    bm_new->noop_cmd = new(char[strlen(this->noop_cmd) + 1]);
    strcpy(bm_new->noop_cmd, this->noop_cmd);
    bm_new->first_cmd = new(char[strlen(this->first_cmd) + 1]);
    strcpy(bm_new->first_cmd, this->first_cmd);
    bm_new->nuke_prefix = new(char[strlen(this->nuke_prefix) + 1]);
    strcpy(bm_new->nuke_prefix, this->nuke_prefix);
    bm_new->msg_string = new(char[strlen(this->msg_string) + 1]);
    strcpy(bm_new->msg_string, this->msg_string);

    bm_new->retry_counter = this->retry_counter;
    bm_new->refresh_rate = this->refresh_rate;
    bm_new->noop_rate = this->noop_rate;
    bm_new->retry_delay = this->retry_delay;
    bm_new->file_sorting = this->file_sorting;
    bm_new->dir_sorting = this->dir_sorting;

    bm_new->use_refresh = this->use_refresh;
    bm_new->use_noop = this->use_noop;
    bm_new->use_startdir = this->use_startdir;
    bm_new->use_autologin = this->use_autologin;
    bm_new->use_chaining = this->use_chaining;
    bm_new->use_exclude_source = this->use_exclude_source;
    bm_new->use_exclude_target = this->use_exclude_target;
    bm_new->use_jump = this->use_jump;
    bm_new->use_track = this->use_track;
    bm_new->use_sections = this->use_sections;
    bm_new->use_pasv = this->use_pasv;
    bm_new->use_ssl = this->use_ssl;
    bm_new->use_ssl_list = this->use_ssl_list;
    bm_new->use_ssl_data = this->use_ssl_data;
    bm_new->use_stealth_mode = this->use_stealth_mode;
    bm_new->use_retry = this->use_retry;
    bm_new->use_firstcmd = this->use_firstcmd;
    bm_new->use_rndrefr = this->use_rndrefr;
    bm_new->use_ssl_fxp = this->use_ssl_fxp;
    bm_new->use_alt_fxp = this->use_alt_fxp;
    bm_new->use_rev_file = this->use_rev_file;
    bm_new->use_rev_dir = this->use_rev_dir;

    // insert bookmark at correct alpha position
    if (!bm_temp) {
        // first bm, just create new start
        global_bookmark = bm_new;
    } else {
        while (!found && bm_temp->next) {
            if (strcmp(bm_new->label, bm_temp->label) < 0) {
                // new entry should be inserted before this one
                if (bm_temp1 == NULL) {
                    // this one is the very first
                    global_bookmark = bm_new;
                    bm_new->next = bm_temp;
                } else {
                    // somewhere within the list
                    bm_temp1->next = bm_new;
                    bm_new->next = bm_temp;
                }
                found = TRUE;
            } else {
                bm_temp1 = bm_temp;
                bm_temp = bm_temp->next;
            }
        }

        if (!found) {
            // use last position
            bm_temp->next = bm_new;
        }
    }
}

void CDisplayHandler::PrefsModifySite(void) {
    BOOKMARK *bm_temp2, *bm_temp1, *bm_temp = global_bookmark;
    bool found = FALSE;

    // find entry with matching magic
    while (!found && bm_temp) {
        if (bm_temp->magic == this->siteopen_bm_realmagic)
            found = TRUE;
        else
            bm_temp = bm_temp->next;
    }

    // if not found.. uhmm... well, ignore (?)
    if (found) {
        delete[](bm_temp->label);
        bm_temp->label = new(char[strlen(this->alias) + 1]);
        strcpy(bm_temp->label, this->alias);
        delete[](bm_temp->host);
        bm_temp->host = new(char[strlen(this->hostname) + 1]);
        strcpy(bm_temp->host, this->hostname);
        delete[](bm_temp->user);
        bm_temp->user = new(char[strlen(this->username) + 1]);
        strcpy(bm_temp->user, this->username);
        delete[](bm_temp->pass);
        bm_temp->pass = new(char[strlen(this->password) + 1]);
        strcpy(bm_temp->pass, this->password);
        delete[](bm_temp->startdir);
        bm_temp->startdir = new(char[strlen(this->startdir) + 1]);
        strcpy(bm_temp->startdir, this->startdir);
        delete[](bm_temp->exclude_source);
        bm_temp->exclude_source = new(char[strlen(this->exclude_source) + 1]);
        strcpy(bm_temp->exclude_source, this->exclude_source);
        delete[](bm_temp->exclude_target);
        bm_temp->exclude_target = new(char[strlen(this->exclude_target) + 1]);
        strcpy(bm_temp->exclude_target, this->exclude_target);
	for (int f = 0; f < NUM_SECTIONS; f++) {
             delete[](bm_temp->section[f]);
             bm_temp->section[f] = new(char[strlen(this->sectionList[f]) + 1]);
             strcpy(bm_temp->section[f], this->sectionList[f]);
        }
	for (int f = 0; f < NUM_PRECOMMANDS; f++) {
             delete[](bm_temp->pre_cmd[f]);
             bm_temp->pre_cmd[f] = new(char[strlen(this->pre_cmd[f]) + 1]);
             strcpy(bm_temp->pre_cmd[f], this->pre_cmd[f]);
        }
        bm_temp->site_rules = new(char[strlen(this->site_rules) + 1]);
        strcpy(bm_temp->site_rules, this->site_rules);
        delete[](bm_temp->site_user);
        bm_temp->site_user = new(char[strlen(this->site_user) + 1]);
        strcpy(bm_temp->site_user, this->site_user);
        delete[](bm_temp->site_wkup);
        bm_temp->site_wkup = new(char[strlen(this->site_wkup) + 1]);
        strcpy(bm_temp->site_wkup, this->site_wkup);
        delete[](bm_temp->noop_cmd);
        bm_temp->noop_cmd = new(char[strlen(this->noop_cmd) + 1]);
        strcpy(bm_temp->noop_cmd, this->noop_cmd);
        delete[](bm_temp->first_cmd);
        bm_temp->first_cmd = new(char[strlen(this->first_cmd) + 1]);
        strcpy(bm_temp->first_cmd, this->first_cmd);
        delete[](bm_temp->nuke_prefix);
        bm_temp->nuke_prefix = new(char[strlen(this->nuke_prefix) + 1]);
        strcpy(bm_temp->nuke_prefix, this->nuke_prefix);
        delete[](bm_temp->msg_string);
        bm_temp->msg_string = new(char[strlen(this->msg_string) + 1]);
        strcpy(bm_temp->msg_string, this->msg_string);

        bm_temp->retry_counter = this->retry_counter;
        bm_temp->refresh_rate = this->refresh_rate;
        bm_temp->noop_rate = this->noop_rate;
        bm_temp->retry_delay = this->retry_delay;
        bm_temp->file_sorting = this->file_sorting;
        bm_temp->dir_sorting = this->dir_sorting;

        bm_temp->use_refresh = this->use_refresh;
        bm_temp->use_noop = this->use_noop;
        bm_temp->use_startdir = this->use_startdir;
        bm_temp->use_autologin = this->use_autologin;
        bm_temp->use_chaining = this->use_chaining;
        bm_temp->use_exclude_source = this->use_exclude_source;
        bm_temp->use_exclude_target = this->use_exclude_target;
        bm_temp->use_jump = this->use_jump;
        bm_temp->use_track = this->use_track;
        bm_temp->use_sections = this->use_sections;
        bm_temp->use_pasv = this->use_pasv;
        bm_temp->use_ssl = this->use_ssl;
        bm_temp->use_ssl_list = this->use_ssl_list;
        bm_temp->use_ssl_data = this->use_ssl_data;
        bm_temp->use_stealth_mode = this->use_stealth_mode;
        bm_temp->use_retry = this->use_retry;
        bm_temp->use_firstcmd = this->use_firstcmd;
        bm_temp->use_rndrefr = this->use_rndrefr;
        bm_temp->use_ssl_fxp = this->use_ssl_fxp;
        bm_temp->use_alt_fxp = this->use_alt_fxp;
        bm_temp->use_rev_file = this->use_rev_file;
        bm_temp->use_rev_dir = this->use_rev_dir;

        // re-sort whole list (in case label changed)
        bm_temp = global_bookmark;
        bm_temp1 = bm_temp2 = NULL;

        while (bm_temp) {
            if (bm_temp1) {
                if (strcmp(bm_temp->label, bm_temp1->label) < 0) {
                    // we need to exchange those entries
                    if (!bm_temp2) {
                        // reassign start of list
                        global_bookmark = bm_temp;
                        bm_temp1->next = bm_temp->next;
                        bm_temp->next = bm_temp1;
                    } else {
                        bm_temp2->next = bm_temp;
                        bm_temp1->next = bm_temp->next;
                        bm_temp->next = bm_temp1;
                    }

                    // and restart list
                    bm_temp2 = bm_temp1 = NULL;
                    bm_temp = global_bookmark;
                } else {
                    bm_temp2 = bm_temp1;
                    bm_temp1 = bm_temp;
                    bm_temp = bm_temp->next;
                }
            } else {
                bm_temp2 = bm_temp1;
                bm_temp1 = bm_temp;
                bm_temp = bm_temp->next;
            }
        }
    }
}

void CDisplayHandler::UpdatePrefsItems(void) {
    int n, m, attr;
    char filler[] =
        { "                                                            " };

    // remove old selection and redraw
    wattrset(this->window_prefs, COLOR_PAIR(STYLE_MARKED) | A_BOLD);
    mvwaddstr(this->window_prefs, 19, 2, "filelist sorting method: ");
    mvwaddstr(this->window_prefs, 20, 2, "dirlist sorting method: ");
    mvwaddstr(this->window_prefs, 22, 2, "site pre commands:");

    mvwaddch(this->window_prefs, 13, 44, '-');
    mvwaddch(this->window_prefs, 14, 44, '-');

    mvwaddch(this->window_prefs, 16, 31, ',');
    mvwaddch(this->window_prefs, 16, 39, ',');
    mvwaddch(this->window_prefs, 16, 47, ',');
    mvwaddch(this->window_prefs, 16, 55, ',');
    mvwaddch(this->window_prefs, 16, 63, ',');
    mvwaddch(this->window_prefs, 17, 15, ',');
    mvwaddch(this->window_prefs, 17, 23, ',');
    mvwaddch(this->window_prefs, 17, 31, ',');
    mvwaddch(this->window_prefs, 17, 39, ',');
    mvwaddch(this->window_prefs, 17, 47, ',');
    mvwaddch(this->window_prefs, 17, 55, ',');
    mvwaddch(this->window_prefs, 17, 63, ',');
    mvwaddch(this->window_prefs, 18, 15, ',');
    mvwaddch(this->window_prefs, 18, 23, ',');
    mvwaddch(this->window_prefs, 18, 31, ',');
    mvwaddch(this->window_prefs, 18, 39, ',');
    mvwaddch(this->window_prefs, 18, 47, ',');
    mvwaddch(this->window_prefs, 18, 55, ',');
    mvwaddch(this->window_prefs, 18, 63, ',');

    for (n = 0; n < 74; n++) {
        if (n == this->prefsdialog_buttonstate) {
            wattrset(this->window_prefs,
                     COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);
            attr =
                COLOR_PAIR(STYLE_MARKED_INVERSE) | A_BOLD | this->inverse_mono;
        } else {
            wattrset(this->window_prefs,
                     COLOR_PAIR(STYLE_MARKED) | A_BOLD);
            attr = COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono;
        }

        switch (n) {
            case 0:
                mvwaddstr(this->window_prefs, 2, 6, "site alias");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 2, 17, filler, 59);
                mvwaddnstr(this->window_prefs, 2, 17, this->alias, 59);
                break;

            case 1:
                mvwaddstr(this->window_prefs, 3, 4, "host/IP:port");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 3, 17, filler, 59);
                mvwaddnstr(this->window_prefs, 3, 17, this->hostname, 59);
                break;

            case 2:
                mvwaddstr(this->window_prefs, 4, 2, "login username");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 4, 17, filler, 21);
                mvwaddnstr(this->window_prefs, 4, 17, this->username, 21);
                break;

            case 3:
                mvwaddstr(this->window_prefs, 4, 40, "login password");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 4, 55, filler, 21);
                m = 0;
                while (m < 55 && *(this->password + m)) {
                    mvwaddch(this->window_prefs, 4, 55 + m, '*');
                    m++;
                }
                break;

            case 4:
                mvwaddstr(this->window_prefs, 5, 2, "[ ] RETRY to login upto");
                if (this->use_retry)
                    mvwaddch(this->window_prefs, 5, 3, 'x');
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 5, 26, filler, 3);
                sprintf(this->temp_string, "%d", this->retry_counter);
                mvwaddnstr(this->window_prefs, 5, 26, this->temp_string, 3);
                break;

            case 5:
                mvwaddstr(this->window_prefs, 5, 30, "times every (secs)");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 5, 49, filler, 3);
                sprintf(this->temp_string, "%d", this->retry_delay);
                mvwaddnstr(this->window_prefs, 5, 49, this->temp_string, 3);
                break;

            case 6:
                mvwaddstr(this->window_prefs, 6, 2, "[ ] SSL Auth");
                if (this->use_ssl)
                    mvwaddch(this->window_prefs, 6, 3, 'x');
                wattrset(this->window_prefs, attr);
                break;

            case 7:
                mvwaddstr(this->window_prefs, 6, 23, "[ ] SSL DirList");
                if (this->use_ssl_list)
                    mvwaddch(this->window_prefs, 6, 24, 'x');
                wattrset(this->window_prefs, attr);
                break;

            case 8:
                mvwaddstr(this->window_prefs, 6, 44, "[ ] SSL Data");
                if (this->use_ssl_data)
                    mvwaddch(this->window_prefs, 6, 45, 'x');
                wattrset(this->window_prefs, attr);
                break;

            case 9:
                mvwaddstr(this->window_prefs, 6, 65, "[ ] SSL FXP");
                if (this->use_ssl_fxp)
                    mvwaddch(this->window_prefs, 6, 66, 'x');
                wattrset(this->window_prefs, attr);
                break;

            case 10:
                mvwaddstr(this->window_prefs, 7, 2, "[ ] use PASSIVE mode");
                if (this->use_pasv)
                    mvwaddch(this->window_prefs, 7, 3, 'x');
                wattrset(this->window_prefs, attr);
                break;

            case 11:
                mvwaddstr(this->window_prefs, 7, 23, "[ ] Alternate FXP");
                if (this->use_alt_fxp)
                    mvwaddch(this->window_prefs, 7, 24, 'x');
                wattrset(this->window_prefs, attr);
                break;

            case 12:
                mvwaddstr(this->window_prefs, 7, 44, "[ ] use STEALTH mode");
                if (this->use_stealth_mode)
                    mvwaddch(this->window_prefs, 7, 45, 'x');
                wattrset(this->window_prefs, attr);
                break;

            case 13:
                mvwaddstr(this->window_prefs, 8, 2, "[ ] after login do");
                if (this->use_firstcmd)
                    mvwaddch(this->window_prefs, 8, 3, 'x');
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 8, 21, filler, 18);
                mvwaddnstr(this->window_prefs, 8, 21, this->first_cmd, 18);
                break;

            case 14:
                mvwaddstr(this->window_prefs, 8, 41, "[ ] use STARTDIR");
                if (this->use_startdir)
                    mvwaddch(this->window_prefs, 8, 42, 'x');
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 8, 58, filler, 18);
                mvwaddnstr(this->window_prefs, 8, 58, this->startdir, 18);
                break;

            case 15:
                mvwaddstr(this->window_prefs, 9, 2, "[ ] exclude from copying/detecting");
                if (this->use_exclude_source)
                    mvwaddch(this->window_prefs, 9, 3, 'x');
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 9, 37, filler, 39);
                mvwaddnstr(this->window_prefs, 9, 37, this->exclude_source, 39);
                break;

            case 16:
                mvwaddstr(this->window_prefs, 10, 2, "[ ] exclude in chaining");
                if (this->use_exclude_target)
                    mvwaddch(this->window_prefs, 10, 3, 'x');
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 10, 37, filler, 39);
                mvwaddnstr(this->window_prefs, 10, 37, this->exclude_target, 39);
                break;

            case 17:
                mvwaddstr(this->window_prefs, 11, 2, "[ ] AUTOREFRESH every (secs)");
                if (this->use_refresh)
                    mvwaddch(this->window_prefs, 11, 3, 'x');
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 11, 31, filler, 3);
                sprintf(this->temp_string, "%d", this->refresh_rate);
                mvwaddnstr(this->window_prefs, 11, 31, this->temp_string, 3);
                break;

            case 18:
                mvwaddstr(this->window_prefs, 12, 2, "[ ] send");
                if (this->use_noop)
                    mvwaddch(this->window_prefs, 12, 3, 'x');
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 12, 11, filler, 6);
                mvwaddnstr(this->window_prefs, 12, 11, this->noop_cmd, 6);
                break;

            case 19:
                mvwaddstr(this->window_prefs, 12, 18, "every (secs)");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 12, 31, filler, 3);
                sprintf(this->temp_string, "%d", this->noop_rate);
                mvwaddnstr(this->window_prefs, 12, 31, this->temp_string, 3);
                break;

            case 20:
                mvwaddstr(this->window_prefs, 12, 36, "[ ] RANDOM noop/refresh");
                if (this->use_rndrefr)
                    mvwaddch(this->window_prefs, 12, 37, 'x');
                break;

            case 21:
                mvwaddstr(this->window_prefs, 13, 2, "[ ] JUMP to newest dir/file after refresh");
                if (this->use_jump)
                    mvwaddch(this->window_prefs, 13, 3, 'x');
                break;

            case 22:
                mvwaddstr(this->window_prefs, 13, 46, "NUKED dir prefix");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 13, 63, filler, 13);
                mvwaddnstr(this->window_prefs, 13, 63, this->nuke_prefix, 13);
                break;

            case 23:
                mvwaddstr(this->window_prefs, 14, 2, "[ ] track new dirs/files in status window");
                if (this->use_track)
                    mvwaddch(this->window_prefs, 14, 3, 'x');
                break;

            case 24:
                mvwaddstr(this->window_prefs, 14, 46, "site MSG string");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 14, 63, filler, 13);
                mvwaddnstr(this->window_prefs, 14, 63, this->msg_string, 13);
                break;

            case 25:
                mvwaddstr(this->window_prefs, 15, 2, "[ ] AUTOLOGIN on startup");
                if (this->use_autologin)
                    mvwaddch(this->window_prefs, 15, 3, 'x');
                break;

            case 26:
                mvwaddstr(this->window_prefs, 15, 28, "[ ] use CHAINING (ftp-mode only)");
                if (this->use_chaining)
                    mvwaddch(this->window_prefs, 15, 29, 'x');
                break;

            case 27:
                mvwaddstr(this->window_prefs, 16, 2, "[ ] use site SECTIONS: ");
                mvwaddnstr(this->window_prefs, 16, 25, sectionlabels[0], 6);
                if (this->use_sections)
                    mvwaddch(this->window_prefs, 16, 3, 'x');
                break;

            case 28:
                mvwaddnstr(this->window_prefs, 16, 33, sectionlabels[1], 6);
                break;

            case 29:
                mvwaddnstr(this->window_prefs, 16, 41, sectionlabels[2], 6);
                break;

            case 30:
                mvwaddnstr(this->window_prefs, 16, 49, sectionlabels[3], 6);
                break;

            case 31:
                mvwaddnstr(this->window_prefs, 16, 57, sectionlabels[4], 6);
                break;

            case 32:
                mvwaddnstr(this->window_prefs, 16, 65, sectionlabels[5], 6);
                break;

            case 33:
                mvwaddnstr(this->window_prefs, 17, 9, sectionlabels[6], 6);
                break;

            case 34:
                mvwaddnstr(this->window_prefs, 17, 17, sectionlabels[7], 6);
                break;

            case 35:
                mvwaddnstr(this->window_prefs, 17, 25, sectionlabels[8], 6);
                break;

            case 36:
                mvwaddnstr(this->window_prefs, 17, 33, sectionlabels[9], 6);
                break;

            case 37:
                mvwaddnstr(this->window_prefs, 17, 41, sectionlabels[10], 6);
                break;

            case 38:
                mvwaddnstr(this->window_prefs, 17, 49, sectionlabels[11], 6);
                break;

            case 39:
                mvwaddnstr(this->window_prefs, 17, 57, sectionlabels[12], 6);
                break;

            case 40:
                mvwaddnstr(this->window_prefs, 17, 65, sectionlabels[13], 6);
                break;

            case 41:
                mvwaddnstr(this->window_prefs, 18, 9, sectionlabels[14], 6);
                break;

            case 42:
                mvwaddnstr(this->window_prefs, 18, 17, sectionlabels[15], 6);
                break;

            case 43:
                mvwaddnstr(this->window_prefs, 18, 25, sectionlabels[16], 6);
                break;

            case 44:
                mvwaddnstr(this->window_prefs, 18, 33, sectionlabels[17], 6);
                break;

            case 45:
                mvwaddnstr(this->window_prefs, 18, 41, sectionlabels[18], 6);
                break;

            case 46:
                mvwaddnstr(this->window_prefs, 18, 49, sectionlabels[19], 6);
                break;

            case 47:
                mvwaddnstr(this->window_prefs, 18, 57, sectionlabels[20], 6);
                break;

            case 48:
                mvwaddnstr(this->window_prefs, 18, 65, sectionlabels[21], 6);
                break;

            case 49:
                mvwaddstr(this->window_prefs, 19, 27, "[ ] name");
                if (this->file_sorting == 1)
                    mvwaddch(this->window_prefs, 19, 28, 'x');
                else
                    mvwaddch(this->window_prefs, 19, 28, ' ');
                break;

            case 50:
                mvwaddstr(this->window_prefs, 19, 39, "[ ] size");
                if (this->file_sorting == 2)
                    mvwaddch(this->window_prefs, 19, 40, 'x');
                else
                    mvwaddch(this->window_prefs, 19, 40, ' ');
                break;

            case 51:
                mvwaddstr(this->window_prefs, 19, 51, "[ ] date");
                if (this->file_sorting == 3)
                    mvwaddch(this->window_prefs, 19, 52, 'x');
                else
                    mvwaddch(this->window_prefs, 19, 52, ' ');
                break;

            case 52:
                mvwaddstr(this->window_prefs, 19, 63, "[ ] REVERSED");
                if (this->use_rev_file)
                    mvwaddch(this->window_prefs, 19, 64, 'x');
                break;

            case 53:
                mvwaddstr(this->window_prefs, 20, 27, "[ ] name");
                if (this->dir_sorting == 1)
                    mvwaddch(this->window_prefs, 20, 28, 'x');
                else
                    mvwaddch(this->window_prefs, 20, 28, ' ');
                break;

            case 54:
                mvwaddstr(this->window_prefs, 20, 39, "[ ] size");
                if (this->dir_sorting == 2)
                    mvwaddch(this->window_prefs, 20, 40, 'x');
                else
                    mvwaddch(this->window_prefs, 20, 40, ' ');
                break;

            case 55:
                mvwaddstr(this->window_prefs, 20, 51, "[ ] date");
                if (this->dir_sorting == 3)
                    mvwaddch(this->window_prefs, 20, 52, 'x');
                else
                    mvwaddch(this->window_prefs, 20, 52, ' ');
                break;

            case 56:
                mvwaddstr(this->window_prefs, 20, 63, "[ ] REVERSED");
                if (this->use_rev_dir)
                    mvwaddch(this->window_prefs, 20, 64, 'x');
                break;

            case 57:
                mvwaddstr(this->window_prefs, 21, 2, "SITE rules");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 21, 13, filler, 10);
                mvwaddnstr(this->window_prefs, 21, 13, this->site_rules, 10);
                break;

            case 58:
                mvwaddstr(this->window_prefs, 21, 24, "user");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 21, 30, filler, 10);
                mvwaddnstr(this->window_prefs, 21, 30, this->site_user, 10);
                break;

            case 59:
                mvwaddstr(this->window_prefs, 21, 41, "wkup");
                wattrset(this->window_prefs, attr);
                mvwaddnstr(this->window_prefs, 21, 48, filler, 10);
                mvwaddnstr(this->window_prefs, 21, 48, this->site_wkup, 10);
                break;
            case 60:
		mvwaddnstr(this->window_prefs, 22, 25, prelabels[0], 6);
                break;
            case 61:
		mvwaddnstr(this->window_prefs, 22, 33, prelabels[1], 6);
                break;
            case 62:
		mvwaddnstr(this->window_prefs, 22, 41, prelabels[2], 6);
                break;
            case 63:
		mvwaddnstr(this->window_prefs, 22, 49, prelabels[3], 6);
                break;
            case 64:
		mvwaddnstr(this->window_prefs, 22, 57, prelabels[4], 6);
                break;
            case 65:
		mvwaddnstr(this->window_prefs, 22, 65, prelabels[5], 6);
                break;
            case 66:
		mvwaddnstr(this->window_prefs, 23, 9, prelabels[6], 6);
                break;
            case 67:
		mvwaddnstr(this->window_prefs, 23, 17, prelabels[7], 6);
                break;
            case 68:
		mvwaddnstr(this->window_prefs, 23, 25, prelabels[8], 6);
                break;
            case 69:
		mvwaddnstr(this->window_prefs, 23, 33, prelabels[9], 6);
                break;
            case 70:
		mvwaddnstr(this->window_prefs, 23, 41, prelabels[10], 6);
                break;
            case 71:
		mvwaddnstr(this->window_prefs, 23, 49, prelabels[11], 6);
                break;
            case 72:
                mvwaddstr(this->window_prefs, 24, 17, "[   'o'kay   ]");
                break;

            case 73:
                mvwaddstr(this->window_prefs, 24, 47, "[  'c'ancel  ]");
                break;
        }
    }

    //wnoutrefresh(window_prefs);
    update_panels();
    doupdate();
}

void CDisplayHandler::ClosePrefsDialog(void) {
    del_panel(this->panel_prefs);
    delwin(this->window_prefs);
    this->window_prefs = NULL;
    this->panel_prefs = NULL;
    update_panels();
    doupdate();

    //this->window_prefs = NULL;
    //this->RebuildScreen();
    /*
    wclear(this->window_command);
    wrefresh(this->window_command);
    wclear(this->window_status);
    wrefresh(this->window_status);
    wclear(this->window_left);
    wrefresh(this->window_left);
    wclear(this->window_right);
    wrefresh(this->window_right);
    if(this->window_dialog) {
        wclear(this->window_dialog);
        wnoutrefresh(this->window_dialog);
    }
    doupdate();
    */
}

void CDisplayHandler::OpenSiteDialog(void) {
    // init site dialog
    this->window_dialog =
        newwin(20, 61, this->terminal_max_y / 2 - 10,
               this->terminal_max_x / 2 - 30);
    this->panel_dialog = new_panel(this->window_dialog);
    leaveok(window_dialog, TRUE);

    wattrset(this->window_dialog, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_dialog, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_dialog);
    //wbkgdset(this->window_dialog, ' ');
    mywborder(this->window_dialog);
    mvwaddstr(this->window_dialog, 0, 2, "[ bookmark manager ]");
//    mvwaddstr(this->window_dialog, 0, 34, "ESC-<letter> = quick seek");

    // the bookmark area
    this->siteopen_bm_magic = this->siteopen_bm_startmagic = 0;
    this->RedrawBookmarkSites();

    // now for the buttons
    //if(!this->filelist_right)
    //    this->siteopen_buttonstate = 1;
    //else {

    if (this->window_tab == this->window_left)
        this->siteopen_buttonstate = 1;
    else
        this->siteopen_buttonstate = 0;

    //    this->siteopen_buttonstate = 0;
    //}

    this->UpdateSiteOpenButtons();
}

void CDisplayHandler::CloseSiteDialog(void) {
    del_panel(this->panel_dialog);
    delwin(this->window_dialog);
    this->window_dialog = NULL;
    this->panel_dialog = NULL;
    update_panels();
    doupdate();

    //this->RebuildScreen();
    /*
    wnoutrefresh(this->window_command);
    wnoutrefresh(this->window_status);
    wnoutrefresh(this->window_left);
    wnoutrefresh(this->window_right);
    doupdate();
    */
}

void CDisplayHandler::ScrollBookmarkSites(bool dir_up) {
    BOOKMARK *bm_temp = global_bookmark;
    int entries = 0;
    bool action = FALSE;

    // determine number of entries
    while (bm_temp) {
        entries++;
        bm_temp = bm_temp->next;
    }

    // lets see if we can scroll up/down
    if (dir_up) {
        if (this->siteopen_bm_magic > 0) {
            // set magic and magic_start
            this->siteopen_bm_magic--;
            if (this->siteopen_bm_startmagic > this->siteopen_bm_magic)
                this->siteopen_bm_startmagic = this->siteopen_bm_magic;

            action = TRUE;
        }
    } else {
        if (this->siteopen_bm_magic < (entries - 1)) {
            // set magic and magic_start
            this->siteopen_bm_magic++;
            if ((this->siteopen_bm_startmagic + 13) <
                this->siteopen_bm_magic)
                this->siteopen_bm_startmagic =
                    this->siteopen_bm_magic - 13;

            action = TRUE;
        }
    }

    if (action)
        this->RedrawBookmarkSites();
}

void CDisplayHandler::PageMoveBookmarkSites(bool dir_up) {
    BOOKMARK *bm_temp = global_bookmark;
    int entries = 0;
    bool action = FALSE;

    // determine number of entries
    while (bm_temp) {
        entries++;
        bm_temp = bm_temp->next;
    }

    // lets see if we can scroll up/down
    if (dir_up) {
        if (this->siteopen_bm_magic > 0) {
            // set magic and magic_start
            this->siteopen_bm_magic -= 7;
            if (this->siteopen_bm_magic < 0)
                this->siteopen_bm_magic = 0;

            if (this->siteopen_bm_startmagic > this->siteopen_bm_magic)
                this->siteopen_bm_startmagic = this->siteopen_bm_magic;

            action = TRUE;
        }
    } else {
        if (this->siteopen_bm_magic < (entries - 1)) {
            // set magic and magic_start
            this->siteopen_bm_magic += 7;
            if (this->siteopen_bm_magic > (entries - 1))
                this->siteopen_bm_magic = entries - 1;

            if ((this->siteopen_bm_startmagic + 13) <
                this->siteopen_bm_magic)
                this->siteopen_bm_startmagic =
                    this->siteopen_bm_magic - 13;

            action = TRUE;
        }
    }

    if (action)
        this->RedrawBookmarkSites();
}

void CDisplayHandler::PageMoveBookmarkSitesEnd(bool dir_up) {
    BOOKMARK *bm_temp = global_bookmark;
    int entries = 0;
    bool action = FALSE;

    // determine number of entries
    while (bm_temp) {
        entries++;
        bm_temp = bm_temp->next;
    }

    // lets see if we can scroll up/down
    if (!dir_up) {
        if (this->siteopen_bm_magic > 0) {
            // set magic and magic_start
            //this->siteopen_bm_magic-= 7;
            //if(this->siteopen_bm_magic < 0)
            this->siteopen_bm_magic = 0;
            if (this->siteopen_bm_startmagic > this->siteopen_bm_magic)
                this->siteopen_bm_startmagic = this->siteopen_bm_magic;

            action = TRUE;
        }
    } else {
        if (this->siteopen_bm_magic < (entries - 1)) {
            // set magic and magic_start
            //this->siteopen_bm_magic+= 7;
            //if(this->siteopen_bm_magic > (entries - 1))
            this->siteopen_bm_magic = entries - 1;
            if ((this->siteopen_bm_startmagic + 13) <
                this->siteopen_bm_magic)
                this->siteopen_bm_startmagic =
                    this->siteopen_bm_magic - 13;

            action = TRUE;
        }
    }

    if (action)
        this->RedrawBookmarkSites();
}

void CDisplayHandler::MoveBookmarkByChar(char chr) {
    BOOKMARK *bm_temp = global_bookmark;
    int entries = 0, magic, n = 0;
    bool action = FALSE;

    // determine number of entries
    while (bm_temp) {
        entries++;
        bm_temp = bm_temp->next;
    }
    bm_temp = global_bookmark;
    magic = this->siteopen_bm_magic;

    while ((n < magic) && bm_temp) { // no segf check
        bm_temp = bm_temp->next;
        n++;
    }
    assert(bm_temp != NULL);
    if (tolower(bm_temp->label[0]) != chr) {
        n = 0;
        bm_temp = global_bookmark;
        while ((n < entries) && bm_temp) { // no segf check
            assert(bm_temp != NULL);
            if (tolower(bm_temp->label[0]) < chr) {
                action = TRUE;
                magic = n;
            } else if (tolower(bm_temp->label[0]) == chr) {
                action = TRUE;
                magic = n;
                break;
            }
            bm_temp = bm_temp->next;
            n++;
        }
    } else {
        bm_temp = bm_temp->next;
        n++;
        while ((n < entries) && bm_temp) { // no segf check
            assert(bm_temp != NULL);
            if (tolower(bm_temp->label[0]) == chr) {
                action = TRUE;
                magic = n;
                break;
            }
            bm_temp = bm_temp->next;
            n++;
        }
        if (action == FALSE) {
            n = 0;
            bm_temp = global_bookmark;
            while ((n < entries) && bm_temp) { // no segf check
                assert(bm_temp != NULL);
                if (tolower(bm_temp->label[0]) == chr) {
                    action = TRUE;
                    magic = n;
                    break;
                }
                bm_temp = bm_temp->next;
                n++;
            }
        }
    }

    if (action) {
        this->siteopen_bm_magic = magic;
        if ((this->siteopen_bm_startmagic) > this->siteopen_bm_magic)
            this->siteopen_bm_startmagic = this->siteopen_bm_magic;
        else if ((this->siteopen_bm_startmagic + 13) <
                 this->siteopen_bm_magic)
            this->siteopen_bm_startmagic = this->siteopen_bm_magic - 13;

        this->RedrawBookmarkSites();
    }
}

void CDisplayHandler::RedrawBookmarkSites(void) {
    BOOKMARK *bm_temp = global_bookmark;
    int m = 0, n, ypos = 2;

    // erase background
    wattrset(this->window_dialog,
//             COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono);
             COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);

    for (n = 0; n < 14; n++)
        mvwaddstr(this->window_dialog, n + 2, 2,
                  "                                                         ");
    // walk to starting magic
    n = 0;
    while (bm_temp && (this->siteopen_bm_startmagic != n)) {
        n++;
        bm_temp = bm_temp->next;
    }

    // display them. we don't take care of the actual length of the list. helpers will set start_magic correct
    m = n;
    n = 1;
    while (bm_temp && (n <= 14)) {
        if (this->siteopen_bm_magic == m) {
            wattrset(this->window_dialog,
//                     COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
                     COLOR_PAIR(STYLE_INVERSE) | A_NORMAL);
            mvwaddstr(this->window_dialog, ypos, 2,
                      "                                                         ");
            mvwaddnstr(this->window_dialog, ypos, 2, bm_temp->label, 57);
            wattrset(this->window_dialog,
//                     COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono);
                     COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);
            this->siteopen_bm_realmagic = bm_temp->magic;
        } else
            mvwaddnstr(this->window_dialog, ypos, 2, bm_temp->label, 57);

        ypos++;
        n++;
        m++;
        bm_temp = bm_temp->next;
    }

    //wnoutrefresh(this->window_dialog);
    update_panels();
    doupdate();
}

void CDisplayHandler::UpdateSiteOpenButtons(void) {
    int n;

    // remove old button selection and draw active one
    for (n = 0; n < 6; n++) {
        if (n == this->siteopen_buttonstate)
            wattrset(this->window_dialog,
                     COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);
        else
            wattrset(this->window_dialog, COLOR_PAIR(STYLE_MARKED) | A_BOLD);

        switch (n) {
            case 0:
                mvwaddstr(this->window_dialog, 17, 2, "[    use l'e'ft window    ]");
                break;

            case 1:
                mvwaddstr(this->window_dialog, 17, 32, "[   use 'r'ight window    ]");
                break;

            case 2:
                mvwaddstr(this->window_dialog, 18, 2, "[  'a'dd   ]");
                break;

            case 3:
                mvwaddstr(this->window_dialog, 18, 17, "[ 'd'elete ]");
                break;

            case 4:
                mvwaddstr(this->window_dialog, 18, 32, "[ 'm'odify ]");
                break;

            case 5:
                mvwaddstr(this->window_dialog, 18, 45, "[ 'p'assword ]");
                break;
        }
    }
    //wnoutrefresh(window_dialog);
    update_panels();
    doupdate();
}

