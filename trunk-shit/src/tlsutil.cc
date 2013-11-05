/* this file has been taken from the ftp-tls project and modified to my needs */
/*
* Copyright (c) Peter 'Luna' Runestig 1999, 2000 <peter@runestig.com>
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LI-
* ABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUEN-
* TIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEV-
* ER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABI-
* LITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <unistd.h>
//#include <fcntl.h>
//#include <string.h>
//#include <stdlib.h>
//#include <errno.h>
//#include <ctype.h>
//#include <sys/param.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <sys/poll.h>
#include <openssl/ssl.h>
//#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
/*
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
*/
#define DEFAULTCIPHERLIST "ALL"

void tls_cleanup(void);

SSL_CTX *ssl_ctx = NULL;
//X509_STORE *crl_store = NULL;
/*
char *tls_key_file = NULL;
char *tls_cert_file = NULL;
char *tls_crl_file = NULL;
char *tls_crl_dir = NULL;
*/
char *tls_rand_file = NULL;
char tls_cipher_list[255] = DEFAULTCIPHERLIST;
//char  tls_hostname[MAXHOSTNAMELEN]; /* hostname used by user to connect */

/*
void tls_set_cipher_list(char *list)
{
    snprintf(tls_cipher_list, sizeof(tls_cipher_list), "%s", list);
}
*/
char *tls_get_commonName(SSL * ssl) {
    static char name[256];
    int err = 0;
    X509 *server_cert;

    if ((server_cert = SSL_get_peer_certificate(ssl))) {
        err = X509_NAME_get_text_by_NID(X509_get_subject_name(server_cert),
                                        NID_commonName, name,
                                        sizeof(name));
        X509_free(server_cert);
    }

    if (err > 0)
        return name;
    else
        return NULL;
}

char *tls_get_issuer_name(SSL * ssl) {
    static char name[256];
    X509 *server_cert;

    if ((server_cert = SSL_get_peer_certificate(ssl))) {
        X509_NAME_oneline(X509_get_issuer_name(server_cert), name,
                          sizeof(name));
        X509_free(server_cert);
        return name;
    } else
        return NULL;
}

char *tls_get_subject_name(SSL * ssl) {
    static char name[256];
    X509 *server_cert;

    if ((server_cert = SSL_get_peer_certificate(ssl))) {
        X509_NAME_oneline(X509_get_subject_name(server_cert), name,
                          sizeof(name));
        X509_free(server_cert);
        return name;
    } else
        return NULL;
}

int seed_PRNG(void) {
    char stackdata[1024];
    static char rand_file[300];
    FILE *fh;

#if OPENSSL_VERSION_NUMBER >= 0x00905100
    if (RAND_status())
        return 0; /* PRNG already good seeded */
#endif

    /* if the device '/dev/urandom' is present, OpenSSL uses it by default.
     * check if it's present, else we have to make random data ourselfs.
     */
    if ((fh = fopen("/dev/urandom", "r"))) {
        fclose(fh);
        return 0;
    }

    if (RAND_file_name(rand_file, sizeof(rand_file)))
        tls_rand_file = rand_file;
    else
        return 1;

    if (!RAND_load_file(rand_file, 1024)) {
        /* no .rnd file found, create new seed */
        unsigned int c;
        c = time(NULL);
        RAND_seed(&c, sizeof(c));
        c = getpid();
        RAND_seed(&c, sizeof(c));
        RAND_seed(stackdata, sizeof(stackdata));
    }

#if OPENSSL_VERSION_NUMBER >= 0x00905100
    if (!RAND_status())
        return 2; /* PRNG still badly seeded */
#endif

    return 0;
}

int tls_init(void) {
    //int err;

    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();
    // gotta play with this one day (prolly will be needed for win ftpds)
    ssl_ctx = SSL_CTX_new(SSLv3_client_method());
    //ssl_ctx=SSL_CTX_new(SSLv23_client_method());
    //ssl_ctx = SSL_CTX_new(TLSv1_client_method());

    if (!ssl_ctx) {
        fprintf(stderr, "SSL_CTX_new() %s\r\n",
                (char *) ERR_error_string(ERR_get_error(), NULL));
        return 1;
    }

    SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL);
    SSL_CTX_set_default_verify_paths(ssl_ctx);
    //SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2);
    //SSL_CTX_set_default_verify_paths(ssl_ctx);
    //SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, verify_callback);
    /* set up session caching  */
    SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_CLIENT);
    SSL_CTX_set_session_id_context(ssl_ctx, (const unsigned char *) "1",
                                   1);

    /* set up the CRL */
    //if ((tls_crl_file || tls_crl_dir) && (crl_store = X509_STORE_new()))
    //    X509_STORE_load_locations(crl_store, tls_crl_file, tls_crl_dir);

    if (seed_PRNG())
        fprintf(stderr, "Wasn't able to properly seed the PRNG!\r\n");

    return 0;
}

void tls_cleanup(void) {
    //if (crl_store) {
    //    X509_STORE_free(crl_store);
    //    crl_store = NULL;
    //}

    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }

    /*
    if (tls_cert_file) {
        free(tls_cert_file);
        tls_cert_file = NULL;
    }
    if (tls_key_file) {
        free(tls_key_file);
        tls_key_file = NULL;
    }
    if (tls_crl_file) {
        free(tls_crl_file);
        tls_crl_file = NULL;
    }
    if (tls_crl_dir) {
        free(tls_crl_dir);
        tls_crl_dir = NULL;
    }
    */

    if (tls_rand_file)
        RAND_write_file(tls_rand_file);
}

