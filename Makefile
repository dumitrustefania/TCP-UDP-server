CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

# Portul pe care asculta serverul
PORT = 12345

# Adresa IP a serverului
IP_SERVER = 127.0.0.1

all: server subscriber

common.o: common.cpp

# Compileaza server.cpp
server: server.cpp common.o

# Compileaza subscriber.cpp
subscriber: subscriber.cpp common.o

.PHONY: clean run_server run_client

clean:
	rm -rf server subscriber *.o
