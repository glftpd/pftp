#include <assert.h>
#include <string.h>
#include <curses.h>
#include <ctype.h>
#include <panel.h>
#include <pthread.h>
#include <stdlib.h>

#include "defines.h"
#include "tcp.h"
#include "server.h"
#include "displayhandler.h"

extern char okay_dir[1024];
extern unsigned int view_lrpos;
extern int mode;
extern SERVERLIST *global_server;
extern char *localwindowlog[LOG_LINES];
extern pthread_mutex_t localwindowloglock;
extern void StripANSI(char *line);
extern int filtermethod;
bool FilterFilename(char *filename, char *filter);

void CDisplayHandler::UpdateMode(void) {
    wattrset(this->window_command,
             COLOR_PAIR(STYLE_INVERSE) | inverse_mono);

    if (mode == MODE_FTP_CHAIN)
        mvwaddstr(this->window_command, 1, 74, "FTP+");
    else if (mode == MODE_FTP_NOCHAIN)
        mvwaddstr(this->window_command, 1, 74, "FTP ");
    else if (mode == MODE_FXP_CHAIN)
        mvwaddstr(this->window_command, 1, 74, "FXP+");
    else
        mvwaddstr(this->window_command, 1, 74, "FXP ");

    //wnoutrefresh(this->window_command);
    update_panels();
    doupdate();
}

void CDisplayHandler::UpdateSpeed(bool left_win, bool force) {
    SERVERLIST *sv_temp = global_server;
    int magic;
    char *busy;
    float *speed;
    bool found = FALSE;

    if (left_win) {
        magic = this->leftwindow_magic;
        busy = this->leftwindow_busy;
        if (force)
            this->speed_left_old = this->speed_left;

        speed = &(this->speed_left);
    } else {
        magic = this->rightwindow_magic;
        busy = this->rightwindow_busy;
        if (force)
            this->speed_right_old = this->speed_right;

        speed = &(this->speed_right);
    }

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found) {
        *speed = sv_temp->server->GetSpeed();
        if (*speed < 0.0)
            *speed = 0.0;
        else if (*speed > 9999.99)
            *speed = 9999.99;

        if (busy && (!strcmp("[LIST]", busy) || !strcmp("[RETR]", busy)
                     || !strcmp("[STOR]", busy)))
            this->DrawSpeed(left_win, force);

        if ((!strcmp("[FXP>]", busy)) || (!strcmp("[FXP<]", busy)))
            this->DrawTime(left_win, force, sv_temp->server->GetTime());
    }
}

void CDisplayHandler::DrawSpeed(bool left_win, bool force) {
    WINDOW *window;
    char speed_string[] = { "[                 " }, temp[20];
    int width;
    float speed, *old_speed, percent;

    if (left_win) {
        window = this->window_left;
        width = this->window_left_width;
        old_speed = &(this->speed_left_old);
        speed = this->speed_left;
    } else {
        window = this->window_right;
        width = this->window_right_width;
        old_speed = &(this->speed_right_old);
        speed = this->speed_right;
    }
    percent = *old_speed * 0.1;

    if (force || (speed >= (*old_speed + percent))
        || (speed <= (*old_speed - percent))) {
        if (!force) {
            *old_speed = speed;
        }
        // draw speed
        sprintf(temp, "%4.02f kb/s]", speed);
        strcpy(speed_string + 14 - strlen(temp), temp);
        wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
        mvwaddstr(window, 0, width - strlen(speed_string) - 8, speed_string);

        //wnoutrefresh(window);
        update_panels();
        doupdate();
    }
}

void CDisplayHandler::DrawTime(bool left_win, bool force, long secs) {
    WINDOW *window;
    char speed_string[20], temp[20];
    int width;
    int mins, sec, *old_time;

    if (left_win) {
        window = this->window_left;
        width = this->window_left_width;
        old_time = &(this->time_left);
    } else {
        window = this->window_right;
        width = this->window_right_width;
        old_time = &(this->time_right);
    }
    mins = secs / 60;
    sec = secs % 60;
    if (mins > 99) {
        mins = 99;
    }

    if (force || (sec != *old_time)) {
        if (!force) {
            *old_time = sec;
        }
        // draw speed
        sprintf(temp, "[ %02d:%02d ]", mins, sec);
        strcpy(speed_string, temp);
        wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
        mvwaddstr(window, 0, width - strlen(speed_string) - 8, speed_string);

        //wnoutrefresh(window);
        update_panels();
        doupdate();
    }
}

bool CDisplayHandler::OpenView(void) {
    FILE *file;
    int n;

    // determine number of lines of the file
    if ((file = fopen(this->view_filename, "r"))) {
        n = 0;
        do {
            fgets(this->view_buffer, VIEW_BUFFER_MAX, file);
            n++;
        } while (!feof(file));

        fclose(file);

        this->view_lines_max = n;
    } else {
        sprintf(this->temp_string, "error opening '%s' for reading",
                this->view_filename);
        this->AddStatusLine(this->temp_string, TRUE);
        return (FALSE);
    }
    view_lrpos = 0;
    this->window_log =
        newwin(this->terminal_max_y, this->terminal_max_x, 0, 0);
    this->panel_log = new_panel(this->window_log);
    leaveok(window_log, TRUE);

    wattrset(this->window_log, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_log, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_log);
    //wbkgdset(this->window_log, ' ');

    this->view_line = 0;
    this->RefreshView();

    return (TRUE);
}

void CDisplayHandler::RefreshView(void) {
    FILE *file;
    int n, m;
    char *c;

    wattrset(this->window_log, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_log, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_log);
    //wbkgdset(this->window_log, ' ');

    // find position in file and display
    if ((file = fopen(this->view_filename, "r"))) {
        // we rely on correct view_line
        for (n = 0; n < this->view_line; n++)
            fgets(this->view_buffer, VIEW_BUFFER_MAX, file);

        n = 0;
        do {
            if (fgets(this->view_buffer, VIEW_BUFFER_MAX, file) != NULL) {
                c = strrchr(this->view_buffer, '\r');
                if (c)
                    *c = '\0';

                c = strrchr(this->view_buffer, '\n');
                if (c)
                    *c = '\0';

                StripANSI(this->view_buffer);
                /*
                for (m = strlen(this->view_buffer), c = this->view_buffer;
                    m > 0; m--, c++) {
                    if (filtermethod == 0) {
                        if (((unsigned char) *c <= 31)
                            || ((unsigned char) *c >= 127))
                            *c = '.';
                        } else {
                            if (((unsigned char) *c <= 31)
                                || ((unsigned char) *c == 127)
                                || ((unsigned char) *c == 155))
                                *c = '.';
                        }
                }
                */
                if (view_lrpos < strlen(this->view_buffer))
                    mvwaddnstr(this->window_log, n, 0,
                               this->view_buffer + view_lrpos,
                               this->terminal_max_x);
                n++;
            }
        } while ((n < (this->terminal_max_y - 1)) && !feof(file));

        fclose(file);
    } else {
        sprintf(this->temp_string, "error opening '%s' for reading",
                this->view_filename);
        mvwaddnstr(this->window_log, 0, 0, this->temp_string,
                   this->terminal_max_x);
        return ;
    }

    wattrset(this->window_log,
             COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono);

    for (m = 0; m < this->terminal_max_x; m++)
        mvwaddch(this->window_log, this->terminal_max_y - 1, m, ' ');

    mvwaddnstr(this->window_log, this->terminal_max_y - 1, 0,
               "View FILE: ESC to close - CURSOR and PGUP/DN to navigate",
               this->terminal_max_x);

    //wnoutrefresh(this->window_log);
    update_panels();
    doupdate();
}

void CDisplayHandler::ScrollView(bool dir_up) {
    bool redraw = FALSE;

    // to be improved (real scroll)
    if (dir_up) {
        if ((this->view_line + this->terminal_max_y - 2) <
            this->view_lines_max) {
            redraw = TRUE;
            this->view_line++;
        }
    } else {
        if (this->view_line > 0) {
            redraw = TRUE;
            this->view_line--;
        }
    }

    if (redraw)
        this->RefreshView();
}

void CDisplayHandler::PageMoveView(bool dir_up) {
    bool redraw = FALSE;

    if (dir_up) {
        if ((this->view_line + this->terminal_max_y - 2) <
            this->view_lines_max) {
            redraw = TRUE;
            this->view_line += this->terminal_max_y / 2;
            if ((this->view_line + this->terminal_max_y - 2) >=
                this->view_lines_max)
                this->view_line =
                    this->view_lines_max - this->terminal_max_y + 2;
        }
    } else {
        if (this->view_line > 0) {
            redraw = TRUE;
            this->view_line -= this->terminal_max_y / 2;
            if (this->view_line < 0)
                this->view_line = 0;
        }
    }

    if (redraw)
        this->RefreshView();
}

void CDisplayHandler::PageMoveViewEnd(bool dir_up) {
    bool redraw = FALSE;

    if (!dir_up) {
        if ((this->view_line + this->terminal_max_y - 2) <
            this->view_lines_max) {
            redraw = TRUE;
            //this->view_line += this->terminal_max_y / 2;
            //if((this->view_line + this->terminal_max_y - 2) >= this->view_lines_max)
            this->view_line =
                this->view_lines_max - this->terminal_max_y + 2;
        }
    } else {
        if (this->view_line > 0) {
            redraw = TRUE;
            //this->view_line -= this->terminal_max_y / 2;
            //if(this->view_line < 0)
            this->view_line = 0;
        }
    }

    if (redraw)
        this->RefreshView();
}

void CDisplayHandler::CloseView(void) {
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
        if (sv_temp->server->GetServerType() != SERVER_TYPE_LOCAL) {
            global_server->server->PostFromDisplay(FOR_SERVER_MSG_REFRESH, "");
            unlink(this->view_filename);
        }
    }

    del_panel(this->panel_log);
    delwin(this->window_log);
    this->window_log = NULL;
    this->panel_log = NULL;
    update_panels();
    doupdate();
    sprintf(this->temp_string, "%s%s.okay", okay_dir, view_fname);
    remove(temp_string);
    sprintf(this->temp_string, "%s%s.error", okay_dir, view_fname);
    remove(temp_string);

    //this->RebuildScreen();
    /*
    wnoutrefresh(this->window_command);
    wnoutrefresh(this->window_status);
    wnoutrefresh(this->window_left);
    wnoutrefresh(this->window_right);
    doupdate();
    */
}

void CDisplayHandler::OpenLog(CServer * server) {
    int n;

    this->window_log = newwin(this->terminal_max_y, this->terminal_max_x, 0, 0);
    this->panel_log = new_panel(this->window_log);
    leaveok(window_log, TRUE);

    wattrset(this->window_log, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_log, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_log);
    //wbkgdset(this->window_log, ' ');

    if (server->GetMagic() != SERVER_MAGIC_START) {
        // obtain log from server
        server->GetTCP()->ObtainLog((this->log));
        // fixup lines
        for (n = 0; n < LOG_LINES; n++) {
            if (this->log[n]) {
                if (strrchr(this->log[n], '\r'))
                    *(strrchr(this->log[n], '\r')) = '\0';
                else if (strrchr(this->log[n], '\n'))
                    *(strrchr(this->log[n], '\n')) = '\0';
            }
        }
    } else {
        pthread_mutex_lock(&(localwindowloglock));
        for (n = 0; n < LOG_LINES; n++) {
            if (localwindowlog[n]) {
                this->log[n] = new(char[strlen(localwindowlog[n]) + 1]);
                strcpy(this->log[n], localwindowlog[n]);
            } else
                this->log[n] = NULL;
        }
        pthread_mutex_unlock(&(localwindowloglock));
        // fixup lines
        for (n = 0; n < LOG_LINES; n++) {
            if (this->log[n]) {
                if (strrchr(this->log[n], '\r'))
                    *(strrchr(this->log[n], '\r')) = '\0';
                else if (strrchr(this->log[n], '\n'))
                    *(strrchr(this->log[n], '\n')) = '\0';
            }
        }
    }

    view_lrpos = 0;
    this->log_start = 0;
    this->RefreshLog();
}

void CDisplayHandler::ScrollLog(bool dir_up) {
    bool redraw = FALSE;

    // to be improved (real scroll)
    if (dir_up) {
        if ((this->log_start + this->terminal_max_y - 2) < (LOG_LINES - 1)) {
            redraw = TRUE;
            this->log_start++;
        }
    } else {
        if (this->log_start > 0) {
            redraw = TRUE;
            this->log_start--;
        }
    }

    if (redraw)
        this->RefreshLog();
}

void CDisplayHandler::PageMoveLog(bool dir_up) {
    bool redraw = FALSE;

    if (dir_up) {
        if ((this->log_start + this->terminal_max_y - 2) < (LOG_LINES - 1)) {
            redraw = TRUE;
            this->log_start += this->terminal_max_y / 2;
            if ((this->log_start + this->terminal_max_y - 2) >= (LOG_LINES - 1))
                this->log_start = (LOG_LINES - 1) - this->terminal_max_y + 2;
        }
    } else {
        if (this->log_start > 0) {
            redraw = TRUE;
            this->log_start -= this->terminal_max_y / 2;
            if (this->log_start < 0)
                this->log_start = 0;
        }
    }

    if (redraw)
        this->RefreshLog();
}

void CDisplayHandler::PageMoveLogEnd(bool dir_up) {
    bool redraw = FALSE;

    if (dir_up) {
        if ((this->log_start + this->terminal_max_y - 2) < (LOG_LINES - 1)) {
            redraw = TRUE;
            //this->log_start += this->terminal_max_y / 2;
            //if((this->log_start + this->terminal_max_y - 2) >= (LOG_LINES-1))
            this->log_start = (LOG_LINES - 1) - this->terminal_max_y + 3;
        }
    } else {
        if (this->log_start > 0) {
            redraw = TRUE;
            //this->log_start -= this->terminal_max_y / 2;
            //if(this->log_start < 0)
            this->log_start = 0;
        }
    }

    if (redraw)
        this->RefreshLog();
}

void CDisplayHandler::RefreshLog(void) {
    int m, n, index = this->log_start + this->terminal_max_y - 2;

    wattrset(this->window_log, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_log, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_log);
    //wbkgdset(this->window_log, ' ');

    for (n = 0; n < (this->terminal_max_y - 1); n++) {
        if (this->log[index])
            /*
            for (m = strlen(this->log[index]), c = this->log[index];
                m > 0; m--, c++) {
                if (filtermethod == 0) {
                    if (((unsigned char) *c <= 31) || ((unsigned char) *c >= 127))
                        *c = '.';
                } else {
                    if (((unsigned char) *c <= 31)
                        || ((unsigned char) *c == 127)
                        || ((unsigned char) *c == 155))
                        *c = '.';
                }
            }
            */
            if (view_lrpos < strlen(this->log[index]))
                mvwaddnstr(this->window_log, n, 0,
                           this->log[index] + view_lrpos,
                           this->terminal_max_x);

        //n++;
        //mvwaddnstr(this->window_log, n, 0, this->log[index], this->terminal_max_x);
        index--;
    }

    wattrset(this->window_log, COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono);

    for (m = 0; m < this->terminal_max_x; m++)
        mvwaddch(this->window_log, n, m, ' ');

    mvwaddnstr(this->window_log, n, 0,
               "View LOG: ESC to close - CURSOR and PGUP/DN to navigate",
               this->terminal_max_x);

    //wnoutrefresh(this->window_log);
    update_panels();
    doupdate();
}

/*
void CDisplayHandler::ShowWelcome(void) {
    int n;
    char logo[][52] = { "     __.                    $$$$$$",
        "    /  |   ____               $$$$ $$$$$'",
        "   /   |___\\__/________/\\_______$$.$$$'_______",
        " _/   _|\\_____________   \\  ___/.$$$$'_______  \\",
        ":\\_   \\   |    |    /    / __|.$$$$ $$.   /____/:",
        ":::\\______|____|___/    /__|.$$$$$$ $$$$. ___\\:::",
        "<---------------- /_____\\-.$$$$$$$$ $$$$$$.----->",
        "    fear the l33tness   .$$$$$$$$$$ $$$$$$$$.",
        "<-pl---------(pFtpfXP)------------- $$$$$$$$$$.->"
    };


    window_welcome =
        newwin(14, 60,
               (this->terminal_max_y - this->status_win_size - 2) / 2 - 7,
               this->terminal_max_x / 2 - 30);
    this->panel_welcome = new_panel(this->window_welcome);

    leaveok(window_welcome, TRUE);

    wattrset(this->window_welcome, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_welcome, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_welcome);
    //wbkgdset(this->window_welcome, ' ');
    mywborder(window_welcome);
    //mvwaddstr(window_welcome, 12, 1, "(c) pSi&HoE");
    mvwaddstr(window_welcome, 12, 59 - strlen(PFTP_VERSION), PFTP_VERSION);
    // simply diplay the logo
    for (n = 0; n < 9; n++)
        mvwaddstr(window_welcome, n + 2, 5, logo[n]);

    mvwaddstr(window_welcome, 0, 1, "welcome to pftp");
    //wnoutrefresh(this->window_welcome);
    update_panels();
    doupdate();
}
*/

/*
void CDisplayHandler::HideWelcome(void) {
    del_panel(this->panel_welcome);
    delwin(this->window_welcome);
    this->window_welcome = NULL;
    this->panel_welcome = NULL;
    update_panels();
    doupdate();
    //this->RebuildScreen();
    //wrefresh(this->window_command);
    //wrefresh(this->window_status);
    //wrefresh(this->window_left);
    //wrefresh(this->window_right);
}
*/

void CDisplayHandler::CloseLog(void) {
    int n;

    // free log
    for (n = 0; n < LOG_LINES; n++) {
        delete[](this->log[n]);
        this->log[n] = NULL;
    }

    del_panel(this->panel_log);
    delwin(this->window_log);
    this->window_log = NULL;
    this->panel_log = NULL;
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

FILELIST *CDisplayHandler::GetFilelistEntry(FILELIST * filelist, int magic) {
    FILELIST *fl_temp = filelist;
    int n = 0;

    while ((n < magic) && fl_temp) {    // no segf check
        fl_temp = fl_temp->next;
        n++;
    }
    assert(fl_temp != NULL);

    return (fl_temp);
}

char *CDisplayHandler::GetFilelistString(FILELIST * filelist, int magic,
        int width, bool * is_marked, int format, int offset) {

    FILELIST *fl_temp = filelist;
    int n = 0;
    char *string, temp_name[PATHLEN], owner[9], group[9];

    while ((n < magic) && fl_temp) {
        fl_temp = fl_temp->next;
        n++;
    }

    string = new(char[width + 1]);

    if (fl_temp) {
        *is_marked = fl_temp->is_marked;

        if (strlen(fl_temp->name) < (unsigned)offset) {
            strcpy(temp_name, "");
        } else {
            strncpy(temp_name, fl_temp->name + offset, PATHLEN);
        }

        temp_name[PATHLEN - 1] = '\0';

        if (format == 0) {
            temp_name[width - 9] = '\0';
            if (!fl_temp->is_dir)
                if (fl_temp->size > 1000000000) {
                    sprintf(string, "%-*s %7.3fG", width - 9, temp_name,
                            (float)fl_temp->size / 1024.0 / 1024.0 / 1024.0);
                } else if (fl_temp->size > 100000000) {
                    sprintf(string, "%-*s %7.1fM", width - 9, temp_name,
                            (float)fl_temp->size / 1024.0 / 1024.0);
                } else if (fl_temp->size > 10000000) {
                    sprintf(string, "%-*s %7.3fM", width - 9, temp_name,
                            (float)fl_temp->size / 1024.0 / 1024.0);
                } else
                    sprintf(string, "%-*s %8ld", width - 9, temp_name,
                            fl_temp->size);
            else {
                if (fl_temp->size >= 1024 * 1000000) {
                    sprintf(string, "%-*s d%6ldM", width - 9, temp_name,
                            (long) (fl_temp->size / 1024 / 1024));
                } else {
                    if (fl_temp->size >= 1024 * 1000) {
                        sprintf(string, "%-*s d%6ldK", width - 9,
                                temp_name, (long) (fl_temp->size / 1024));
                    } else {
                        sprintf(string, "%-*s d%7ld", width - 9, temp_name,
                                fl_temp->size);
                    }
                }
            }
        } else if (format == 1) {
            temp_name[width] = '\0';
            sprintf(string, "%-*s", width, temp_name);
        } else if (format == 2) {
            temp_name[width - 24] = '\0';
            if (!fl_temp->is_dir)
                sprintf(string, "%-*s %s %s", width - 24, temp_name,
                        fl_temp->mode, fl_temp->date);
            else
                sprintf(string, "%-*s %s %s", width - 24, temp_name,
                        fl_temp->mode, fl_temp->date);
        } else if (format == 3) {
            temp_name[width - 18] = '\0';

            strcpy(group, "        ");
            group[8 - strlen(fl_temp->group)] = '\0';
            strcat(group, fl_temp->group);
            strcpy(owner, "        ");
            owner[8 - strlen(fl_temp->owner)] = '\0';
            strcat(owner, fl_temp->owner);
            sprintf(string, "%-*s %s %s", width - 18, temp_name, owner, group);
        } else if (format == 4) {
            temp_name[width - 18] = '\0';

            strcpy(owner, "        ");
            owner[8 - strlen(fl_temp->owner)] = '\0';
            strcat(owner, fl_temp->owner);

            if (!fl_temp->is_dir)
                if (fl_temp->size > 1000000000) {
                    sprintf(string, "%-*s %7.3fG %s", width - 18, temp_name,
                            (float)fl_temp->size / 1024.0 / 1024.0 / 1024.0, owner);
                } else if (fl_temp->size > 100000000) {
                    sprintf(string, "%-*s %7.1fM %s", width - 18, temp_name,
                            (float)fl_temp->size / 1024.0 / 1024.0, owner);
                } else if (fl_temp->size > 10000000) {
                    sprintf(string, "%-*s %7.3fM %s", width - 18, temp_name,
                            (float)fl_temp->size / 1024.0 / 1024.0, owner);
                } else
                    sprintf(string, "%-*s %8ld %s", width - 18, temp_name,
                            fl_temp->size, owner);
            else {
                if (fl_temp->size >= 1024 * 1000000) {
                    sprintf(string, "%-*s d%6ldM %s", width - 18, temp_name,
                            (long) (fl_temp->size / 1024 / 1024), owner);
                } else {
                    if (fl_temp->size >= 1024 * 1000) {
                        sprintf(string, "%-*s d%6ldK %s", width - 18,
                                temp_name, (long) (fl_temp->size / 1024), owner);
                    } else {
                        sprintf(string, "%-*s d%7ld %s", width - 18, temp_name,
                                fl_temp->size, owner);
                    }
                }
            }
        }
    } else {
        // should never happen, just to avoid segfault
        // should never get here !
        assert(0);
        *is_marked = FALSE;
        strcpy(string, "<INVALID ERROR>");
    }

    return (string);
}

void CDisplayHandler::UpdateTabBar(bool dont_remove) {
    FILELIST *filelist_remove = NULL, *filelist_draw = NULL;
    char *remove_string = NULL, *draw_string;
    int remove_magic = -1, draw_magic = -1, remove_ypos = -1, draw_ypos = -1,
        width_remove = -1, width_draw = -1;
    WINDOW *window_remove = NULL, *window_draw = NULL;
    bool is_marked = FALSE;
    int format_remove = 0, format_draw = 0;
    int soffset_remove = 0, soffset_draw = 0;

    if (this->window_tab == this->window_left) {
        if (this->filelist_right) {
            // okay to remove old bar
            soffset_remove = this->filelist_right_soffset;
            filelist_remove = this->filelist_right;
            remove_magic = this->filelist_right_magic;
            window_remove = this->window_right;
            remove_ypos = this->filelist_right_ypos;
            width_remove = this->window_right_width - 2;
            format_remove = this->filelist_right_format;
        }
        if (this->filelist_left) {
            // it's okay to paint new tab-bar
            soffset_draw = this->filelist_left_soffset;
            filelist_draw = this->filelist_left;
            draw_magic = this->filelist_left_magic;
            window_draw = this->window_left;
            draw_ypos = this->filelist_left_ypos;
            width_draw = this->window_left_width - 2;
            format_draw = this->filelist_left_format;
        }
    } else {
        // it's okay to paint new tab-bar
        if (this->filelist_left) {
            // okay to remove old bar
            soffset_remove = this->filelist_left_soffset;
            filelist_remove = this->filelist_left;
            remove_magic = this->filelist_left_magic;
            window_remove = this->window_left;
            remove_ypos = this->filelist_left_ypos;
            width_remove = this->window_left_width - 2;
            format_remove = this->filelist_left_format;
        }
        if (this->filelist_right) {
            soffset_draw = this->filelist_right_soffset;
            filelist_draw = this->filelist_right;
            draw_magic = this->filelist_right_magic;
            window_draw = this->window_right;
            draw_ypos = this->filelist_right_ypos;
            width_draw = this->window_right_width - 2;
            format_draw = this->filelist_right_format;
        }
    }

    // remove old bar
    if (filelist_remove && !dont_remove) {
        remove_string =
            this->GetFilelistString(filelist_remove, remove_magic,
                                    width_remove, &is_marked,
                                    format_remove, soffset_remove);
        if (is_marked)
            wattrset(window_remove, COLOR_PAIR(STYLE_MARKED) | A_BOLD);
        else if (FilterFilename(remove_string, this->exclude_source) == FALSE && this->use_exclude_source && strncmp(remove_string, "..", 2))
            wattrset(window_remove, COLOR_PAIR(STYLE_EXCLUDED) | A_BOLD);
        else
            wattrset(window_remove, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);

        mvwaddstr(window_remove, remove_ypos, 1, remove_string);

        if (format_remove == 0) {
            mvwaddch(window_remove, remove_ypos, width_remove - 8, ACS_VLINE);
            //mvwaddch(window_remove, remove_ypos, width_remove - 12, ACS_VLINE);
        } else if (format_remove == 4) {
            mvwaddch(window_remove, remove_ypos, width_remove - 8, ACS_VLINE);
            mvwaddch(window_remove, remove_ypos, width_remove - 17, ACS_VLINE);
        } else if (format_remove == 3) {
            mvwaddch(window_remove, remove_ypos, width_remove - 8, ACS_VLINE);
            mvwaddch(window_remove, remove_ypos, width_remove - 17, ACS_VLINE);
        } else if (format_remove == 2) {
            mvwaddch(window_remove, remove_ypos, width_remove - 12, ACS_VLINE);
            mvwaddch(window_remove, remove_ypos, width_remove - 23, ACS_VLINE);
        }

        delete[](remove_string);
        //wnoutrefresh(window_remove);
        update_panels();
        doupdate();
    }

    if (filelist_draw) {
        draw_string =
            this->GetFilelistString(filelist_draw, draw_magic, width_draw,
                                    &is_marked, format_draw, soffset_draw);
        if (is_marked)
            wattrset(window_draw, COLOR_PAIR(STYLE_MARKED_INVERSE) | A_BOLD | this->inverse_mono);
        else if (FilterFilename(draw_string, this->exclude_source) == FALSE && this->use_exclude_source && strncmp(draw_string, "..", 2))
            wattrset(window_draw, COLOR_PAIR(STYLE_EXCLUDED_INVERSE) | A_BOLD | this->inverse_mono);
        else
            wattrset(window_draw, COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono);

        mvwaddstr(window_draw, draw_ypos, 1, draw_string);

        if (format_draw == 0) {
            mvwaddch(window_draw, draw_ypos, width_draw - 8, ACS_VLINE);
            //mvwaddch(window_draw, draw_ypos, width_draw - 12, ACS_VLINE);
        } else if (format_draw == 4) {
            mvwaddch(window_draw, draw_ypos, width_draw - 8, ACS_VLINE);
            mvwaddch(window_draw, draw_ypos, width_draw - 17, ACS_VLINE);
        } else if (format_draw == 3) {
            mvwaddch(window_draw, draw_ypos, width_draw - 8, ACS_VLINE);
            mvwaddch(window_draw, draw_ypos, width_draw - 17, ACS_VLINE);
        } else if (format_draw == 2) {
            mvwaddch(window_draw, draw_ypos, width_draw - 12, ACS_VLINE);
            mvwaddch(window_draw, draw_ypos, width_draw - 23, ACS_VLINE);
        }

        delete[](draw_string);
        //wnoutrefresh(window_draw);
        update_panels();
        doupdate();
    }

    //doupdate();
}

void CDisplayHandler::FetchBusy(CServer * server, WINDOW * window) {
    char *busy;
    int n;

    busy = server->ObtainBusy();

    if (window == this->window_left) {
        if (this->leftwindow_busy)
            delete[](this->leftwindow_busy);

        this->leftwindow_busy = NULL;

        // remove in every case
        wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
        for (n = 0; n < 22; n++)
            mvwaddch(window, 0, this->window_left_width - 23 + n, ACS_HLINE);

        /*
        for(n = 0; n < 7; n++)
            mvwaddch(window, 0, this->window_left_width - 8 + n, ACS_HLINE);
        */

        if (busy) {
            sprintf(this->temp_string, "[%s]", busy);
            this->leftwindow_busy =
                new(char[strlen(this->temp_string) + 1]);
            strcpy(this->leftwindow_busy, this->temp_string);
            delete[](busy);

            // draw on display
            mvwaddstr(window, 0,
                      this->window_left_width - strlen(this->temp_string) - 1,
                      this->temp_string);
        }
    } else {
        if (this->rightwindow_busy)
            delete[](this->rightwindow_busy);

        this->rightwindow_busy = NULL;

        // remove in every case
        wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
        for (n = 0; n < 22; n++)
            mvwaddch(window, 0, this->window_right_width - 23 + n,
                     ACS_HLINE);

        /*
        for(n = 0; n < 7; n++)
            mvwaddch(window, 0, this->window_right_width - 8 + n, ACS_HLINE);
        */

        if (busy) {
            sprintf(this->temp_string, "[%s]", busy);
            this->rightwindow_busy =
                new(char[strlen(this->temp_string) + 1]);
            strcpy(this->rightwindow_busy, this->temp_string);
            delete[](busy);

            // draw on display
            mvwaddstr(window, 0,
                      window_right_width - strlen(this->temp_string) - 1,
                      this->temp_string);
        }
    }

    //wnoutrefresh(window);
    update_panels();
    doupdate();
}

void CDisplayHandler::UpdateServerBusy(WINDOW * window, char *busy) {
    char *old_busy, *new_busy;
    int n, width;

    if (window == this->window_left) {
        old_busy = this->leftwindow_busy;
        this->leftwindow_busy = NULL;
        width = this->window_left_width;
    } else {
        old_busy = this->rightwindow_busy;
        this->rightwindow_busy = NULL;
        width = this->window_right_width;
    }

    if (old_busy)
        delete[](old_busy);

    wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);

    if (!busy) {
        // remove if not busy
        for (n = 0; n < 22; n++)
            mvwaddch(window, 0, width - 23 + n, ACS_HLINE);
    } else {
        // set new busy reason
        sprintf(this->temp_string, "[%s]", busy);

        new_busy = new(char[strlen(this->temp_string) + 1]);
        strcpy(new_busy, this->temp_string);

        if (window == this->window_left)
            this->leftwindow_busy = new_busy;
        else
            this->rightwindow_busy = new_busy;

        for (n = 0; n < 22; n++)
            mvwaddch(window, 0, width - 23 + n, ACS_HLINE);

        // draw on display
        mvwaddstr(window, 0, width - strlen(new_busy) - 1, new_busy);
    }

    //wnoutrefresh(window);
    //doupdate();
    update_panels();
    doupdate();
}

void CDisplayHandler::UpdateServerLabel(int magic) {
    char *llabel = NULL;
    FILELIST *fl_start = NULL;
    SERVERLIST *sv_temp = global_server;
    WINDOW *window = NULL;
    bool found = FALSE;

    if (magic == this->leftwindow_magic) {
        fl_start = this->filelist_left;
        llabel = this->window_left_label;
        window = this->window_left;
    } else if (magic == this->rightwindow_magic) {
        llabel = this->window_right_label;
        fl_start = this->filelist_right;
        window = this->window_right;
    }

    if (!llabel)
        return ;

    while (!found && sv_temp) {
        if (sv_temp->server->GetMagic() == magic)
            found = TRUE;
        else
            sv_temp = sv_temp->next;
    }

    if (found == FALSE)
        return ;

#ifdef TLS
    if (sv_temp->server->TLS_Status()) {
        llabel[0] = '*';
        strcpy(llabel + 1, sv_temp->server->GetAlias());
    } else
#endif

        strcpy(llabel, sv_temp->server->GetAlias());

    if (!fl_start)
        return ;

    this->UpdateFilelistNewPosition(window);
    this->UpdateTabBar(FALSE);
}

void CDisplayHandler::UpdateServerCWD(CServer * server, WINDOW * window) {
    int width;
    char *cwd;

    // updates CWD
    if (window == this->window_left) {
        width = this->window_left_width;
        cwd = this->window_left_cwd;
    } else {
        width = this->window_right_width;
        cwd = this->window_right_cwd;
    }
    // obtain copy of new working-dir from server
    server->ObtainWorkingDir(cwd);

    // display cwd
    wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);

    if (strlen(cwd) <= (unsigned) (width - 2))
        mvwaddstr(window, this->terminal_max_y - this->status_win_size - 5,
                  1, cwd);
    else
        mvwaddstr(window, this->terminal_max_y - this->status_win_size - 5,
                  1, cwd + strlen(cwd) - (width - 2));

    //wnoutrefresh(window);
    update_panels();
    doupdate();
}

void CDisplayHandler::FileToggleMark(void) {
    int magic;
    FILELIST *fl_temp;
    bool found = FALSE;

    if (this->window_tab == this->window_left) {
        fl_temp = this->filelist_left;
        magic = this->filelist_left_magic;
    } else {
        fl_temp = this->filelist_right;
        magic = this->filelist_right_magic;
    }

    while (!found && fl_temp) {
        if (fl_temp->magic == magic)
            found = TRUE;
        else
            fl_temp = fl_temp->next;
    }

    if (found) {
        fl_temp->is_marked = !fl_temp->is_marked;
        this->UpdateFilelistScroll(TRUE, TRUE);
    }
}

void CDisplayHandler::UpdateFilelistScroll(bool dir_up,
        bool redraw_everytime) {
    FILELIST *fl_start;
    char *cwd, *label, *string, *busy, *temp_label;
    int n, width, height =
        this->terminal_max_y - this->status_win_size - 6, old_ypos, *ypos,
        old_magic, *magic, entries;
    bool is_left, movetab = redraw_everytime, scroll = FALSE, is_marked;
    int format, soffset;

    if (this->window_tab == this->window_left) {
        fl_start = this->filelist_left;
        soffset = this->filelist_left_soffset;
        entries = this->filelist_left_entries;
        magic = &(this->filelist_left_magic);
        ypos = &(this->filelist_left_ypos);
        cwd = this->window_left_cwd;
        busy = this->leftwindow_busy;
        label = this->window_left_label;
        width = this->window_left_width - 2;
        format = this->filelist_left_format;
        is_left = TRUE;
    } else {
        fl_start = this->filelist_right;
        soffset = this->filelist_right_soffset;
        entries = this->filelist_right_entries;
        magic = &(this->filelist_right_magic);
        ypos = &(this->filelist_right_ypos);
        cwd = this->window_right_cwd;
        busy = this->rightwindow_busy;
        label = this->window_right_label;
        width = this->window_right_width - 2;
        format = this->filelist_right_format;
        is_left = FALSE;
    }

    if (fl_start == NULL)
        return ;

    // determine direction
    old_ypos = *ypos;
    old_magic = *magic;

    if (dir_up) {
        if (*magic < (entries - 1)) {
            (*magic)++;
            if (*ypos < height) {
                movetab = TRUE;
                (*ypos)++;
            } else
                scroll = TRUE;
        }
    } else {
        if (*magic > 0) {
            (*magic)--;
            if (*ypos > 1) {
                movetab = TRUE;
                (*ypos)--;
            } else
                scroll = TRUE;
        }
    }

    // actually reflect the changings
    if (movetab || scroll) {
        // remove tabbar
        wattrset(this->window_tab, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
        string =
            this->GetFilelistString(fl_start, old_magic, width, &is_marked,
                                    format, soffset);
        if (is_marked)
            wattrset(this->window_tab, COLOR_PAIR(STYLE_MARKED) | A_BOLD);
	else if (FilterFilename(string, this->exclude_source) == FALSE && this->use_exclude_source && strncmp(string, "..", 2))
            wattrset(this->window_tab, COLOR_PAIR(STYLE_EXCLUDED) | A_BOLD);
        else
            wattrset(this->window_tab, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);

        mvwaddstr(this->window_tab, old_ypos, 1, string);
        if (format == 0) {
            mvwaddch(this->window_tab, old_ypos, width - 8, ACS_VLINE);
            //mvwaddch(this->window_tab, old_ypos, width - 12, ACS_VLINE);
        } else if (format == 4) {
            mvwaddch(this->window_tab, old_ypos, width - 8, ACS_VLINE);
            mvwaddch(this->window_tab, old_ypos, width - 17, ACS_VLINE);
        } else if (format == 3) {
            mvwaddch(this->window_tab, old_ypos, width - 8, ACS_VLINE);
            mvwaddch(this->window_tab, old_ypos, width - 17, ACS_VLINE);
        } else if (format == 2) {
            mvwaddch(this->window_tab, old_ypos, width - 12, ACS_VLINE);
            mvwaddch(this->window_tab, old_ypos, width - 23, ACS_VLINE);
        }
        delete[](string);
    }

    if (scroll) {
        wattrset(this->window_tab, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
        mywborder(this->window_tab);

        if (dir_up) {
            // scroll [2:height] one up
            wscrl(this->window_tab, 1);
        } else {
            // scroll [1:height-1] one down
            wscrl(this->window_tab, -1);
        }

        // we wiped some info, redraw
        wattrset(this->window_tab, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
        mywborder(this->window_tab);

        if (cwd) {
            wattrset(this->window_tab, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);

            if (strlen(cwd) <= (unsigned) width)
                mvwaddstr(this->window_tab,
                          this->terminal_max_y - this->status_win_size - 5,
                          1, cwd);
            else
                mvwaddstr(this->window_tab,
                          this->terminal_max_y - this->status_win_size - 5,
                          1, cwd + strlen(cwd) - width);
        }

        if (label) {
            wattrset(this->window_tab, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
            temp_label = new char[strlen(label) + 5];
	    sprintf(temp_label, "[ %s ]", label);
	    
            if (strlen(temp_label) <= (unsigned) width)
                mvwaddstr(this->window_tab, 0, 1, temp_label);
            else
                mvwaddstr(this->window_tab, 0, 1,
                          temp_label + strlen(temp_label) - width);
            delete[] temp_label;
        }

        wattrset(this->window_tab, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);

        for (n = 0; n < 22; n++)
            mvwaddch(this->window_tab, 0, width + 2 - 23 + n, ACS_HLINE);

        if (busy) {
            mvwaddstr(this->window_tab, 0, width + 2 - strlen(busy) - 1, busy);
            this->UpdateSpeed(is_left, FALSE);
        }
    }

    if (movetab || scroll) {
        // draw actual line with bar
        string =
            this->GetFilelistString(fl_start, *magic, width, &is_marked,
                                    format, soffset);
        if (is_marked)
            wattrset(this->window_tab, COLOR_PAIR(STYLE_MARKED_INVERSE) | A_BOLD | this->inverse_mono);
	else if (FilterFilename(string, this->exclude_source) == FALSE && this->use_exclude_source && strncmp(string, "..", 2))
            wattrset(this->window_tab, COLOR_PAIR(STYLE_EXCLUDED_INVERSE) | A_BOLD | this->inverse_mono);
        else
            wattrset(this->window_tab, COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono);

        mvwaddstr(this->window_tab, *ypos, 1, string);

        if (format == 0) {
            mvwaddch(this->window_tab, *ypos, width - 8, ACS_VLINE);
        } else if (format == 4) {
            mvwaddch(this->window_tab, *ypos, width - 8, ACS_VLINE);
            mvwaddch(this->window_tab, *ypos, width - 17, ACS_VLINE);
        } else if (format == 3) {
            mvwaddch(this->window_tab, *ypos, width - 8, ACS_VLINE);
            mvwaddch(this->window_tab, *ypos, width - 17, ACS_VLINE);
        } else if (format == 2) {
            mvwaddch(this->window_tab, *ypos, width - 12, ACS_VLINE);
            mvwaddch(this->window_tab, *ypos, width - 23, ACS_VLINE);
        }
        delete[](string);

        //wnoutrefresh(this->window_tab);
        update_panels();
        doupdate();
    }
}

void CDisplayHandler::UpdateFilelistPageMove(bool dir_up) {
    WINDOW *window;
    int *ypos, entries, *magic, jump =
        (this->terminal_max_y - this->status_win_size - 6) / 2;

    if (this->window_tab == this->window_left) {
        if (this->filelist_left == NULL)
            return ;

        window = this->window_left;
        entries = this->filelist_left_entries;
        magic = &(this->filelist_left_magic);
        ypos = &(this->filelist_left_ypos);
    } else {
        if (this->filelist_right == NULL)
            return ;

        window = this->window_right;
        entries = this->filelist_right_entries;
        magic = &(this->filelist_right_magic);
        ypos = &(this->filelist_right_ypos);
    }
    // determine how far we should jump
    if (dir_up) {
        if (jump >
            (this->terminal_max_y - this->status_win_size - 4) - *ypos + 1)
            jump =
                (this->terminal_max_y - this->status_win_size - 4) -
                *ypos + 1;

        if ((*magic + jump) > (entries - 1))
            *magic = entries - 1;
        else
            *magic += jump;
    } else {
        if ((*magic - jump) < 0)
            *magic = 0;
        else
            *magic -= jump;
    }

    this->UpdateFilelistNewPosition(window);
    this->UpdateTabBar(FALSE);
}

void CDisplayHandler::UpdateFilelistPageEnd(bool dir_up) {
    WINDOW *window;
//    int *ypos, entries, *magic;
    int entries, *magic;

    if (this->window_tab == this->window_left) {
        if (this->filelist_left == NULL)
            return ;

        window = this->window_left;
        entries = this->filelist_left_entries;
        magic = &(this->filelist_left_magic);
//        ypos = &(this->filelist_left_ypos);
    } else {
        if (this->filelist_right == NULL)
            return ;

        window = this->window_right;
        entries = this->filelist_right_entries;
        magic = &(this->filelist_right_magic);
//        ypos = &(this->filelist_right_ypos);
    }

    // determine how far we should jump
    if (!dir_up) {
        //if (jump>(this->terminal_max_y - this->status_win_size - 4)-*ypos+1)
        //    jump=(this->terminal_max_y - this->status_win_size - 4)-*ypos+1;
        //if((*magic + jump) > (entries - 1))
        *magic = entries - 1;
        //else
        //    *magic += jump;
    } else {
        //if((*magic - jump) < 0)
        *magic = 0;
        //else
        //*magic -= jump;
    }

    this->UpdateFilelistNewPosition(window);
    this->UpdateTabBar(FALSE);
}

void CDisplayHandler::UpdateFilelistByChar(char chr) {
    WINDOW *window;
    FILELIST *fl_temp, *fl_tempold;
    bool change = FALSE;
    int entries, *magic, n = 0;

    if (this->window_tab == this->window_left) {
        window = this->window_left;
        entries = this->filelist_left_entries;
        magic = &(this->filelist_left_magic);
        fl_temp = this->filelist_left;
    } else {
        window = this->window_right;
        entries = this->filelist_right_entries;
        magic = &(this->filelist_right_magic);
        fl_temp = this->filelist_right;
    }

    fl_tempold = fl_temp;

    while ((n < *magic) && fl_temp) {   // no segf check
        fl_temp = fl_temp->next;
        n++;
    }
    if (fl_temp == NULL)
        return; // like jumping in empty filelist

    //assert(fl_temp!=NULL);
    if (tolower(fl_temp->name[0]) != chr) {
        n = 0;
        fl_temp = fl_tempold;
        while ((n < entries) && fl_temp) { // no segf check
            assert(fl_temp != NULL);

            if (tolower(fl_temp->name[0]) < chr) {
                change = TRUE;
                *magic = n;
            } else if (tolower(fl_temp->name[0]) == chr) {
                change = TRUE;
                *magic = n;
                break;
            }
            fl_temp = fl_temp->next;
            n++;
        }
    } else {
        fl_temp = fl_temp->next;
        n++;
        while ((n < entries) && fl_temp) {      // no segf check
            assert(fl_temp != NULL);

            if (tolower(fl_temp->name[0]) == chr) {
                change = TRUE;
                *magic = n;
                break;
            }
            fl_temp = fl_temp->next;
            n++;
        }

        if (change == FALSE) {
            n = 0;
            fl_temp = fl_tempold;
            while ((n < entries) && fl_temp) {  // no segf check
                assert(fl_temp != NULL);

                if (tolower(fl_temp->name[0]) == chr) {
                    change = TRUE;
                    *magic = n;
                    break;
                }
                fl_temp = fl_temp->next;
                n++;
            }
        }
    }

    if (change == TRUE) {
        this->UpdateFilelistNewPosition(window);
        this->UpdateTabBar(FALSE);
    }
}

void CDisplayHandler::UpdateFilelistNewPosition(WINDOW * window) {
    FILELIST *fl_start, *fl_temp;
    char *busy, *cwd, *label, *string, *temp_label;
    int entries, magic;
//    int draw_ypos = 1, *ypos, pos, n, width, height =
    int draw_ypos = 1, *ypos, n, width, height =
        this->terminal_max_y - this->status_win_size - 6;
    int format, soffset;
    bool is_left;

    // refresh filelist from scratch, using actual magic
    // determine number of entries on filelist
    if (window == this->window_left) {
        fl_start = this->filelist_left;
        soffset = this->filelist_left_soffset;
        width = this->window_left_width - 2;
        entries = this->filelist_left_entries;
        magic = this->filelist_left_magic;
        ypos = &(this->filelist_left_ypos);
        cwd = this->window_left_cwd;
        busy = this->leftwindow_busy;
        label = this->window_left_label;
        format = this->filelist_left_format;
        is_left = TRUE;
    } else {
        fl_start = this->filelist_right;
        soffset = this->filelist_right_soffset;
        width = this->window_right_width - 2;
        entries = this->filelist_right_entries;
        magic = this->filelist_right_magic;
        ypos = &(this->filelist_right_ypos);
        cwd = this->window_right_cwd;
        busy = this->rightwindow_busy;
        label = this->window_right_label;
        format = this->filelist_right_format;
        is_left = FALSE;
    }

    // now we got to determine how to display the filelist
    // strategie is as follows:
    // - entries <= height : don't care, start from beginning and make *ypos = magic + 1
    // - entries > height  : see if we can set *ypos to 1 and draw the remaining list until height reached
    //                       otherwise draw that last entry is on last window_line and set *ypos to reflect the tab-height

    // erase screen
    wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(window, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(window);
    //wbkgdset(window, ' ');
    wborder(window, 0, 0, 0, 0, 0, 0, 0, 0);

    if (fl_start == NULL)
        return ;

    /*
    for(n = 0; n < width; n++)
        filler[n] = ' ';

    filler[n] = '\0';
    */

    if (entries <= height) {
        // the easy case, less entries than screenheight
        *ypos = magic + 1;
        fl_temp = fl_start;
    } else {
        if (magic <= (entries - height)) {
            // the not-so-easy-but-handleable-case
            *ypos = 1;

            fl_temp = fl_start;
            for (n = 0; n < magic; n++) {
                fl_temp = fl_temp->next; // it cannot segfault here
            }
        } else {
            // worst case. as with murphy, i'm sure it's in here almost all the time :P
            fl_temp = fl_start;
            for (n = 0; n < (entries - height); n++)
                fl_temp = fl_temp->next; // again, this is safe

            *ypos = magic - (entries - height) + 1;
        }
    }

    // actually draw the lines
    while ((draw_ypos <= height) && fl_temp) {
//        pos = width - strlen(fl_temp->name) - 22;

        /*
        if(pos >= 0) {
            filler[pos] = '\0';
            if(!format) {
                if(!fl_temp->is_dir)
                    sprintf(this->temp_string, "%s%s %8ld %s", fl_temp->name, filler, (fl_temp->size>=100000000?99999999:fl_temp->size), fl_temp->date);
                else
                    sprintf(this->temp_string, "%s%s      DIR %s", fl_temp->name, filler, fl_temp->date);
            } else {
                strcpy(owner, "        ");
                owner[8 - strlen(fl_temp->owner)] = '\0';
                strcat(owner, fl_temp->owner);
                sprintf(this->temp_string, "%s%s %s   %s", fl_temp->name, filler, owner, fl_temp->mode);
            }
            filler[pos] = ' ';
        } else {
            strcpy(temp_name, fl_temp->name);
            temp_name[width - 22] = '\0';

            if(!format) {
                if(!fl_temp->is_dir)
                    sprintf(this->temp_string, "%s %8ld %s", temp_name, (fl_temp->size>=100000000?99999999:fl_temp->size), fl_temp->date);
                else
                    sprintf(this->temp_string, "%s      DIR %s", temp_name, fl_temp->date);
            } else {
                strcpy(owner, "        ");
                owner[8 - strlen(fl_temp->owner)] = '\0';
                strcat(owner, fl_temp->owner);
                sprintf(this->temp_string, "%s %s   %s", temp_name, owner, fl_temp->mode);
            }
        }
        */

        string =
            this->GetFilelistString(fl_temp, 0, width, &fl_temp->is_marked,
                                    format, soffset);
        if (fl_temp->is_marked)
            wattrset(window, COLOR_PAIR(STYLE_MARKED) | A_BOLD);
        else if (FilterFilename(fl_temp->name, this->exclude_source) == FALSE && this->use_exclude_source && strncmp(string, "..", 2))
	    wattrset(window, COLOR_PAIR(STYLE_EXCLUDED) | A_BOLD);
        else
            wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);

        mvwaddstr(window, draw_ypos, 1, string);

        if (format == 0) {
            mvwaddch(window, draw_ypos, width - 8, ACS_VLINE);
        } else if (format == 4) {
            mvwaddch(window, draw_ypos, width - 8, ACS_VLINE);
            mvwaddch(window, draw_ypos, width - 17, ACS_VLINE);
        } else if (format == 3) {
            mvwaddch(window, draw_ypos, width - 8, ACS_VLINE);
            mvwaddch(window, draw_ypos, width - 17, ACS_VLINE);
        } else if (format == 2) {
            mvwaddch(window, draw_ypos, width - 12, ACS_VLINE);
            mvwaddch(window, draw_ypos, width - 23, ACS_VLINE);
        }
        delete[](string);
        fl_temp = fl_temp->next;
        draw_ypos++;
    }

    /*
    if (draw_ypos!=1) {
        while(draw_ypos <= height) {
            mvwaddch(window, draw_ypos, width - 21, ACS_VLINE);
            mvwaddch(window, draw_ypos, width - 12, ACS_VLINE);
            draw_ypos++;
        }
    }
    */

    // we wiped some information, redraw...
    if (cwd) {
        wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);

        if (strlen(cwd) <= (unsigned) width)
            mvwaddstr(window,
                      this->terminal_max_y - this->status_win_size - 5, 1, cwd);
        else
            mvwaddstr(window,
                      this->terminal_max_y - this->status_win_size - 5, 1,
                      cwd + strlen(cwd) - width);
    }

    if (label) {
        wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
	temp_label = new char[strlen(label) + 5]; 
        sprintf(temp_label, "[ %s ]", label);
	wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);

        if (strlen(temp_label) <= (unsigned) width)
            mvwaddstr(window, 0, 1, temp_label);
        else
            mvwaddstr(window, 0, 1, temp_label + strlen(temp_label) - width);
        delete[] temp_label;
    }

    wattrset(window, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);

    for (n = 0; n < 22; n++)
        mvwaddch(window, 0, width + 2 - 23 + n, ACS_HLINE);

    if (busy) {
        mvwaddstr(window, 0, width + 2 - strlen(busy) - 1, busy);
        this->UpdateSpeed(is_left, FALSE);
    }

    //wnoutrefresh(window);
    update_panels();
    doupdate();
}

void CDisplayHandler::UpdateServerFilelist(CServer * server, WINDOW * window) {
    FILELIST *fl_temp, *fl_temp1;
    int n, *entries, *magic, *jump_stack_pos, *jump_stack, m;
    bool use_jump, has_dirs = FALSE, found;
    time_t elapsed_time, newest_time[JUMP_STACK];

    // free old list and obtain copy of new filelist
    if (window == this->window_left) {
        fl_temp = this->filelist_left;
        magic = &(this->filelist_left_magic);
        jump_stack_pos = &(this->jump_stack_left_pos);
        jump_stack = this->jump_stack_left;
        entries = &(this->filelist_left_entries);
    } else {
        fl_temp = this->filelist_right;
        magic = &(this->filelist_right_magic);
        jump_stack_pos = &(this->jump_stack_right_pos);
        jump_stack = this->jump_stack_right;
        entries = &(this->filelist_right_entries);
    }

    while (fl_temp) {
        fl_temp1 = fl_temp;
        fl_temp = fl_temp->next;
        delete[](fl_temp1->name);
        delete(fl_temp1);
    }

    if (window == this->window_left)
        fl_temp = this->filelist_left = server->ObtainFilelist(&use_jump);
    else
        fl_temp = this->filelist_right = server->ObtainFilelist(&use_jump);

    // no seg-fault protection. it MUST work since noone modifies the list/magic other than this thread
    fl_temp1 = fl_temp;
    *entries = 0;
    while (fl_temp1) {
        if (fl_temp1->is_dir && strcmp(fl_temp1->name, ".."))
            has_dirs = TRUE;

        (*entries)++;
        fl_temp1 = fl_temp1->next;
    }

    for (n = 0; n < JUMP_STACK; n++)
        *(jump_stack + n) = -1;

    *jump_stack_pos = 0;

    if (use_jump) {
        for (n = 0; n < JUMP_STACK; n++)
            newest_time[n] = 0;

        // now determine the X newest dirs/files (used for cycle-through-newest)
        fl_temp1 = fl_temp;
        while (fl_temp1) {
            if (fl_temp1->is_dir == has_dirs) {
                if (FilterFilename(fl_temp1->name, server->GetFilter())) {
                    elapsed_time = fl_temp1->time;

                    // now see if we got a newer entry
                    found = FALSE;
                    n = 0;
                    while (!found && (n < JUMP_STACK)) {
                        if (elapsed_time >= newest_time[n])
                            found = TRUE;
                        else
                            n++;
                    }

                    if (found) {
                        // okiez, insert this one as 'n'
                        for (m = JUMP_STACK - 1; m > n; m--) {
                            newest_time[m] = newest_time[m - 1];
                            *(jump_stack + m) = *(jump_stack + m - 1);
                        }

                        newest_time[n] = elapsed_time;
                        *(jump_stack + n) = fl_temp1->magic;
                    }
                }
            }
            fl_temp1 = fl_temp1->next;
        }

        if (*jump_stack != -1)
            *magic = *jump_stack;
        else
            *magic = 0;
    } else
        *magic = 0;

    this->UpdateFilelistNewPosition(window);
    this->UpdateTabBar(TRUE);
}

void CDisplayHandler::ScrollStatusUp(void) {
    if ((this->status_line < (STATUS_LOG - this->status_win_size))
        && (this->statuslog[this->status_line + 1] != NULL))
        this->status_line += 1;

    this->DisplayStatusLog();
}

void CDisplayHandler::ScrollStatusDown(void) {
    if (this->status_line > 0)
        this->status_line -= 1;

    this->DisplayStatusLog();
}

void CDisplayHandler::DisplayStatusLog(void) {
    int n, ypos = this->status_win_size - 1;

    wattrset(this->window_status, COLOR_PAIR(STYLE_WHITE) | A_NORMAL);
    wbkgdset(this->window_status, ' ' | COLOR_PAIR(STYLE_WHITE));
    werase(this->window_status);
    //wbkgdset(this->window_status, ' ');

    for (n = 0; n < this->status_win_size; n++) {
        if (this->statuslog_highlight[this->status_line + n])
            wattrset(this->window_status,
                     COLOR_PAIR(STYLE_MARKED_STATUS) | A_BOLD);
        else
            wattrset(this->window_status,
                     COLOR_PAIR(STYLE_WHITE) | A_NORMAL);

        mvwaddnstr(this->window_status, ypos, 0,
                   this->statuslog[this->status_line + n],
                   this->terminal_max_x);
        ypos--;
    }

    //wnoutrefresh(this->window_status);
    update_panels();
    doupdate();
}

void CDisplayHandler::AddStatusLine(char *line, bool highlight) {
    char *new_line;
    int n;

    this->status_line = 0;
    new_line = new(char[strlen(line) + 1]);
    strcpy(new_line, line);

    // move log up
    delete(this->statuslog[STATUS_LOG - 1]);
    for (n = (STATUS_LOG - 2); n >= 0; n--) {
        this->statuslog[n + 1] = this->statuslog[n];
        this->statuslog_highlight[n + 1] = this->statuslog_highlight[n];
    }

    this->statuslog[0] = new_line;
    this->statuslog_highlight[0] = highlight;

    this->DisplayStatusLog();
}

