#ifndef __DAGAME_PLAYER_H__
#define __DAGAME_PLAYER_H__

#include "net.h"
#include "uistandalone.h"

typedef struct {
  // if id == -1, player is inactive 
  int id;
  int x;
  int y;
  int team;
  int state;
  int flag;
  int shovel;
  FDType fd;
  UI_Player * uip;
} Player;

typedef enum  {
  MOVE_LEFT,
  MOVE_RIGHT,
  MOVE_UP,
  MOVE_DOWN
} Player_Move;

typedef enum {
  PLAYER_FREE,
  PLAYER_JAILED
} Player_State;


void player_copy(Player *p1, Player *p2);
int player_marshall(Proto_Session *s, Player * player);
int player_unmarshall(Proto_Session *s, int offset, Player * player);

#endif
