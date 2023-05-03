CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

# Portul pe care asculta serverul
PORT = 12345

# Adresa IP a serverului
IP_SERVER = 127.0.0.1

ID = "100"

all: server subscriber

common.o: common.cpp

# Compileaza server.c
server: server.cpp common.o

# Compileaza subscriber.c
subscriber: subscriber.cpp common.o

.PHONY: clean run_server run_client

clean:
	rm -rf server subscriber *.o *.dSYM
