#include <string.h>
#include <stdio.h>
#include <curses.h>
#include <panel.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#include "defines.h"
#include "tcp.h"
#include "server.h"
#include "displayhandler.h"
#include "keyhandler.h"

extern BOOKMARK *global_bookmark;
extern int bm_magic_max;
//extern char *startcwd;
extern char startcwd[SERVER_WORKINGDIR_SIZE];
extern char *prelabels[NUM_PRECOMMANDS];
extern char *section;

void CDisplayHandler::SaveBookmarks() {
    FILE *file_out;
    char *line = new(char[2048]), *enc = new(char[4096]);
    BOOKMARK *bm_temp = global_bookmark;
    char tempcwd[SERVER_WORKINGDIR_SIZE];

    getcwd(tempcwd, SERVER_WORKINGDIR_SIZE);
    chdir(startcwd);

    // backup old bookmark-file
    rename(BOOKMARK_RC, BOOKMARK_RC_BAK);

    pass_pos = 0;
    if ((file_out = fopen(BOOKMARK_RC, "w"))) {
        // put magic and version number
        strcpy(line, PASS_MAGIC);
        Encrypt(line, enc);
        fprintf(file_out, "%s%s\n", BOOKMARK_ID, enc);

        // put the bookmarks
        while (bm_temp) {
            Encrypt(bm_temp->label, enc);
            fprintf(file_out, "%s\n", enc);
            //debuglog("host save %s", bm_temp->host);
            Encrypt(bm_temp->host, enc);
            fprintf(file_out, "%s\n", enc);
            Encrypt(bm_temp->user, enc);
            fprintf(file_out, "%s\n", enc);
            Encrypt(bm_temp->pass, enc);
            fprintf(file_out, "%s\n", enc);
            Encrypt(bm_temp->startdir, enc);
            fprintf(file_out, "%s\n", enc);
            Encrypt(bm_temp->exclude_source, enc);
            fprintf(file_out, "%s\n", enc);
            Encrypt(bm_temp->exclude_target, enc);
            fprintf(file_out, "%s\n", enc);
            for (int f = 0; f < NUM_SECTIONS; f++) {
               Encrypt(bm_temp->section[f], enc);
               fprintf(file_out, "%s\n", enc);
            }  
            Encrypt(bm_temp->site_rules, enc);
            fprintf(file_out, "%s\n", enc);
            Encrypt(bm_temp->site_user, enc);
            fprintf(file_out, "%s\n", enc);
            Encrypt(bm_temp->site_wkup, enc);
            fprintf(file_out, "%s\n", enc);
            Encrypt(bm_temp->noop_cmd, enc);
            fprintf(file_out, "%s\n", enc);
            Encrypt(bm_temp->first_cmd, enc);
            fprintf(file_out, "%s\n", enc);
            for (int f = 0; f < NUM_PRECOMMANDS; f++) { 
               Encrypt(bm_temp->pre_cmd[f], enc);
               fprintf(file_out, "%s\n", enc);
            } 

            sprintf(line, "%d", bm_temp->retry_counter);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);
            sprintf(line, "%d", bm_temp->refresh_rate);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);
            sprintf(line, "%d", bm_temp->noop_rate);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);
            sprintf(line, "%d", bm_temp->retry_delay);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_refresh)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_noop)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_jump)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_track)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_startdir)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_exclude_source)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_exclude_target)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_autologin)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_chaining)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_sections)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            sprintf(line, "%d", bm_temp->file_sorting);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            sprintf(line, "%d", bm_temp->dir_sorting);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_pasv)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_ssl)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_ssl_list)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_ssl_data)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_stealth_mode)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_retry)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_firstcmd)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_rndrefr)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_ssl_fxp)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_alt_fxp)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_rev_file)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            if (bm_temp->use_rev_dir)
                strcpy(line, TRUE_MAGIC);
            else
                strcpy(line, FALSE_MAGIC);
            Encrypt(line, enc);
            fprintf(file_out, "%s\n", enc);

            Encrypt(bm_temp->nuke_prefix, enc);
            fprintf(file_out, "%s\n", enc);

            Encrypt(bm_temp->msg_string, enc);
            fprintf(file_out, "%s\n", enc);

            bm_temp = bm_temp->next;
        }
        fclose(file_out);
    }
    delete[](line);
    delete[](enc);
    chdir(tempcwd);
}

void CDisplayHandler::Decrypt(char *in, char *out) {
    int decrypt, value, prefix;
    char *c = in, *o = out;

    if (strrchr(in, '\r'))
        *(strrchr(in, '\r')) = '\0';

    if (strrchr(in, '\n'))
        *(strrchr(in, '\n')) = '\0';

    // decrypt string
    while (*c) {
        prefix = (int) * c;
        c += 1;

        if (prefix == 127)
            value = 0;
        else if (prefix == 128)
            value = (int) '\n';
        else if (prefix == 129)
            value = (int) '\r';
        else
            value = (int) * c;

        decrypt = value - (int) (this->custom_password[pass_pos]);
        c += 1;

        if (decrypt < 0)
            decrypt += 256;

        *o = (char) decrypt;

        o += 1;
        if (this->custom_password[pass_pos + 1])
            pass_pos++;
        else
            pass_pos = 0;
    }
    *o = '\0';
}

void CDisplayHandler::Encrypt(char *in, char *out) {
    int encrypt;
    char *c = in, *o = out, prefix;

    // encrypt string
    while (*c) {
        encrypt = (int) (*c) + (int) (this->custom_password[pass_pos]);

        if (encrypt == 0) {
            prefix = 127;
            encrypt = 32;
        } else if (encrypt == '\n') {
            prefix = 128;
            encrypt = 32;
        } else if (encrypt == '\r') {
            prefix = 129;
            encrypt = 32;
        } else {
            prefix = 130;
            if (encrypt < 0)
                encrypt += 256;
        }

        *o = prefix;
        o += 1;

        *o = (char) encrypt;
        o += 1;
        c += 1;

        if (this->custom_password[pass_pos + 1])
            pass_pos++;
        else
            pass_pos = 0;
    }
    *o = '\0';
}

bool CDisplayHandler::ReadBookmarks(void) {
    FILE *file_in;
    char temp[256];
    char *line = new(char[4096]), *out = new(char[2048]);
    BOOKMARK *bm_new, *bm_temp = NULL;
    char tempcwd[SERVER_WORKINGDIR_SIZE];

    getcwd(tempcwd, SERVER_WORKINGDIR_SIZE);
    chdir(startcwd);

    pass_pos = 0;
    if ((file_in = fopen(BOOKMARK_RC, "r"))) {
        // check bookmark-id
        fgets(line, 4095, file_in);

        if (strcmp(line, "II@0.11.2mew5\n") == 0) {
            debuglog("0.11.2mew5 - old bookmarks, will try to upgrade");
            // nothing else can happen.... probe check
            // parse magic
            fgets(line, 4095, file_in);
            this->Decrypt(line, out);
            if (strcmp(PASS_MAGIC, out)) {
                fclose(file_in);
                delete[](line);
                delete[](out);
                chdir(tempcwd);
                return (FALSE);
            }

            // parse bookmarks
            while (!feof(file_in)) {

                fgets(line, 4095, file_in);
                if (!feof(file_in)) {
                    bm_new = new(BOOKMARK);
                    bm_new->next = NULL;

                    Decrypt(line, out);
                    bm_new->label = new(char[strlen(out) + 1]);
                    strcpy(bm_new->label, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->host = new(char[strlen(out) + 1]);
                    strcpy(bm_new->host, out);
                    //debuglog("host load %s", bm_new->host);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->user = new(char[strlen(out) + 1]);
                    strcpy(bm_new->user, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->pass = new(char[strlen(out) + 1]);
                    strcpy(bm_new->pass, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->startdir = new(char[strlen(out) + 1]);
                    strcpy(bm_new->startdir, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->exclude_source = new(char[strlen(out) + 1]);
                    strcpy(bm_new->exclude_source, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->exclude_target = new(char[strlen(out) + 1]);
                    strcpy(bm_new->exclude_target, out);
                    
                    for (int f = 0; f < NUM_SECTIONS; f++) { 
                         fgets(line, 4095, file_in);
                         Decrypt(line, out);
                         bm_new->section[f] = new(char[strlen(out) + 1]);
                         strcpy(bm_new->section[f], out);
                    }

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_rules = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_rules, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_user = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_user, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_wkup = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_wkup, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->noop_cmd = new(char[strlen(out) + 1]);
                    strcpy(bm_new->noop_cmd, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->first_cmd = new(char[strlen(out) + 1]);
                    strcpy(bm_new->first_cmd, out);
                    
                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->retry_counter = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->refresh_rate = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->noop_rate = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->retry_delay = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_refresh = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_noop = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_jump = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_track = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_startdir = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_exclude_source = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_exclude_target = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_autologin = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_chaining = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_sections = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->file_sorting = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->dir_sorting = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_pasv = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_list = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_data = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_stealth_mode = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_retry = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_firstcmd = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rndrefr = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    // start new bookmark entries ...
                    bm_new->use_ssl_fxp = FALSE;
                    bm_new->use_alt_fxp = FALSE;
                    bm_new->use_rev_file = FALSE;
                    bm_new->use_rev_dir = FALSE;
		    
                    for (int f = 0; f < NUM_PRECOMMANDS; f++) { 
                       sprintf(temp, "PRE %%R %s", prelabels[f]);
                       bm_new->pre_cmd[f] = new(char[strlen(temp) + 1]);
                       strcpy(bm_new->pre_cmd[f], temp); 
                    }
                    bm_new->nuke_prefix = new(char[strlen("NUKED-") + 1]);
                    strcpy(bm_new->nuke_prefix, "NUKED-");
                    bm_new->msg_string = new(char[strlen("250- You have messages") + 1]);
                    strcpy(bm_new->msg_string, "250- You have messages");
                    // ... end new bookmark entries.

                    bm_magic_max++;
                    bm_new->magic = bm_magic_max;

                    if (bm_temp)
                        bm_temp->next = bm_new;
                    else
                        global_bookmark = bm_new;

                    bm_temp = bm_new;
                }
            }
        } // end of 0.11.2mew5
        else if (strcmp(line, "II@0.11.2mew5+\n") == 0) {
            debuglog("0.11.2mew5+ - old bookmarks, will try to upgrade");
            // nothing else can happen.... probe check
            // parse magic
            fgets(line, 4095, file_in);
            this->Decrypt(line, out);
            if (strcmp(PASS_MAGIC, out)) {
                fclose(file_in);
                delete[](line);
                delete[](out);
                chdir(tempcwd);
                return (FALSE);
            }

            // parse bookmarks
            while (!feof(file_in)) {

                fgets(line, 4095, file_in);
                if (!feof(file_in)) {
                    bm_new = new(BOOKMARK);
                    bm_new->next = NULL;

                    Decrypt(line, out);
                    bm_new->label = new(char[strlen(out) + 1]);
                    strcpy(bm_new->label, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->host = new(char[strlen(out) + 1]);
                    strcpy(bm_new->host, out);
                    //debuglog("host load %s", bm_new->host);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->user = new(char[strlen(out) + 1]);
                    strcpy(bm_new->user, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->pass = new(char[strlen(out) + 1]);
                    strcpy(bm_new->pass, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->startdir = new(char[strlen(out) + 1]);
                    strcpy(bm_new->startdir, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->exclude_source = new(char[strlen(out) + 1]);
                    strcpy(bm_new->exclude_source, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->exclude_target = new(char[strlen(out) + 1]);
                    strcpy(bm_new->exclude_target, out);
                    
                    for (int f = 0; f < NUM_SECTIONS; f++) { 
                         fgets(line, 4095, file_in);
                         Decrypt(line, out);
                         bm_new->section[f] = new(char[strlen(out) + 1]);
                         strcpy(bm_new->section[f], out);
                    }

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_rules = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_rules, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_user = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_user, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_wkup = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_wkup, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->noop_cmd = new(char[strlen(out) + 1]);
                    strcpy(bm_new->noop_cmd, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->first_cmd = new(char[strlen(out) + 1]);
                    strcpy(bm_new->first_cmd, out);

                    fgets(line, 4095, file_in); //read old pre_cmd
                    Decrypt(line, out);
                    
                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->retry_counter = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->refresh_rate = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->noop_rate = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->retry_delay = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_refresh = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_noop = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_jump = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_track = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_startdir = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_exclude_source = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_exclude_target = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_autologin = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_chaining = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_sections = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->file_sorting = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->dir_sorting = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_pasv = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_list = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_data = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_stealth_mode = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_retry = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_firstcmd = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rndrefr = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    // start new bookmark entries ...
                    bm_new->use_ssl_fxp = FALSE;
                    bm_new->use_alt_fxp = FALSE;
                    bm_new->use_rev_file = FALSE;
                    bm_new->use_rev_dir = FALSE;

                    for (int f = 0; f < NUM_PRECOMMANDS; f++) { 
                       sprintf(temp, "PRE %%R %s", prelabels[f]);
                       bm_new->pre_cmd[f] = new(char[strlen(temp) + 1]);
                       strcpy(bm_new->pre_cmd[f], temp); 
                    }
                    bm_new->nuke_prefix = new(char[strlen("NUKED-") + 1]);
                    strcpy(bm_new->nuke_prefix, "NUKED-");
                    bm_new->msg_string = new(char[strlen("250- You have messages") + 1]);
                    strcpy(bm_new->msg_string, "250- You have messages");
                    // ... end new bookmark entries.

                    bm_magic_max++;
                    bm_new->magic = bm_magic_max;

                    if (bm_temp)
                        bm_temp->next = bm_new;
                    else
                        global_bookmark = bm_new;

                    bm_temp = bm_new;
                }
            }
        } // end of 0.11.2mew5+
	else if (strcmp(line, "II@0.11.2mew6\n") == 0) {
            debuglog("0.11.2mew6 - not mew6+1, will try to upgrade");
            fgets(line, 4095, file_in);
            this->Decrypt(line, out);
            if (strcmp(PASS_MAGIC, out)) {
                fclose(file_in);
                delete[](line);
                delete[](out);
                chdir(tempcwd);
                return (FALSE);
            }

            // parse bookmarks
            while (!feof(file_in)) {

                fgets(line, 4095, file_in);
                if (!feof(file_in)) {
                    bm_new = new(BOOKMARK);
                    bm_new->next = NULL;

                    Decrypt(line, out);
                    bm_new->label = new(char[strlen(out) + 1]);
                    strcpy(bm_new->label, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->host = new(char[strlen(out) + 1]);
                    strcpy(bm_new->host, out);
                    //debuglog("host load %s", bm_new->host);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->user = new(char[strlen(out) + 1]);
                    strcpy(bm_new->user, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->pass = new(char[strlen(out) + 1]);
                    strcpy(bm_new->pass, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->startdir = new(char[strlen(out) + 1]);
                    strcpy(bm_new->startdir, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->exclude_source = new(char[strlen(out) + 1]);
                    strcpy(bm_new->exclude_source, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->exclude_target = new(char[strlen(out) + 1]);
                    strcpy(bm_new->exclude_target, out);
                     
                    for (int f = 0; f < NUM_SECTIONS; f++) {
                         fgets(line, 4095, file_in);
                         Decrypt(line, out);
                         bm_new->section[f] = new(char[strlen(out) + 1]);
                         strcpy(bm_new->section[f], out);
                    }
 
                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_rules = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_rules, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_user = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_user, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_wkup = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_wkup, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->noop_cmd = new(char[strlen(out) + 1]);
                    strcpy(bm_new->noop_cmd, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->first_cmd = new(char[strlen(out) + 1]);
                    strcpy(bm_new->first_cmd, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->retry_counter = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->refresh_rate = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->noop_rate = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->retry_delay = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_refresh = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_noop = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_jump = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_track = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_startdir = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_exclude_source = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_exclude_target = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_autologin = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_chaining = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_sections = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->file_sorting = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->dir_sorting = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_pasv = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_list = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_data = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_stealth_mode = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_retry = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_firstcmd = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rndrefr = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_fxp = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_alt_fxp = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rev_file = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rev_dir = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->nuke_prefix = new(char[strlen(out) + 1]);
                    strcpy(bm_new->nuke_prefix, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->msg_string = new(char[strlen(out) + 1]);
                    strcpy(bm_new->msg_string, out);
                    
                    for (int f = 0; f < NUM_PRECOMMANDS; f++) { 
                       sprintf(temp, "PRE %%R %s", prelabels[f]);
                       bm_new->pre_cmd[f] = new(char[strlen(temp) + 1]);
                       strcpy(bm_new->pre_cmd[f], temp); 
                    }
                   
                    bm_magic_max++;
                    bm_new->magic = bm_magic_max;

                    if (bm_temp)
                        bm_temp->next = bm_new;
                    else
                        global_bookmark = bm_new;

                    bm_temp = bm_new;
                }
            }
        } // end of 0.11.2mew6
        else if (strcmp(line, "II@0.11.2mew6+1\n") == 0) {
            debuglog("0.11.2mew6+1 - not mew6+2, will try to upgrade");
            // parse magic
            fgets(line, 4095, file_in);
            this->Decrypt(line, out);
            if (strcmp(PASS_MAGIC, out)) {
                fclose(file_in);
                delete[](line);
                delete[](out);
                chdir(tempcwd);
                return (FALSE);
            }

            // parse bookmarks
            while (!feof(file_in)) {

                fgets(line, 4095, file_in);
                if (!feof(file_in)) {
                    bm_new = new(BOOKMARK);
                    bm_new->next = NULL;

                    Decrypt(line, out);
                    bm_new->label = new(char[strlen(out) + 1]);
                    strcpy(bm_new->label, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->host = new(char[strlen(out) + 1]);
                    strcpy(bm_new->host, out);
                    //debuglog("host load %s", bm_new->host);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->user = new(char[strlen(out) + 1]);
                    strcpy(bm_new->user, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->pass = new(char[strlen(out) + 1]);
                    strcpy(bm_new->pass, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->startdir = new(char[strlen(out) + 1]);
                    strcpy(bm_new->startdir, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->exclude_source = new(char[strlen(out) + 1]);
                    strcpy(bm_new->exclude_source, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->exclude_target = new(char[strlen(out) + 1]);
                    strcpy(bm_new->exclude_target, out);

                    for (int f = 0; f < NUM_SECTIONS; f++) {
                         fgets(line, 4095, file_in);
                         Decrypt(line, out);
                         bm_new->section[f] = new(char[strlen(out) + 1]);
                         strcpy(bm_new->section[f], out);
                    }
 
                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_rules = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_rules, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_user = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_user, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_wkup = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_wkup, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->noop_cmd = new(char[strlen(out) + 1]);
                    strcpy(bm_new->noop_cmd, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->first_cmd = new(char[strlen(out) + 1]);
                    strcpy(bm_new->first_cmd, out);

                    fgets(line, 4095, file_in); //read old pre_cmd
                    Decrypt(line, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->retry_counter = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->refresh_rate = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->noop_rate = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->retry_delay = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_refresh = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_noop = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_jump = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_track = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_startdir = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_exclude_source = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_exclude_target = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_autologin = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_chaining = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_sections = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->file_sorting = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->dir_sorting = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_pasv = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_list = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_data = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_stealth_mode = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_retry = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_firstcmd = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rndrefr = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_fxp = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_alt_fxp = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rev_file = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rev_dir = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->nuke_prefix = new(char[strlen(out) + 1]);
                    strcpy(bm_new->nuke_prefix, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->msg_string = new(char[strlen(out) + 1]);
                    strcpy(bm_new->msg_string, out);

                    for (int f = 0; f < NUM_PRECOMMANDS; f++) { 
                       sprintf(temp, "PRE %%R %s", prelabels[f]);
                       bm_new->pre_cmd[f] = new(char[strlen(temp) + 1]);
                       strcpy(bm_new->pre_cmd[f], temp); 
                    }

                    bm_magic_max++;
                    bm_new->magic = bm_magic_max;

                    if (bm_temp)
                        bm_temp->next = bm_new;
                    else
                        global_bookmark = bm_new;

                    bm_temp = bm_new;
                }
            }
        } // end of 0.11.2mew6+1
        else {
            // nothing else can happen.... probe check
            // parse magic
            fgets(line, 4095, file_in);
            this->Decrypt(line, out);
            if (strcmp(PASS_MAGIC, out)) {
                fclose(file_in);
                delete[](line);
                delete[](out);
                chdir(tempcwd);
                return (FALSE);
            }

            // parse bookmarks
            while (!feof(file_in)) {

                fgets(line, 4095, file_in);
                if (!feof(file_in)) {
                    bm_new = new(BOOKMARK);
                    bm_new->next = NULL;

                    Decrypt(line, out);
                    bm_new->label = new(char[strlen(out) + 1]);
                    strcpy(bm_new->label, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->host = new(char[strlen(out) + 1]);
                    strcpy(bm_new->host, out);
                    //debuglog("host load %s", bm_new->host);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->user = new(char[strlen(out) + 1]);
                    strcpy(bm_new->user, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->pass = new(char[strlen(out) + 1]);
                    strcpy(bm_new->pass, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->startdir = new(char[strlen(out) + 1]);
                    strcpy(bm_new->startdir, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->exclude_source = new(char[strlen(out) + 1]);
                    strcpy(bm_new->exclude_source, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->exclude_target = new(char[strlen(out) + 1]);
                    strcpy(bm_new->exclude_target, out);

                    for (int f = 0; f < NUM_SECTIONS; f++) {
                         fgets(line, 4095, file_in);
                         Decrypt(line, out);
                         bm_new->section[f] = new(char[strlen(out) + 1]);
                         strcpy(bm_new->section[f], out);
                    }
 
                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_rules = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_rules, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_user = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_user, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->site_wkup = new(char[strlen(out) + 1]);
                    strcpy(bm_new->site_wkup, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->noop_cmd = new(char[strlen(out) + 1]);
                    strcpy(bm_new->noop_cmd, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->first_cmd = new(char[strlen(out) + 1]);
                    strcpy(bm_new->first_cmd, out);

                    for (int f = 0; f < NUM_PRECOMMANDS; f++) {
                         fgets(line, 4095, file_in);
                         Decrypt(line, out);
                         bm_new->pre_cmd[f] = new(char[strlen(out) + 1]);
                         strcpy(bm_new->pre_cmd[f], out);
                    }

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->retry_counter = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->refresh_rate = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->noop_rate = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->retry_delay = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_refresh = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_noop = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_jump = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_track = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_startdir = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_exclude_source = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_exclude_target = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_autologin = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_chaining = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_sections = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->file_sorting = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->dir_sorting = atoi(out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_pasv = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_list = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_data = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_stealth_mode = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_retry = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_firstcmd = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rndrefr = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_ssl_fxp = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_alt_fxp = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rev_file = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->use_rev_dir = strcmp(out, TRUE_MAGIC) ? FALSE : TRUE;

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->nuke_prefix = new(char[strlen(out) + 1]);
                    strcpy(bm_new->nuke_prefix, out);

                    fgets(line, 4095, file_in);
                    Decrypt(line, out);
                    bm_new->msg_string = new(char[strlen(out) + 1]);
                    strcpy(bm_new->msg_string, out);

                    bm_magic_max++;
                    bm_new->magic = bm_magic_max;

                    if (bm_temp)
                        bm_temp->next = bm_new;
                    else
                        global_bookmark = bm_new;

                    bm_temp = bm_new;
                }
            }
        } // end current version bookmark

        fclose(file_in);
        delete[](line);
        delete[](out);
        chdir(tempcwd);
        return (TRUE);
    }
    delete[](line);
    delete[](out);
    chdir(tempcwd);
    return (FALSE);
}

int CDisplayHandler::ProbeBookmarkRC(void) {
    FILE *file_probe;
    char tempcwd[SERVER_WORKINGDIR_SIZE];
    char *line;

    getcwd(tempcwd, SERVER_WORKINGDIR_SIZE);
    chdir(startcwd);

    if ((file_probe = fopen(BOOKMARK_RC, "r"))) {
        // check bookmark-id
        line = new(char[4096]);
        fgets(line, 4095, file_probe);
        fclose(file_probe);
        chdir(tempcwd);
        debuglog("read bookmark id : %s", line);

        if (!strcmp(line, BOOKMARK_ID)) {
            debuglog("id %s was latest", line);
            delete[](line);
            return (0);
        } else if (!strcmp(line, "II@0.11.2mew6+1\n")) {
            debuglog("id %s was 0.11.2mew6+1", line);
	    delete[](line);
	    return (0);
        } else if (!strcmp(line, "II@0.11.2mew6\n")) {
            debuglog("id %s was 0.11.2mew6", line);
	    delete[](line);
	    return (0);
        } else if (!strcmp(line, "II@0.11.2mew5+\n")) {
            debuglog("id %s was 0.11.2mew5", line);
            delete[](line);
            return (0);
        } else if (!strcmp(line, "II@0.11.2mew5\n")) {
            debuglog("id %s was 0.11.2mew5", line);
            delete[](line);
            return (0);
        }

        debuglog("id %s was unknown", line);
        delete[](line);
        return (1);
    } else {
        chdir(tempcwd);
        return (2);
    }
}

void CDisplayHandler::DialogNotice(char *notice, char *button) {
    // initialize notice window
    this->window_notice =
        newwin(5, 60, this->terminal_max_y / 2 - 2,
            this->terminal_max_x / 2 - 30);
    this->panel_notice = new_panel(this->window_notice);
    leaveok(window_notice, TRUE);

    wattrset(this->window_notice, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_notice, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_notice);
    //wbkgdset(this->window_notice, ' ');
    mywborder(this->window_notice);

    mvwaddnstr(this->window_notice, 1, 2, notice, 56);
    wattrset(this->window_notice, COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);
    mvwaddstr(this->window_notice, 3, 30 - strlen(button) / 2, button);

    //wnoutrefresh(window_notice);
    update_panels();
    doupdate();
}

void CDisplayHandler::CloseNotice(void) {
    del_panel(this->panel_notice);
    delwin(this->window_notice);
    this->panel_notice = NULL;
    this->window_notice = NULL;
    update_panels();
    doupdate();
    //this->RebuildScreen();
    /*
    wnoutrefresh(this->window_command);
    wnoutrefresh(this->window_status);
    wnoutrefresh(this->window_left);
    wnoutrefresh(this->window_right);
    if(this->window_dialog)
        wnoutrefresh(this->window_dialog);
    if(this->window_input)
        wnoutrefresh(this->window_input);
    doupdate();
    */
}

void CDisplayHandler::NoticeNoMatch(void) {
    this->DialogNotice(NOTICE_PASSNOMATCH, DEFAULT_OKAY);
}

void CDisplayHandler::NoticeNoBookmark(void) {
    this->DialogNotice(NOTICE_NOBOOKMARK, DEFAULT_OKAY);
}

void CDisplayHandler::NoticeNoPasswd(void) {
    this->DialogNotice(NOTICE_NOPASSWD, DEFAULT_OKAY);
}

void CDisplayHandler::NoticeBadPasswd(void) {
    this->DialogNotice(NOTICE_BADPASSWD, DEFAULT_OKAY);
}

void CDisplayHandler::OpenPasswordInput(bool hidden) {
    strcpy(this->password, "");
    this->DialogInput(DIALOG_ENTERPASS, this->password, PASSWORD_SIZE - 1,
                      hidden);
}

void CDisplayHandler::OpenChangePasswordInput(bool confirm) {
    if (!confirm) {
        strcpy(this->password_new, "");
        this->DialogInput(DIALOG_ENTERCHPASS, this->password_new,
                          PASSWORD_SIZE - 1, TRUE);
    } else {
        strcpy(this->password_confirm, "");
        this->DialogInput(DIALOG_ENTERCHPASSCON, this->password_confirm,
                          PASSWORD_SIZE - 1, TRUE);
    }
}

void CDisplayHandler::CloseInput(void) {
    del_panel(this->panel_input);
    delwin(this->window_input);
    this->window_input = NULL;
    this->panel_input = NULL;
    update_panels();
    doupdate();
    //this->RebuildScreen();
    /*
    wnoutrefresh(this->window_command);
    wnoutrefresh(this->window_status);
    wnoutrefresh(this->window_left);
    wnoutrefresh(this->window_right);
    if(this->window_dialog)
        wnoutrefresh(this->window_dialog);
    if(this->window_prefs)
        wnoutrefresh(this->window_prefs);
    doupdate();
    */
}

void CDisplayHandler::DialogInput(char *title, char *input, int max,
                                  bool hidden) {
    unsigned int n;

    // initialize input window
    this->window_input = newwin(5, 60, this->terminal_max_y / 2 - 2,
                                this->terminal_max_x / 2 - 30);
    this->panel_input = new_panel(this->window_input);
    //leaveok(this->window_input, TRUE);

    wattrset(this->window_input, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_input, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_input);
    //wbkgdset(this->window_input, ' ');
    mywborder(this->window_input);

    mvwaddnstr(this->window_input, 1, 2, title, 56);
    this->input = input;

    for (n = 0; n < strlen(input); n++)
        *(this->hidden_input + n) = '*';

    *(this->hidden_input + n) = '\0';

    this->input_hidden = hidden;
    this->input_chars = strlen(this->input);
    this->input_maxchars = max;
    this->cursorpos = this->input_chars;
    this->UpdateInput();

    //wnoutrefresh(window_input);
    update_panels();
    doupdate();
}

void CDisplayHandler::DialogXSite(bool for_each) {
    unsigned int n;

    for_all = for_each;
    // initialize input window
    this->window_input = newwin(7, 60, this->terminal_max_y / 2 - 3,
                                this->terminal_max_x / 2 - 30);
    this->panel_input = new_panel(this->window_input);
    //leaveok(this->window_input, TRUE);

    wattrset(this->window_input, COLOR_PAIR(STYLE_NORMAL) | A_NORMAL);
    wbkgdset(this->window_input, ' ' | COLOR_PAIR(STYLE_NORMAL));
    werase(this->window_input);
    //wbkgdset(this->window_input, ' ');
    mywborder(this->window_input);

    mvwaddnstr(this->window_input, 1, 2,
               "enter SITE command (use QUOTE for raw) or use aliases:", 56);
    this->input = this->input_temp;

    for (n = 0; n < strlen(input); n++)
        *(this->hidden_input + n) = '*';

    *(this->hidden_input + n) = '\0';

    this->input_hidden = FALSE;
    this->input_chars = strlen(this->input);
    this->input_maxchars = INPUT_TEMP_MAX - 1;
    this->cursorpos = this->input_chars;
    this->UpdateInput();

    this->xsite_buttonstate = 0;
    this->UpdateXSiteButtons();
}

void CDisplayHandler::UpdateXSiteButtons(void) {
    int n;

    // remove old button selection and draw active one
    for (n = 0; n < 4; n++) {
        if (n == this->xsite_buttonstate)
            wattrset(this->window_input,
                     COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);
        else
            wattrset(this->window_input, COLOR_PAIR(STYLE_MARKED) | A_BOLD);

        switch (n) {
            case 1:
                mvwaddstr(this->window_input, 5, 7, "[ RULES ]");
                break;

            case 2:
                mvwaddstr(this->window_input, 5, 26, "[ USER ]");
                break;

            case 3:
                mvwaddstr(this->window_input, 5, 45, "[ WKUP ]");
                break;
        }

    }

    //wnoutrefresh(window_input);
    update_panels();
    doupdate();
}

bool CDisplayHandler::CloseSiteInput(void) {
    del_panel(this->panel_input);
    delwin(this->window_input);
    this->window_input = NULL;
    this->panel_input = NULL;
    update_panels();
    doupdate();
    return for_all;
    //this->RebuildScreen();
    /*
    wnoutrefresh(this->window_command);
    wnoutrefresh(this->window_status);
    wnoutrefresh(this->window_left);
    wnoutrefresh(this->window_right);
    doupdate();
    */
}

void CDisplayHandler::UpdateInput(void) {
    int x;
    char c, c2;

    wattrset(this->window_input, COLOR_PAIR(STYLE_INVERSE) | this->inverse_mono);
    mvwaddstr(this->window_input, 3, 2,
              "                                                        ");

    if (this->input_chars <= 55) {
        // show full string
        if (!this->input_hidden) {
            c = *(this->input + this->cursorpos);
            *(this->input + this->cursorpos) = '\0';
            mvwaddstr(this->window_input, 3, 2, this->input);
            *(this->input + this->cursorpos) = c;
            mvwaddstr(this->window_input, 3, 2 + this->cursorpos + 1,
                      this->input + this->cursorpos);
        } else {
            mvwaddstr(this->window_input, 3, 2, this->hidden_input);
            mvwaddstr(this->window_input, 3, 2 + this->input_chars, "*");
        }

        wattrset(this->window_input,
                 COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);
        mvwaddch(this->window_input, 3, 2 + this->cursorpos, ' ');
        x = 2 + this->cursorpos;
    } else {
        int truncpos = this->cursorpos - 55 / 2, newpos;

        if (this->cursorpos <= 55 / 2) {
            truncpos = 0;
        } else if (truncpos + 55 >= this->input_chars) {
            truncpos = this->input_chars - 55;
        }
        newpos = this->cursorpos - truncpos;

        // ugly ugly, but works great :)
        if (!this->input_hidden) {
            c = *(this->input + this->cursorpos);
            c2 = *(this->input + truncpos + 55);
            *(this->input + this->cursorpos) = '\0';
            *(this->input + truncpos + 55) = '\0';
            mvwaddstr(this->window_input, 3, 2, this->input + truncpos);
            *(this->input + this->cursorpos) = c;
            mvwaddstr(this->window_input, 3, 2 + newpos + 1,
                      this->input + truncpos + newpos);
            *(this->input + truncpos + 55) = c2;
        } else {
            c = *(this->hidden_input + cursorpos);
            c2 = *(this->hidden_input + truncpos + 55);
            *(this->hidden_input + this->cursorpos) = '\0';
            *(this->hidden_input + truncpos + 55) = '\0';
            mvwaddstr(this->window_input, 3, 2, this->hidden_input + truncpos);
            *(this->hidden_input + this->cursorpos) = c;
            mvwaddstr(this->window_input, 3, 2 + newpos + 1,
                      this->hidden_input + truncpos + newpos);
            *(this->hidden_input + truncpos + 55) = c2;
        }
        // draw "cursor"
        wattrset(this->window_input, COLOR_PAIR(STYLE_WHITE) | this->inverse_mono);
        mvwaddch(this->window_input, 3, 2 + newpos, ' ');
        x = 2 + newpos;
    }
    wmove(this->window_input, 3, x);
    update_panels();
    //move(this->terminal_max_y / 2 - 2+3, this->terminal_max_x / 2 - 30+x);
    doupdate();
}

void CDisplayHandler::InputAppendChar(char c) {
    if (this->input_chars < this->input_maxchars) {
        // we are allowed to append a char
        // this should work
        int i = 0;

        while (*(this->input + this->cursorpos + i) != '\0') {
            i++;
        }
        while (i >= 0) {
            *(this->input + this->cursorpos + i + 1) =
                *(this->input + this->cursorpos + i);
            i--;
        }
        //strcpy(this->input + this->cursorpos+1,this->input + this->cursorpos);
        *(this->input + this->cursorpos) = c;
        //*(this->input + this->input_chars + 1) = '\0';

        if (this->input_hidden) {
            *(this->hidden_input + this->input_chars) = '*';
            *(this->hidden_input + this->input_chars + 1) = '\0';
        }

        (this->input_chars)++;
        (this->cursorpos)++;
        this->UpdateInput();
    }
}

void CDisplayHandler::InputRight(void) {
    if (this->cursorpos < this->input_chars) {
        (this->cursorpos)++;
        this->UpdateInput();
    }
}

void CDisplayHandler::InputLeft(void) {
    if (this->cursorpos > 0) {
        (this->cursorpos)--;
        this->UpdateInput();
    }
}

void CDisplayHandler::InputEnd(void) {
    this->cursorpos = this->input_chars;
    this->UpdateInput();
}

void CDisplayHandler::InputHome(void) {
    this->cursorpos = 0;
    this->UpdateInput();
}

void CDisplayHandler::InputBackspace(void) {
    if (this->cursorpos > 0) {
        memmove(this->input + this->cursorpos - 1, this->input + this->cursorpos, strlen(this->input + this->cursorpos) + 1);
        if (this->input_hidden)
            *(this->hidden_input + this->input_chars - 1) = '\0';

        (this->input_chars)--;
        (this->cursorpos)--;
        this->UpdateInput();
    }
}

void CDisplayHandler::InputDelline(void) {
    this->input_chars = 0;
    this->cursorpos = 0;
    *(this->input) = '\0';
    if (this->input_hidden)
        *(this->hidden_input) = '\0';

    this->UpdateInput();
}

