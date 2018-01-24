# telex-mqtt

This toolset is used to send text messages over MQTT to our demo telex.

It consists of 2 programs:

  * telexCtrl - commandline utility to send text to / read texts from a telex

  * telexmqtt - commandline utility that listens for messages on a channel on a MQTT broker and sends these to a telex

See the --help options in the utilities for more details

## Installing (Ubuntu)

install libmosquitto:

    sudo apt-get install libmosquitto-dev

sendmessage script needs the mosquitto commandline clients:

    sudo apt-get install mosquitto-clients

run make in the main folder to build telexmqtt

Note: always run telexmqtt with -d / --dummy option, otherwise the utility will crash

## Installing (Raspbian wheezy / jessie <- not tested)

Check raspbian version:

    cat /etc/os-release

Installing the libraries:

[[source: https://mosquitto.org/2013/01/mosquitto-debian-repository/](URL)]

    sudo apt-get update
    wget http://repo.mosquitto.org/debian/mosquitto-repo.gpg.key
    sudo apt-key add mosquitto-repo.gpg.key
    cd /etc/apt/sources.list.d/
    cat /etc/os-release
    # sudo wget http://repo.mosquitto.org/debian/mosquitto-wheezy.list
    # sudo wget http://repo.mosquitto.org/debian/mosquitto-jessie.list
    apt-get update
    sudo apt-get update
    apt-cache search mosquitto
    sudo apt-get install libmosquitto-dev mosquitto-clients

clone the repo and make

    git clone git@github.com:mosbuma/telex-mqtt.git
    cd telex-mqtt/
    make

run

    ./telexmqtt

use the sendmessages script from another terminal window to simulate transmissions

    sendmessages <host> [<port> [<delay in seconds>]]

## Testing with MQTT

The utility monitors /telex/incoming-sat/ channel on the MQTT broker for incoming messages.

Use an MQTT client such as MQTT.fx to connect to the broker and manually send messages.

Start the utility with telex

    sudo ./telexmqtt -n <server> -p 1883

Start the utility without telex (messages are printed in the console)

   ./telexmqtt -n <server> -p 1883 --dummy

For testing you can also use the sendmessages script from a second terminal

    ./sendmessages <server> <port> <interval>
