#ifndef __DAGAME_PLAYER_H__
#define __DAGAME_PLAYER_H__

#include "net.h"

typedef struct {
  int id;
  int x;
  int y;
  int team;
  int state;
  FDType fd;
} Player;


int player_marshall(Proto_Session *s, Player *player);
int player_unmarshall(Proto_Session *s, int offset, Player *player);

#endif
