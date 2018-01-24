#ifndef TELEX_H
#define TELEX_H

#include <exception>
#include <stdint.h>

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
		uint8_t pinPowerPhase;

		static const uint8_t alphabet1[32];
		static const uint8_t alphabet2[32];

		uint8_t currentAlphabet;

	public:
		telex(uint8_t pinWriterOut=17, uint8_t pinKeyboardIn=18, uint8_t pinPowerControl=27, uint8_t pinPowerPhase=22, uint8_t legacyIOMapping=0);
		unsigned pin2Mask(uint8_t pin);
		void digitalWrite(uint8_t pin, uint8_t value, uint8_t filter=1);
		uint8_t digitalRead(uint8_t pin);
		void setPower(uint8_t onOff);
		uint8_t baudotGetAlphabet(uint8_t *data);
		uint8_t baudotEncodeChar(uint8_t *data, uint8_t filter=1);
		uint8_t baudotDecodeChar(uint8_t data);
		void baudotSendChar(uint8_t data);
		uint8_t detectStartBit(void);
		uint8_t baudotReceiveChar(uint8_t localEcho=0);
		void sendChar(uint8_t data, uint8_t filter=1);
		void sendString(uint8_t *data, uint8_t filter=1);

};
#endif
