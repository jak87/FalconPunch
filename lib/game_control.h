#ifndef __DAGAME_GAME_CONTROL_H__
#define __DAGAME_GAME_CONTROL_H__

//#include "net.h"
//#include "protocol.h"
#include "objects.h"
#include "protocol_session.h"
#include "maze.h"
#include "player.h"

#define MAX_NUM_PLAYERS 100

typedef enum {
  WAITING_TO_START,
  IN_PROGRESS,
  TEAM_0_VICTORY,
  TEAM_1_VICTORY
} Game_State;

struct {
  Player *players[2][MAX_NUM_PLAYERS];
  Game_State gameStatus;
  int numPlayers[2];
  Object objects[4];
  Cell * changedCell;
  pthread_mutex_t masterLock;
} GameState;

struct {
  Player *me;
  Player players[2][MAX_NUM_PLAYERS];
  Object objects[4];
  Game_State gameStatus;
  pthread_mutex_t masterLock;
} ClientGameState;

extern int game_load_board();

/**
 * Initialize a game control instance. Loads the default board
 * to be used as a starting state of the game.
 */
extern void game_init();

/**
 * Create a player in the game. If team is 2, assign the player to
 * the team with less players. If team is 0 or 1, assign the player
 * to that team.
 */
extern Player* game_create_player(int team);

/**
 * Move player p in the given direction. Any change of state
 * will be communicated to Player and Cell objects. Player and Cell
 * objects are responsible for notifying anything else that needs to
 * know about the updated state (like the connected clients).
 *
 * Return 1 if anything in the game state changed. 0 otherwise.
 */
extern int game_move_player(Player* p, Player_Move direction);

extern int game_player_pickup(Player* p);
extern int player_drop_flag(Player* p);
extern int player_drop_shovel(Player* p);
extern int player_pickup_flag(Player* p);
extern int player_pickup_shovel(Player* p);

#endif
