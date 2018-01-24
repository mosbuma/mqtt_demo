# telex-mqtt

This tool is used to send text messages over MQTT to our demo telex

## Installing (Ubuntu)

install libmosquitto:

    sudo apt-get install libmosquitto-dev

sendmessage script needs the mosquitto commandline clients:

    sudo apt-get install mosquitto-clients

run make in the main folder to build telexmqtt

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
    