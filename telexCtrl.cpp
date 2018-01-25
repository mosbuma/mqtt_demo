#include "telex.h"
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

static void print_usage(const char *prog)
{
  printf("Teleprinter (Teletype,Telex) commandline utility for Raspberry Pi.\nThe following connections are assumed:\n");
  printf("- writer output = GPIO17\n");
  printf("- keyboard input = GPIO18\n");
  printf("- power switch output = GPIO27\n");
	printf("Usage: %s [-pfrsnetlh]\n", prog);
	puts("  -p --print print text on telex \"line 1|_line2|_\" ('%'=BELL,'|'=CR,'_'=NL,'*'=NULL) \n"
       "  -f --format print one line of text with timestamp header \"line of text to print on telex\" \n"
       "  -r --read reads data from telex\n"
       "  -s --stop cut power to telex\n"
	     "  -n --number number of characters to read\n"
	     "  -e --echo enable local echo\n"
	     "  -t --timeout number of seconds to wait for next character (default 5 seconds)\n"
       "  -l --legacy use this option for enabling legacy IO-mapping (Rapberry Pi 1 and Zero)\n"
		   "  -h --help display this message\n"
       "Hint: please be careful with the number of newlines as to save the paper");
	exit(1);
}

uint8_t mode=0; // 1=print, 2=read
uint8_t echo=0; // disable local echo
uint8_t legacy=0; // use legacy IO mapping (Raspberry Pi 1 & Zero)
char *data;
uint16_t number=0;
uint8_t timeout=5;

static void parse_opts(int argc, char *argv[])
{
	static const struct option lopts[] = {
		{ "print",  required_argument, 0, 'p' },
    { "format",  required_argument, 0, 'f' },
    { "read", no_argument, 0, 'r' },
    { "stop", no_argument, 0, 's' },
		{ "number", required_argument, 0, 'n' },
		{ "echo", no_argument, 0, 'e' },
		{ "timeout", required_argument, 0, 't' },
    { "legacy", no_argument, 0, 'l' },
    { "help", no_argument, 0, 'h' },
		{ NULL, 0, 0, 0 }
	};

	int c;

	while (1)
	{
		c = getopt_long(argc, argv, "p:f:rsn:et:lh", lopts, NULL);

		if (c == -1)
		{
			if (mode==0)
			{
				printf("Invalid parameters: please specify operation mode (print, format, read, stop)\n");
				print_usage(argv[0]);
			}
      if ((mode==3)&&((!timeout)&&(!number)))
      {
        printf("Invalid parameters: please specify at least number or timeout in read mode\n");
				print_usage(argv[0]);
        mode=0;
      }
      break;
		}

		switch (c)
		{
			case 'p':
        if (mode)
        {
          printf("Invalid parameters: only one mode specifier allowed (print, format, read, stop)\n");
          mode=0;
          break;
        }
        mode=1; // print
				data=optarg;
				break;
			case 'f':
        if (mode)
        {
          printf("Invalid parameters: only one mode specifier allowed (print, format, read, stop)\n");
          mode=0;
          break;
        }
				mode=2;
				data=optarg;
				break;
			case 'r':
        if (mode)
        {
          printf("Invalid parameters: only one mode specifier allowed (print, format, read, stop)\n");
          mode=0;
          break;
        }
				mode=3;
				break;
      case 's':
        if (mode)
        {
          printf("Invalid parameters: only one mode specifier allowed (print, format, read, stop)\n");
          mode=0;
          break;
        }
				mode=4;
				break;
			case 'n':
				number=abs(atoi(optarg));
				break;
			case 'e':
				echo=1;
				break;
			case 't':
				timeout=abs(atoi(optarg));
				break;
      case 'l':
				legacy=1;
				break;
			case 'h':
			default:
				print_usage(argv[0]);
		}
	}
}

int main(int argc, char **argv)
{
	parse_opts(argc, argv);

	if (!mode) return 0;

	telex *t=new telex(17,18,27,22,legacy);

  switch(mode)
  {
    case 1:
  		{
  			int g=0;
    		while(data[g])
    		{
    			if (data[g]=='|') data[g]='\r';
    			if (data[g]=='_') data[g]='\n';
    			g++;
    		}
			  t->setPower(1);
			  usleep(4000000);
    		t->sendString((uint8_t*)data,0);
    		t->setPower(0);
    		usleep(2000000);
      }
      break;
    case 2:
      {
        int g=0;
        while(data[g])
        {
          if ((data[g]=='\r')||(data[g]=='\n'))
          {
            data[g]=0;
            break;
          }
          if (g>=58)
          {
            data[g]=0;
            break;
          }
          g++;
        }
        if (!g) return 0; // nothing to print
        char data2[150];
        time_t now = time(NULL);
        strftime(data2,32,"+++ %Y/%m/%d %H:%M:%S +++\r\n= ",localtime(&now));
        strcpy(data2+strlen(data2),data);
        strcpy(data2+strlen(data2)," =\r\n%");//+++ end +++\r\n");
        t->setPower(1);
    	  usleep(4000000);
    	  t->sendString((uint8_t*)data2);
        t->setPower(0);
   		  usleep(2000000);
      }
      break;
    case 3:
      {
        uint32_t lastKeyStrokeTimer=0;
        int keyStrokeCounter=0;
        printf("Listening for keyboard input\n");
        t->setPower(1);
        while(1)
    		{
    			if (t->detectStartBit())
    			{
            lastKeyStrokeTimer=0;
            keyStrokeCounter++;

            uint8_t data=t->baudotDecodeChar(t->baudotReceiveChar(echo));
    				printf("%c\n",data);
    				if (data=='$')
    				  break;
    			}
    			usleep(100);
          lastKeyStrokeTimer++;
          if (lastKeyStrokeTimer>=(timeout*10000))
          {
            printf("Timeout on keyboard input\n");
            break;
          }
          if ((number)&&(keyStrokeCounter>=number))
          {
            printf("Requested number (%d) of characters read from keyboard\n",number);
            break;
          }
    		}
        t->setPower(0);
        usleep(2000000);
      }
      break;
    case 4:
      {
        printf("Cutting power to telex\n");
        t->setPower(0);
        usleep(2000000);
      }
      break;
	}
}
