#ifndef __KEYHANDLER_H
#define __KEYHANDLER_H

class CKeyHandler {
    private:
        bool want_quit, bm_save, want_local;

        void PostToDisplay(int msg);
        void PostToDisplay(int msg, int extended);
        void PostToDisplay(int msg, int extended, int function_key);

        void ShutdownServers(void);

    public:
        CKeyHandler();
        ~CKeyHandler();
        bool Init(void);
        void Loop(void);
        void WantQuit(bool bm_save) {
            this->want_quit = TRUE;
            this->bm_save = bm_save;
        };
        void WantFireupLocal(void) {this->want_local = TRUE;};
};

#endif
