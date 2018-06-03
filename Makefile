CPP=g++
CPFLAGS=-g
BIN=client server

.PHONY:clean all

all:$(BIN)

client:chatcli.cpp
	$(CPP) $(CPFLAGS) $^ -o $@

server:chatsrv.cpp
	$(CPP) $(CPFLAGS) $^ -o $@

clean:
	rm -f $(BIN)
