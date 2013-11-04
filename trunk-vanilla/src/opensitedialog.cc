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

void
 CDisplayHandler::OpenPrefsDialog(void)
{
    this->window_prefs =
        newwin(24, 61, this->terminal_max_y / 2 - 12,
               this->terminal_max_x / 2 - 30);
    this->panel_prefs = new_panel(this->window_prefs);
    leaveok(window_prefs, TRUE);

    wattrset(this->window_prefs, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_prefs, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_prefs);
//      wbkgdset(this->window_prefs, ' ');
    mywborder(this->window_prefs);
    mvwaddstr(this->window_prefs, 0, 2, "bookmark properties");

    this->prefsdialog_buttonstate = 0;
    this->UpdatePrefsItems();
}


void CDisplayHandler::WipeBookmark(void)
{
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
        delete[](bm_temp->exclude);
        delete[](bm_temp->util_dir);
        delete[](bm_temp->game_dir);
        delete[](bm_temp->site_who);
        delete[](bm_temp->site_user);
        delete[](bm_temp->site_wkup);
        delete[](bm_temp->noop_cmd);
        delete[](bm_temp->first_cmd);
        delete(bm_temp);

        this->siteopen_bm_magic = this->siteopen_bm_startmagic = 0;
        this->RedrawBookmarkSites();
    }
}


void CDisplayHandler::FillInfoForPrefs(void)
{
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
        strcpy(this->exclude, bm_temp->exclude);
        strcpy(this->util_dir, bm_temp->util_dir);
        strcpy(this->game_dir, bm_temp->game_dir);
        strcpy(this->site_who, bm_temp->site_who);
        strcpy(this->site_user, bm_temp->site_user);
        strcpy(this->site_wkup, bm_temp->site_wkup);
        strcpy(this->noop_cmd, bm_temp->noop_cmd);
        strcpy(this->first_cmd, bm_temp->first_cmd);
        this->refresh_rate = bm_temp->refresh_rate;
        this->noop_rate = bm_temp->noop_rate;
        this->sorting = bm_temp->sorting;
        this->retry_counter = bm_temp->retry_counter;
        this->retry_delay = bm_temp->retry_delay;
        this->use_refresh = bm_temp->use_refresh;
        this->use_noop = bm_temp->use_noop;
        this->use_jump = bm_temp->use_jump;
        this->use_track = bm_temp->use_track;
        this->use_startdir = bm_temp->use_startdir;
        this->use_exclude = bm_temp->use_exclude;
        this->use_autologin = bm_temp->use_autologin;
        this->use_chaining = bm_temp->use_chaining;
        this->use_utilgames = bm_temp->use_utilgames;
        this->use_pasv = bm_temp->use_pasv;
        this->use_ssl = bm_temp->use_ssl;
        this->use_ssl_list = bm_temp->use_ssl_list;
        this->use_ssl_data = bm_temp->use_ssl_data;
        this->use_stealth_mode = bm_temp->use_stealth_mode;
        this->use_retry = bm_temp->use_retry;
        this->use_firstcmd = bm_temp->use_firstcmd;
        this->use_rndrefr = bm_temp->use_rndrefr;
    }
}


void CDisplayHandler::PrefsAddSite(void)
{
    BOOKMARK *bm_new, *bm_temp1 = NULL, *bm_temp = global_bookmark;
    bool found = FALSE;

    bm_new = new BOOKMARK;
    bm_new->next = NULL;
    bm_magic_max++;
    bm_new->magic = bm_magic_max;

    bm_new->label = new char[strlen(this->alias) + 1];
    strcpy(bm_new->label, this->alias);
    bm_new->host = new char[strlen(this->hostname) + 1];
    strcpy(bm_new->host, this->hostname);
    bm_new->user = new char[strlen(this->username) + 1];
    strcpy(bm_new->user, this->username);
    bm_new->pass = new char[strlen(this->password) + 1];
    strcpy(bm_new->pass, this->password);
    bm_new->startdir = new char[strlen(this->startdir) + 1];
    strcpy(bm_new->startdir, this->startdir);
    bm_new->exclude = new char[strlen(this->exclude) + 1];
    strcpy(bm_new->exclude, this->exclude);
    bm_new->util_dir = new char[strlen(this->util_dir) + 1];
    strcpy(bm_new->util_dir, this->util_dir);
    bm_new->game_dir = new char[strlen(this->game_dir) + 1];
    strcpy(bm_new->game_dir, this->game_dir);
    bm_new->site_who = new char[strlen(this->site_who) + 1];
    strcpy(bm_new->site_who, this->site_who);
    bm_new->site_user = new char[strlen(this->site_user) + 1];
    strcpy(bm_new->site_user, this->site_user);
    bm_new->site_wkup = new char[strlen(this->site_wkup) + 1];
    strcpy(bm_new->site_wkup, this->site_wkup);
    bm_new->noop_cmd = new char[strlen(this->noop_cmd) + 1];
    strcpy(bm_new->noop_cmd, this->noop_cmd);
    bm_new->first_cmd = new char[strlen(this->first_cmd) + 1];
    strcpy(bm_new->first_cmd, this->first_cmd);

    bm_new->retry_delay = this->retry_delay;
    bm_new->retry_counter = this->retry_counter;
    bm_new->refresh_rate = this->refresh_rate;
    bm_new->noop_rate = this->noop_rate;
    bm_new->sorting = this->sorting;

    bm_new->use_refresh = this->use_refresh;
    bm_new->use_noop = this->use_noop;
    bm_new->use_startdir = this->use_startdir;
    bm_new->use_autologin = this->use_autologin;
    bm_new->use_chaining = this->use_chaining;
    bm_new->use_exclude = this->use_exclude;
    bm_new->use_jump = this->use_jump;
    bm_new->use_track = this->use_track;
    bm_new->use_utilgames = this->use_utilgames;
    bm_new->use_pasv = this->use_pasv;
    bm_new->use_ssl = this->use_ssl;
    bm_new->use_ssl_list = this->use_ssl_list;
    bm_new->use_ssl_data = this->use_ssl_data;
    bm_new->use_stealth_mode = this->use_stealth_mode;
    bm_new->use_retry = this->use_retry;
    bm_new->use_firstcmd = this->use_firstcmd;
    bm_new->use_rndrefr = this->use_rndrefr;

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


void CDisplayHandler::PrefsModifySite(void)
{
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
        bm_temp->label = new char[strlen(this->alias) + 1];
        strcpy(bm_temp->label, this->alias);
        delete[](bm_temp->host);
        bm_temp->host = new char[strlen(this->hostname) + 1];
        strcpy(bm_temp->host, this->hostname);
        delete[](bm_temp->user);
        bm_temp->user = new char[strlen(this->username) + 1];
        strcpy(bm_temp->user, this->username);
        delete[](bm_temp->pass);
        bm_temp->pass = new char[strlen(this->password) + 1];
        strcpy(bm_temp->pass, this->password);
        delete[](bm_temp->startdir);
        bm_temp->startdir = new char[strlen(this->startdir) + 1];
        strcpy(bm_temp->startdir, this->startdir);
        delete[](bm_temp->exclude);
        bm_temp->exclude = new char[strlen(this->exclude) + 1];
        strcpy(bm_temp->exclude, this->exclude);
        delete[](bm_temp->util_dir);
        bm_temp->util_dir = new char[strlen(this->util_dir) + 1];
        strcpy(bm_temp->util_dir, this->util_dir);
        delete[](bm_temp->game_dir);
        bm_temp->game_dir = new char[strlen(this->game_dir) + 1];
        strcpy(bm_temp->game_dir, this->game_dir);
        delete[](bm_temp->site_who);
        bm_temp->site_who = new char[strlen(this->site_who) + 1];
        strcpy(bm_temp->site_who, this->site_who);
        delete[](bm_temp->site_user);
        bm_temp->site_user = new char[strlen(this->site_user) + 1];
        strcpy(bm_temp->site_user, this->site_user);
        delete[](bm_temp->site_wkup);
        bm_temp->site_wkup = new char[strlen(this->site_wkup) + 1];
        strcpy(bm_temp->site_wkup, this->site_wkup);
        delete[](bm_temp->noop_cmd);
        bm_temp->noop_cmd = new char[strlen(this->noop_cmd) + 1];
        strcpy(bm_temp->noop_cmd, this->noop_cmd);
        delete[](bm_temp->first_cmd);
        bm_temp->first_cmd = new char[strlen(this->first_cmd) + 1];
        strcpy(bm_temp->first_cmd, this->first_cmd);

        bm_temp->retry_delay = this->retry_delay;
        bm_temp->retry_counter = this->retry_counter;
        bm_temp->refresh_rate = this->refresh_rate;
        bm_temp->noop_rate = this->noop_rate;
        bm_temp->sorting = this->sorting;

        bm_temp->use_refresh = this->use_refresh;
        bm_temp->use_noop = this->use_noop;
        bm_temp->use_startdir = this->use_startdir;
        bm_temp->use_autologin = this->use_autologin;
        bm_temp->use_chaining = this->use_chaining;
        bm_temp->use_exclude = this->use_exclude;
        bm_temp->use_jump = this->use_jump;
        bm_temp->use_track = this->use_track;
        bm_temp->use_utilgames = this->use_utilgames;
        bm_temp->use_pasv = this->use_pasv;
        bm_temp->use_ssl = this->use_ssl;
        bm_temp->use_ssl_list = this->use_ssl_list;
        bm_temp->use_ssl_data = this->use_ssl_data;
        bm_temp->use_stealth_mode = this->use_stealth_mode;
        bm_temp->use_retry = this->use_retry;
        bm_temp->use_firstcmd = this->use_firstcmd;
        bm_temp->use_rndrefr = this->use_rndrefr;

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


void CDisplayHandler::UpdatePrefsItems(void)
{
    int n, m, attr;
    char filler[] =
        { "                                                            " };

// remove old selection and redraw
    wattrset(this->window_prefs, COLOR_PAIR(STYLE_MARKED) | A_BOLD);
    mvwaddstr(this->window_prefs, 19, 2, "filelist sorting method: ");

    for (n = 0; n < 32; n++) {
        if (n == this->prefsdialog_buttonstate) {
            wattrset(this->window_prefs,
                     COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);
            attr =
                COLOR_PAIR(STYLE_MARKED_INVERSE) | A_BOLD | this->
                inverse_mono;
        } else {
            wattrset(this->window_prefs,
                     COLOR_PAIR(STYLE_MARKED) | A_BOLD);
            attr = COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono;
        }

        switch (n) {
        case 0:
            mvwaddstr(this->window_prefs, 1, 10, "alias");
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 1, 16, filler, 43);
            mvwaddnstr(this->window_prefs, 1, 16, this->alias, 43);
            break;

        case 1:
            mvwaddstr(this->window_prefs, 2, 3, "host/IP:port");
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 2, 16, filler, 43);
            mvwaddnstr(this->window_prefs, 2, 16, this->hostname, 43);
            break;

        case 2:
            mvwaddstr(this->window_prefs, 3, 7, "username");
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 3, 16, filler, 16);
            mvwaddnstr(this->window_prefs, 3, 16, this->username, 16);
            break;

        case 3:
            mvwaddstr(this->window_prefs, 3, 34, "password");
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 3, 43, filler, 16);
            m = 0;
            while (m < 43 && *(this->password + m)) {
                mvwaddch(this->window_prefs, 3, 43 + m, '*');
                m++;
            }

            break;

        case 4:
            mvwaddstr(this->window_prefs, 5, 2, "[ ] RETRY attempts");
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
            mvwaddstr(this->window_prefs, 6, 47, "[ ] SSL Data");
            if (this->use_ssl_data)
                mvwaddch(this->window_prefs, 6, 48, 'x');
            wattrset(this->window_prefs, attr);
            break;

        case 9:
            mvwaddstr(this->window_prefs, 7, 2, "[ ] use PASSIVE mode");
            if (this->use_pasv)
                mvwaddch(this->window_prefs, 7, 3, 'x');
            wattrset(this->window_prefs, attr);
            break;

        case 10:
            mvwaddstr(this->window_prefs, 7, 23, "[ ] use STEALTH mode");
            if (this->use_stealth_mode)
                mvwaddch(this->window_prefs, 7, 24, 'x');
            wattrset(this->window_prefs, attr);
            break;
        case 11:
            mvwaddstr(this->window_prefs, 8, 2, "[ ] after login do");
            if (this->use_firstcmd)
                mvwaddch(this->window_prefs, 8, 3, 'x');
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 8, 21, filler, 10);
            mvwaddnstr(this->window_prefs, 8, 21, this->first_cmd, 10);
            break;

        case 12:
            mvwaddstr(this->window_prefs, 8, 32, "[ ] use startdir");
            if (this->use_startdir)
                mvwaddch(this->window_prefs, 8, 33, 'x');
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 8, 49, filler, 10);
            mvwaddnstr(this->window_prefs, 8, 49, this->startdir, 10);
            break;

        case 13:
            mvwaddstr(this->window_prefs, 9, 2,
                      "[ ] exclude from copying/detecting");
            if (this->use_exclude)
                mvwaddch(this->window_prefs, 9, 3, 'x');
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 9, 37, filler, 22);
            mvwaddnstr(this->window_prefs, 9, 37, this->exclude, 22);
            break;

        case 14:
            mvwaddstr(this->window_prefs, 10, 2,
                      "[ ] autorefresh every (secs)");
            if (this->use_refresh)
                mvwaddch(this->window_prefs, 10, 3, 'x');
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 10, 31, filler, 3);
            sprintf(this->temp_string, "%d", this->refresh_rate);
            mvwaddnstr(this->window_prefs, 10, 31, this->temp_string, 3);
            break;

        case 15:
            mvwaddstr(this->window_prefs, 11, 2, "[ ] send");
            if (this->use_noop)
                mvwaddch(this->window_prefs, 11, 3, 'x');
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 11, 11, filler, 6);
            mvwaddnstr(this->window_prefs, 11, 11, this->noop_cmd, 6);
            break;

        case 16:
            mvwaddstr(this->window_prefs, 11, 18, "every (secs)");
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 11, 31, filler, 3);
            sprintf(this->temp_string, "%d", this->noop_rate);
            mvwaddnstr(this->window_prefs, 11, 31, this->temp_string, 3);
            break;

        case 17:
            mvwaddstr(this->window_prefs, 11, 35,
                      "[ ] RANDOM noop/refresh");
            if (this->use_rndrefr)
                mvwaddch(this->window_prefs, 11, 36, 'x');
            break;

        case 18:
            mvwaddstr(this->window_prefs, 12, 2,
                      "[ ] jump to newest dir/file after refresh");
            if (this->use_jump)
                mvwaddch(this->window_prefs, 12, 3, 'x');
            break;

        case 19:
            mvwaddstr(this->window_prefs, 13, 2,
                      "[ ] track new dirs/files in status window");
            if (this->use_track)
                mvwaddch(this->window_prefs, 13, 3, 'x');
            break;

        case 20:
            mvwaddstr(this->window_prefs, 14, 2,
                      "[ ] autologin on startup");
            if (this->use_autologin)
                mvwaddch(this->window_prefs, 14, 3, 'x');
            break;

        case 21:
            mvwaddstr(this->window_prefs, 15, 2,
                      "[ ] use chaining on this site (ftp-mode only)");
            if (this->use_chaining)
                mvwaddch(this->window_prefs, 15, 3, 'x');
            break;

        case 22:
            mvwaddstr(this->window_prefs, 17, 2,
                      "[ ] util/games site   util");
            mvwaddstr(this->window_prefs, 18, 24, "game");
            if (this->use_utilgames)
                mvwaddch(this->window_prefs, 17, 3, 'x');
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 17, 29, filler, 30);
            mvwaddnstr(this->window_prefs, 17, 29, this->util_dir, 30);
            break;

        case 23:
            mvwaddstr(this->window_prefs, 18, 24, "game");
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 18, 29, filler, 30);
            mvwaddnstr(this->window_prefs, 18, 29, this->game_dir, 30);
            break;

        case 24:
            mvwaddstr(this->window_prefs, 19, 27, "[ ] name");
            if (this->sorting == 1)
                mvwaddch(this->window_prefs, 19, 28, 'x');
            else
                mvwaddch(this->window_prefs, 19, 28, ' ');
            break;

        case 25:
            mvwaddstr(this->window_prefs, 19, 39, "[ ] size");
            if (this->sorting == 2)
                mvwaddch(this->window_prefs, 19, 40, 'x');
            else
                mvwaddch(this->window_prefs, 19, 40, ' ');
            break;

        case 26:
            mvwaddstr(this->window_prefs, 19, 51, "[ ] date");
            if (this->sorting == 3)
                mvwaddch(this->window_prefs, 19, 52, 'x');
            else
                mvwaddch(this->window_prefs, 19, 52, ' ');
            break;

        case 27:
            mvwaddstr(this->window_prefs, 21, 2, "SITE who");
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 21, 11, filler, 10);
            mvwaddnstr(this->window_prefs, 21, 11, this->site_who, 10);
            break;

        case 28:
            mvwaddstr(this->window_prefs, 21, 23, "user");
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 21, 28, filler, 10);
            mvwaddnstr(this->window_prefs, 21, 28, this->site_user, 10);
            break;

        case 29:
            mvwaddstr(this->window_prefs, 21, 40, "wkup");
            wattrset(this->window_prefs, attr);
            mvwaddnstr(this->window_prefs, 21, 45, filler, 10);
            mvwaddnstr(this->window_prefs, 21, 45, this->site_wkup, 10);
            break;

        case 30:
            mvwaddstr(this->window_prefs, 22, 10, "[   'o'kay   ]");
            break;

        case 31:
            mvwaddstr(this->window_prefs, 22, 39, "[  'c'ancel  ]");
            break;
        }
    }

//wnoutrefresh(window_prefs);
    update_panels();
    doupdate();
}


void CDisplayHandler::ClosePrefsDialog(void)
{
    del_panel(this->panel_prefs);
    delwin(this->window_prefs);
    this->window_prefs = NULL;
    this->panel_prefs = NULL;
    update_panels();
    doupdate();
//      this->window_prefs = NULL;
//        this->RebuildScreen();
/*        wclear(this->window_command);
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
doupdate(); */
}


void CDisplayHandler::OpenSiteDialog(void)
{
// init site dialog
    this->window_dialog =
        newwin(20, 61, this->terminal_max_y / 2 - 10,
               this->terminal_max_x / 2 - 30);
    this->panel_dialog = new_panel(this->window_dialog);
    leaveok(window_dialog, TRUE);
    wattrset(this->window_dialog, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_dialog, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_dialog);
    mywborder(this->window_dialog);
//      wbkgdset(this->window_dialog, ' ');

    mvwaddstr(this->window_dialog, 0, 2, "bookmark manager");

// the bookmark area
    this->siteopen_bm_magic = this->siteopen_bm_startmagic = 0;
    this->RedrawBookmarkSites();

// now for the buttons
//      if(!this->filelist_right)
//              this->siteopen_buttonstate = 1;
//      else {
    if (this->window_tab == this->window_left)
        this->siteopen_buttonstate = 1;
    else
        this->siteopen_buttonstate = 0;

//              this->siteopen_buttonstate = 0;
//      }

    this->UpdateSiteOpenButtons();
}


void CDisplayHandler::CloseSiteDialog(void)
{
    del_panel(this->panel_dialog);
    delwin(this->window_dialog);
    this->window_dialog = NULL;
    this->panel_dialog = NULL;
    update_panels();
    doupdate();
//        this->RebuildScreen();
/*  wnoutrefresh(this->window_command);
   wnoutrefresh(this->window_status);
   wnoutrefresh(this->window_left);
   wnoutrefresh(this->window_right);
   doupdate();
 */
}


void CDisplayHandler::ScrollBookmarkSites(bool dir_up)
{
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


void CDisplayHandler::PageMoveBookmarkSites(bool dir_up)
{
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


void CDisplayHandler::PageMoveBookmarkSitesEnd(bool dir_up)
{
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
//                      this->siteopen_bm_magic-= 7;
//                      if(this->siteopen_bm_magic < 0)
            this->siteopen_bm_magic = 0;

            if (this->siteopen_bm_startmagic > this->siteopen_bm_magic)
                this->siteopen_bm_startmagic = this->siteopen_bm_magic;

            action = TRUE;
        }
    } else {
        if (this->siteopen_bm_magic < (entries - 1)) {
// set magic and magic_start
//                      this->siteopen_bm_magic+= 7;
//                      if(this->siteopen_bm_magic > (entries - 1))
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


void CDisplayHandler::MoveBookmarkByChar(char chr)
{
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

// no segf check
    while ((n < magic) && bm_temp) {
        bm_temp = bm_temp->next;
        n++;
    }
    assert(bm_temp != NULL);
    if (tolower(bm_temp->label[0]) != chr) {
        n = 0;
        bm_temp = global_bookmark;
// no segf check
        while ((n < entries) && bm_temp) {
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
// no segf check
        while ((n < entries) && bm_temp) {
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
// no segf check
            while ((n < entries) && bm_temp) {
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


void CDisplayHandler::RedrawBookmarkSites(void)
{
    BOOKMARK *bm_temp = global_bookmark;
    int m = 0, n, ypos = 2;

// erase background
    wattrset(this->window_dialog,
             COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono);
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
                     COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
            mvwaddstr(this->window_dialog, ypos, 2,
                      "                                                         ");
            mvwaddnstr(this->window_dialog, ypos, 2, bm_temp->label, 57);
            wattrset(this->window_dialog,
                     COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono);
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


void CDisplayHandler::UpdateSiteOpenButtons(void)
{
    int n;

// remove old button selection and draw active one

    for (n = 0; n < 5; n++) {
        if (n == this->siteopen_buttonstate)
            wattrset(this->window_dialog,
                     COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);
        else
            wattrset(this->window_dialog,
                     COLOR_PAIR(STYLE_MARKED) | A_BOLD);

        switch (n) {
        case 0:
            mvwaddstr(this->window_dialog, 17, 2,
                      "[    use 'l'eft window    ]");
            break;

        case 1:
            mvwaddstr(this->window_dialog, 17, 32,
                      "[   use 'r'ight window    ]");
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
        }

    }

//wnoutrefresh(window_dialog);
    update_panels();
    doupdate();
}
