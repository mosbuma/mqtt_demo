#ifndef TELEX_H
#define TELEX_H

#include <exception>
#include <stdint.h>
#include <time.h>

class telexMemoryException: public std::exception
{
	virtual const char* what() const throw()
	{
		return "Could not open /dev/mem for reading and writing";
	}
};

class telexMMAPException: public std::exception
{
	virtual const char* what() const throw()
	{
		return "Call to mmap failed";
	}
};

class telex
{
	private:
		volatile unsigned *gpio;
		unsigned ioPinMask;

	public:
		uint8_t pinWriterOut;
		uint8_t pinKeyboardIn;
		uint8_t pinPowerControl;
		uint8_t pinColorControl;

		static const uint8_t alphabet1[32];
		static const uint8_t alphabet2[32];

		time_t powerState;
		uint8_t powerTimeout;
		uint8_t currentAlphabet;
		uint8_t cursorPos;

	public:
		telex(uint8_t pinWriterOut=17, uint8_t pinKeyboardIn=18, uint8_t pinPowerControl=27, uint8_t pinColorControl=23, uint8_t legacyIOMapping=0, uint8_t powerTimout=10);
		unsigned pin2Mask(uint8_t pin);
		void digitalWrite(uint8_t pin, uint8_t value, uint8_t filter=1);
		uint8_t digitalRead(uint8_t pin);
		void setColor(uint8_t redBlack=0);
		void setPower(uint8_t onOff);
		uint8_t getPower(void);
		void setPowerTimout(void);
		uint8_t checkPowerTimeout(void);
		uint8_t getBaudotAlphabet(uint8_t *data);
		uint8_t encodeBaudotChar(uint8_t *data);
		uint8_t decodeBaudotChar(uint8_t data);
		uint8_t isBaudotPrintChar(uint8_t data);
		void printBaudotChar(uint8_t data);
		void updateState(uint8_t data);
		void sendRawChar(uint8_t data);
		uint8_t detectStartBit(void);
		uint8_t receiveRawChar(uint8_t localEcho=1);
		void sendChar(uint8_t data, uint8_t filter=1);
		void sendString(uint8_t *data, uint8_t filter=1);
		uint8_t receiveChar(uint8_t localEcho=1);
};
#endif
