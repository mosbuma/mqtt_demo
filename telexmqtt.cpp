/*
 * Telex Code - Copyright (c) 2018 Marc and Dario Buma <mosbuma@bumos.nl>
 * MQTT Demo - Copyright (c) 2014 Scott Vokes <vokes.s@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "telex.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include <string>
#include <ctime>

#include <stdlib.h>

#include <mosquitto.h>

/* The linked code creates a client that connects to a broker at
 * localhost:1883, subscribes to the topics "tick", "control/#{PID}",
 * and "control/all", and publishes its process ID and uptime (in
 * seconds) on "tock/#{PID}" every time it gets a "tick" message. If the
 * message "halt" is sent to either "control" endpoint, it will
 * disconnect, free its resources, and exit. */

#ifdef DEBUG
#define LOG(...) do { printf(__VA_ARGS__); } while (0)
#else
#define LOG(...)
#endif

/* How many seconds the broker should wait between sending out
 * keep-alive messages. */
#define KEEPALIVE_SECONDS 60

/* Hostname and port for the MQTT broker. */
#define BROKER_HOSTNAME "http://luggage.dynds.tv:54378"
#define BROKER_PORT 1883

#define TELEX_INCOMING_FROM_SAT "telex/incoming-sat"
#define TELEX_CONTROL_ALL "telex/control/all"
#define TELEX_CONTROL_PID "telex/control/%d"

struct client_info {
    struct mosquitto *m;
    pid_t pid;
    // uint32_t tick_ct;
};

static void die(const char *msg);
static struct mosquitto *init(struct client_info *info);
static bool set_callbacks(struct mosquitto *m);
static bool connect(struct mosquitto *m);
static int run_loop(struct client_info *info);


static void print_usage(const char *prog)
{
	printf("Usage: %s [-npDh]\n", prog);
	puts("  -n --hostname : mqtt host IP or name\n"
	     "  -p --port : mqtt port on host\n"
       "  -u --user : mqtt username (omit to login anonymously)\n"
       "  -P --pass : mqtt password\n"
       "  -d --dummy : dummy telex mode: send messages to console\n"
       "  -w --wrap : set auto text wrap at X cnaracters \n"
  		 "  -h --help : display this message\n");
	exit(1);
}

uint8_t echo=0; // disable local echo
uint8_t timeout=0;

char *hostname;
int port;
int dummyMode=0;

char *username;
char *password;

int wrap = 60;

static void parse_opts(int argc, char *argv[])
{
	static const struct option lopts[] = {
		{ "hostname", required_argument, 0, 'n' },
		{ "port", required_argument, 0, 'p' },
    { "dummy", no_argument, 0, 'd' },
    { "user", no_argument, 0, 'u' },
    { "pass", no_argument, 0, 'P' },
    { "wrap", no_argument, 0, 'w' },
		{ "help", no_argument, 0, 'h' },
		{ NULL, 0, 0, 0 }
	};

	int c;

	while (1)
	{
		c = getopt_long(argc, argv, "n:p:u:P:dw:h", lopts, NULL);
		if (c==-1)
		{
      if(hostname==0||port==0) {
  			printf("Invalid  parameters: please specify at least hostname and port\n");
  			print_usage(argv[0]);
        exit(0);
      }
			break;
		}

		switch (c)
		{
			case 'n':
				// change host name
        hostname=optarg;
				break;
			case 'p':
        port=atoi(optarg);
				break;
      case 'd':
        dummyMode=1;
				break;
      case 'u':
        username=optarg;
				break;
      case 'P':
        password=optarg;
				break;
      case 'w':
        wrap=atoi(optarg);
				break;
			case 'h':
			default:
				print_usage(argv[0]);
		}
	}
}

telex *pDaTelex=0;

void handle_signal (int x)
{
  exit(x); // -> calls ceannup via atexit
}

void cleanup_resources ()
{
  if(pDaTelex!=0) {
    pDaTelex->sendString((uint8_t*) "\n");
    pDaTelex->setPower(0);
  }
}

int main(int argc, char **argv) {
    atexit (cleanup_resources);
    signal(SIGINT, handle_signal); // catch ctrl+c for cleanup
    signal(SIGABRT, handle_signal); // catch abort for cleanup

    parse_opts(argc, argv);

    if(hostname==0) {
      return 0;
    }

    pid_t pid = getpid();

    if(dummyMode==0) {
      pDaTelex=new telex();
      // pDaTelex=0;
    } else {
      pDaTelex=0;
    }

    mosquitto_lib_init();

    struct client_info info;
    memset(&info, 0, sizeof(info));
    info.pid = pid;

    struct mosquitto *m = init(&info);

    if (m == NULL) { die("init() failure\n"); }
    info.m = m;

    if(0!=username) {
      printf("Setting username to '%s'\n", username);
      mosquitto_username_pw_set(m, username, password);
    }

    if (!set_callbacks(m)) { die("set_callbacks() failure\n"); }

    if (!connect(m)) { die("connect() failure\n"); }

    int res = run_loop(&info);

    mosquitto_lib_cleanup();

    exit (res);
}

/* Fail with an error message. */
static void die(const char *msg) {
    fprintf(stderr, "%s", msg);

    mosquitto_lib_cleanup();

    exit(1);
}

/* Initialize a mosquitto client. */
static struct mosquitto *init(struct client_info *info) {
    void *udata = (void *)info;
    int buf_sz = 32;
    char buf[buf_sz];
    if (buf_sz < snprintf(buf, buf_sz, "client_%d", info->pid)) {
        return NULL;            /* snprintf buffer failure */
    }
    /* Create a new mosquitto client, with the name "client_#{PID}". */
    struct mosquitto *m = mosquitto_new(buf, true, udata);

    return m;
}

/* Callback for successful connection: add subscriptions. */
static void on_connect(struct mosquitto *m, void *udata, int res) {
    if (res == 0) {             /* success */
        struct client_info *info = (struct client_info *)udata;
        mosquitto_subscribe(m, NULL, TELEX_INCOMING_FROM_SAT, 0);
        mosquitto_subscribe(m, NULL, TELEX_CONTROL_ALL, 0);
        int sz = 32;
        char control_pid[sz];
        if (sz < snprintf(control_pid, sz, TELEX_CONTROL_PID, info->pid)) {
            die("snprintf\n");
        }
        mosquitto_subscribe(m, NULL, control_pid, 0);
//        mosquitto_subscribe(m, NULL, "tick", 0);
    } else {
        die("connection refused\n");
    }
}

/* A message was successfully published. */
static void on_publish(struct mosquitto *m, void *udata, int m_id) {
    LOG("-- published successfully\n");
}

static bool match(const char *topic, const char *key) {
    return 0 == strncmp(topic, key, strlen(key));
}

/* Handle a message that just arrived via one of the subscriptions. */
static void on_message(struct mosquitto *m, void *udata,
                       const struct mosquitto_message *msg) {
    if (msg == NULL) { return; }
    // LOG("-- got message @ %s: (%d, QoS %d, %s) '%s'\n",
    //     (char *) msg->topic, msg->payloadlen, msg->qos, msg->retain ? "R" : "!r",
    //     (char *) msg->payload);

//    struct client_info *info = (struct client_info *)udata;

    if (match(msg->topic, TELEX_INCOMING_FROM_SAT)) {
        std::string base=(char *) msg->payload;
        if(base.length()>60) {
          base = base.substr(0, 57).append("...");
        }

        if(pDaTelex!=0) {
      		pDaTelex->sendString((uint8_t*) base.c_str());
          pDaTelex->sendString((uint8_t*)"\n");
        } else {
          printf("Dummy Telex says: message from satellite '%s'\n", (char *) base.c_str());
        }
    } else if (match(msg->topic, TELEX_CONTROL_ALL)) {
        LOG("incoming from control: %s\n", (char *) msg->payload);
        /* This will cover both "control/all" and "control/$(PID)".
         * We won'st see "control/$(OTHER_PID)" because we won't be
         * subscribed to them.*/
        if (0 == strncmp((char *) msg->payload, "halt", msg->payloadlen)) {
            LOG("*** halt\n");
            (void)mosquitto_disconnect(m);
        }
    }
}

/* Successful subscription hook. */
static void on_subscribe(struct mosquitto *m, void *udata, int mid,
                         int qos_count, const int *granted_qos) {
    LOG("-- subscribed successfully\n");
}

/* Register the callbacks that the mosquitto connection will use. */
static bool set_callbacks(struct mosquitto *m) {
    mosquitto_connect_callback_set(m, on_connect);
    mosquitto_publish_callback_set(m, on_publish);
    mosquitto_subscribe_callback_set(m, on_subscribe);
    mosquitto_message_callback_set(m, on_message);
    return true;
}

/* Connect to the network. */
static bool connect(struct mosquitto *m) {
//    int res = mosquitto_connect(m, BROKER_HOSTNAME, BROKER_PORT, KEEPALIVE_SECONDS);
    int res = mosquitto_connect(m, hostname, port, KEEPALIVE_SECONDS);
    return res == MOSQ_ERR_SUCCESS;
}

/* Loop until it is explicitly halted or the network is lost, then clean up. */
static int run_loop(struct client_info *info) {
//    int res = mosquitto_loop_forever(info->m, 1000, 1 /* unused */);
    int res;

    while(1)
    {
      // TODO: reconnect in case connection was lost (this is done automatically in mosquitto_loop_forever)

      res = mosquitto_loop(info->m, 1000, 1 /* unused */);

      if(pDaTelex!=0) {
        pDaTelex->checkPowerTimeout();
      }

    }
    mosquitto_destroy(info->m);
    (void)mosquitto_lib_cleanup();

    if (res == MOSQ_ERR_SUCCESS) {
        return 0;
    } else {
        return 1;
    }
}
