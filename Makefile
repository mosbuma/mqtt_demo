CC = g++
CFLAGS += -O3 -g3 -Wall -fPIC #-Werror
LDLIBS += -lmosquitto

# Uncomment this to print out debugging info.
CFLAGS += -DDEBUG

all: telexmqtt telexCtrl

telexmqtt:
	$(CC) $(CFLAGS) "telex.cpp" "telexmqtt.cpp" -o "telexmqtt" $(LDLIBS)

telexCtrl:
	$(CC) $(CFLAGS) "telex.cpp" "telexCtrl.cpp" -o "telexCtrl" $(LDLIBS)

clean:
	rm -rf *.o telexmqtt telexCtrl
