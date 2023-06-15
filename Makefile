PORT=31337
CFLAGS= -DPORT=$(PORT) -g -Wall
CLIENT_DIR=helpers
SERVER_DIR=helpers

CLIENT_SRC = dbclient.c
SERVER_SRC = dbserver.c

CLIENT_HEADERS = $(wildcard *.h)
SERVER_HEADERS = $(wildcard *.h)

all: dbclient dbserver

dbclient: $(CLIENT_DIR)/wrapsock.c $(CLIENT_DIR)/writen.c $(CLIENT_DIR)/readn.c $(CLIENT_SRC)
	gcc ${CFLAGS} -o $@ $^

dbserver: $(SERVER_DIR)/wrapsock.c $(SERVER_DIR)/writen.c $(SERVER_DIR)/readn.c $(SERVER_DIR)/filedata.c $(SERVER_SRC)
	gcc ${CFLAGS} -o $@ $^

clean:
	rm -f dbclient dbserver

.PHONY: clean
