#include <string.h>
#include <curses.h>
#include <panel.h>
#include <pthread.h>

#include "defines.h"
#include "tcp.h"
#include "server.h"
#include "displayhandler.h"

extern SERVERLIST *global_server;

void
 CDisplayHandler::OpenSwitchWindow(int magic)
{
    SERVERLIST *sv_temp = global_server;

    this->window_switch =
        newwin(20, 61, this->terminal_max_y / 2 - 10,
               this->terminal_max_x / 2 - 30);
    this->panel_switch = new_panel(this->window_switch);
    leaveok(window_switch, TRUE);

    wattrset(this->window_switch, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_switch, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_switch);
//      wbkgdset(this->window_switch, ' ');
    mywborder(this->window_switch);
    mvwaddstr(this->window_switch, 0, 2, "select server");
    mvwaddstr(this->window_switch, 19, 2, "ENTER-select C-Kill");

    this->switch_pos = 0;
    this->switch_count = 0;
    while (sv_temp) {
        if (sv_temp->server->GetMagic() == magic) {
            this->switch_pos = this->switch_count;
        }
        this->switch_count++;
        sv_temp = sv_temp->next;
    }

    this->RedrawSwitchWindow();

}


void CDisplayHandler::ScrollSwitch(bool dir_up)
{
    bool redraw = FALSE;

    if (!dir_up) {
        if (this->switch_pos < (this->switch_count - 1)) {
            this->switch_pos++;
            redraw = TRUE;
        }
    } else {
        if (this->switch_pos > 0) {
            this->switch_pos--;
            redraw = TRUE;
        }
    }

    if (redraw)
        this->RedrawSwitchWindow();
}


void CDisplayHandler::PageMoveSwitch(bool dir_up)
{
    bool redraw = FALSE;

    if (dir_up) {
        if (this->switch_pos < (this->switch_count - 1)) {
            this->switch_pos += 9;
            if (this->switch_pos > (this->switch_count - 1))
                this->switch_pos = this->switch_count - 1;

            redraw = TRUE;
        }
    } else {
        if (this->switch_pos > 0) {
            this->switch_pos -= 9;
            if (this->switch_pos < 0)
                this->switch_pos = 0;

            redraw = TRUE;
        }
    }

    if (redraw)
        this->RedrawSwitchWindow();
}


void CDisplayHandler::PageMoveSwitchEnd(bool dir_up)
{
    bool redraw = FALSE;

    if (!dir_up) {
        if (this->switch_pos < (this->switch_count - 1)) {
//                      this->switch_pos += 9;
//                      if(this->switch_pos > (this->switch_count-1))
            this->switch_pos = this->switch_count - 1;

            redraw = TRUE;
        }
    } else {
        if (this->switch_pos > 0) {
//                      this->switch_pos -= 9;
//                      if(this->switch_pos < 0)
            this->switch_pos = 0;

            redraw = TRUE;
        }
    }

    if (redraw)
        this->RedrawSwitchWindow();
}


void CDisplayHandler::RedrawSwitchWindow(void)
{
    SERVERLIST *sv_temp = global_server;
    SERVERLIST *sv_temp2;
    char label[ALIAS_MAX + 1];
    int ypos, n = 0, magic, issrc;
    char cwd[SERVER_WORKINGDIR_SIZE];
    if (sv_temp) {
// see if we have to correct start
        if ((switch_start + 17) < switch_pos)
            switch_start = switch_pos - 17;
        else if (switch_start > switch_pos)
            switch_start = switch_pos;

// now walk to pos and from there display a maximum of 18 servers
        while (sv_temp && (n < this->switch_start)) {
            n++;
            sv_temp = sv_temp->next;
        }

        if (sv_temp) {
// k, now draw all entries
            ypos = 0;
            while (sv_temp && (ypos < 18)) {
                magic = sv_temp->server->GetMagic();
                if (n != this->switch_pos) {
                    if ((magic == this->leftwindow_magic)
                        || (magic == this->rightwindow_magic))
                        wattrset(this->window_switch,
                                 COLOR_PAIR(STYLE_MARKED) | A_BOLD);
                    else
                        wattrset(this->window_switch,
                                 COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
                } else {
                    if ((magic == this->leftwindow_magic)
                        || (magic == this->rightwindow_magic))
                        wattrset(this->window_switch,
                                 COLOR_PAIR(STYLE_MARKED_INVERSE) | A_BOLD
                                 | this->inverse_mono);
                    else
                        wattrset(this->window_switch,
                                 COLOR_PAIR(STYLE_INVERSE) | this->
                                 inverse_mono);
                }

                sprintf(this->temp_string,
                        "                                                           ");
                mvwaddnstr(this->window_switch, ypos + 1, 1,
                           this->temp_string, 59);
#ifdef TLS

                if (sv_temp->server->TLS_Status()) {
                    label[0] = '*';
                    strcpy(label + 1, sv_temp->server->GetAlias());
                } else
#endif

                    strcpy(label, sv_temp->server->GetAlias());
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
                    if (sv_temp2 == NULL) {
                        sprintf(temp_string, "%s", label);
                    } else {
                        if (issrc == 1) {
                            sprintf(temp_string, "%s [-> %s]", label,
                                    sv_temp2->server->GetAlias());
                        } else {
                            sprintf(temp_string, "%s [<- %s]", label,
                                    sv_temp2->server->GetAlias());
                        }
                    }
                } else {
                    sprintf(temp_string, "%s", label);
                }
                temp_string[strlen(temp_string)] = ' ';
                temp_string[19] = ' ';
                temp_string[20] = '\0';
                sv_temp->server->ObtainWorkingDir(cwd);
                if (strlen(cwd) < 40) {
                    strcat(this->temp_string, cwd);
//                                      this->temp_string[20 + strlen(cwd)] = ' ';
//                                      this->temp_string[59] = '\0';
                } else {
                    strcat(this->temp_string, "...");
                    strcat(this->temp_string, cwd + strlen(cwd) - 36);
                }
                mvwaddnstr(this->window_switch, ypos + 1, 1,
                           this->temp_string, 59);

                n++;
                ypos++;
                sv_temp = sv_temp->next;
            }

//wnoutrefresh(this->window_switch);
            update_panels();
            doupdate();
        }
    }
}


void CDisplayHandler::CloseSwitchWindow(void)
{
    del_panel(this->panel_switch);
    delwin(this->window_switch);
    this->panel_switch = NULL;
    this->window_switch = NULL;
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
