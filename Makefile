CFLAGS += -std=c99 -g -O3 -Wall #-Werror
LDLIBS += -lmosquitto

# Uncomment this to print out debugging info.
CFLAGS += -DDEBUG

PROJECT=telexmqtt

all: ${PROJECT}

client: telexmqtt.o

client.o: Makefile

clean:
	rm -rf *.o ${PROJECT}
