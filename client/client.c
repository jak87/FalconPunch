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
#include <unistd.h>
#include <pthread.h>
#include "../lib/types.h"
#include "../lib/protocol_client.h"
#include "../lib/protocol_utils.h"
#include "../lib/maze.h"
#include "../lib/tty.h"
#include "../lib/objects.h"
#include "../lib/uistandalone.h"
#include "../lib/player.h"
#include "../lib/game_control.h"


#define STRLEN 81

typedef struct ClientState  {
  int data;
  Proto_Client_Handle ph;
} Client;

struct Globals {
  char host[STRLEN];
  PortType port;
  //Player player;
  int connection_id;
  Client* client_inst;
} globals;

UI *ui;



static int
clientInit(Client *C)
{
  bzero(C, sizeof(Client));

  bzero(&ClientGameState, sizeof(ClientGameState));

  pthread_mutex_init(&(ClientGameState.masterLock), 0);

  // initialize the client protocol subsystem
  if (proto_client_init(&(C->ph))<0) {
    fprintf(stderr, "client: main: ERROR initializing proto system\n");
    return -1;
  }
  return 1;
}

static void
initializeGameState() {
  int i,j;
  //set all ids = -1 to show they don't exist yet
  for(i = 0; i < 2; i++) {
    for(j = 0; j < MAX_NUM_PLAYERS; j++)
      ClientGameState.players[i][j].id = -1;
  }
}

extern int
disconnect_handler(Proto_Session *s)
{
  proto_client_disconnect();
  printf("\nError: Server has disconnected you!\n");
  exit(1);
}


extern int
update_event_handler(Proto_Session *s)
{

  // n is the number of players that will be sent
  int rc = 1, n=0, offset=0, i, j;
  Player *p = malloc(sizeof(Player));

  // Lock while updating values on the client players array.
  pthread_mutex_lock(&(ClientGameState.masterLock));

  initializeGameState();

  offset = object_unmarshall(s, offset, &ClientGameState.objects[0]);
  offset = object_unmarshall(s, offset, &ClientGameState.objects[1]);
  offset = object_unmarshall(s, offset, &ClientGameState.objects[2]);
  offset = object_unmarshall(s, offset, &ClientGameState.objects[3]);

  //printf("Entering proto_client_player_update_handler\n");
  offset = proto_session_body_unmarshall_int(s, offset,&n);
  //printf("Receiving %d players\n",n);		\

  for(i = 0; i < n; i++) {
    offset = player_unmarshall(s,offset,p);
    if (offset < 0)
      return offset;
    player_copy(&(ClientGameState.players[p->team][p->id]),p);
  }
  pthread_mutex_unlock(&(ClientGameState.masterLock));
  // Error-checking!

  //printf("\n New update!\n");

  /*
  for(j = 0; j < 2; j++) {
    for(i = 0; i < MAX_NUM_PLAYERS; i++) {
      if(ClientGameState.players[j][i].id > -1) {
	    player_dump(&(ClientGameState.players[j][i]));
      }
    }
    }*/
  //printf("Done getting update with all players\n");

  ui_update(ui);

  //printf("UI Updated!\n");

  return 1;
}

static int
gameover_event_handler(Proto_Session *s)
{
//  Proto_Client_Handle pch = ((Client *) proto_session_get_data(s))->ph;

  char winner;
  proto_session_body_unmarshall_char(s, 0, &winner);

  fprintf(stderr, "Game Over\n");

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

    if (h != NULL) {

      proto_client_set_event_handler(C->ph, PROTO_MT_EVENT_BASE_UPDATE_PLAYERS,
				     h);
    }
    //also add handler for game over
    proto_client_set_event_handler(C->ph, PROTO_MT_EVENT_BASE_GAMEOVER, gameover_event_handler);

    proto_client_set_event_handler(C->ph, PROTO_MT_EVENT_BASE_DISCONNECT, disconnect_handler);


    return 1;
  }
  return 0;
}


int
prompt(char * buf, int menu)
{
  int ret = 1;

  if (menu) printf("\nPlayer> ");
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

extern sval
ui_keypress(UI *ui, SDL_KeyboardEvent *e)
{
  int rc;
  SDLKey sym = e->keysym.sym;
  SDLMod mod = e->keysym.mod;

  if (e->type == SDL_KEYDOWN) {
    if (sym == SDLK_LEFT && mod == KMOD_NONE) {
      fprintf(stderr, "%s: move left\n", __func__);
      rc = proto_client_move(globals.client_inst->ph, MOVE_LEFT);
      ui_center_on_player(ui);
      return rc;
    }
    if (sym == SDLK_RIGHT && mod == KMOD_NONE) {
      fprintf(stderr, "%s: move right\n", __func__);
      rc = proto_client_move(globals.client_inst->ph, MOVE_RIGHT);
      ui_center_on_player(ui);
      return rc;
    }
    if (sym == SDLK_UP && mod == KMOD_NONE)  {  
      fprintf(stderr, "%s: move up\n", __func__);
      rc = proto_client_move(globals.client_inst->ph, MOVE_UP);
      printf("Attempting to center...\n");
      ui_center_on_player(ui);
      printf("Centered!\n");
      return rc;
    }
    if (sym == SDLK_DOWN && mod == KMOD_NONE)  {
      fprintf(stderr, "%s: move down\n", __func__);
      rc = proto_client_move(globals.client_inst->ph, MOVE_DOWN);
      ui_center_on_player(ui);
      return rc;
    }
    if (sym == SDLK_r && mod == KMOD_NONE)  {  
      fprintf(stderr, "%s: dummy pickup red flag\n", __func__);
      //return ui_dummy_pickup_red(ui);
    }
    if (sym == SDLK_g && mod == KMOD_NONE)  {   
      fprintf(stderr, "%s: dummy pickup green flag\n", __func__);
      //return ui_dummy_pickup_green(ui);
    }
    if (sym == SDLK_j && mod == KMOD_NONE)  {   
      fprintf(stderr, "%s: dummy jail\n", __func__);
      //return ui_dummy_jail(ui);
    }
    if (sym == SDLK_n && mod == KMOD_NONE)  {   
      fprintf(stderr, "%s: dummy normal state\n", __func__);
      //return ui_dummy_normal(ui);
    }
    if (sym == SDLK_t && mod == KMOD_NONE)  {   
      fprintf(stderr, "%s: dummy toggle team\n", __func__);
      //return ui_dummy_toggle_team(ui);
    }
    if (sym == SDLK_i && mod == KMOD_NONE)  {   
      fprintf(stderr, "%s: dummy inc player id \n", __func__);
      //return ui_dummy_inc_id(ui);
    }
    if (sym == SDLK_q) return -1;
    if (sym == SDLK_z && mod == KMOD_NONE) return ui_zoom(ui, 1);
    if (sym == SDLK_z && mod & KMOD_SHIFT ) return ui_zoom(ui,-1);
    if (sym == SDLK_LEFT && mod & KMOD_SHIFT) return ui_pan(ui,-1,0);
    if (sym == SDLK_RIGHT && mod & KMOD_SHIFT) return ui_pan(ui,1,0);
    if (sym == SDLK_UP && mod & KMOD_SHIFT) return ui_pan(ui, 0,-1);
    if (sym == SDLK_DOWN && mod & KMOD_SHIFT) return ui_pan(ui, 0,1);
    else {
      fprintf(stderr, "%s: key pressed: %d\n", __func__, sym); 
    }
  } else {
    fprintf(stderr, "%s: key released: %d\n", __func__, sym);
  }
  return 1;
}

int launch(){
  tty_init(STDIN_FILENO);
  ui_init(&(ui));
  ui_main_loop(ui,320,320);
}

int 
docmd(Client *C, char * buf)
{
  int rc = 1;
  int temp = 0;
  int x = -1;
  int y = -1;
  int team = 0;
  char n = ' ';
  int cinfo_buf[] = {0, 0, 0};

  char cmd[STRLEN];
  sscanf(buf, "%s", cmd);
  
  // Quits
  if (strcmp(cmd,"quit") == 0 || (strcmp(cmd,"q") == 0)) {
    proto_client_goodbye(C->ph,globals.connection_id,ClientGameState.me);
    return -1;
  }
  
  // Displays the Number of Home Cells
  else if (strcmp(cmd,"numhome") == 0) {
    sscanf(buf, "%s%d", cmd, &team);
    if (team == 1 || team == 2) {
      if (team == 1)
	n = 'h';
      else
	n = 'H';
      printf("%d\n", proto_client_maze_info(C->ph, n));
    } else if (team == 0)
      printf("Please enter a number (team 1 or 2) after numhome!\n");
    else
      printf("Team %d is not a correct team! Enter 1 or 2!\n", team);
  }

  else if (strcmp(cmd,"numjail") == 0) {
    sscanf(buf, "%s%d", cmd, &team);
    if (team == 1 || team == 2) {
      if (team == 1)
	n = 'j';
      else
	n = 'J';
      printf("%d\n",proto_client_maze_info(C->ph, n));
    } else if (team == 0)
      printf("Please enter a number (Team 1 or 2) after numjail!\n");
    else
      printf("Team %d is not a correct team! Enter 1 or 2!\n", team);
  }

  else if (strcmp(cmd,"numwall") == 0) {
    printf("%d\n", proto_client_maze_info(C->ph, '#'));
  }

  else if (strcmp(cmd,"numfloor") == 0) {
    printf("%d\n", proto_client_maze_info(C->ph, ' '));
  }

  else if (strcmp(cmd,"dim") == 0) {
    temp = proto_client_maze_info(C->ph, 'd');
    printf("Dimensions: %d x %d\n", temp, temp);
  }

  else if (strcmp(cmd,"cinfo") == 0) {
    sscanf(buf, "%s%d,%d", cmd, &x, &y);
    if (x == -1 || y == -1) {
      printf("Error, incorrect format! Please enter cinfo x,y\n");
    } else {
      proto_client_cell_info(C->ph, x, y, cinfo_buf);
      if (cinfo_buf[0] == 0 || cinfo_buf[0] == 'i') {
	printf("Coordinates out of bounds\n");
	return rc;
      }
      printf("Cell info at (%d,%d):\nType: ", x, y);
      if (cinfo_buf[0] == 'h')
	printf("Home\n");
      else if (cinfo_buf[0] == 'j')
	printf("Jail\n");
      else if (cinfo_buf[0] == '#')
	printf("Wall\n");
      else if (cinfo_buf[0] == ' ')
	printf("Floor\n");
      else
	printf("Error in cinfo!\n");
      printf("Team: %d\nOccupied: ", cinfo_buf[1]);
      if (cinfo_buf[2] == 1)
	printf("Yes\n");
      else
	printf("No\n");
    }
  }

  else if (strcmp(cmd,"dump") == 0) {
    proto_client_dump_maze(C->ph);
  }

  else if(strcmp(cmd, "launch") == 0){
    launch();
  }

  else if (strcmp(cmd,"h") == 0) {
    printf("Command options:\n");
    printf("Launch the ui: launch\n");
    printf("Display number of home cells: numhome <1 or 2>\n");
    printf("Display number of jail cells: numjail <1 or 2>\n");
    printf("Display number of wall cells: numwall\n");
    printf("Display number of floor cells (this includes home/jail cells): numfloor\n");
    printf("Display dimensions of the maze: dim\n");
    printf("Display cell info: cinfo <x,y>\n");
    printf("Dump the maze in ASCII text to the server: dump\n");
    printf("Quit: quit (or just q)\n");
  }
  
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
    else rc = -1;
    
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
      return 0;
    }
    sscanf(readBuf, "%s", buf);

    if (strcmp("quit",buf) == 0 || strcmp("q",buf) == 0) return 0;

    if (strcmp("connect",buf) == 0) {
      bzero(buf,sizeof(buf));
      if (sscanf(readBuf, "%s%d.%d.%d.%d:%d", buf, &ip1, &ip2, &ip3, &ip4, &port) != 6) {
        bzero(buf,sizeof(buf));
        if (sscanf(readBuf, "%s%d", buf, &port) == 2) {
          strncpy(globals.host, "localhost", STRLEN);
	  globals.port = port;
          return 1;
        }
        printf("connect takes args <ip:port>, or <port> if you want to default to localhost");
      }
      else {
        sprintf(globals.host, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
	globals.port = port;
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
  bzero(&Board, sizeof(Board));

  // store a pointer to the Client instance we just created. This will be used for
  // the entire lifespan of the client, such that any code can access the proto
  // client handle and other Client state.
  globals.client_inst = &c;

  //initialize yourself! (sorta)
  // no need to initialize yourself. .me will always point to the real memory location reserved
  // for the player based on team and id.
//  ClientGameState.me = malloc(sizeof(Player));
  //initialize players
  initializeGameState();

  if (initialShell(&c) == 0) return 0;

  // ok startup our connection to the server
  if (startConnection(&c, globals.host, globals.port, update_event_handler)<0) {
    fprintf(stderr, "ERROR: Connection failed\n");
    return -1;
  }

  // connect to the server
  proto_client_hello(c.ph);

  //printf("Registering new player...\n");
  // register as a new player
  if(proto_client_new_player(c.ph, &(globals.connection_id)) < 1) {
    fprintf(stderr, "ERROR: Couldn't create new player\n");
    return -1;
  }
  printf("My id is %d!\n", globals.connection_id);
  printf("My player (ClientGameState.me) is:\n");
  player_dump(ClientGameState.me);

  printf("Connected to <%s:%i>\n", globals.host, globals.port);
  printf("For command options, please type 'h'\n");

  shell(&c);

  return 0;
}

