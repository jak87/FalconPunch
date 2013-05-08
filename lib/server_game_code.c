#include "server_game_code.h"

extern int do_send_game_state() {
  Proto_Session *s;
  Proto_Msg_Hdr hdr;

  s = proto_server_event_session();
  bzero(&hdr, sizeof(s));
  hdr.type = PROTO_MT_EVENT_BASE_UPDATE_GAME_STATE;
  proto_session_hdr_marshall(s, &hdr);
  proto_session_body_marshall_int(s, GameState.gameStatus);
  proto_server_post_event();
  //proto_disconnect();
  return 1;
}

extern int do_send_players_state()
{
  Proto_Session *s;
  Proto_Msg_Hdr hdr;

  s = proto_server_event_session();
  bzero(&hdr, sizeof(s));
  hdr.type = PROTO_MT_EVENT_BASE_UPDATE_PLAYERS;
  proto_session_hdr_marshall(s, &hdr);

  //TODO: lock, handle errors

  //pthread_mutex_lock(&GameState.masterLock);

  // Marshall objects first
  object_marshall(s, &(GameState.objects[0]));
  object_marshall(s, &(GameState.objects[1]));
  object_marshall(s, &(GameState.objects[2]));
  object_marshall(s, &(GameState.objects[3]));

  int changed = GameState.changedCell != NULL;
  proto_session_body_marshall_int(s, changed);
  if (changed)
  {
    maze_marshall_cell(s, GameState.changedCell);
    GameState.changedCell = NULL;
  }

  // Then marshall number of players
  int totalPlayers = GameState.numPlayers[0] + GameState.numPlayers[1];
  proto_session_body_marshall_int(s, totalPlayers);

  int i;
  for (i = 0; i < MAX_NUM_PLAYERS; i++)
  {
    if (GameState.players[0][i] != NULL)
      player_marshall(s, GameState.players[0][i]);
    if (GameState.players[1][i] != NULL)
      player_marshall(s, GameState.players[1][i]);
  }
  
  //pthread_mutex_unlock(&GameState.masterLock);

  //printf("Posting event!\n");
  proto_server_post_event();

  return 1;
}


extern int
game_maze_info_handler(Proto_Session *s)
{
  int rc=1;
  char c;
  int value = -1;
  Proto_Msg_Hdr h;

  // read in character to see what info the client wants
  proto_session_body_unmarshall_char(s,0,&c);

  switch (c)
  {
    case 'j':
      value = Board.total_j;
      break;
    case 'J':
	  value = Board.total_J;
	  break;
    case 'h':
	  value = Board.total_h;
	  break;
    case 'H':
	  value = Board.total_H;
	  break;
    case '#':
	  value = Board.total_wall;
	  break;
    case ' ':
	  value = Board.total_floor;
	  break;
    case 'd':
	  value = Board.size;
	  break;
    default:
      value = -1;
      break;
  }

  bzero(&h, sizeof(s));
  h.type = PROTO_MT_REP_BASE_GET_MAZE_INFO;
  proto_session_hdr_marshall(s, &h);
  proto_session_body_marshall_int(s, value);

  rc = proto_session_send_msg(s,1);

  return rc;
}

/**
 * Expects 2 integers for x, y
 * Returns
 */
extern int
game_cell_info_handler(Proto_Session *s)
{
  int rc=1;
  Proto_Msg_Hdr h;
  int x, y;

  proto_session_body_unmarshall_int(s, 0, &x);
  proto_session_body_unmarshall_int(s, sizeof(int), &y);

  bzero(&h, sizeof(s));
  h.type = PROTO_MT_REP_BASE_GET_CELL_INFO;
  proto_session_hdr_marshall(s, &h);

  if (x < 0 || x >= Board.size || y < 0 || y > Board.size)
  {
    // if cell coordinates are invalid, return 'i'
    proto_session_body_marshall_char(s, 'i');
  }
  else
  {
    proto_session_body_marshall_char(s, tolower(Board.cells[x][y]->type));
    proto_session_body_marshall_int(s, Board.cells[x][y]->team);
    // for now, all cells are unoccupied
    proto_session_body_marshall_int(s, 0);
  }

  rc = proto_session_send_msg(s,1);

  return rc;
}

/* Searches through players struct to find player with given fd
 *
 * param: FDType fd - File Descriptor to search for
 * Returns pointer to player if found, NULL otherwise
 */
static Player *
find_player_by_fd(FDType fd){
  int i;
  int j = 0;
  Player *player;
  for(i=0; i <= 1;i++){
    while ((player = GameState.players[i][j]) != NULL){
      if(player->fd == fd) return player;
    }
    j = 0;
  }
  return NULL;
}

extern int
game_move_handler(Proto_Session *s)
{
  int rc=1;
  Proto_Msg_Hdr h;

  Player clientPlayer;
  int offset = player_unmarshall(s, 0, &clientPlayer);

  //player_dump(&clientPlayer);

  int direction;
  proto_session_body_unmarshall_int(s, offset, &direction);

  // find the server version of this player
  Player* serverPlayer = GameState.players[clientPlayer.team][clientPlayer.id];

  if(serverPlayer == NULL) {return -1;}
  //printf("Located server representation of this player:\n");
  //player_dump(serverPlayer);

  int value = game_move_player(serverPlayer, (Player_Move) direction);

  bzero(&h, sizeof(s));
  h.type = PROTO_MT_REP_BASE_MOVE;
  proto_session_hdr_marshall(s, &h);

  proto_session_body_marshall_int(s, value);
  player_marshall(s, serverPlayer); 

  rc = proto_session_send_msg(s,1);

  // send updates to all clients with the states of all players after this move
  do_send_players_state();

  return rc;
}

extern int
pickup_flag_handler(Proto_Session *s) 
{ 
  int rc = 1, i;
  Proto_Msg_Hdr h;
  Player clientPlayer;

  player_unmarshall(s, 0, &clientPlayer);
 
  Player* serverPlayer = GameState.players[clientPlayer.team][clientPlayer.id];
  i = player_pickup_flag(serverPlayer);

  bzero(&h, sizeof(s));
  h.type = PROTO_MT_REP_BASE_PICKUP_FLAG;
  proto_session_hdr_marshall(s, &h);

  proto_session_body_marshall_int(s,i);
  player_marshall(s, serverPlayer); 

  rc = proto_session_send_msg(s,1);  

  do_send_players_state();
 
  return rc;
}

extern int
drop_flag_handler(Proto_Session *s) 
{ 
  int rc = 1, i;
  Proto_Msg_Hdr h;
  Player clientPlayer;

  player_unmarshall(s, 0, &clientPlayer);
 
  Player* serverPlayer = GameState.players[clientPlayer.team][clientPlayer.id];
  i = player_drop_flag(serverPlayer);

  // Game is over!
  if (i > 1) {
    GameState.gameStatus = i;
    do_send_game_state();
  }

  bzero(&h, sizeof(s));
  h.type = PROTO_MT_REP_BASE_DROP_FLAG;
  proto_session_hdr_marshall(s, &h);
    
  proto_session_body_marshall_int(s,1);
  player_marshall(s, serverPlayer); 
  
  rc = proto_session_send_msg(s,1);  
  
  do_send_players_state();
  
  return rc;
}

extern int
pickup_shovel_handler(Proto_Session *s)  
{ 
  int rc = 1, i;
  Proto_Msg_Hdr h;
  Player clientPlayer;

  player_unmarshall(s, 0, &clientPlayer);
 
  Player* serverPlayer = GameState.players[clientPlayer.team][clientPlayer.id];
  i = player_pickup_shovel(serverPlayer);

  bzero(&h, sizeof(s));
  h.type = PROTO_MT_REP_BASE_PICKUP_SHOVEL;
  proto_session_hdr_marshall(s, &h);

  proto_session_body_marshall_int(s,i);
  player_marshall(s, serverPlayer); 

  rc = proto_session_send_msg(s,1);  

  do_send_players_state();
 
  return rc;
}

extern int
drop_shovel_handler(Proto_Session *s) 
{ 
  int rc = 1, i;
  Proto_Msg_Hdr h;
  Player clientPlayer;

  player_unmarshall(s, 0, &clientPlayer);
 
  Player* serverPlayer = GameState.players[clientPlayer.team][clientPlayer.id];
  i = player_drop_shovel(serverPlayer);

  bzero(&h, sizeof(s));
  h.type = PROTO_MT_REP_BASE_DROP_SHOVEL;
  proto_session_hdr_marshall(s, &h);

  proto_session_body_marshall_int(s,i);
  player_marshall(s, serverPlayer); 

  rc = proto_session_send_msg(s,1);  

  do_send_players_state();
 
  return rc;
}

extern int
game_dump_handler(Proto_Session *s)
{
  int rc=1;
  Proto_Msg_Hdr h;

  dump();

  bzero(&h, sizeof(s));
  h.type = PROTO_MT_REP_BASE_DUMP;
  proto_session_hdr_marshall(s, &h);

  rc = proto_session_send_msg(s,1);

  return rc;
}

extern void
remove_player(int fd_id) {

  int i;
  Player *p = NULL;

  for(i = 0; i < MAX_NUM_PLAYERS; i++) {
    if(GameState.players[0][i] != NULL) {
      if(GameState.players[0][i]->fd == fd_id) {
	p = GameState.players[0][i];
	break;
      }
    }
    if(GameState.players[1][i] != NULL) {
      if(GameState.players[1][i]->fd == fd_id) {
	p = GameState.players[1][i];
	break;
      }
    }
  }

  //Player already removed
  if (p == NULL) {
    //printf("The player was already removed\n");
    return;
  }

  // Adjust the gamestate accordingly
  Board.cells[p->y][p->x]->occupant = NULL;
  GameState.players[p->team][p->id] = NULL;
  GameState.numPlayers[p->team]--;
  free(GameState.players[p->team][p->id]);
  
  // printf("Player should be successfully removed!\n");

  // Update the players
  do_send_players_state();
}


extern int
goodbye_handler(Proto_Session *s)
{
  Player p;
  int fd_id, offset;

  // Gets the id in the EventSubscriber array first, then the player info
  offset = proto_session_body_unmarshall_int(s,0,&fd_id);
  if (offset > 0)
    player_unmarshall(s,offset,&p);
  else
    printf("Error!\n");

  printf("p.fd = %d\n", p.fd);
  remove_player(p.fd);

  return -1;
}

extern int
hello_handler(Proto_Session *s)
{
  int i, rc=1;
  Proto_Msg_Hdr h;
  proto_session_body_unmarshall_int(s,0,&i);

  bzero(&h, sizeof(s));
  h.type = PROTO_MT_REP_BASE_HELLO;
  proto_session_hdr_marshall(s, &h);
  
  maze_marshall_row(s,i);

  rc = proto_session_send_msg(s,1);
  return rc;
}

extern int
new_player_handler(Proto_Session *s)
{
  int rc=1;
  Proto_Msg_Hdr h;
  // nothing to unmarshall.

  // create a new player on a team that has fewer players.

  Player* p = game_create_player(2);
  // remember the id of the connection

  p->fd = s->fd; 

  bzero(&h, sizeof(s));
  h.type = PROTO_MT_REP_BASE_NEW_PLAYER;
  proto_session_hdr_marshall(s, &h);
  player_marshall(s, p);

  rc = proto_session_send_msg(s,1);

  if(GameState.numPlayers[0] + GameState.numPlayers[1] > 1) {
    GameState.gameStatus = IN_PROGRESS;
    do_send_game_state();
  }

  return rc;
}
