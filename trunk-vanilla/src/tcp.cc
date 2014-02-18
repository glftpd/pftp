#include <assert.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curses.h>
#include <panel.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <net/if.h>
#include <errno.h>

#include "defines.h"
#include "tcp.h"

#ifdef TLS
#include "tlsutil.h"
//#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
//#include <openssl/x509v3.h>
extern SSL_CTX *ssl_ctx;
#endif

#ifndef O_BINARY
#define O_BINARY 0
/* O_BINARY */
#endif

//extern        bool use_own_ip;

#ifdef RHSUX
extern int errno;
#endif

extern char okay_dir[1024];
extern void StripANSI(char *line);

char own_ip[256];

bool DetermineOwnIP(char *device)
{
    static struct ifreq ifr;
    int s;

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return (FALSE);

    strcpy(ifr.ifr_name, device);
    ifr.ifr_addr.sa_family = AF_INET;
    if (ioctl(s, SIOCGIFADDR, &ifr) < 0)
        return (FALSE);

    strcpy(own_ip,
           inet_ntoa(((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr));

    close(s);
    return (TRUE);
}


CTCP::CTCP()
{
    int n;

    this->speed = 0.0;
    this->site_msg = FALSE;
    
    for (n = 0; n < MSG_STACK; n++)
        this->msg_stack[n] = -1;

    for (n = 0; n < LOG_LINES; n++)
        this->log[n] = NULL;
    this->reply_line[0] = '\0';
    this->already_read = 0;
    this->control_connected = FALSE;
    this->haveIP = FALSE;
    this->in_xdupe = FALSE;
    this->have_accepted = FALSE;
#ifdef TLS

    this->ctrl_con = NULL;
    this->data_con = NULL;
    prot = 0;
#endif

    this->xdupehead = NULL;
    this->stealth = FALSE;
    this->in_stealth = FALSE;
    this->stealth_fd = NULL;

    pthread_mutex_init(&(this->log_lock), NULL);
}


CTCP::~CTCP()
{
    XDUPELIST *xtmp;
    int n;

    this->in_xdupe = FALSE;

    for (n = LOG_LINES - 1; n >= 0; n--)
        delete[](log[n]);

    while (xdupehead != NULL) {
        xtmp = this->xdupehead;
        this->xdupehead = xtmp->next;
        delete[](xtmp->name);
        delete(xtmp);
    }
}


void
 CTCP::CloseControl(void)
{
    if (this->control_connected) {
#ifdef TLS
        if (this->ctrl_con) {
            SSL_shutdown(this->ctrl_con);
            SSL_free(this->ctrl_con);
            this->ctrl_con = NULL;
        }
#endif
        close(this->control_sock_fd);
        this->control_connected = FALSE;
    }
}


bool CTCP::GetIP(char *address, struct in_addr *ip_address)
{
    struct hostent *host_ent;

// first try dotted style
    if (inet_aton(address, ip_address)) {
        return (TRUE);
    } else {
// use DNS lookup
        if ((host_ent = gethostbyname(address)) == NULL) {
            this->error = E_CTRL_ADDRESS_RESOLVE;
            return (FALSE);
        }

        bcopy(host_ent->h_addr, (char *) ip_address, host_ent->h_length);
    }

    return (TRUE);
}


bool CTCP::OpenControl(BOOKMARK * bm)
{
    struct sockaddr_in socket_in;
    struct in_addr ip_address;
    struct sockaddr_in sourcedef;
    char tempstr[1024], *tempptr, *tempptr1;
    char *port;

// open control socket (telnet)
    socket_in.sin_family = AF_INET;

    strncpy(tempstr, bm->host, sizeof(tempstr));
    tempptr1 = tempstr;
    tempptr = strsep(&tempptr1, " ");
    while (tempptr) {
        if (strlen(tempptr) == 0) {
            tempptr = strsep(&tempptr1, " ");
            continue;
        }
        if ((port = strrchr(tempptr, ':')) != NULL) {
            *port = '\0';
            port++;
            socket_in.sin_port = htons(atol(port));
        } else {
            socket_in.sin_port = htons(21);
        }

        if (!this->GetIP(tempptr, &ip_address)) {
            tempptr = strsep(&tempptr1, " ");
            continue;
        }
        socket_in.sin_addr = ip_address;

        if ((this->control_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            this->error = E_CTRL_SOCKET_CREATE;
            return (FALSE);
        }
        memset(&sourcedef, '\0', sizeof(struct sockaddr_in));
        sourcedef.sin_addr.s_addr = inet_addr(own_ip);
        sourcedef.sin_family = AF_INET;
        sourcedef.sin_port = 0;

        if (bind
            (this->control_sock_fd, (struct sockaddr *) &sourcedef,
             sizeof(struct sockaddr_in)) < 0) {
            close(this->control_sock_fd);
            this->error = E_SOCKET_BIND;
            return (FALSE);
        }
        if (connect
            (this->control_sock_fd, (struct sockaddr *) &socket_in,
             sizeof(struct sockaddr))) {
            close(this->control_sock_fd);
            tempptr = strsep(&tempptr1, " ");
            continue;
        } else {
            break;
        }
//while
    }
    if (tempptr == NULL) {
        this->error = E_CTRL_SOCKET_CONNECT;
        return (FALSE);
    }
//nonblock sounds good
    fcntl(this->control_sock_fd, F_SETFL, O_NONBLOCK);

    this->control_connected = TRUE;
    return (TRUE);

}


#ifdef TLS
bool CTCP::SecureControl(void)
{
    int err;
    char *temp;
    if (!this->control_connected)
        return FALSE;

    if (this->ctrl_con) {
//      fprintf(ttyout, "Already TLS connected!\r\n");
        return FALSE;
    }
    this->ctrl_con = SSL_new(ssl_ctx);
    if (!this->ctrl_con) {
//      fprintf(ttyout, "SSL_new() %s\r\n",
//              (char *)ERR_error_string(ERR_get_error(), NULL));
        return FALSE;
    }
//  SSL_set_cipher_list(this->ctrl_con, tls_cipher_list);
    SSL_set_fd(this->ctrl_con, this->control_sock_fd);
/* we do not use client cert in pftp yet */

    err = SSL_connect(this->ctrl_con);

/* this took me 10 hours to find out :) */
//nonblock sux
    while (err <= 0) {
        int errs;
        pthread_testcancel();
        errs = SSL_get_error(this->ctrl_con, err);
        if ((errs != SSL_ERROR_WANT_READ) && (errs != SSL_ERROR_WANT_WRITE)
            && (errs != SSL_ERROR_WANT_X509_LOOKUP))
            return FALSE;
//        usleep(1000);
        err = SSL_connect(this->ctrl_con);
    }

    if (err == 1) {
        if ((temp = tls_get_subject_name(this->ctrl_con))) {
            sprintf(this->temp_string, "[Ctrl - Subject: %s]", temp);
            this->AddLogLine(this->temp_string);
        }
        if ((temp = tls_get_issuer_name(this->ctrl_con)))
            sprintf(this->temp_string, "[Ctrl - Issuer:  %s]", temp);
        this->AddLogLine(this->temp_string);

        sprintf(this->temp_string, "[Ctrl - Cipher: %s %s (%d bits)]",
		SSL_get_cipher_version(ctrl_con),
                SSL_get_cipher(this->ctrl_con),
                SSL_get_cipher_bits(this->ctrl_con, NULL));
        this->AddLogLine(this->temp_string);

        if (!(temp = tls_get_commonName(this->ctrl_con)))
            temp = "Not even a commonName!";
        sprintf(this->temp_string, "[Ctrl - Common Name: %s]", temp);
        this->AddLogLine(this->temp_string);

        return TRUE;
/* TLS connection failed */
    } else {
/*      fprintf(ttyout, "SSL_connect() = %d, %s\r\n", err,
   (char *)ERR_error_string(ERR_get_error(), NULL));
         */
        SSL_shutdown(this->ctrl_con);
        SSL_free(this->ctrl_con);
        this->ctrl_con = NULL;
        return FALSE;
    }
}


bool CTCP::SecureData(void)
{
    int err;

    this->data_con = SSL_new(ssl_ctx);
    if (!this->data_con) {
//      fprintf(ttyout, "SSL_new() %s\r\n",
//              (char *)ERR_error_string(ERR_get_error(), NULL));
        return FALSE;
    }
//  SSL_set_cipher_list(this->ctrl_con, tls_cipher_list);
    SSL_set_fd(this->data_con, this->real_data_sock_fd);
//this will speed up ssl negotiation !
    SSL_copy_session_id(this->data_con, this->ctrl_con);

/* we do not use client cert in pftp yet */
    err = SSL_connect(this->data_con);

/* this took me 10 hours to find out :) */
    while (err <= 0) {
        int errs;
        pthread_testcancel();
        errs = SSL_get_error(this->data_con, err);
        if ((errs != SSL_ERROR_WANT_READ) && (errs != SSL_ERROR_WANT_WRITE)
            && (errs != SSL_ERROR_WANT_X509_LOOKUP)) {
            char errbuf[120];
            ERR_error_string_n(ERR_get_error(), errbuf, 120);
            sprintf(this->temp_string, "SSL Error : (%d) %s", errs, errbuf);
            AddLogLine(this->temp_string);

            SSL_shutdown(this->data_con);
            SSL_free(this->data_con);
            this->data_con = NULL;
            return FALSE;
        }
//        usleep(1000);
        err = SSL_connect(this->data_con);
    }

    if (err == 1) {
        char* temp;
	if ((temp = tls_get_subject_name(this->data_con))) {
            sprintf(this->temp_string, "[Data - Subject: %s]", temp);
            this->AddLogLine(this->temp_string);
        }
        if ((temp = tls_get_issuer_name(this->data_con)))
            sprintf(this->temp_string, "[Data - Issuer:  %s]", temp);
        this->AddLogLine(this->temp_string);

        sprintf(this->temp_string, "[Data - Cipher: %s %s (%d bits)]",
                SSL_get_cipher_version(data_con),
                SSL_get_cipher(this->data_con),
                SSL_get_cipher_bits(this->data_con, NULL));
        this->AddLogLine(this->temp_string);

        if (!(temp = tls_get_commonName(this->data_con)))
            temp = "Not even a commonName!";
        sprintf(this->temp_string, "[Data - Common Name: %s]", temp);
        this->AddLogLine(this->temp_string);

        sprintf(this->temp_string, "[Data - Handshake reused : %s]", SSL_session_reused(data_con) ? "yes" : "no");
        this->AddLogLine(this->temp_string);
        
        return TRUE;
/* TLS connection failed */
    } else {
        char errbuf[120];
        ERR_error_string_n(ERR_get_error(), errbuf, 120);
        sprintf(this->temp_string, "SSL Error : %s", errbuf);  
        AddLogLine(this->temp_string);
        
        SSL_shutdown(this->data_con);
        SSL_free(this->data_con);
        this->data_con = NULL;
        return FALSE;
    }
}
#endif

bool CTCP::SendData(char *msg)
{
#ifdef TLS
    int err;
#endif

    if (this->control_connected) {
#ifdef TLS
        if (this->ctrl_con)
            err = SSL_write(this->ctrl_con, msg, strlen(msg));
        else
            err = write(this->control_sock_fd, msg, strlen(msg));

        if (err != -1) {
#else
        if (write(this->control_sock_fd, msg, strlen(msg)) != -1) {
#endif
                sprintf(this->temp_string, ">>> %s", msg);
                this->AddLogLine(this->temp_string);
            return (TRUE);
        } else {
//WTF how can it be not connected ?
//            this->control_connected = FALSE;
            return (FALSE);
        }
    }
    return (FALSE);
}

int CTCP::SearchStack(void)
{
    int n;

    if (this->msg_stack[0] == -1)
// stack empty, load new stack
        return (1);

    if (((this->msg_stack[0] >= 200) && (this->msg_stack[0] < 300))
        || (this->msg_stack[0] == 125) || (this->msg_stack[0] == 331)
        || (this->msg_stack[0] == 350) || (this->msg_stack[0] == 150)) {
// *pheew* was the correct msg
        for (n = 1; n < MSG_STACK; n++)
            this->msg_stack[n - 1] = this->msg_stack[n];

        this->msg_stack[MSG_STACK - 1] = -1;
// found msg, removed
        return (2);
    }
// bad luck, the wrong msg is on the stack...
    return (0);
}

void CTCP::ObtainLog(char *log_out[LOG_LINES])
{
    int n;

    pthread_mutex_lock(&(this->log_lock));

    for (n = 0; n < LOG_LINES; n++) {
        if (this->log[n]) {
            log_out[n] = new char[strlen(this->log[n]) + 1];
            strcpy(log_out[n], this->log[n]);
        } else
            log_out[n] = NULL;
    }

    pthread_mutex_unlock(&(this->log_lock));
}

void CTCP::AddLogLine(char *line)
{
    int n;
    time_t timenow = time(NULL);
    struct tm *ptr;

    pthread_mutex_lock(&(this->log_lock));

    debuglog("SERVER LOG LINE : %s", line);

    if (this->log[LOG_LINES - 1]) {
        delete[](this->log[LOG_LINES - 1]);
        this->log[LOG_LINES - 1] = NULL;
    }
// "scroll" lines
    for (n = LOG_LINES - 1; n > 0; n--)
        this->log[n] = this->log[n - 1];

// add line
    this->log[0] = new char[strlen(line) + 1 + 6];
    ptr = (struct tm *) localtime(&timenow);
    strftime(this->log[0], 7, "%H:%M ", ptr);

//      sprintf(this->log[0],"[%02d:%02d] ",tm.tm_hour,time1.tm_mins);
    strcpy(this->log[0] + 6, line);

    pthread_mutex_unlock(&(this->log_lock));
}

void CTCP::PrepareXDupe()
{
    assert(this->in_xdupe == FALSE);
    this->in_xdupe = TRUE;
    assert(this->xdupehead == NULL);
}

XDUPELIST *CTCP::GetXDupe()
{
    return (this->xdupehead);
}

void CTCP::StopXDupe()
{
    XDUPELIST *xtmp;
    while (xdupehead != NULL) {
        xtmp = this->xdupehead;
        this->xdupehead = xtmp->next;
        delete[](xtmp->name);
        delete(xtmp);
    }
    this->in_xdupe = FALSE;
}

void CTCP::UpdateStack()
{
    char *start = this->control_buffer, *end, temp_stack[5];
    char *tmp;
    int stack_cnt = 0, msg_nr;
    XDUPELIST *xtmp;

// analyze this->control_buffer and put entries in stack
// hack!
    while ((end = strstr(start, "\r\n"))) {
        *end = '\0';

//ok we got line, remove all bad /n
        for (tmp = start; tmp != end; tmp++)
            if (*tmp == '\n')
                *tmp = ' ';
//ok we got line, remove all bad /r too
        for (tmp = start; tmp != end; tmp++)
            if (*tmp == '\r')
                *tmp = ' ';
        StripANSI(start);

// stealth
        if (this->in_stealth) {
            if (!strncmp(start, "211 ", 4)) {
                this->in_stealth = FALSE;
                fclose(this->stealth_fd);
            } else if (!strncmp(start, "212 ", 4)) {
                this->in_stealth = FALSE;
                fclose(this->stealth_fd);
            } else if (!strncmp(start, "213 ", 4)) {
                this->in_stealth = FALSE;
                fclose(this->stealth_fd);
            } else {
                fprintf(this->stealth_fd, "%s\n", start);
                debuglog("stealth - %s", start);
                start = end + 2;
                continue;
            }
        }

        if (strlen(start) < 4) {
//wow what a weird reply we got lets ignore it
            start = end + 2;
            continue;
        }

        if (!this->in_stealth) {
            this->AddLogLine(start);
        }
// can we use this msg?
        if (*(start + 3) == ' ') {
            strncpy(temp_stack, start, 3);
            temp_stack[3] = '\0';
            msg_nr = atoi(temp_stack);
// i hate zipcheckers
            if (msg_nr > 0) {
                this->msg_stack[stack_cnt] = msg_nr;
                strncpy(reply_line, start, 256);
                reply_line[255] = '\0';
                stack_cnt++;
            }
        } else if (strstr(start,"You have messages to read") != NULL) {
                   this->site_msg = TRUE;
        }

        if (this->in_xdupe) {
            if (!strncmp(start, "553- X-DUPE: ", 13)) {
                xtmp = new(XDUPELIST);
                xtmp->name = new char[strlen(start) - 13 + 1];
                strcpy(xtmp->name, start + 13);
                xtmp->next = this->xdupehead;
                this->xdupehead = xtmp;
            }
        }
// stealth
        if (this->stealth) {
            if (!strncmp(start, "211-", 4)) {
                this->in_stealth = TRUE;
                this->stealth_fd = fopen(this->stealth_file, "w");
                this->AddLogLine("dirlist was here");
            } else if (!strncmp(start, "212-", 4)) {
                this->in_stealth = TRUE;
                this->stealth_fd = fopen(this->stealth_file, "w");
                this->AddLogLine("dirlist was here");
            } else if (!strncmp(start, "213-", 4)) {
                this->in_stealth = TRUE;
                this->stealth_fd = fopen(this->stealth_file, "w");
                this->AddLogLine("dirlist was here");
            } else if (!strncmp(start, "total ", 6)) {
                this->in_stealth = TRUE;
                this->stealth_fd = fopen(this->stealth_file, "w");
                this->AddLogLine("dirlist was here");
                fprintf(this->stealth_fd, "%s\n", start);
            }
        }
        start = end + 2;
    }
//ok we parsed all lines, lets move the rest of buffer to front
//and fix already_read
    if (*start != '\0') {
//our temp buff is only 2048 bytes, lets make sure i doesnt overflow here
//if it does we will have to make it bigger or someone will have to fix
//his idiotic ftpd
        assert(strlen(start) < 2048);
        strcpy(temp_string, start);
        strcpy(this->control_buffer, temp_string);
        already_read = strlen(this->control_buffer);
        debuglog("in control buffer left = %d bytes = \"%s\"",
                 already_read, this->control_buffer);
    } else {
        already_read = 0;
        debuglog("in control buffer left = %d bytes", already_read);
    }
}

void CTCP::FlushStack(void)
{
    this->msg_stack[0] = -1;
    this->FlushSocket();
}

//returns 0=no data so far
//        1=fatal error
//        2=something was actually read
int CTCP::WaitForDataAndRead(int already_read, int *len1)
{
    int len, n;
    int sock_fd = this->control_sock_fd;

//soo, first we try to read some data, if nothing then we check error
//and if really nothing then select() until data comes OR until timeout
//when data comes we read again

  again:
    pthread_testcancel();

#ifdef TLS
    if (this->ctrl_con) {
        len =
            SSL_read(this->ctrl_con,
                     this->control_buffer + already_read,
                     CONTROL_BUFFER_SIZE - already_read);
    } else
#endif
        len =
            read(sock_fd, this->control_buffer + already_read,
                 CONTROL_BUFFER_SIZE - already_read);
    if (len == 0) {
//means connection was closed
        return 1;
    }
    if (len == -1) {
#ifdef TLS
        if (ctrl_con) {
//ssl
            int err = SSL_get_error(ctrl_con, len);
            switch (err) {
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_X509_LOOKUP:
//means we should try ssl_read again
                return 0;
                break;
            case SSL_ERROR_WANT_READ:
//good this is EAGAIN :)
                break;
            default:
                return 1;
//fatal
                break;
            }
        } else
#endif
        {
            if (errno != EAGAIN) {
                return 1;
//FATAL ERROR
            }
        }
        debuglog("wait for data and read will now select()");
//ok non fatal error lets do the trick
/* timeout = 1 second */
        fd_set rfds;
        struct timeval tv;

        FD_ZERO(&rfds);
        FD_SET(sock_fd, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        n = select(sock_fd + 1, &rfds, NULL, NULL, &tv);
        if (n > 0)
            goto again;
        if (n == 0)
            return 0;
//fatal
        return 1;
    } else {
//wow we recieved SOMETHING
        debuglog
            ("wait for data and read received %d bytes, already read was %d",
             len, already_read);
        *len1 = len;
        return (2);
    }
    return (0);
}

bool CTCP::WaitForMessage(void)
{
    return (this->WaitForMessage(CONTROL_TIMEOUT));
}

bool CTCP::WaitForMessage(int timeout)
{
    int read_length, okay, time = 0;
    char *buffer_pos, *null_ptr;
    bool full_lines;

    okay = this->SearchStack();

    if (okay == 2) {
// found a positive msg in stack and removed it, okay
        return (TRUE);
    } else if (okay == 0) {
// bad luck, we received a negative message
        this->FlushStack();
        return (FALSE);
    }
// there is no message on the stack
    do {
        full_lines = FALSE;
// wait until one or more full lines are received, at least one
        do {
            int result;

//check for full buffer
            debuglog("in control buffer is = %d bytes", already_read);
//10 bytes of 64kb buffer
            if (CONTROL_BUFFER_SIZE - already_read <= 10) {
//we have full buffer, this means pretty long line -> buggy server
//workaround=lets play that we didnt read anything yet
                debuglog("wait for message had overflow in buffer");
                already_read = 0;
            }

            pthread_testcancel();
            while (((result =
                     this->WaitForDataAndRead(already_read,
                                              &read_length)) == 0)
                   && ((timeout == 0) || (time < timeout))) {
                pthread_testcancel();
                time++;
            }
            if (result == 1) {
                this->error = E_CONTROL_RESET;
                return (FALSE);
            }
            if ((timeout != 0) && (time >= timeout)) {
                this->error = E_CTRL_TIMEOUT;
                already_read = 0;
                return (FALSE);
            }

            time = 0;

            buffer_pos = this->control_buffer + already_read;

// remove all occurances of '\0' b4 the real end
            null_ptr = buffer_pos;
            while (null_ptr < (buffer_pos + read_length)) {
                if (*null_ptr == '\0')
                    *null_ptr = ' ';
                null_ptr++;
            }

//lets try this way
            char *c = control_buffer;
            while (c < (buffer_pos + read_length - 1)) {
                if (*c == '\r') {
                    if (*(c + 1) == '\n') {
                        full_lines = TRUE;
                        break;
                    }
                }
                c++;
            }

            already_read += read_length;

        } while (!full_lines);

        *(buffer_pos + read_length) = '\0';

// add possible msg's to stack
        this->UpdateStack();
        okay = this->SearchStack();

// found positive end-of-output msg
        if (okay == 2) {
            return (TRUE);
// stack contained a negative msg
        } else if (okay == 0) {
// recover
            this->FlushStack();
            this->error = E_BAD_MSG;
            return (FALSE);
        }
// the stack hadn't yet the end-of-output msg, give it another try (strange implementations of some servers)
        pthread_testcancel();
    } while (TRUE);
}

void CTCP::FlushSocket(void)
{
    int len;
#ifdef TLS

    if (this->ctrl_con) {
        do {
            pthread_testcancel();
            len =
                SSL_read(this->ctrl_con, this->control_buffer,
                         CONTROL_BUFFER_SIZE);
        } while (len > 0);
    } else {
        do {
            pthread_testcancel();
            len =
                read(this->control_sock_fd, this->control_buffer,
                     CONTROL_BUFFER_SIZE);
        } while (len > 0);
    }
#else
    do {
        pthread_testcancel();
        len =
            read(this->control_sock_fd, this->control_buffer,
                 CONTROL_BUFFER_SIZE);
    } while (len > 0);
#endif
//well we flushed it
    this->already_read=0;
    this->control_buffer[0] = '\0';
}

bool CTCP::OpenDataPASV(char *pasv_str)
{
    int pasv_resp[6];
    int port;
    char host[256];
    struct sockaddr_in socket_in;
    struct in_addr ip_address;
    struct sockaddr_in sourcedef;

/* Parse the PASV string */

    for (port = 0; port < 6; port++)
        pasv_resp[port] = port;

    sscanf(pasv_str, "%d,%d,%d,%d,%d,%d", &pasv_resp[0], &pasv_resp[1],
           &pasv_resp[2], &pasv_resp[3], &pasv_resp[4], &pasv_resp[5]);
/*todo : error check */
    port = (pasv_resp[4] * 256) + pasv_resp[5];
    sprintf(host, "%d.%d.%d.%d", pasv_resp[0], pasv_resp[1], pasv_resp[2],
            pasv_resp[3]);

    socket_in.sin_family = AF_INET;
    socket_in.sin_port = htons(port);

    if (!this->GetIP(host, &ip_address))
        return (FALSE);

    socket_in.sin_addr = ip_address;

    if ((this->real_data_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        this->error = E_SOCKET_DATA_CREATE;
        return (FALSE);
    }

    memset(&sourcedef, '\0', sizeof(struct sockaddr_in));
    sourcedef.sin_addr.s_addr = inet_addr(own_ip);
    sourcedef.sin_family = AF_INET;
    sourcedef.sin_port = 0;

    if (bind
        (this->real_data_sock_fd, (struct sockaddr *) &sourcedef,
         sizeof(struct sockaddr_in)) < 0) {
        close(this->real_data_sock_fd);
        this->error = E_SOCKET_BIND;
        return (FALSE);
    }

    if (connect
        (this->real_data_sock_fd, (struct sockaddr *) &socket_in,
         sizeof(struct sockaddr))) {
        close(this->real_data_sock_fd);
/* fixme */
        this->error = E_DATA_TIMEOUT;
        return (FALSE);
    }
// now we avoid select() calls since they got useless with this multithreading model
// but we need timing information to not confuse our user, so... nonblocking
    fcntl(this->real_data_sock_fd, F_SETFL, O_NONBLOCK);
    this->have_accepted = TRUE;

    return (TRUE);

}

bool CTCP::OpenData(char *port_str, bool use_local)
{
/**start,*/
    char myhost_str[256];
    struct in_addr ip_address;

    socklen_t sockaddr_len = sizeof(struct sockaddr_in);

    if ((this->data_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        this->error = E_SOCKET_DATA_CREATE;
        return (FALSE);
    }

    if (!this->haveIP) {
/*              if(!use_own_ip)
   gethostname(myhost_str, 256);
   else
         */
        strcpy(myhost_str, own_ip);

        if (!this->GetIP(myhost_str, &ip_address))
            return (FALSE);

        this->haveIP = TRUE;
        bcopy((char *) &ip_address, (char *) &(this->stored_ip_address),
              sizeof(struct in_addr));
// since the damn lookups lock when asking them fast (????) we have to overcome this
    } else {
        bcopy((char *) &(this->stored_ip_address), (char *) &ip_address,
              sizeof(struct in_addr));
    }

//      this->data_sock_in.sin_addr.s_addr = INADDR_ANY;
    memset(&this->data_sock_in, '\0', sizeof(struct sockaddr_in));
    this->data_sock_in.sin_addr.s_addr = inet_addr(own_ip);
    this->data_sock_in.sin_family = AF_INET;
    this->data_sock_in.sin_port = 0;

    if (bind
        (this->data_sock_fd, (struct sockaddr *) &data_sock_in,
         sizeof(struct sockaddr_in)) < 0) {
        close(this->data_sock_fd);
        this->error = E_SOCKET_BIND;
        return (FALSE);
    }

    if (getsockname
        (this->data_sock_fd, (struct sockaddr *) &data_sock_in,
         &sockaddr_len) < 0) {
        close(this->data_sock_fd);
        this->error = E_SOCKET_BIND;
        return (FALSE);
    }

    this->data_port = ntohs(this->data_sock_in.sin_port);

    if (listen(this->data_sock_fd, 1) < 0) {
        close(this->data_sock_fd);
        this->error = E_SOCKET_LISTEN;
        return (FALSE);
    }
    strcpy(myhost_str, own_ip);

    *(strchr(myhost_str, '.')) = ',';
    *(strchr(myhost_str, '.')) = ',';
    *(strchr(myhost_str, '.')) = ',';

    strcpy(port_str, myhost_str);

    if (use_local)
        strcpy(port_str, "127,0,0,1");

    sprintf(myhost_str, "%d,%d", this->data_port / 256,
            this->data_port - this->data_port / 256 * 256);
    strcat(port_str, ",");
    strcat(port_str, myhost_str);

// now we avoid select() calls since they got useless with this multithreading model
// but we need timing information to not confuse our user, so... nonblocking
    fcntl(this->data_sock_fd, F_SETFL, O_NONBLOCK);

    return (TRUE);
}

bool CTCP::AcceptData()
{
    socklen_t sockaddr_len = sizeof(struct sockaddr_in);
    int n = 0;

    if (!this->have_accepted) {

// give em 400 chances during 20 seconds
        do {
            pthread_testcancel();
            this->real_data_sock_fd =
                accept(this->data_sock_fd,
                       (struct sockaddr *) &(this->data_sock_in),
                       &sockaddr_len);
            n++;
            if (this->real_data_sock_fd == -1)
// 0.05 secs
                usleep(50000);
        } while ((this->real_data_sock_fd == -1) && (n < 400));

        if (this->real_data_sock_fd == -1) {
            close(this->data_sock_fd);
            this->error = E_SOCKET_ACCEPT_TIMEOUT;
            return (FALSE);
        }

        close(this->data_sock_fd);

        if (this->real_data_sock_fd < 0) {
            this->error = E_SOCKET_ACCEPT;
            return (FALSE);
        }

        fcntl(this->real_data_sock_fd, F_SETFL, O_NONBLOCK);

        this->have_accepted = TRUE;
    }
#ifdef TLS
    if (prot) {
        if (SecureData() == FALSE) {
            this->have_accepted = FALSE;
            this->error = E_SOCKET_ACCEPT;
            return (FALSE);
        }
    }
#endif

    return (TRUE);
}

void CTCP::CloseData(void)
{
// this fuck took 2 weeks of my life, a small bug... grrrrrrrrrrrrr
    if (this->have_accepted) {
        this->have_accepted = FALSE;
#ifdef TLS
        if (this->data_con) {
            SSL_shutdown(this->data_con);
            SSL_free(this->data_con);
            this->data_con = NULL;
        }
#endif
        close(this->real_data_sock_fd);
    } else
        close(this->data_sock_fd);
}

bool CTCP::WriteFile(char *read_name, bool no_ok)
{
    char error_file[512], ok_file[512];
    int block_wait = 0, write_length, infile, try_file =
        0, sent, already_sent;
    bool found = FALSE, error = FALSE, exists, finished = FALSE;
    struct stat stat_out;

    this->size = 0;
    sprintf(error_file, "%s%s.error", okay_dir,
            strrchr(read_name, '/') + 1);
    sprintf(ok_file, "%s%s.okay", okay_dir, strrchr(read_name, '/') + 1);

    do {
        pthread_testcancel();
        if (stat(read_name, &stat_out) == -1) {
// 0.03
            usleep(30000);
            try_file++;
        } else
            found = TRUE;
    } while (!found && (try_file < 1200));

    if (!found) {
        this->error = E_WRITEFILE_TIMEOUT;
        return (FALSE);
    }

    if ((infile = open(read_name, O_RDONLY | O_BINARY)) == -1) {
        this->error = E_WRITEFILE_ERROR;
        return (FALSE);
    }
// start uploading
    gettimeofday(&tv_before, NULL);
    exists = no_ok;
    do {
// check if we already have our STOP-files
        pthread_testcancel();
        if (!exists) {
            if (!stat(error_file, &stat_out) || !stat(ok_file, &stat_out))
                exists = TRUE;
        }
// try to push max
        write_length = read(infile, control_buffer, WRITE_SIZE);

        if (write_length > 0) {
            this->size += write_length;
// we have more bytes again, push em
            already_sent = 0;

            do {
                pthread_testcancel();
#ifdef TLS
                if (data_con)
                    sent = SSL_write(this->data_con,
                                     control_buffer + already_sent,
                                     write_length - already_sent);
                else
#endif

                    sent = write(this->real_data_sock_fd,
                                 control_buffer + already_sent,
                                 write_length - already_sent);
                if (sent < 0) {
#ifdef TLS
                    if (data_con) {
                        switch (SSL_get_error(data_con, sent)) {
                        case SSL_ERROR_WANT_READ:
                        case SSL_ERROR_WANT_WRITE:
                            errno = EAGAIN;
                            break;
                        default:
//ugly i know
                            errno = EAGAIN + 1;
                            break;
                        }
                    }
#endif
                    if (errno != EAGAIN)
                        error = TRUE;
                    else {
// blocking
                        if (block_wait < (DATA_TIMEOUT * 100)) {
// 0.01 secs
                            usleep(10000);
                            block_wait++;
                        } else
                            error = TRUE;
                    }
                } else {
                    already_sent += sent;
                    block_wait = 0;
                }
            } while (!error && (already_sent < write_length));

// calc transfer speed
// between tv_before and tv_after we transferred write_length-bytes
            gettimeofday(&tv_after, NULL);
            seconds = tv_after.tv_sec - tv_before.tv_sec;
            micros = tv_after.tv_usec - tv_before.tv_usec;
            micros += seconds * 1000000 + 1;
            this->speed =
                ((float) (this->size) / ((float) (micros) / 1000000.0)) /
                1024.0;

        } else {
// no more data available (at least not now)
            if (exists)
                finished = TRUE;
            else
// 0.02 secs
                usleep(20000);
        }
    } while (!error && !finished);

    this->speed = 0.0;

    close(infile);

    if (error) {
        if (block_wait >= (DATA_TIMEOUT * 100))
            this->error = E_DATA_TIMEOUT;
        else
            this->error = E_WRITEFILE_ERROR;

        return (FALSE);
    }

    return (TRUE);
}

bool CTCP::ReadFile(char *name, long size_min)
{
    int read_length = -1, outfile_fd, block_wait = 0;
    FILE *ok_file;
    bool write_error = FALSE, error = FALSE;

    this->size = 0;

    if ((outfile_fd =
         open(name, O_CREAT | O_TRUNC | O_RDWR | O_BINARY, S_IRUSR | S_IWUSR)) == -1) {
        this->error = E_BAD_LOCALFILE;
        return (FALSE);
    }

    fchmod(outfile_fd, S_IRUSR | S_IWUSR);

    gettimeofday(&tv_before, NULL);

    while (!write_error && !error && (read_length != 0)) {
        pthread_testcancel();
#ifdef TLS
        if (this->data_con)
            read_length =
                SSL_read(this->data_con, this->control_buffer, WRITE_SIZE);
        else
#endif

            read_length =
                read(this->real_data_sock_fd, this->control_buffer,
                     WRITE_SIZE);
        if (read_length > 0) {
// write data
            this->size += read_length;
            if (write(outfile_fd, this->control_buffer, read_length) !=
                read_length)
                write_error = TRUE;
// calc transfer speed
// between tv_before and tv_after we transferred read-bytes
            gettimeofday(&tv_after, NULL);
            seconds = tv_after.tv_sec - tv_before.tv_sec;
            micros = tv_after.tv_usec - tv_before.tv_usec;
            micros += seconds * 1000000 + 1;
            this->speed =
                ((float) (this->size) / ((float) (micros) / 1000000.0)) /
                1024.0;
            block_wait = 0;
        } else if (read_length == -1) {
// we had an error
#ifdef TLS
            if (data_con) {
                switch (SSL_get_error(data_con, read_length)) {
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_WANT_WRITE:
                    errno = EAGAIN;
                    break;
                default:
//ugly i know
                    errno = EAGAIN + 1;
                    break;
                }
            }
#endif
            if (errno != EAGAIN) {
                error = TRUE;
            } else {
                fd_set rfds;
                struct timeval tv;

                FD_ZERO(&rfds);
                FD_SET(real_data_sock_fd, &rfds);
                tv.tv_sec = 0;
                tv.tv_usec = 10000;
                int n =
                    select(real_data_sock_fd + 1, &rfds, NULL, NULL, &tv);
                if (n == 0) {
                    if (block_wait >= (DATA_TIMEOUT * 100)) {
                        error = TRUE;
                    } else
                        block_wait++;
                }
                if (n < 0)
                    error = TRUE;
            }
        }
    }

//      fsync(outfile_fd);
    close(outfile_fd);

// write error?
    if (write_error) {
        this->error = E_BAD_LOCALFILE;
        if (size_min > -1) {
            sprintf(this->temp_string, "%s%s%s", okay_dir,
                    strrchr(name, '/') + 1, ".error");
            ok_file = fopen(this->temp_string, "w");
            fclose(ok_file);
        }
        this->speed = 0.0;
        return (FALSE);
    }
// see if we have a read error
    if ((read_length < 0) || (this->size < size_min)) {
        if (block_wait >= (DATA_TIMEOUT * 100))
            this->error = E_DATA_TIMEOUT;
        else
            this->error = E_DATA_TCPERR;

        if (size_min > -1) {
            sprintf(this->temp_string, "%s%s%s", okay_dir,
                    strrchr(name, '/') + 1, ".error");
            ok_file = fopen(this->temp_string, "w");
            fclose(ok_file);
        }
        this->speed = 0.0;
        return (FALSE);
    }
// all was okay
    if (size_min > -1) {
        sprintf(this->temp_string, "%s%s%s", okay_dir,
                strrchr(name, '/') + 1, ".okay");
        ok_file = fopen(this->temp_string, "w");
        fclose(ok_file);
    }
    this->speed = 0.0;
    return (TRUE);
}

void break_handler(int sig)
{
//      signal(SIGINT, break_handler);

// fool -Wall
    sig = 0;

/*  if(tcp_active) {
   tcp_active = FALSE;
   DisplayStatus("User aborted operation, breaking TCP/IP graceless...", TRUE, TRUE);
   stop_me = TRUE;
   }
 */
}
