CFLAGS = -Wall -g

all: server subscriber

# Compileaza server.cpp
server: server.cpp

# Compileaza subscriber.cpp
subscriber: subscriber.cpp

.PHONY: clean

clean:
	rm -f server subscriber
