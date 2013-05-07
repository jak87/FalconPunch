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
#include <string.h>
/*
#include "net.h"
#include "protocol.h"
#include "protocol_utils.h"
#include "protocol_server.h"
#include "maze.h"
#include "game_control.h"*/

#include "server_game_code.h"

#define PROTO_SERVER_MAX_EVENT_SUBSCRIBERS 1024

struct {
  FDType   RPCListenFD;
  PortType RPCPort;


  FDType             EventListenFD;
  PortType           EventPort;
  pthread_t          EventListenTid;
  pthread_mutex_t    EventSubscribersLock;
  int                EventLastSubscriber;
  int                EventNumSubscribers;
  FDType             EventSubscribers[PROTO_SERVER_MAX_EVENT_SUBSCRIBERS];
  Proto_Session      EventSession; 
  pthread_t          RPCListenTid;
  Proto_MT_Handler   session_lost_handler;
  Proto_MT_Handler   base_req_handlers[PROTO_MT_REQ_BASE_RESERVED_LAST - 
				       PROTO_MT_REQ_BASE_RESERVED_FIRST-1];
} Proto_Server;

extern PortType proto_server_rpcport(void) { return Proto_Server.RPCPort; }
extern PortType proto_server_eventport(void) { return Proto_Server.EventPort; }
extern Proto_Session *

proto_server_event_session(void) 
{ 
  return &Proto_Server.EventSession; 
}

extern int
proto_server_set_session_lost_handler(Proto_MT_Handler h)
{
  Proto_Server.session_lost_handler = h;
}


/***********************************
 ***** Basic RPC & Event Stuff *****
 ***********************************/

extern int
proto_server_set_req_handler(Proto_Msg_Types mt, Proto_MT_Handler h)
{
  int i;

  if (mt>PROTO_MT_REQ_BASE_RESERVED_FIRST &&
      mt<PROTO_MT_REQ_BASE_RESERVED_LAST) {
    i = mt - PROTO_MT_REQ_BASE_RESERVED_FIRST - 1;

    Proto_Server.base_req_handlers[i] = h;

    return 1;
  } else {
    return -1;
  }
}



// Records a new subscriber to the event port in the EventSubscriber array 

static int
proto_server_record_event_subscriber(int fd, int *num)
{
  int rc=-1;

  pthread_mutex_lock(&Proto_Server.EventSubscribersLock);

  if (Proto_Server.EventLastSubscriber < PROTO_SERVER_MAX_EVENT_SUBSCRIBERS
      && Proto_Server.EventSubscribers[Proto_Server.EventLastSubscriber]
      ==-1) {
    *num = Proto_Server.EventLastSubscriber;
    Proto_Server.EventSubscribers[Proto_Server.EventLastSubscriber] = fd;
    Proto_Server.EventLastSubscriber++;
    Proto_Server.EventNumSubscribers++;
    rc = 1;
  } 
  else 
  {
    int i;
    for (i=0; i< PROTO_SERVER_MAX_EVENT_SUBSCRIBERS; i++) {
      if (Proto_Server.EventSubscribers[i]==-1) {
	*num=i;
	Proto_Server.EventSubscribers[i] = fd;
        Proto_Server.EventNumSubscribers++;
	rc=1;
      }
    }
  }

  pthread_mutex_unlock(&Proto_Server.EventSubscribersLock);

  return rc;
}



// Listens for new connections to the event port, then records
// them to the EventSubscribers array

static void *
proto_server_event_listen(void *arg)
{
  //fprintf(stderr, "proto_server_event_listen\n");
  int fd = Proto_Server.EventListenFD;
  int connfd;

  if (net_listen(fd)<0) {
    exit(-1);
  }

  for (;;) {
    connfd = net_accept(fd);
    if (connfd < 0) {
      fprintf(stderr, "Error: EventListen accept failed (%d)\n", errno);
    } else {
      int i;
      fprintf(stderr, "EventListen: connfd=%d -> ", connfd);

      if (proto_server_record_event_subscriber(connfd,&i)<0) {
	fprintf(stderr, "oops no space for any more event subscribers\n");
	close(connfd);
      } else {
	fprintf(stderr, "subscriber num %d\n", i);
      }
    } 
  }
} 



// Goes through every member of the EventSubscriber array (until it
// hits the last subscriber) and sends an update to them!

void
proto_server_post_event(void) 
{
  int i;
  int num;

  pthread_mutex_lock(&Proto_Server.EventSubscribersLock);

  i = 0;
  printf("NumSubscribers = %d\n", Proto_Server.EventNumSubscribers);
  num = Proto_Server.EventNumSubscribers;

  //fprintf(stderr, "Server will send updates to %i subscribers\n\n", num);

  while (num) { 
    Proto_Server.EventSession.fd = Proto_Server.EventSubscribers[i];

    // If the EventSession == -1, then it was a player that left so we
    // skip it
    if (Proto_Server.EventSession.fd != -1) {
      num--;
      // try to send the message, without resetting it
      //fprintf(stderr, "Send update to subscriber\n");
      if (proto_session_send_msg(&(Proto_Server.EventSession), 0) < 0) {

	//fprintf(stderr, "\nPost_Event has encountered a dead player!\n");
	//printf("Event Update Failed, removing dead player...\n");

	// if the message failed, it means that the player is no longer
	// in the game, so we set their EventSubscriber fd to -1, reduce
	// the number of subscribers, and close the port.

	Proto_Server.EventSubscribers[i]=-1;
	Proto_Server.EventNumSubscribers--;
	close(Proto_Server.EventSession.fd);
	//Proto_Server.session_lost_handler(&(Proto_Server.EventSession));
      } 
      else {
	//printf("Event Update was Successful! (fd = %d)\n", 
	//     Proto_Server.EventSession.fd);
      }

      // FIXME: add ack message here to ensure that game is updated 
      // correctly everywhere... at the risk of making server dependent
      // on client behaviour  (use time out to limit impact... drop
      // clients that misbehave but be carefull of introducing deadlocks
    }
    i++;
  }
  proto_session_reset_send(&Proto_Server.EventSession);
  pthread_mutex_unlock(&Proto_Server.EventSubscribersLock);
}



// Listens for new RPC requests and calls the methods corresponding
// to the message type in the header of the session

static void *
proto_server_req_dispatcher(void * arg)
{
  //fprintf(stderr, "proto_server_req_dispatcher\n");

  Proto_Session s;
  Proto_Msg_Types mt;
  Proto_MT_Handler hdlr;
  int i;
  unsigned long arg_value = (unsigned long) arg;
  
  pthread_detach(pthread_self());

  proto_session_init(&s);

  s.fd = (FDType) arg_value;

  //fprintf(stderr, "proto_rpc_dispatcher: %p: Started: fd=%d\n", 
  //  pthread_self(), s.fd);

  // Listening for RPCs
  for (;;) {
    if (proto_session_rcv_msg(&s)==1) {
        mt = proto_session_hdr_unmarshall_type(&s);

	// if mt == 0 (which is would in case of a disconnect),
	// then it requests Proto_Server.base_req_handlers[-1]
	// (thus segfaulting!), so disconnect the RPC!
	if(mt < 1)
	  goto leave;
	hdlr = Proto_Server.base_req_handlers[mt - PROTO_MT_REQ_BASE_RESERVED_FIRST - 1];
	
	if (hdlr(&s)<0) goto leave;
    } else {
      goto leave;
    }
  }
 leave:
  //Proto_Server.session_lost_handler(&s);
  close(s.fd);
  printf("Attempting to remove s.fd = %d (From req_dispatcher)\n",s.fd);
  remove_player(s.fd);
  return NULL;
}



// Listens for new connections to the RPC port

static void *
proto_server_rpc_listen(void *arg)
{
  //fprintf(stderr, "proto_server_rpc_listen\n");

  int fd = Proto_Server.RPCListenFD;
  unsigned long connfd;
  pthread_t tid;
  
  if (net_listen(fd) < 0) {
    fprintf(stderr, "Error: proto_server_rpc_listen listen failed (%d)\n", errno);
    exit(-1);
  }

  for (;;) {
    connfd = net_accept(fd);
    if (connfd < 0) {
      fprintf(stderr, "Error: proto_server_rpc_listen accept failed (%d)\n", errno);
    } else {
      pthread_create(&tid, NULL, &proto_server_req_dispatcher,
		     (void *)connfd);
    }
  }
}

extern int
proto_server_start_rpc_loop(void)
{
  //fprintf(stderr, "proto_server_start_rpc_loop\n");


  if (pthread_create(&(Proto_Server.RPCListenTid), NULL, 
		     &proto_server_rpc_listen, NULL) !=0) {
    fprintf(stderr, 
        "proto_server_rpc_listen: pthread_create: create RPCListen thread failed\n");
    perror("pthread_create:");
    return -3;
  }
  return 1;
}



/*****************************
 **** Some Basic Handlers ****
 *****************************/


static int 
proto_session_lost_default_handler(Proto_Session *s)
{
  //fprintf(stderr, "Session lost...:\n");
  //proto_session_dump(s);
  return -1;
}

static int 
proto_server_mt_null_handler(Proto_Session *s)
{
  int rc=1;
  Proto_Msg_Hdr h;
  
  fprintf(stderr, "proto_server_mt_null_handler: invoked for session:\n");
  proto_session_dump(s);

  // setup dummy reply header : set correct reply message type and 
  // everything else empty
  bzero(&h, sizeof(s));
  h.type = proto_session_hdr_unmarshall_type(s);
  h.type += PROTO_MT_REP_BASE_RESERVED_FIRST;
  proto_session_hdr_marshall(s, &h);

  // setup a dummy body that just has a return code 
  proto_session_body_marshall_int(s, 0xdeadbeef);

  rc=proto_session_send_msg(s,1);

  return rc;
}



/*************************************************
 ********* Disconnect and Initialization *********
 *************************************************/



extern void proto_disconnect() 
{
  Proto_Session *s;
  Proto_Msg_Hdr hdr;

  s = proto_server_event_session();
  bzero(&hdr, sizeof(s));
  hdr.type = PROTO_MT_EVENT_BASE_DISCONNECT;
  proto_session_hdr_marshall(s, &hdr);
   
  printf("Posting disconnect event!\n");
  proto_server_post_event();
}

extern int
proto_server_init(void)
{
  fprintf(stderr, "proto_server_init\n");

  // initialize Game
  game_init();

  int i;
  int rc;

  proto_session_init(&Proto_Server.EventSession);

  proto_server_set_session_lost_handler(
				     proto_session_lost_default_handler);
  for (i=PROTO_MT_REQ_BASE_RESERVED_FIRST+1; 
       i<PROTO_MT_REQ_BASE_RESERVED_LAST; i++) {
    switch (i) {
      case PROTO_MT_REQ_BASE_HELLO:
	proto_server_set_req_handler(i, hello_handler);
	break;
      case PROTO_MT_REQ_BASE_NEW_PLAYER:
	proto_server_set_req_handler(i, new_player_handler);
	break;
      case PROTO_MT_REQ_BASE_GET_MAZE_INFO:
        proto_server_set_req_handler(i, game_maze_info_handler);
        break;
      case PROTO_MT_REQ_BASE_GET_CELL_INFO:
        proto_server_set_req_handler(i, game_cell_info_handler);
        break;
      case PROTO_MT_REQ_BASE_DUMP:
        proto_server_set_req_handler(i, game_dump_handler);
        break;
      case PROTO_MT_REQ_BASE_MOVE:
        proto_server_set_req_handler(i, game_move_handler);
        break;
      case PROTO_MT_REQ_BASE_GOODBYE:
	proto_server_set_req_handler(i, goodbye_handler);
	break;
      case PROTO_MT_REQ_BASE_PICKUP_FLAG:
	proto_server_set_req_handler(i, pickup_flag_handler);
	break;
      case PROTO_MT_REQ_BASE_DROP_FLAG:
	proto_server_set_req_handler(i, drop_flag_handler);
	break;
      case PROTO_MT_REQ_BASE_PICKUP_SHOVEL:
	proto_server_set_req_handler(i, pickup_shovel_handler);
	break;
      case PROTO_MT_REQ_BASE_DROP_SHOVEL:
	proto_server_set_req_handler(i, drop_shovel_handler);
	break;
      default:
        proto_server_set_req_handler(i, proto_server_mt_null_handler);
        break;
    }
  }


  for (i=0; i<PROTO_SERVER_MAX_EVENT_SUBSCRIBERS; i++) {
    Proto_Server.EventSubscribers[i]=-1;
  }
  Proto_Server.EventNumSubscribers=0;
  Proto_Server.EventLastSubscriber=0;
  pthread_mutex_init(&(Proto_Server.EventSubscribersLock), 0);


  rc=net_setup_listen_socket(&(Proto_Server.RPCListenFD),
			     &(Proto_Server.RPCPort));

  if (rc==0) { 
    fprintf(stderr, "prot_server_init: net_setup_listen_socket: FAILED for RPCPort\n");
    return -1;
  }

  Proto_Server.EventPort = Proto_Server.RPCPort + 1;

  rc=net_setup_listen_socket(&(Proto_Server.EventListenFD),
			     &(Proto_Server.EventPort));

  if (rc==0) { 
    fprintf(stderr, "proto_server_init: net_setup_listen_socket: FAILED for EventPort=%d\n", 
	    Proto_Server.EventPort);
    return -2;
  }

  if (pthread_create(&(Proto_Server.EventListenTid), NULL, 
		     &proto_server_event_listen, NULL) !=0) {
    fprintf(stderr, 
	    "proto_server_init: pthread_create: create EventListen thread failed\n");
    perror("pthread_createt:");
    return -3;
  }

  return 0;
}


