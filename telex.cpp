#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "telex.h"

//#include <sys/time.h>
//#include <math.h>

// Raspberry PI direct IO based on: https://elinux.org/Rpi_Datasheet_751_GPIO_Registers
#define PERIPHERAL_BASE 0x3F000000
#define PERIPHERAL_BASE_LEGACY 0x20000000
#define GPIO_BASE (PERIPHERAL_BASE + 0x200000)
#define GPIO_BASE_LEGACY (PERIPHERAL_BASE_LEGACY + 0x200000)

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(this->gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(this->gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(this->gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(this->gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(this->gpio+10) // clears bits which are 1 ignores bits which are 0
#define GPIO_GET *(this->gpio+13)

// delay in microseconds
#define POWER_UP_DELAY 4000000
#define POWER_DOWN_DELAY 2000000

// Telex ITA2 characterset defenitions
// https://en.wikipedia.org/wiki/Baudot_code
// https://en.wikipedia.org/wiki/Teleprinter
#define BAUDOT_LF 0x02
#define BAUDOT_CR 0x08
#define BAUDOT_WRU 0x09
#define BAUDOT_BELL 0x0b
#define BAUDOT_NULL 0x00
#define BAUDOT_ALPHABET_1 0x1f
#define BAUDOT_ALPHABET_2 0x1b
#define BAUDOT_NATIONAL_1 0x0d
#define BAUDOT_NATIONAL_2 0x14
#define BAUDOT_NATIONAL_3 0x1a

// $ = who are you? (WRU)
// % = bell
// * = null
// ~ = switch to letter alphabet (alphabet=1)
// ^ = switch to digit alphabet (alphabet=2)
// ! = national 1
// # = national 2
// & = national 3

const uint8_t telex::alphabet1[32]={'*','e',0x0a,'a',' ','s', 'i','u',0x0d,'d','r','j','n','f','c','k','t','z','l','w','h','y','p','q','o','b','g','~','m','x','v','^'};
const uint8_t telex::alphabet2[32]={'*','3',0x0a,'-',' ','\'','8','7',0x0d,'$','4','%',',','!',':','(','5','+',')','2','#','6','0','1','9','?','&','~','.','/','=','^'};

// SYMBOL_TIME = 20000 for 50 bd Telex (micro seconds)
// SYMBOL_TIME = 22000 for 45.5 bd Telex (micro seconds)
#define SYMBOL_TIME 20000

telex::telex(uint8_t pinWriterOut, uint8_t pinKeyboardIn, uint8_t pinPowerControl, uint8_t pinColorControl, uint8_t legacyIOMapping, uint8_t powerTimout)
{
	this->currentAlphabet=0;
	this->cursorPos=0;
	this->powerState=0;
	this->powerTimeout=powerTimout;

	unsigned long gpio_base_offset=(legacyIOMapping)?GPIO_BASE_LEGACY:GPIO_BASE;
	int mem_fd;
	if ((mem_fd=open("/dev/mem",O_RDWR|O_SYNC))<0) throw telexMemoryException();

	this->gpio = (volatile unsigned *) mmap(
		NULL,             //Any adddress in our space will do
		BLOCK_SIZE,       //Map length
		PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
		MAP_SHARED,       //Shared with other processes
		mem_fd,           //File to map
		gpio_base_offset  //Offset to GPIO peripheral
	);
	close(mem_fd); //No need to keep mem_fd open after mmap

	if (this->gpio==MAP_FAILED) throw telexMMAPException();

	this->pinKeyboardIn=pinKeyboardIn; // input
	INP_GPIO(this->pinKeyboardIn);

	this->pinWriterOut=pinWriterOut; // output
	INP_GPIO(this->pinWriterOut);
	OUT_GPIO(this->pinWriterOut);
	this->digitalWrite(this->pinWriterOut,1,0);

	this->pinColorControl=pinColorControl; // output
	INP_GPIO(this->pinColorControl);
	OUT_GPIO(this->pinColorControl);
	this->digitalWrite(this->pinColorControl,0,0);

	usleep(500000);

	this->pinPowerControl=pinPowerControl; // output
	INP_GPIO(this->pinPowerControl);
	OUT_GPIO(this->pinPowerControl);
	this->digitalWrite(this->pinPowerControl,0,0);

	this->ioPinMask=0;
}

unsigned telex::pin2Mask(uint8_t pin)
{
		return (1<<pin);
}

void telex::digitalWrite(uint8_t pin, uint8_t value, uint8_t filter)
{
	unsigned mask=this->pin2Mask(pin);
	if (value)
	{
		if (filter) GPIO_SET=mask&~this->ioPinMask; // only set pins that where clear to avoid glitches
		else GPIO_SET=mask;
		this->ioPinMask|=mask;
	}
	else
	{
		if (filter)	GPIO_CLR=mask&this->ioPinMask; // only clear pins that where set to avoid glitches
		else GPIO_CLR=mask;
		this->ioPinMask&=~mask;
	}
}

uint8_t telex::digitalRead(uint8_t pin)
{
	return (((GPIO_GET)&this->pin2Mask(pin))!=0);
}

void telex::setColor(uint8_t redBlack)
{
	this->digitalWrite(this->pinColorControl,redBlack);
  usleep(SYMBOL_TIME);
}

void telex::setPower(uint8_t onOff)
{
	this->digitalWrite(this->pinPowerControl,onOff);
	this->powerState=onOff?time(NULL):0;
  usleep(onOff?POWER_UP_DELAY:POWER_DOWN_DELAY);
}

uint8_t telex::getPower(void)
{
	return (this->powerState!=0);
}

void telex::setPowerTimout(void)
{
	this->powerState=time(NULL);
}

uint8_t telex::checkPowerTimeout(void)
{
		//printf("Power timeout time=%d\n",this->powerTimeout);
		//printf("Power state=%ld\n",this->powerState);
		//printf("Time=%ld\n",time(NULL));
		//printf("Diff=%d",(int)difftime(time(NULL),this->powerState));

		if ((this->powerState)&&(difftime(time(NULL),this->powerState)>=this->powerTimeout))
		{
			printf("[Power timeout -> cut power!]\n");
			this->setPower(0);
			return 1;
		}
		return 0;
}

uint8_t telex::getBaudotAlphabet(uint8_t *data)
{
  for (uint8_t zz=0;zz<32;zz++)
  {  // first search in current alphabet to prevent unnecesarry switching
     if (this->currentAlphabet==1)
     {
       if (this->alphabet1[zz]==data[0])
         return 1;
       if (this->alphabet2[zz]==data[0])
         return 2;
     }
     else
     {
       if (this->alphabet2[zz]==data[0])
         return 2;
       if (this->alphabet1[zz]==data[0])
         return 1;
     }
  }
  return 0;
}

uint8_t telex::encodeBaudotChar(uint8_t *data)
{
  // transform character so it is suitable to send to telex
  // function returns alphabet to be used
  uint8_t alphabet;

  // set to lowercase
  if ((data[0]>=65)&&(data[0]<=90))
		data[0]+=32;

	// replace all quote type to single quote (') character
  if ((data[0]=='"')||(data[0]=='`'))
		data[0]='\'';

	alphabet=this->getBaudotAlphabet(data);
  if (!alphabet) // character not in alphabet, so replace with '+' character
  {
    data[0]='+';
    alphabet=2;
  }

  for (uint8_t zz=0;zz<32;zz++)
  {
    if (alphabet==1)
    {
      if (this->alphabet1[zz]==data[0])
      {
      	data[0]=zz;
      	zz=32;
      }
    }
    else
    {
      if (this->alphabet2[zz]==data[0])
      {
        data[0]=zz;
        zz=32;
      }
    }
  }
  return alphabet;
}


uint8_t telex::decodeBaudotChar(uint8_t data)
{
  if (data==BAUDOT_ALPHABET_1) this->currentAlphabet=1;
  if (data==BAUDOT_ALPHABET_2) this->currentAlphabet=2;
  if (this->currentAlphabet==2) return this->alphabet2[data];
  else return this->alphabet1[data]; // assume alphabet is 1 if alphabet is not set (0)
}

uint8_t telex::isBaudotPrintChar(uint8_t data)
{
	switch(data)
	{
		case BAUDOT_CR:
		case BAUDOT_LF:
		case BAUDOT_BELL:
		case BAUDOT_NULL:
		case BAUDOT_ALPHABET_1:
		case BAUDOT_ALPHABET_2:
			return 0;
		default:
			break;
	}
	return 1;
}

void telex::printBaudotChar(uint8_t data)
{
	if (this->currentAlphabet==1)
	{
		switch(data)
		{
			case BAUDOT_NULL:
				printf("<NULL>");
				break;
			case BAUDOT_CR:
				printf("<CR>");
				break;
			case BAUDOT_LF:
				printf("<LF>\n");
				break;
			case BAUDOT_ALPHABET_1:
				printf("<ALPHABET1>");
				break;
			case BAUDOT_ALPHABET_2:
				printf("<ALPHABET2>");
				break;
			default:
				printf("%c",this->decodeBaudotChar(data));
				break;
		}
	}
	else
	{
		switch(data)
		{
			case BAUDOT_BELL:
				printf("<BELL>");
				break;
			case BAUDOT_WRU:
				printf("<WRU?>");
				break;
			case BAUDOT_NULL:
				printf("<NULL>");
				break;
			case BAUDOT_CR:
				printf("<CR>");
				break;
			case BAUDOT_LF:
				printf("<LF>\n");
				break;
			case BAUDOT_ALPHABET_1:
				printf("<ALPHABET1>");
				break;
			case BAUDOT_ALPHABET_2:
				printf("<ALPHABET2>");
				break;
			case BAUDOT_NATIONAL_1:
				printf("<NATIONAL1>");
				break;
			case BAUDOT_NATIONAL_2:
				printf("<NATIONAL2>");
				break;
			case BAUDOT_NATIONAL_3:
				printf("<NATIONAL3>");
				break;
			default:
				printf("%c",this->decodeBaudotChar(data));
				break;
		}
	}
}

void telex::updateState(uint8_t data)
{
	switch(data) // set current alphabet and update cursor position
	{
		case BAUDOT_LF:
		case BAUDOT_NULL:
			//printf("No print char!\n");
			break;
		case BAUDOT_ALPHABET_1:
		case BAUDOT_ALPHABET_2:
			this->currentAlphabet=(data==BAUDOT_ALPHABET_1)?1:2;
			//printf("Alphabet switched to %d\n",this->currentAlphabet);
			break;
		case BAUDOT_CR:
			//printf("Reset cursor pos!\n");
			this->cursorPos=0;
			break;
		case BAUDOT_BELL:
			if (this->currentAlphabet==1)
			{
				//printf("Print char (increment)!\n");
				this->cursorPos++;
			}
			else
				//printf("No print char!\n");
			break;
		default:
			//printf("Print char (increment)!\n");
			this->cursorPos++;
			break;
	}
	//printf("Current pos=%d\n",this->cursorPos);
}

void telex::sendRawChar(uint8_t data)
{
	uint8_t shift=data;
	if (!this->getPower())
		this->setPower(1);

	this->digitalWrite(this->pinWriterOut,0); // startbit
  usleep(SYMBOL_TIME);
  for (uint8_t zz=0;zz<5;zz++)
  {
    this->digitalWrite(this->pinWriterOut,shift&0x01);
    shift>>=1;
    usleep(SYMBOL_TIME);
  }
  this->digitalWrite(this->pinWriterOut,1); // stopbit
	if ((data==BAUDOT_ALPHABET_1)||(data==BAUDOT_ALPHABET_2))
		usleep(SYMBOL_TIME*5); // allow for some extra time to perform mechanical alphabet switch
	else
		usleep(SYMBOL_TIME*1.5); // TODO: tweak this for optimum speed ... 1.5 stopbits?


	this->printBaudotChar(data);
	this->updateState(data);
	this->setPowerTimout();
}

uint8_t telex::detectStartBit(void)
{
	if (!this->getPower())
		this->setPower(1);
  return (this->digitalRead(this->pinKeyboardIn)!=0);
}

uint8_t telex::receiveRawChar(uint8_t localEcho)
{
	// first call function detect startbit before calling this function
	uint8_t data=0;

  usleep(SYMBOL_TIME/2); // wait until we are half way into the start bit
  if (localEcho) this->digitalWrite(this->pinWriterOut,0);
  usleep(SYMBOL_TIME);
  for (uint8_t zz=0;zz<5;zz++)
  {
    if (!this->digitalRead(this->pinKeyboardIn))
    {
      if (localEcho)
				this->digitalWrite(this->pinWriterOut,1);
      data|=0x20;
    }
    else
    {
      if (localEcho)
				this->digitalWrite(this->pinWriterOut,0);
    }
    data>>=1;
    usleep(SYMBOL_TIME);
  }
  if (localEcho)
	{
		this->digitalWrite(this->pinWriterOut,1);
		this->updateState(data);
	}
  usleep(SYMBOL_TIME/2+1000); // wait until we are finished with the last bit to avoid detecting false startbit
	this->setPowerTimout();
	this->printBaudotChar(data);
	return data;
}

void telex::sendChar(uint8_t data, uint8_t filter)
{
	switch(filter) // first filter off illegal characters
	{
		case 4:
			if ((data=='!')||(data=='#')||(data=='&')) // filter off national characters
				data='+';
		case 3:
			if (data=='%') // filter off bell character
				data='+';
		case 2:
			if (data=='*') // filter off null character
				data='+';
		case 1:
			if ((data=='~')||(data=='^')||(data=='$')) // filter off char/digit switch (~), digit/char switch (^) and WRU - who are you ($)
				data='+';
		default:
			break;
	}

	uint8_t alphabet=this->encodeBaudotChar(&data);

	if (data==BAUDOT_CR)
	{
		//printf("Ignoring <CR>\n");
		return; // ignore <CR>, as it is added to <LF> automatically
	}
	if (data==BAUDOT_LF)
	{
		printf("[Translating <LF> to <CR><LF>]\n");
		this->sendRawChar(BAUDOT_CR); // insert <CR> before every <LF>
		this->sendRawChar(BAUDOT_LF);
		return;
	}
	if ((this->isBaudotPrintChar(data))&&(this->cursorPos>=69)) // 69 characters per line, automatically insert CR and LF on line end
	{
		printf("[Inserting extra <CR><LF>]\n");
		this->sendRawChar(BAUDOT_CR); // insert <CR> before every <LF>
		this->sendRawChar(BAUDOT_LF);
	}
	if (alphabet!=this->currentAlphabet)
  {
		this->sendRawChar(BAUDOT_NULL);
		this->sendRawChar((alphabet==1)?BAUDOT_ALPHABET_1:BAUDOT_ALPHABET_2);
		this->sendRawChar((alphabet==1)?BAUDOT_ALPHABET_1:BAUDOT_ALPHABET_2);
    this->currentAlphabet=alphabet;
  }
  this->sendRawChar(data);
}

void telex::sendString(uint8_t *data, uint8_t filter)
{
  uint16_t zz=0;
  while(data[zz])
  {
		this->sendChar(data[zz],filter);
    zz++;
  }
	printf("\n");
}

uint8_t telex::receiveChar(uint8_t localEcho)
{
	if (!this->detectStartBit()) return 0;
	return this->decodeBaudotChar(this->receiveRawChar(localEcho));
}


/*
int main(void)
{
	uint8_t send[]={'^','^','^','a','b','c','d','e','f','g','h','~','~','i','j','k','l','m','n','4','4','4','4','4','4','o','p','q','r','s','t','u','v','w','x','y','z','1','2','3','4','5','6','7','8','9','0','(',')',0x0a,0x0d,0};
	telex *t=new telex();
	t->sendString(send,0);
//	t->setPower(0); // don't wait for power

	while(t->getPower())
	{
		uint8_t data=t->receiveChar(1);
		if (data)
		{
			printf("%c\n",data);
			if (data=='$')
			{
			  t->setPower(0);
			}
		}

		usleep(100);
		t->checkPowerTimeout();
	}
}
*/
