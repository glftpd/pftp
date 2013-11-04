SSLDIR          = /usr
CPPFLAGS        = -Wall -D_REENTRANT -I../include -O2 -DTLS
LINKFLAGS       = -lpanel -lcurses -lpthread -lssl -lcrypto -DTLS -ldl
DO              = strip pftp
OBJS            = tlsutil.o
BINARY          = pftp
CPP             = g++
.cc.o	:
	        $(CPP) -c $(CPPFLAGS) $<

all	:	main.o switchserver.o tcp.o displayupdate.o server.o bookmark.o opensitedialog.o displayhandler.o keyhandler.o $(OBJS)
		$(CPP) -o pftp main.o switchserver.o tcp.o displayupdate.o server.o bookmark.o opensitedialog.o displayhandler.o keyhandler.o $(OBJS) $(LINKFLAGS)
		$(DO)
		cp -f $(BINARY) ..

tcp.o       :	../include/tcp.h ../include/defines.h ../include/tlsutil.h
tlsutil.o   :	../include/tlsutil.h
bookmark.o  :	../include/tcp.h ../include/server.h ../include/defines.h ../include/displayhandler.h ../include/keyhandler.h ../include/tlsutil.h
displayhandler.o   :	../include/tcp.h ../include/server.h ../include/defines.h ../include/displayhandler.h ../include/keyhandler.h ../include/tlsutil.h
displayupdate.o    :	../include/tcp.h ../include/server.h ../include/defines.h ../include/displayhandler.h ../include/tlsutil.h
keyhandler.o       :	../include/tcp.h ../include/server.h ../include/defines.h ../include/displayhandler.h ../include/keyhandler.h ../include/tlsutil.h
main.o      :	../include/tcp.h ../include/server.h ../include/defines.h ../include/displayhandler.h ../include/keyhandler.h ../include/tlsutil.h
opensitedialog.o   :	../include/tcp.h ../include/server.h ../include/defines.h ../include/displayhandler.h ../include/tlsutil.h
server.o    :	../include/tcp.h ../include/server.h ../include/defines.h ../include/displayhandler.h ../include/tlsutil.h
switchserver.o     :	../include/tcp.h ../include/server.h ../include/defines.h ../include/displayhandler.h ../include/tlsutil.h


dynamic	:	main.o switchserver.o tcp.o displayupdate.o server.o bookmark.o opensitedialog.o displayhandler.o keyhandler.o $(OBJS)
		$(CPP) -o pftp main.o switchserver.o tcp.o displayupdate.o server.o bookmark.o opensitedialog.o displayhandler.o keyhandler.o $(OBJS) $(LINKFLAGS)
		$(DO)
		cp -f $(BINARY) ..

static	:	main.o switchserver.o tcp.o displayupdate.o server.o bookmark.o opensitedialog.o displayhandler.o keyhandler.o $(OBJS)
		$(CPP) -o pftp main.o switchserver.o tcp.o displayupdate.o server.o bookmark.o opensitedialog.o displayhandler.o keyhandler.o $(OBJS) $(LINKFLAGS) -static
		$(DO)
		cp -f $(BINARY) ..

clean	:	
		rm -f *.o; rm -f ../$(BINARY);rm -f $(BINARY)


