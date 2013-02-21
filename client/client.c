/******************************************************************************
* Copyright (C) 2011 by Jonathan Appavoo, Boston University
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/types.h"
#include "../lib/protocol_client.h"
#include "../lib/protocol_utils.h"

#define STRLEN 81

struct Globals {
  char host[STRLEN];
  PortType port;
} globals;


typedef struct ClientState  {
  int data;
  Proto_Client_Handle ph;
} Client;

static int
clientInit(Client *C)
{
  bzero(C, sizeof(Client));

  // initialize the client protocol subsystem
  if (proto_client_init(&(C->ph))<0) {
    fprintf(stderr, "client: main: ERROR initializing proto system\n");
    return -1;
  }
  return 1;
}


static int
update_event_handler(Proto_Session *s)
{
  Client *C = proto_session_get_data(s);

  fprintf(stderr, "%s: called", __func__);
  return 1;
}


int 
startConnection(Client *C, char *host, PortType port, Proto_MT_Handler h)
{
  if (globals.host[0]!=0 && globals.port!=0) {
    if (proto_client_connect(C->ph, host, port)!=0) {
      fprintf(stderr, "failed to connect\n");
      return -1;
    }
    proto_session_set_data(proto_client_event_session(C->ph), C);
#if 0
    if (h != NULL) {
      proto_client_set_event_handler(C->ph, PROTO_MT_EVENT_BASE_UPDATE, 
				     h);
    }
#endif
    return 1;
  }
  return 0;
}


int
prompt(char * buf, int menu) 
{
  static char MenuString[] = "\nclient> ";
  int ret = 1;

  if (menu) printf("%s", MenuString);
  fflush(stdout);
  if (fgets(buf, STRLEN, stdin) == NULL) {
    fprintf(stderr, "fgets error in prompt\n");
    ret = 0;
  }
  return ret;
}


// FIXME:  this is ugly maybe the speration of the proto_client code and
//         the game code is dumb
int
game_process_reply(Client *C)
{
  Proto_Session *s;

  s = proto_client_rpc_session(C->ph);

  fprintf(stderr, "%s: do something %p\n", __func__, s);

  return 1;
}


int 
doRPCCmd(Client *C, char c) 
{
  int rc=-1;

  switch (c) {
  case 'h':  
    {
      rc = proto_client_hello(C->ph);
      printf("hello: rc=%x\n", rc);
      if (rc > 0) game_process_reply(C);
    }
    break;
  case 'm':
    scanf("%c", &c);
    rc = proto_client_move(C->ph, c);
    break;
  case 'g':
    rc = proto_client_goodbye(C->ph);
    break;
  default:
    printf("%s: unknown command %c\n", __func__, c);
  }
  // NULL MT OVERRIDE ;-)
  printf("%s: rc=0x%x\n", __func__, rc);
  if (rc == 0xdeadbeef) rc=1;
  return rc;
}

int
doRPC(Client *C)
{
  int rc;
  char c;

  printf("enter (h|m<c>|g): ");
  getchar();
  scanf("%c", &c);
  rc=doRPCCmd(C,c);

  printf("doRPC: rc=0x%x\n", rc);

  return rc;
}


int 
docmd(Client *C, char * buf)
{
  int rc = 1;

  char cmd[STRLEN];
  sscanf(buf, "%s", cmd);
  
  // need to send request to disconnect
  if (strcmp(cmd,"disconnect") == 0) return -1;
  
  // display board
  else if (strcmp(cmd,"\n") == 0) return -1;

  else if (cmd[0] == '[') {
    if (cmd[1] > '0' && cmd[1] <= '9') {
      proto_client_move(C->ph, cmd[1]);
    }
    else {
      printf("Error, invalid move\n");
    }
  }

  else if (strcmp(cmd,"where") == 0)
    printf("host: %s, port: %d\n", globals.host, globals.port);

  // disconnect first
  else if (strcmp(cmd,"quit") == 0) return -1;
  
  else
    printf("Nice try, guy. %s isn't a command...\n", cmd);

  return rc;
}

void *
shell(void *arg)
{
  Client *C = arg;
  char buf[STRLEN];
  int rc;
  int menu=1;

  while (1) {
    if (prompt(buf, menu) != 0) rc=docmd(C, buf);
    if (rc<0) break;
    if (rc==1) menu=1; else menu=0;
  }

  fprintf(stderr, "terminating\n");
  fflush(stdout);
  return NULL;
}

void 
usage(char *pgm)
{
  fprintf(stderr, "USAGE: %s <port|<<host port> [shell] [gui]>>\n"
           "  port     : rpc port of a game server if this is only argument\n"
           "             specified then host will default to localhost and\n"
	   "             only the graphical user interface will be started\n"
           "  host port: if both host and port are specifed then the game\n"
	   "examples:\n" 
           " %s 12345 : starts client connecting to localhost:12345\n"
	  " %s localhost 12345 : starts client connecting to locaalhost:12345\n",
	   pgm, pgm, pgm, pgm);
 
}

void
initGlobals(int argc, char **argv)
{
  bzero(&globals, sizeof(globals));

  if (argc==1) {
    usage(argv[0]);
    exit(-1);
  }

  if (argc==2) {
    strncpy(globals.host, "localhost", STRLEN);
    globals.port = atoi(argv[1]);
  }

  if (argc>=3) {
    strncpy(globals.host, argv[1], STRLEN);
    globals.port = atoi(argv[2]);
  }

}

/*
 * Shell that runs before a connection is established
 */
int
initialShell(void * arg) {
  char buf[STRLEN];
  char readBuf[STRLEN];
  int ip1=0, ip2=0, ip3=0, ip4=0, port=0;
  while(1) {
    printf("\nclient>");
    fflush(stdout);
    bzero(buf,sizeof(buf));
    bzero(readBuf,sizeof(readBuf));

    if (fgets(readBuf, sizeof(readBuf), stdin) == NULL) {
      fprintf(stderr, "ERROR: fgets error\n");
      return;
    }
    sscanf(readBuf, "%s", buf);

    if (strcmp("q",buf) == 0) return 0;

    if (strcmp("connect",buf) == 0) {
      bzero(buf,sizeof(buf));
      if (sscanf(readBuf, "%s%d.%d.%d.%d:%d", buf, &ip1, &ip2, &ip3, &ip4, &port) != 6) {
        bzero(buf,sizeof(buf));
        if (sscanf(readBuf, "%s%d", buf, &port) == 2) {
          strncpy(globals.host, "localhost", STRLEN);
					globals.port = port;
          return 1;
        }
        printf("connect takes args <ip:port>, or <port> if you want to default to localh\
ost");
      }
      else {
        sprintf(globals.host, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
        return 1;
      }
    } 

    else printf("Must first connect to a network! (Or quit with 'q')");
  }
}

int 
main(int argc, char **argv)
{
  Client c;

  if (clientInit(&c) < 0) {
    fprintf(stderr, "ERROR: clientInit failed\n");
    return -1;
  }
	
  bzero(&globals, sizeof(globals));
  if (initialShell(&c) == 0) return 0;

  // ok startup our connection to the server
  if (startConnection(&c, globals.host, globals.port, update_event_handler)<0) {
    fprintf(stderr, "ERROR: startConnection failed\n");
    return -1;
  }

  shell(&c);

  return 0;
}

