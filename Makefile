#   help:http://albert-oma.blogspot.tw/2012/04/cunit.html
	#error : dereferencing pointer to incomplete type 「WINDOW {aka struct _win_st}」=> -DNCURSES_OPAQUE=0
	#grep "struct _win_st" /usr/include/ -R 
	#	in /usr/include/ncurses.h line 418: #if !NCURSES_OPAQUE
	#	in /usr/include/ncurses.h line 109:
											#ifndef NCURSES_INTERNALS
											#define NCURSES_OPAQUE 1
											#endif => -DNCURSES_INTERNALS
all:
	gcc server.c -o server -pthread #-lsocket -lnsl
	gcc client.c -o client -pthread -lcurses -DNCURSES_INTERNALS -DNCURSES_OPAQUE=0 #-lsocket -lnsl 
clean:
	rm -f server client
