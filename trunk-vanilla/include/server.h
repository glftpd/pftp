#ifndef __SERVER_H
#define	__SERVER_H

typedef struct _FILELIST
{
	char			*name;
	char			owner[9], group[9],mode[11], date[13];
	unsigned long		size;
	time_t			time;
	bool			is_dir, is_marked;
	int			magic;
	struct _FILELIST	*next;
} FILELIST;

#define TYPE_I 0
#define TYPE_A 1

class CServer {
private:
        int type;
	int		magic, bm_magic, server_type, error, noop_slept, refresh_slept, pid;
	bool		have_undo, use_local;
        char            label[ALIAS_MAX+1];
	char		*busy, *param, working_dir[SERVER_WORKINGDIR_SIZE], undo_cwd[SERVER_WORKINGDIR_SIZE], undo_mkd[256], rename_temp[INPUT_TEMP_MAX], temp_string[PATHLEN];
        int            fxpsource,fxpdest,fxpmethod;
        int		sort_method;
	pthread_t	thread;
	pthread_mutex_t	busy_lock, cwd_lock, filelist_lock, displaymsg_lock;
	CTCP		tcp;
	FILELIST	*actual_filelist, *internal_filelist, *fl_src;
	BOOKMARK	prefs;
	DIRCACHE	*dir_cache;
	ACTION_MSG_LIST	*display_msg_stack;
	struct timeval	fxptime;
        bool            pasv_listing;
	bool		urgent, is_busy, use_stealth,is_transfering;
		
	void		PostBusy(char *);
	
	bool            SetProt(int i);
	bool		LocalChangeWorkingDir(char *dir);
	void		LocalGetWorkingDir(void);
	void		LocalGetDirlist(void);
	void		LocalMakeDir(char *, bool);
	void		LocalDeleteFile(char *);
	void		LocalDeleteDir(char *);
	void		LocalRenFrom(char *);
	void		LocalRenTo(char *);
	void		PostToDisplay(int msg);

	void		HandleMessage(int msg, char *param, int magic);
	void		EvalError(void);
	void		SortFilelist(bool);

	void		PostStatusFile(char *, char *);
	void		PostStatusFile(char *, char *, char *);
	bool		WaitForStatusFile(int, char *, char *);
	bool		WaitForStatusFile(int, char *, char *, char *);
	
	// remote actions
	void		KillMe(bool);
	bool		Login(void);
	bool		Noop(void);
	bool		ChangeWorkingDir(char *dir);
	bool		GetWorkingDir(void);
	bool		MakeDir(char *, bool, bool);
	bool		DeleteFile(char *);
	bool		DeleteDir(char *);
	void		RenFrom(char *);
	bool		RenTo(char *);
	bool		SendSITE(char *);
	bool		Nuke(char *);
	bool		UnNuke(char *);
	bool	        Wipe(char *);
	bool		UnDupe(char *);
	void		FormatFilelist(char *);
	void		UseDirCache(void);
	bool		RefreshFiles(void);
	bool		LeechFile(char *, int, bool, int);
	bool		LeechDir(char *, int, bool, int);
        bool            DoFXP(int dest_magic,bool as_ok,bool);
        bool            DoFXPStop(int dest_magic);
        bool            DoFXPDir(char *dir, int dest_magic);
        bool            DoFXPDirCWDUP(char *dir, int dest_magic);
//        bool            CopyFXPDirStart(char *dir, int dest_magic);
//        bool            CopyFXPDirStop(int dest_magic,char *);
//	bool		FXPFileSrc(char *, int);
	bool		DoFXPFile(char *, bool,int);
//	bool		FXPFileStart(char *, int);
//	bool		FXPFileStop(int,char *);
	bool		UploadFile(char *, bool, bool);
	void		UploadDirStart(char *);
	bool		UploadDir(char *, bool, bool);
	void		AddEntryToCache(char *);
        void            RemoveFromQueue(int,char *);
	bool		CheckStealth(char *);
        void            StartTime(void);		
        bool		stop_transfer;
	CServer         *destsrv;
public:
	CServer();
	~CServer();

	void            StopTransfer(void);
	long            GetTime(void);
        int             GetFXPSourceMagic(void) {return fxpsource;};
        int             GetFXPDestMagic(void) {return fxpdest;};
	void		SetMagic(int magic) {this->magic = magic;};
	int		GetMagic(void) {return(this->magic);};
	void		SetServerType(int type) {this->server_type = type;};
	int		GetServerType(void) {return(this->server_type);};
	void		SetServerPrefs(BOOKMARK *bm);
	BOOKMARK	*GetPrefs(void) {return(&(this->prefs));};
	char		*GetSiteAlias(int);
	bool		GetChaining(void) {return(this->prefs.use_chaining);};
	char		*GetFilter(void);
	CTCP		*GetTCP(void) {return(&(this->tcp));};
	pthread_t	*GetThreadAddress(void) {return(&(this->thread));};
	pthread_t	GetThread(void) {return(this->thread);};
	void		Run(void);
	void		ObtainWorkingDir(char *);
	FILELIST	*ObtainFilelist(bool *use_jump);
	char		*ObtainBusy(void);
	void		PostFromDisplay(int msg, char *param);
	void		PostUrgentFromDisplay(int msg, char *param);
	void		PostFromDisplay(int msg, char *param, int magic);
	char		*GetAlias(void);
	void		SetBMMagic(int bmm) {this->bm_magic = bmm;};
	int		GetBMMagic(void) {return(this->bm_magic);};
	int		GetPID(void) {return(this->pid);};
	float		GetSpeed(void) {return(this->tcp.GetSpeed());};
	bool		IsBusy(void) {return(this->is_busy);};
	bool		IsTransfering(void) {return(this->is_transfering);};
	void		SetTransfering(void) {this->is_transfering=TRUE;};
	void		SetTransferFilelist(FILELIST *fl_src) {this->fl_src=fl_src;};
	void		SetDestServer(CServer *dest) {this->destsrv=dest;};
	void		SetSrcServer(CServer *src) {this->destsrv=src;};
	void		ClearTransfering(void) {this->is_transfering=FALSE;};
        int             nossl;	
#ifdef TLS
        int             TLS_Status(void) { return this->tcp.TLS_Status(); }
#endif

};

typedef struct _SERVERLIST
{
	CServer			*server;
	int			magic;
	struct _SERVERLIST	*next;
} SERVERLIST;

int wild_match(unsigned char *m, unsigned char *n);
#endif
