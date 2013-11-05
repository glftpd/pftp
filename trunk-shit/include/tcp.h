#ifndef __TCP_H
#define __TCP_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>

#ifdef TLS
#include <openssl/ssl.h>
#endif

#ifdef OSX
typedef int socklen_t;
#endif

#define MSG_STACK           200
#define CONTROL_BUFFER_SIZE 65536
#define WRITE_SIZE          65536
#define CONTROL_TIMEOUT     120
#define DATA_TIMEOUT        60

class CTCP {
    private:
        long already_read;
        int msg_stack[MSG_STACK];
        bool control_connected, have_accepted, haveIP, in_xdupe, site_msg;
        int error, control_sock_fd, data_sock_fd, real_data_sock_fd;
        float speed;
        char *log[LOG_LINES], control_buffer[CONTROL_BUFFER_SIZE + 1],
            temp_string[2048], stealth_file[256];
        char reply_line[256], site_msg_string[256];

        struct in_addr stored_ip_address;
        struct sockaddr_in data_sock_in;
        unsigned short int data_port;

        struct timeval tv_before, tv_after;
        long seconds, micros, size;
        XDUPELIST *xdupehead;
        bool stealth, in_stealth;
        FILE *stealth_fd;

#ifdef TLS
        SSL *ctrl_con;
        SSL *data_con;
        //SSL_CTX  *ssl_ctx = NULL;
        //X509_STORE *crl_store = NULL;
#endif

        pthread_mutex_t log_lock;

        bool GetIP(char *, struct in_addr *);
        int SearchStack(void);
        void UpdateStack(void);
        int WaitForDataAndRead(int, int *);
        void FlushSocket(void);
        void AddLogLine(char *);

    public:
        CTCP();
        ~CTCP();

        bool OpenControl(BOOKMARK *bm);
        bool SendData(char *msg);
        bool WaitForMessage(void);
        bool WaitForMessage(int);
        int GetError(void) { return (this->error);};

        void CloseControl(void);
        void ObtainLog(char *log[LOG_LINES]);
        char *GetControlBuffer(void) { return (this->reply_line);};

        void SetSiteMsgString(char *temp_msg_string) {
            strcpy(this->site_msg_string, temp_msg_string);
        };

        bool SiteMessage(void) { return (this->site_msg);};

        void ResetMessage(void) { this->site_msg = FALSE;};

        float GetSpeed(void) { return (this->speed);};

        void FlushStack(void);
        bool OpenDataPASV(char *pasv_str);
        bool OpenData(char *, bool);
        bool AcceptData();
        void CloseData(void);
        bool ReadFile(char *, long);
        bool WriteFile(char *, bool);

        void GetTimevals(struct timeval *before, struct timeval *after,
                        long *size) {
            *before = this->tv_before;
            *after = this->tv_after;
            *size = this->size;
        };

        void PrepareXDupe();
        void StopXDupe();
        XDUPELIST *GetXDupe();

#ifdef TLS
        int prot;
        bool SecureControl(void);
        bool SecureData(void);
        int TLS_Status(void) { if (this->ctrl_con) return 1; else return 0; }
#endif

        void SetStealth(bool stealth_set, char *file) {
            this->stealth = stealth_set;
            strcpy(this->stealth_file, file);
        };
};

void break_handler(int sig);

#endif
