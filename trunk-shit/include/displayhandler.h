#ifndef __DISPLAYHANDLER_H
#define __DISPLAYHANDLER_H

typedef struct _SWITCH {
    bool used;
    int magic_left, magic_right;
} _SWITCH;

class CDisplayHandler {
    private:

        struct _SWITCH switch_list[19];
        void NextSwitch(bool);
        int cursorpos;
        bool use_pasv, use_ssl, use_ssl_list, use_ssl_data, use_stealth_mode;
        bool use_ssl_fxp, use_alt_fxp, use_rev_file, use_rev_dir;
        int waiting;
        bool thread_attr_init, screen_too_small;
        bool thread_running, default_windows, bm_save;
        int inverse_mono, terminal_max_y, terminal_max_x, status_win_size;
        int window_left_width, window_right_width;
        int leftwindow_magic, rightwindow_magic;
        int jump_stack_left[JUMP_STACK], jump_stack_right[JUMP_STACK],
            jump_stack_left_pos, jump_stack_right_pos;
        char password[PASSWORD_SIZE], password_new[PASSWORD_SIZE],
            password_confirm[PASSWORD_SIZE], custom_password[PASSWORD_SIZE],
            temp_string[512], *statuslog[STATUS_LOG];
        char input_temp[INPUT_TEMP_MAX], old_input_temp[INPUT_TEMP_MAX];
        float speed_left, speed_left_old, speed_right, speed_right_old;
        int time_left, time_right;
        bool statuslog_highlight[STATUS_LOG];
        MSG_LIST *message_stack;
        SERVER_MSG_LIST *server_message_stack;
        STATUS_MSG_LIST *status_msg_stack;
        char *leftwindow_busy, *rightwindow_busy, *log[LOG_LINES],
            window_left_cwd[SERVER_WORKINGDIR_SIZE],
            window_right_cwd[SERVER_WORKINGDIR_SIZE],
            window_left_label[ALIAS_MAX + 5], window_right_label[ALIAS_MAX + 5];

        FILELIST *filelist_left, *filelist_right;
        int filelist_left_magic, filelist_right_magic, filelist_left_ypos,
            filelist_right_ypos;
        int filelist_left_entries, filelist_right_entries;
        int filelist_left_soffset, filelist_right_soffset;
        int xsite_buttonstate, pre_buttonstate;
        int filelist_left_format, filelist_right_format;
        WINDOW *window_welcome, *window_switch, *window_log, *window_tab,
            *window_input, *window_notice, *window_prefs, *window_dialog,
            *window_probe, *window_status, *window_command, *window_left,
            *window_right, *window_pre;
        PANEL *panel_welcome, *panel_switch, *panel_log, *panel_tab,
            *panel_input, *panel_notice, *panel_prefs, *panel_dialog,
            *panel_probe, *panel_status, *panel_command, *panel_left,
            *panel_right, *panel_pre;
        void InitColors(void);
        void OpenDefaultWindows(void);
        void DrawCommandKeys(char *, int);
        void RebuildScreen(int, int);
        void BuildScreen(void);
        void HandleMessage(int msg, int extended, int function_key);
        void HandleServerMessage(int msg, int magic, char *data);
        void KillServer(CServer *server);
        void PostToServers(int msg, char *message);

        // update routines
        char *GetFilelistString(FILELIST *, int, int, bool *, int, int);
        FILELIST *GetFilelistEntry(FILELIST *, int);
        void UpdateServerCWD(CServer *, WINDOW *window);
        void UpdateServerFilelist(CServer *, WINDOW *window);
        void UpdateTabBar(bool dont_remove);
        void UpdateFilelistByChar(char);
        void UpdateFilelistNewPosition(WINDOW *window);
        void UpdateFilelistScroll(bool dir_up, bool);
        void UpdateFilelistPageMove(bool dir_up);
        void UpdateFilelistPageEnd(bool dir_up);
        void UpdateServerBusy(WINDOW *, char *);
        void UpdateServerLabel(int);
        void FetchBusy(CServer *, WINDOW *);
        void FileToggleMark(void);
        void UpdateXSiteButtons(void);
        void UpdateMode(void);
        void DetermineScreenSize(void);
        void LeechOkayNowUpload(int, char *, bool, bool);
        void DirOkayNowMKD(int magic, char *dir, bool single);
        void DirOkayNowCWD(int magic, char *dir, bool single);
        void TransferOkayNowRefresh(int, bool);
        void ResetTimer(void);
        void UpdateSpeed(bool left_win, bool force);
        void DrawSpeed(bool left_win, bool force);
        void DrawTime(bool left_win, bool force, long);
        void ChangeFileSorting(void);
        void ChangeDirSorting(void);
        void ChangeReverseSorting(bool dirlist);
        void JumpSwitch(int i);
        void AssignSwitch(int i);
        void AutoAssignSwitch(void);
	int IsOkSwitchKey(int i); 

        // view window
        int view_line, view_lines_max;
        char view_buffer[VIEW_BUFFER_MAX], view_filename[SERVER_WORKINGDIR_SIZE + 64];
        char view_fname[SERVER_WORKINGDIR_SIZE + 64];
        void NoticeViewFile(char *);
        void ViewFile(void);
        bool OpenView(void);
        void CloseView(void);
        void RefreshView(void);

        // switch window
        int switch_pos, switch_count, switch_start;

        void OpenSwitchWindow(int magic);
        void CloseSwitchWindow(void);
        void ScrollSwitch(bool);
        void PageMoveSwitch(bool);
        void PageMoveSwitchEnd(bool);
        void RedrawSwitchWindow(void);
        void ScrollView(bool);
        void PageMoveView(bool);
        void PageMoveViewEnd(bool);

        // log window
        int log_start;

        CServer *TryOpenLog(void);
        void OpenLog(CServer *server);
        void ScrollLog(bool dir_up);
        void PageMoveLog(bool dir_up);
        void PageMoveLogEnd(bool dir_up);
        void RefreshLog(void);
        void CloseLog(void);

        // prefs dialog
        char alias[ALIAS_MAX], hostname[HOSTNAME_MAX], username[USERNAME_MAX],
            pass[PASSWORD_MAX], noop_cmd[INPUT_TEMP_MAX], first_cmd[INPUT_TEMP_MAX];
        char startdir[STARTDIR_MAX], exclude_source[EXCLUDE_MAX], pre_cmd[INPUT_TEMP_MAX][GAMEUTILDIR_MAX], 
            exclude_target[EXCLUDE_MAX], sectionList[NUM_SECTIONS][GAMEUTILDIR_MAX],
            site_rules[SITE_MAX], site_user[SITE_MAX], site_wkup[SITE_MAX], num_string[10];
        char nuke_prefix[256], msg_string[256];
        int retry_counter, retry_delay, refresh_rate, noop_rate, file_sorting,
            dir_sorting;
        bool use_refresh, use_noop, use_startdir, use_exclude_source,
            use_exclude_target, use_autologin, use_chaining, use_jump, use_track,
            use_sections, use_retry, use_firstcmd, use_rndrefr;
        int prefsdialog_buttonstate, prefs_inputtype;
        CServer *siteprefs_server;

        void OpenPrefsDialog(void);
        void ClosePrefsDialog(void);
        void UpdatePrefsItems(void);

        void FillInfoForPrefs(void);
        void PrefsAddSite(void);
        void PrefsModifySite(void);
        void UpdateSitePrefs(CServer *);
        CServer *TryPrefs(int *);

        // siteopen dialog
        int siteopen_buttonstate;

        void OpenSiteDialog(void);
        void CloseSiteDialog(void);
        void UpdateSiteOpenButtons();

        // bookmark-selection in opensite
        int siteopen_bm_magic, siteopen_bm_startmagic, siteopen_bm_realmagic;

        void ScrollBookmarkSites(bool dir_up);
        void PageMoveBookmarkSites(bool dir_up);
        void PageMoveBookmarkSitesEnd(bool dir_up);
        void MoveBookmarkByChar(char);
        void WipeBookmark(void);
        void RedrawBookmarkSites(void);

        // status log
        int status_line;

        void ScrollStatusUp(void);
        void ScrollStatusDown(void);
        void DisplayStatusLog(void);

        // misc
        //void ShowWelcome(void);
        //void HideWelcome(void);

        void DialogPre(void);
	void DialogPreUpdate(void);
	void DialogPreClose(void);

        void PostUrgent(int, int, char *);
        void AutoLogin(void);
        void GetSiteAlias(int);
        int TrySite(WINDOW *);
        void MarkFiles(bool);
        //bool NamesEqual(char *, char *);
        //int DoCompare(FILELIST *, FILELIST *,char *,int);
        void CompareFiles(void);
        void Sections(int);
        void RefreshSite(bool);
        void RefreshSiteS(void);
        void DeleteFile(void);
        void TransferFile(bool, bool);
        void PrepDir(int);
        void WipePrep(void);
        void ChangeDir(int);
        void DeMark(WINDOW *, char *);
        void Mark(WINDOW *, char *);

    public:
        void AddStatusLine(char *, bool);
        // bookmark stuff
    public:
        int ProbeBookmarkRC(void);
        void sigwinch(int);
        void SaveBookmarks();

    private:
        int input_chars, input_maxchars, input_cursorpos, pass_pos;
        bool input_hidden, for_all;
        char *input, hidden_input[512];

        void NoticeNoBookmark(void);
        void DialogNotice(char *, char *);
        void CloseNotice(void);
        void DialogInput(char *, char *, int, bool);
        void DialogXSite(bool);
        bool CloseSiteInput(void);
        void CloseInput(void);
        void InputAppendChar(char);
        void InputRight(void);
        void InputLeft(void);
        void InputHome(void);
        void InputEnd(void);
        void InputBackspace(void);
        void InputDelline(void);
        void UpdateInput(void);

        void OpenPasswordInput(bool hidden);
        void OpenChangePasswordInput(bool confirm);
        void NoticeNoPasswd(void);
        void NoticeBadPasswd(void);
        void NoticeNoMatch(void);
        bool ReadBookmarks(void);
        void Decrypt(char *in, char *out);
        void Encrypt(char *in, char *out);

        // fireup stiff
        void FireupLocalFilesys(void);
        int FireupServer(char *);

        // to server
        void PostToServer(int, bool, int);
        void CommandLineLogin(void);
        void UdpLogin(void);

	void AutoTransferDir(void);

        void FreeWindow(WINDOW *);
        void PerformSwitch(void);
        void PerformSwitchClose(void);
        void CloseSite(void);

    public:
        int internal_state, internal_state_previous;
        pthread_t thread;
        pthread_attr_t thread_attr;
        pthread_mutex_t message_lock, server_message_lock, status_lock;

        CDisplayHandler();
        ~CDisplayHandler();

        bool Init(void);
        void Loop(void);

        void PostMessage(int msg);
        void PostMessage(int msg, int extended);
        void PostMessage(int msg, int extended, int function_key);
        void PostMessageFromServer(int msg, int magic, char *);
        void PostStatusLine(char *line, bool highlight);
        void DrawSwitchPairs(void);
        int GetPreDest(void) { return this->pre_buttonstate; }
};

void flag_winch (int dummy);
void mywborder(WINDOW *win);

#endif
