#include "telex.h"
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static void print_usage(const char *prog)
{
	printf("Usage: %s [-prneth]\n", prog);
	puts("  -p --print \"text to print on telex\"\n"
	     "  -r --read reads data from telex\n"
	     "  -n --number number of characters to read\n"
	     "  -e --echo enable local echo\n"
	     "  -t --timeout time to wait for next character (seconds)\n"
			 "  -l --listen listen for lines on stdin\n"
		 "  -h --help display this message\n");
	exit(1);
}

uint8_t mode=0; // 1=print, 2=read, 3=listen
uint8_t echo=0; // disable local echo
char *data;
uint16_t number=0;
uint8_t timeout=0;

static void parse_opts(int argc, char *argv[])
{
	static const struct option lopts[] = {
		{ "print",  required_argument, 0, 'p' },
		{ "read",   no_argument, 0, 'r' },
		{ "number", required_argument, 0, 'n' },
		{ "echo", no_argument, 0, 'e' },
		{ "timeout", required_argument, 0, 't' },
		{ "listen", no_argument, 0, 'l' },
		{ "help", no_argument, 0, 'h' },
		{ NULL, 0, 0, 0 }
	};

	int c;

	while (1)
	{
		c = getopt_long(argc, argv, "p:r:n:e:t:l:h", lopts, NULL);

		if (c == -1)
		{
			if (mode==0)
			{
				printf("Invalid  parameters: please specify at least print or read mode\n");
				print_usage(argv[0]);
			}
			break;
		}

		switch (c)
		{
			case 'p':
				mode=1; // print
				data=optarg;
				break;
			case 'r':
				mode=2;
				break;
			case 'l':
				mode=3;
				break;
			case 'n':
				number=atoi(optarg);
				break;
			case 'e':
				echo=1;
				break;
			case 't':
				timeout=atoi(optarg);
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

	telex *t=new telex();

	if (mode==1)
	{
		t->setPower(1);
		usleep(4000000);
		t->sendString((uint8_t*)data);
		usleep(2000000);
		t->setPower(0);
	}
	else if (mode==2)
	{
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
	} else {
		while(1)
		{
			char *buffer = NULL;
	    int read;
	    size_t len;
	    read = getline(&buffer, &len, stdin);
	    if (-1 != read)
	        puts(buffer);
	    else
	        printf("No line read...\n");

	    printf("Size read: %d\n ", read); // Len: %u\n, len
	    free(buffer);

			usleep(100);
		}
	}
}
