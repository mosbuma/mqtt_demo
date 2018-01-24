#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
//#include <stdint.h>
#include "telex.h"

//#include <sys/time.h>
//#include <math.h>

// Access from ARM Running Linux
#define PERIPHERAL_BASE 0x3F000000
#define PERIPHERAL_BASE_LEGACY 0x20000000
#define GPIO_BASE (PERIPHERAL_BASE + 0x200000) /* GPIO controller */
#define GPIO_BASE_LEGACY (PERIPHERAL_BASE_LEGACY + 0x200000) /* GPIO controller */

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(this->gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(this->gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(this->gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(this->gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(this->gpio+10) // clears bits which are 1 ignores bits which are 0
#define GPIO_GET *(this->gpio+13)

#define BELL 0x0b
#define SELECT_ALPHABET_1 0x1f
#define SELECT_ALPHABET_2 0x1b

const uint8_t telex::alphabet1[32]={'*','e',0x0a,'a',' ','s', 'i','u',0x0d,'d','r','j','n','f','c','k','t','z','l','w','h','y','p','q','o','b','g','~','m','x','v','^'};
const uint8_t telex::alphabet2[32]={'*','3',0x0a,'-',' ','\'','8','7',0x0d,'$','4','%',',','!',':','(','5','+',')','2','#','6','0','1','9','?','&','~','.','/','=','^'};

#define SYMBOL_TIME 20000

telex::telex(uint8_t pinWriterOut, uint8_t pinKeyboardIn, uint8_t pinPowerControl, uint8_t pinPowerPhase, uint8_t legacyIOMapping)
{
	this->currentAlphabet=0;

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

	this->pinPowerPhase=pinPowerPhase; // input
	INP_GPIO(this->pinPowerPhase);

	this->pinWriterOut=pinWriterOut; // output
	INP_GPIO(this->pinWriterOut);
	OUT_GPIO(this->pinWriterOut);
	this->digitalWrite(this->pinWriterOut,1,0);

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

void telex::setPower(uint8_t onOff)
{
	this->digitalWrite(this->pinPowerControl,onOff);
}

uint8_t telex::baudotGetAlphabet(uint8_t *data)
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

uint8_t telex::baudotEncodeChar(uint8_t *data, uint8_t filter)
{
  // transform character so it is suitable to send to telex
  // function returns alphabet to be used
  uint8_t alphabet;

  // set to lowercase
  if ((data[0]>=65)&&(data[0]<=90)) data[0]+=32;
  // replace all quote type to single quote (') character
  if ((data[0]=='"')||(data[0]=='`')) data[0]='\'';
  // replace '*','~','^' characters with '+'
  // '*' character is used to encode null, '~' is used for char/digit switch and '^' is used for digit/char switch
  if ((filter)&&((data[0]=='*')||(data[0]=='~')||(data[0]=='^'))) data[0]='+';
  alphabet=this->baudotGetAlphabet(data);
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

uint8_t telex::baudotDecodeChar(uint8_t data)
{
  if (data==0x1f) this->currentAlphabet=1;
  if (data==0x1b) this->currentAlphabet=2;
  if (this->currentAlphabet==2) return this->alphabet2[data];
  else return this->alphabet1[data]; // assume alphabet is 1 if alphabet is not set (0)
}

void telex::baudotSendChar(uint8_t data)
{
  if ((data==SELECT_ALPHABET_1)||(data==SELECT_ALPHABET_2))
  {
	this->currentAlphabet=(data==SELECT_ALPHABET_1)?1:2;
    printf("Change alphabet to %d\n",this->currentAlphabet);
  }

  this->digitalWrite(this->pinWriterOut,0); // startbit
  usleep(SYMBOL_TIME);
  for (uint8_t zz=0;zz<5;zz++)
  {
    this->digitalWrite(this->pinWriterOut,data&0x01);
    data>>=1;
    usleep(SYMBOL_TIME);
  }
  this->digitalWrite(this->pinWriterOut,1); // stopbit
  usleep(SYMBOL_TIME);
}

uint8_t telex::detectStartBit(void)
{
  return (this->digitalRead(this->pinKeyboardIn)!=0);
}

uint8_t telex::baudotReceiveChar(uint8_t localEcho)
{
  uint8_t data=0;

  usleep(SYMBOL_TIME/2); // wait until we are half way into the start bit
  if (localEcho) this->digitalWrite(this->pinWriterOut,0);
  usleep(SYMBOL_TIME);
  for (uint8_t zz=0;zz<5;zz++)
  {
    if (!this->digitalRead(this->pinKeyboardIn))
    {
      if (localEcho) this->digitalWrite(this->pinWriterOut,1);
      data|=0x20;
    }
    else
    {
      if (localEcho) this->digitalWrite(this->pinWriterOut,0);
    }
    data>>=1;
    usleep(SYMBOL_TIME);
  }
  if (localEcho) this->digitalWrite(this->pinWriterOut,1);
  usleep(SYMBOL_TIME/2+1000); // wait until we are finished with the last bit to avoid detecting false startbit
  return data;
}

void telex::baudotSendAlphabetSelect(uint8_t alphabet)
{
  if (alphabet==1) baudotSendChar(SELECT_ALPHABET_1);
  else baudotSendChar(SELECT_ALPHABET_2);
  usleep(SYMBOL_TIME*4);
}

void telex::sendChar(uint8_t data, uint8_t filter)
{
  uint8_t baudot=data,
		  alphabet=this->baudotEncodeChar(&baudot,filter);

  if (alphabet!=this->currentAlphabet)
  {
    this->baudotSendAlphabetSelect(alphabet);
    this->currentAlphabet=alphabet;
  }
  printf("Print %c\n",data);
  this->baudotSendChar(baudot);
}

void telex::sendString(uint8_t *data, uint8_t filter)
{
  uint16_t zz=0;
  while(data[zz])
  {
    this->sendChar(data[zz],filter);
    zz++;
    usleep(20000);
  }
}

/*
int main(void)
{
	uint8_t send[]={'^','^','^','a','b','c','d','e','f','g','h','~','~','i','j','k','l','m','n','4','4','4','4','4','4','o','p','q','r','s','t','u','v','w','x','y','z','1','2','3','4','5','6','7','8','9','0','(',')',0x0a,0x0d,0};
	telex *t=new telex();
	t->setPower(1);
	usleep(4000000);
	t->sendString(send,0);
	usleep(2000000);
	t->setPower(0);
	return 0;

	while(1)
	{
		if (t->detectStartBit())
		{
			uint8_t data=t->baudotDecodeChar(t->baudotReceiveChar(1));
			printf("%c\n",data);
			if (data=='$')
			{
			  t->setPower(0);
			  return 0;
			}
		}
		usleep(100);
	}
}
*/
