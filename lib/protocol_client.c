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
#include <sys/types.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>

#include "protocol.h"
#include "protocol_utils.h"
#include "protocol_client.h"
#include "maze.h"
#include "player.h"
#include "game_control.h"


typedef struct {
  Proto_Session rpc_session;
  Proto_Session event_session;
  pthread_t EventHandlerTid;
  Proto_MT_Handler session_lost_handler;
  Proto_MT_Handler base_event_handlers[PROTO_MT_EVENT_BASE_RESERVED_LAST 
				       - PROTO_MT_EVENT_BASE_RESERVED_FIRST
				       - 1];
} Proto_Client;

extern Proto_Session *
proto_client_rpc_session(Proto_Client_Handle ch)
{
  Proto_Client *c = ch;
  return &(c->rpc_session);
}

extern Proto_Session *
proto_client_event_session(Proto_Client_Handle ch)
{
  Proto_Client *c = ch;
  return &(c->event_session);
}

extern int
proto_client_set_session_lost_handler(Proto_Client_Handle ch, Proto_MT_Handler h)
{
  Proto_Client *c = ch;
  c->session_lost_handler = h;
  return 1;
}

extern int
proto_client_set_event_handler(Proto_Client_Handle ch, Proto_Msg_Types mt,
			       Proto_MT_Handler h)
{
  int i;
  Proto_Client *c = ch;

  if (mt>PROTO_MT_EVENT_BASE_RESERVED_FIRST && 
      mt<PROTO_MT_EVENT_BASE_RESERVED_LAST) {
    i=mt - PROTO_MT_EVENT_BASE_RESERVED_FIRST - 1;
    c->base_event_handlers[i] = h;
    return 1;
  } else {
    return -1;
  }
}

static int 
proto_client_session_lost_default_hdlr(Proto_Session *s)
{
  fprintf(stderr, "Session lost...:\n");
  proto_session_dump(s);
  return -1;
}

static int 
proto_client_event_null_handler(Proto_Session *s)
{
  fprintf(stderr, 
	  "proto_client_event_null_handler: invoked for session:\n");
  proto_session_dump(s);

  return 1;
}

static void *
proto_client_event_dispatcher(void * arg)
{
  Proto_Client *c;
  Proto_Session *s;
  Proto_Msg_Types mt;
  Proto_MT_Handler hdlr;
  int i;

  pthread_detach(pthread_self());

  c = (Proto_Client *) arg;
  s = &(c->event_session);

  for (;;) {
    if (proto_session_rcv_msg(s)==1) {
      mt = proto_session_hdr_unmarshall_type(s);
      if (mt > PROTO_MT_EVENT_BASE_RESERVED_FIRST && 
	  mt < PROTO_MT_EVENT_BASE_RESERVED_LAST) {
	mt=mt - PROTO_MT_EVENT_BASE_RESERVED_FIRST - 1;
	hdlr = c->base_event_handlers[mt];
	if (hdlr(s)<0) goto leave;
      }
    } else {
      c->session_lost_handler(s);
      goto leave;
    }
  }
 leave:
  close(s->fd);
  return NULL;
}

extern int
proto_client_init(Proto_Client_Handle *ch)
{
  Proto_Msg_Types mt;
  Proto_Client *c;
 
  c = (Proto_Client *)malloc(sizeof(Proto_Client));
  if (c==NULL) return -1;
  bzero(c, sizeof(Proto_Client));

  proto_client_set_session_lost_handler(c, 
			      	proto_client_session_lost_default_hdlr);

  for (mt=PROTO_MT_EVENT_BASE_RESERVED_FIRST+1;
       mt<PROTO_MT_EVENT_BASE_RESERVED_LAST; mt++)
    proto_client_set_event_handler(c, mt, proto_client_event_null_handler);

  *ch = c;
  return 1;
}

int
proto_client_connect(Proto_Client_Handle ch, char *host, PortType port)
{
  Proto_Client *c = (Proto_Client *)ch;

  if (net_setup_connection(&(c->rpc_session.fd), host, port)<0) 
    return -1;

  if (net_setup_connection(&(c->event_session.fd), host, port+1)<0) 
    return -2; 

  if (pthread_create(&(c->EventHandlerTid), NULL, 
		     &proto_client_event_dispatcher, c) !=0) {
    fprintf(stderr, 
	    "proto_client_init: create EventHandler thread failed\n");
    perror("proto_client_init:");
    return -3;
  }

  return 0;
}

static void
marshall_mtonly(Proto_Session *s, Proto_Msg_Types mt) {
  Proto_Msg_Hdr h;
  
  bzero(&h, sizeof(h));
  h.type = mt;
  proto_session_hdr_marshall(s, &h);
};

// all rpc's are assume to only reply only with a return code in the body
// eg.  like the null_mes
static int 
do_generic_dummy_rpc(Proto_Client_Handle ch, Proto_Msg_Types mt)
{
  int rc;
  Proto_Session *s;
  Proto_Client *c = ch;

  s = &(c->rpc_session);
  // marshall

  marshall_mtonly(s, mt);
  rc = proto_session_rpc(s);

  if (rc==1) {
    proto_session_body_unmarshall_int(s, 0, &rc);
  } else {
    c->session_lost_handler(s);
  }
  
  return rc;
}


extern int
proto_client_hello(Proto_Client_Handle ch)
{
  int i = 0, rc = 1, offset = 0;
  Proto_Session *s;
  Proto_Client *c = ch;
  s = &(c->rpc_session);

  printf("Loading...\n\n");

  marshall_mtonly(s, PROTO_MT_REQ_BASE_HELLO);
  proto_session_body_marshall_int(s,i);
 
  rc = proto_session_rpc(s);
  
  if (rc == 1) 
    maze_unmarshall_row(s, offset, i);
  else
    c->session_lost_handler(s);
  
  if (Board.size / 20 > 0) {
    for (i = 1; i < Board.size / 20; i++) {
  
      proto_session_reset_send(s);
      marshall_mtonly(s, PROTO_MT_REQ_BASE_HELLO);
      proto_session_body_marshall_int(s,i);
      rc = proto_session_rpc(s);
      
      if (rc == 1)
	  maze_unmarshall_row(s, 0, i);
      else
	  c->session_lost_handler(s);
    }
  }
  //dump(); 

  return i;
}

extern int
proto_client_new_player(Proto_Client_Handle ch, int * id)
{
  int rc = 1, offset = 0;
  Proto_Session *s;
  Proto_Client *c = ch;
  Player clientPlayer;
  memset(&clientPlayer, '\0', sizeof(clientPlayer));
  s = &(c->rpc_session);

  //printf("Requesting to create a new player...\n\n");

  marshall_mtonly(s, PROTO_MT_REQ_BASE_NEW_PLAYER);

  rc = proto_session_rpc(s);
  
  if (rc != 1)
  {
	  printf("new player rpc returned -1\n");
	  c->session_lost_handler(s);
	  return rc;
  }
  else 
  {

    rc = player_unmarshall(s,0, &clientPlayer);
    *id = clientPlayer.fd;
   
    // put the client player in it's proper place based on team and id.
    player_copy(&(ClientGameState.players[clientPlayer.team][clientPlayer.id]),&clientPlayer);
    
    //player_dump(&(ClientGameState.players[clientPlayer.team][clientPlayer.id]));
    // set the ClientGameState.me pointer to this address. This is where our player will
    // be unmarshalled from now on, with every update from the server.
    ClientGameState.me = &(ClientGameState.players[clientPlayer.team][clientPlayer.id]);
    
    //player_dump(ClientGameState.me);
  }

  //printf("new player id = %d, team = %d\n",p->id,p->team);

  if (rc < 0) printf("Player_unmarshall Error!\n");
  return rc;

}


extern int
proto_client_move(Proto_Client_Handle ch, Player_Move direction)
{
  int rc = 1, offset = 0;
  Proto_Session *s;
  Proto_Client *c = ch;
  s = &(c->rpc_session);

  //printf("Sending move command to server...\n\n");


  marshall_mtonly(s, PROTO_MT_REQ_BASE_MOVE);
  player_marshall(s, ClientGameState.me);
  proto_session_body_marshall_int(s, direction);

  rc = proto_session_rpc(s);

  if (rc != 1)
  {
	  c->session_lost_handler(s);
	  return rc;
  }
  else
  {
	offset = proto_session_body_unmarshall_int(s, 0, &rc);
	player_unmarshall(s, offset, ClientGameState.me);
  }

  if (rc < 0) printf("player move Error!\n");
  // else if (rc == 0) printf("player move rejected by server!\n");
//  else printf("Player move successful.\n");
  return rc;

}


extern int
proto_client_maze_info(Proto_Client_Handle ch, char type) {
  int rc;
  Proto_Session *s;
  Proto_Client *c = ch;

  s = &(c->rpc_session);
  marshall_mtonly(s, PROTO_MT_REQ_BASE_GET_MAZE_INFO);
  proto_session_body_marshall_char(s, type);
  rc = proto_session_rpc(s);

  if (rc == 1)
    proto_session_body_unmarshall_int(s, 0, &rc);
  else
    c->session_lost_handler(s);

  return rc;
}

extern int
proto_client_cell_info(Proto_Client_Handle ch, int x, int y, int * buf) {
  int team, oc, rc;
  char cell_type;
  Proto_Session *s;
  Proto_Client *c = ch;

  s = &(c->rpc_session);
  marshall_mtonly(s, PROTO_MT_REQ_BASE_GET_CELL_INFO);
  proto_session_body_marshall_int(s, x);
  proto_session_body_marshall_int(s, y);
  rc = proto_session_rpc(s);

  if (rc == 1) {
    proto_session_body_unmarshall_char(s, 0, &cell_type);
    if(cell_type == 'i')
      return -1;
    proto_session_body_unmarshall_int(s, sizeof(char), &team);
    proto_session_body_unmarshall_int(s, sizeof(char) + sizeof(int), &oc);

    buf[0] = (int)cell_type;
    buf[1] = team;
    buf[2] = oc;
  }

  else
    c->session_lost_handler(s);

  return rc;
}

extern int
proto_client_dump_maze(Proto_Client_Handle ch) {
  int rc;
  Proto_Session *s;
  Proto_Client *c = ch;

  s = &(c->rpc_session);
  marshall_mtonly(s, PROTO_MT_REQ_BASE_DUMP);
  rc = proto_session_rpc(s);

  if (rc == 1)
    proto_session_body_unmarshall_int(s, 0, &rc);
  else
    c->session_lost_handler(s);

  return rc;
}

extern int 
proto_client_goodbye(Proto_Client_Handle ch, int id, Player * p)
{
  int rc;
  Proto_Session *s;
  Proto_Client *c = ch;

  s = &(c->rpc_session);
  marshall_mtonly(s, PROTO_MT_REQ_BASE_GOODBYE);

  proto_session_body_marshall_int(s,id);
  player_marshall(s,p);

  rc = proto_session_rpc(s);

  close(&(c->rpc_session.fd)); 
  close(&(c->event_session.fd));

  return rc;
}

extern int
proto_client_drop_flag(Proto_Client_Handle ch) {
  Player *p = ClientGameState.me;
  int rc, offset;
  Proto_Session *s;
  Proto_Client *c = ch;

  s = &(c->rpc_session);
  marshall_mtonly(s, PROTO_MT_REQ_BASE_DROP_FLAG);
  player_marshall(s,p);

  rc = proto_session_rpc(s);  

  if (rc != 1)
  {
        c->session_lost_handler(s);
	return rc;
  }
  else
  {
	offset = proto_session_body_unmarshall_int(s, 0, &rc);
	player_unmarshall(s, offset, ClientGameState.me);
  }

  return rc;
}

extern int
proto_client_drop_shovel(Proto_Client_Handle ch) {
  Player *p = ClientGameState.me;
  int rc, offset;
  Proto_Session *s;
  Proto_Client *c = ch;

  s = &(c->rpc_session);
  marshall_mtonly(s, PROTO_MT_REQ_BASE_DROP_SHOVEL);
  player_marshall(s,p);

  rc = proto_session_rpc(s);  


  if (rc != 1)
  {
        c->session_lost_handler(s);
	return rc;
  }
  else
  {
	offset = proto_session_body_unmarshall_int(s, 0, &rc);
	player_unmarshall(s, offset, ClientGameState.me);
  }

  return rc;
}

extern int
proto_client_pickup_flag(Proto_Client_Handle ch) {
  Player *p = ClientGameState.me;
  int rc, offset;
  Proto_Session *s;
  Proto_Client *c = ch;

  s = &(c->rpc_session);
  marshall_mtonly(s, PROTO_MT_REQ_BASE_PICKUP_FLAG);
  player_marshall(s,p);

  rc = proto_session_rpc(s);  

  if (rc != 1)
  {
        c->session_lost_handler(s);
	return rc;
  }
  else
  {
	offset = proto_session_body_unmarshall_int(s, 0, &rc);
	player_unmarshall(s, offset, ClientGameState.me);
  }

  return rc;
}

extern int
proto_client_pickup_shovel(Proto_Client_Handle ch) {
  int rc, offset;
  Player *p = ClientGameState.me;
  Proto_Session *s;
  Proto_Client *c = ch;

  s = &(c->rpc_session);
  marshall_mtonly(s, PROTO_MT_REQ_BASE_PICKUP_SHOVEL);
  player_marshall(s,p);

  rc = proto_session_rpc(s);  

  if (rc != 1)
  {
        c->session_lost_handler(s);
	return rc;
  }
  else
  {
	offset = proto_session_body_unmarshall_int(s, 0, &rc);
	player_unmarshall(s, offset, ClientGameState.me);
  }

  return rc;
}

extern void
proto_client_disconnect(Proto_Client_Handle ch)
{
  Proto_Client *c = ch;
  
  close(&(c->rpc_session.fd));
  close(&(c->event_session.fd));
}
  



