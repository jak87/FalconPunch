#ifndef __DAGAME_PLAYER_H__
#define __DAGAME_PLAYER_H__

typedef struct {
  int id;
  int x;
  int y;
  int team;
  int state;
  FDType fd;
} Player;


int player_marshall(Proto_Session *s, Player *player);

#endif
