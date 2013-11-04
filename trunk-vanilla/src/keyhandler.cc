#include <string.h>
#include <curses.h>
#include <panel.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>

#include "defines.h"
#include "tcp.h"
#include "server.h"
#include "displayhandler.h"
#include "keyhandler.h"

extern CDisplayHandler *display;
extern SERVERLIST *global_server;
extern bool no_autologin;
extern char own_ip[256];
extern bool need_resize;

void FireupLocalFilesys(void);

CKeyHandler::CKeyHandler()
{
    this->want_quit = FALSE;
    this->bm_save = FALSE;
    this->want_local = FALSE;
}


CKeyHandler::~CKeyHandler()
{
}


bool CKeyHandler::Init(void)
{
    return (TRUE);
}


void CKeyHandler::PostToDisplay(int msg)
{
    display->PostMessage(msg);
}


void CKeyHandler::PostToDisplay(int msg, int extended)
{
    display->PostMessage(msg, extended);
}


void CKeyHandler::Loop(void)
{
    int keycode, esct = 0, slept = 0;

    if (display->ProbeBookmarkRC() == 0) {
        display->PostStatusLine("found bookmarks", FALSE);
        PostToDisplay(MSG_DISPLAY_PASSWORD);
    } else
        PostToDisplay(MSG_DISPLAY_NOBOOKMRK);

    do {
//              display->PostStatusLine("pred getch()", FALSE);
        if (need_resize) {
            sleep(1);
            continue;
        }
        keycode = getch();
//              display->PostStatusLine("po getch()", FALSE);
        switch (keycode) {
#ifdef KEY_RESIZE
//   display->need_resize=TRUE;
        case KEY_RESIZE:
            break;
#endif

        case KEY_LEFT:
            PostToDisplay(MSG_KEY_LEFT);
            break;

        case KEY_RIGHT:
            PostToDisplay(MSG_KEY_RIGHT);
            break;

        case KEY_UP:
            PostToDisplay(MSG_KEY_UP);
            break;

        case KEY_DOWN:
            PostToDisplay(MSG_KEY_DOWN);
            break;

        case KEY_HOME:
            PostToDisplay(MSG_KEY_HOME);
            break;

        case KEY_END:
            PostToDisplay(MSG_KEY_END);
            break;

        case KEY_NPAGE:
            PostToDisplay(MSG_KEY_PGUP);
            break;

        case KEY_PPAGE:
            PostToDisplay(MSG_KEY_PGDN);
            break;

        case 13:
            PostToDisplay(MSG_KEY_RETURN);
            break;

        case 9:
            PostToDisplay(MSG_KEY_TAB);
            break;

        case 21:
            PostToDisplay(MSG_KEY_DELLINE);
            break;

        case 8:
        case KEY_BACKSPACE:
            PostToDisplay(MSG_KEY_BACKSPACE);
            break;

        case 331:
            PostToDisplay(MSG_KEY_STATUS_UP);
            break;

        case 127:
        case 330:
            PostToDisplay(MSG_KEY_STATUS_DOWN);
            break;

        case -1:
            if (display->internal_state == DISPLAY_STATE_WELCOME) {
                slept++;
                if (slept == WELCOME_SLEEP)
                    PostToDisplay(MSG_KEY_RETURN);
            }
            break;
// escape handler is back :P
//bah, ncurses owns me
        case 27:
            keypad(stdscr, FALSE);
            keycode = getch();
            keypad(stdscr, TRUE);

//only one esc pressed and passed timeout
            if (keycode == -1) {
                esct++;
                if (esct < 30) {
                    ungetch(27);
//display->PostStatusLine("having fun with ncurses ?", FALSE);
//this makes us to be able to give esc-key commands
//longer time
                    usleep(100000);
                    break;
                }
                esct = 0;
                PostToDisplay(MSG_KEY_ESC);
                break;
            }
            esct = 0;
//esc 2x
            if (keycode == 27) {
                PostToDisplay(MSG_KEY_ESC);
                break;

            }
            PostToDisplay(MSG_KEY_ESCAPE, keycode);
/*
   //keycode = getch();
   switch (keycode) {
   case 54:
   PostToDisplay(MSG_KEY_PGUP);
   break;
   case 53:
   PostToDisplay(MSG_KEY_PGDN);
   break;
   default:

break;
}
*/
            break;

/*          case    27:
   PostToDisplay(MSG_KEY_ESC);
   break;
 */

        default:
            PostToDisplay(MSG_KEY_EXTENDED, keycode);
            break;
        }

// handle a few events
        if (this->want_local) {
            this->want_local = FALSE;
            FireupLocalFilesys();
        }
    } while (!this->want_quit);

    display->PostStatusLine("shutting down pftp II...", TRUE);

    if (this->bm_save) {
        display->PostStatusLine("saving bookmarks...", TRUE);
        display->SaveBookmarks();
    }
    this->ShutdownServers();
}


void CKeyHandler::ShutdownServers(void)
{
    SERVERLIST *sv_temp = global_server, *sv_temp1;

// first kill the threads
    while (sv_temp) {
        pthread_cancel(sv_temp->server->GetThread());
        pthread_join(sv_temp->server->GetThread(), NULL);
        sv_temp = sv_temp->next;
    }

// now free memory and nuke object
    sv_temp = global_server;
    while (sv_temp) {
        sv_temp1 = sv_temp;
        sv_temp = sv_temp->next;
        delete(sv_temp1->server);
        delete(sv_temp1);
    }
}
