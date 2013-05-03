#ifndef __DAGAME_PLAYER_H__
#define __DAGAME_PLAYER_H__

#include "net.h"
#include "uistandalone.h"

typedef enum {
  PLAYER_NORMAL,
  PLAYER_OWN_FLAG,
  PLAYER_OPPONENT_FLAG,
  PLAYER_JAILED,
  PLAYER_FREE
} Player_State;

typedef enum  {
  MOVE_LEFT,
  MOVE_RIGHT,
  MOVE_UP,
  MOVE_DOWN
} Player_Move;

typedef struct {
  // if id == -1, player is inactive 
  int id;
  int x;
  int y;
  int team;
  Player_State state;
  int shovel;
  FDType fd;
  UI_Player * uip;
} Player;


void player_dump(Player *p);
void player_copy(Player *p1, Player *p2);
int player_marshall(Proto_Session *s, Player * player);
int player_unmarshall(Proto_Session *s, int offset, Player * player);

#endif
