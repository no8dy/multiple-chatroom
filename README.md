# multiple-chatroom
Use ncurses and socket make a multiple-chatroom

environment:Mint(Linux)

compiler:gcc

compile flags:

server: -pthread -lsocket -lnsl

client: -pthread -lsocket -lnsl -lncurses (if your don't have ncurses , use -lcurses)
