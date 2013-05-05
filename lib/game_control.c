#include "net.h"
#include "protocol.h"
#include "protocol_session.h"
#include "maze.h"
#include "objects.h"
#include "player.h"
#include "game_control.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define flag_0 GameState.objects[0]
#define flag_1 GameState.objects[1]
#define shovel_0 GameState.objects[2]
#define shovel_1 GameState.objects[3]

extern int game_load_board()
{
  int rc = 1;
  bzero(&Board, sizeof(Board));

  FILE* map = fopen(MAP_PATH, "r");
  if(map == NULL) {return -1; }

  char buf[MAX_BOARD_SIZE];
  int row = 0;
  int col = 0;
  while(fgets(buf, MAX_BOARD_SIZE, map) != NULL) {
    int mid = strlen(buf)/2;
    Board.size = mid*2;
    for (col=0; buf[col]; col++) {
      Board.cells[row][col] = (Cell *) malloc(sizeof(Cell));
      switch(buf[col]) {

        case ' ': //floor cell
          Board.total_floor++;
          break;

        case '#': //wall cell
          if (row > 0 && row < Board.size-1 && col > 0 && col < Board.size -1)
            // Any wall that's not an outer wall is initially set to be destructable
            Board.cells[row][col]->destructable = 1;
          Board.total_wall++;
          break;

        case 'h': //team1 home cell
          // set the first home cell of team 0 to be the home location for their shovel
          if (Board.shovel_home[0] == NULL) 
	    Board.shovel_home[0] = Board.cells[row][col];
          Board.home_cells[0][Board.total_h] = Board.cells[row][col];
          Board.total_floor++;
          Board.total_h++;
          break;

        case 'H': //team2 home cell
          // set the last home cell of team 1 to be the home location for their shovel
          Board.shovel_home[1] = Board.cells[row][col];
          Board.home_cells[1][Board.total_H] = Board.cells[row][col];
          Board.total_floor++;
          Board.total_H++;
          break;

        case 'j': //team1 jail cell
          Board.jail_cells[0][Board.total_j] = Board.cells[row][col];
          Board.total_floor++;
          Board.total_j++;
          break;

        case 'J': //team2 jail cell
          Board.jail_cells[1][Board.total_J] = Board.cells[row][col];
          Board.total_floor++;
          Board.total_J++;
          break;

        case '\n':
          continue;

        default: //unknown
          fclose(map);
          return -2;
      }
      Board.cells[row][col]->x = col;
      Board.cells[row][col]->y = row;
      Board.cells[row][col]->type = buf[col];
      Board.cells[row][col]->team = (col<mid) ? 0 : 1;
    }
    row++;
  }
  fclose(map);

  printf("Board loaded!\n");
  // TODO: handle some error cases and return -1 if something went wrong
  return rc;
}

extern void make_neighboring_cells_indestructable(Cell * cell)
{
  // It doesn't matter that we make some floor/jail/home cells
  // indestructable. That only matters for walls.
  // We are safe in assuming that a jail or home cell will always
  // have neighbors in all directions, because of the outer wall.
  Board.cells[cell->y-1][cell->x]->destructable = 0;
  Board.cells[cell->y+1][cell->x]->destructable = 0;
  Board.cells[cell->y][cell->x-1]->destructable = 0;
  Board.cells[cell->y][cell->x+1]->destructable = 0;
}

extern void game_set_indestructable_cells()
{
  int t, i;
  for (t = 0; t < 2; t++)
  {
    for (i = 0; i < MAX_HOME_CELLS; i++)
      if (Board.home_cells[t][i] != NULL)
        make_neighboring_cells_indestructable(Board.home_cells[t][i]);

    for (i = 0; i < MAX_JAIL_CELLS; i++)
      if (Board.jail_cells[t][i] != NULL)
        make_neighboring_cells_indestructable(Board.jail_cells[t][i]);
  }
}


extern int game_set_flag_start_position(Object* object)
{
  int x,y,success = 0;
  srand(time(NULL));

  while (success == 0)
  {
    // we should hit an empty cell sooner or later
    y = rand() % Board.size;
	x = rand() % (Board.size / 2);

	// adjust x according to team
	x = x + (Board.size / 2) * object->team;

	Cell * myCell = Board.cells[y][x];

	// Don't spawn flag on a wall or in jail, or on a shovel's home
    if (myCell->type != '#' && myCell->type != 'j' && myCell->type != 'J'
    		&& !(myCell->x == Board.shovel_home[object->team]->x
    		     && myCell->y == Board.shovel_home[object->team]->y))
    {
      object->x = x;
      object->y = y;
      printf("Setting team %d's flag to start position: %d,%d\n", object->team, object->x, object->y);
	  success = 1;
	}
  }

  return success;
}


extern int game_set_shovel_start_position(Object* object)
{
  object->x = Board.shovel_home[object->team]->x;
  object->y = Board.shovel_home[object->team]->y;
  printf("Setting team %d's shovel to start position: %d,%d\n", object->team, object->x, object->y);
  return 1;
}

extern int game_init_objects()
{
  int i;

  for (i = 0; i < 4; i++)
  {
    GameState.objects[i].visible = 1;
    GameState.objects[i].team = i % 2;
    if (i < 2)
    {
      GameState.objects[i].type = FLAG;
      game_set_flag_start_position(&(GameState.objects[i]));
    }
    else
    {
      GameState.objects[i].type = SHOVEL;
      game_set_shovel_start_position(&(GameState.objects[i]));
    }
  }

  return 1;
}

extern void game_init()
{
  bzero(&GameState, sizeof(GameState));

  pthread_mutex_init(&(GameState.masterLock), 0);

  game_load_board();
  game_set_indestructable_cells();
  game_init_objects();

  GameState.gameStatus = 0;
  GameState.numPlayers[0] = 0;
  GameState.numPlayers[1] = 0;
}

/**
 * Set player position in the given cell. Assuming cell is empty.
 * Updates the cell's pointers and the player's coordinates.
 * Should be locked by whoever calls this method.
 */
void game_set_player_position(Player* p, Cell* c, int updateOldCell)
{
  printf("Updating player position to %d,%d\n", c->x, c->y);

  if (updateOldCell)
  {
    Cell* oldCell = Board.cells[p->y][p->x];
    oldCell->occupant = NULL;
  }
  p->x = c->x;
  p->y = c->y;
  c->occupant = p;

  // If the player is carrying a shovel, move it along
  if (p->shovel > 0)
  {
    GameState.objects[p->shovel+1].x = p->x;
    GameState.objects[p->shovel+1].y = p->y;
  }

  // If a player is carrying a flag, move it along
  if (p->state == PLAYER_OWN_FLAG || p->state == PLAYER_OPPONENT_FLAG)
  {
    // So,
    // if player is team 0, and is carrying own flag (state 1), we need to change object 0
    // if player is team 0, and is carrying opp flag (state 2), we need to change object 1
    // if player is team 1, and is carrying own flag (state 1), we need to change object 1
    // if player is team 1, and is carrying opp flag (state 2), we need to change object 0
	// this reduces to the following:
	int flagIndex = (p->team + p->state) == 2;
    GameState.objects[flagIndex].x = p->x;
    GameState.objects[flagIndex].y = p->y;
  }
}

/**
 * Set starting position of a newly created player
 */
void game_set_player_start_position(Player* p)
{
  int i,success = 0;
  srand(time(NULL));

  while (success == 0)
  {
    // we'll be in trouble if no home cell is available...
    i = rand() % MAX_HOME_CELLS;

    // Find an unoccupied home cell
    if (Board.home_cells[p->team][i] != NULL && Board.home_cells[p->team][i]->occupant == NULL)
    {
      // update position pointers consistently, 3rd parameter is NULL because
      // this is a new player and wasn't in any cell before now.
      game_set_player_position(p, Board.home_cells[p->team][i], 0);
      success = 1;
    }
  }

}

void game_set_player_jail_position(Player* p)
{
  int i, success = 0;
  int enemy_team = (p->team+1)%2;
  srand(time(NULL));

  while (success == 0)
  {
    // we'll be in trouble if no jail cell is available...
    i = rand() % MAX_JAIL_CELLS;

    // Fixed this - should transport them to the enemy jail, not theirs!
    if (Board.jail_cells[enemy_team][i] != NULL && 
	Board.jail_cells[enemy_team][i]->occupant == NULL)
    {
      game_set_player_position(p, Board.jail_cells[enemy_team][i], 1);
      success = 1;
    }
  }
}

extern Player* game_create_player(int team)
{
  // TODO: do we need to lock here?
  int i=0, playerTeam;
  Player *p;

  pthread_mutex_lock(&(GameState.masterLock));

  if (team < 2)
	  playerTeam = team;
  else
    // if team 1 has less players, set playerTeam to 1. 0 otherwise
    playerTeam = GameState.numPlayers[1] < GameState.numPlayers[0];

  // New code (Uses same if statement)
  i = 0;
  while (GameState.players[playerTeam][i] != NULL && i < MAX_NUM_PLAYERS)
    i++;

  //printf("Alright, assigning a new player pointer!\n");
  if (GameState.players[playerTeam][i] == NULL)
    {
	  p = (Player*) malloc(sizeof(Player));

	  // slot i is available for the new player.
      GameState.players[playerTeam][i] = p;
      GameState.numPlayers[playerTeam]++;

      // assign id. 2 players can have the same id, as long as they
      // are on different teams
      p->id = i;
      p->team = playerTeam;

      //put player on the first unoccupied cell in its home territory
      game_set_player_start_position(p);

      pthread_mutex_unlock(&(GameState.masterLock));
      return p;
    }
  else {
    printf("ERROR, GAME FULL!\n");
    pthread_mutex_unlock(&(GameState.masterLock));
    return NULL;
  }
}

Cell* findCellForMove(int x, int y, Player_Move direction)
{
  int newX, newY;
  newX = x;
  newY = y;

  switch (direction)
  {
	case MOVE_LEFT:
	  newX--;
	  break;
	case MOVE_RIGHT:
	  newX++;
	  break;
	case MOVE_UP:
	  newY--;
	  break;
	case MOVE_DOWN:
	  newY++;
	  break;
  }

  return Board.cells[newY][newX];
}


/**
 * A player tries to move into a wall
 */
int game_move_into_wall(Player* p, Cell* newCell)
{
  printf("Moving into a wall\n");

  // return 0 for all of these?
  if (newCell->y == 0)
  {
    return 0;
  }
  else if (newCell->y == Board.size - 1)
  {
    return 0;
  }
  else if (newCell->x == 0)
  {
    return 0;
  }
  else if (newCell->x == Board.size - 1)
  {
    return 0;
  }

  if (newCell->destructable && p->shovel != 0)
  {
	// erase the wall and turn it into floor space
    newCell->type = ' ';
    GameState.changedCell = newCell;

	// p->shovel is 1 or 2. shovels are at objects[2] and objects[3]
    game_set_shovel_start_position(&(GameState.objects[p->shovel+1]));
    p->shovel = 0;
    game_set_player_position(p, newCell, 1);
    return 1;
  }
  return 0;
}

void game_free_jailed_players(int team)
{
  int i;
  for (i = 0; i < MAX_NUM_PLAYERS; i++)
  {
    if (GameState.players[team][i] != NULL && GameState.players[team][i]->state == PLAYER_JAILED)
    {
      GameState.players[team][i]->state = PLAYER_NORMAL;
    }
  }
}

void game_jail_player(Player* p)
{
  // just forget about holding the objects.
  // they will stay at this location.
  p->shovel = 0;
  p->state = PLAYER_JAILED;
  game_set_player_jail_position(p);
}

/**
 * Handle collision of 2 players. Assumptions:
 * - GameState is locked
 * - Cell newCell is occupied by a player
 */
int game_collide_players(Player* p, Cell* newCell)
{
  Player* otherPlayer = newCell->occupant;

  // Do nothing if colliding with player on the same team
  if (p->team == otherPlayer->team)
    return 0;

  // If the other guy is in enemy territory, tag him
  if (otherPlayer->team != newCell->team)
  {
    game_jail_player(otherPlayer);
    game_set_player_position(p, newCell, 1);
    return 1;
  }
  // If you are on enemy territory, you are screwed
  Cell* oldCell = Board.cells[p->y][p->x];
  if (p->team != oldCell->team)
  {
    game_jail_player(p);
    return 1;
  }

  // This would happen if enemy players run into each other at the edge
  // between the two sides. Both players are on home territory, so no one
  // moves.
  return 0;
}

extern int game_move_player(Player* p, Player_Move direction)
{
  int didMove = 0;

  pthread_mutex_lock(&(GameState.masterLock));

  // calculate new position from current position and direction
  Cell* newCell = findCellForMove(p->x, p->y, direction);

//  player_dump(p);
  // printf(" ..wants to move to cell : %d, %d = %c\n", newCell->x, newCell->y, newCell->type);

  // if new cell is a wall, check if we can destroy it.
  if (newCell->type == '#')
  {
    didMove = game_move_into_wall(p, newCell);
  }
  // if new cell contains another player, handle the collision.
  else if (newCell->occupant != NULL)
  {
    didMove = game_collide_players(p, newCell);
  }
  // if a free player enters the enemy team's jail, free everybody!
  else if (p->state != PLAYER_JAILED && ((p->team == 0 && newCell->type == 'J')
	                                  || (p->team == 1 && newCell->type == 'j')))
  {
    game_set_player_position(p, newCell, 1);
    game_free_jailed_players(p->team);
    didMove = 1;
  }
  // if a player is jailed and tries to move outside the enemy team's jail
  else if (p->state == PLAYER_JAILED && !((p->team == 0 && newCell->type == 'J')
                                       || (p->team == 1 && newCell->type == 'j')))
  {
	  // do nothing, can't move outside your own jail
	  didMove = 0;
  }
  else
  {

    // All other cases, just move
    game_set_player_position(p, newCell, 1);
    didMove = 1;
  }

  pthread_mutex_unlock(&(GameState.masterLock));
  return didMove;
}

extern int player_pickup_flag(Player* p) {

  if (GameState.objects[p->team].x == p->x &&
      GameState.objects[p->team].y == p->y) {
    p->state = PLAYER_OWN_FLAG;
  }

  else if (GameState.objects[((p->team)+1)%2].x == p->x &&
	   GameState.objects[((p->team)+1)%2].y == p->y) {
    p->state = PLAYER_OPPONENT_FLAG;
  }

  else {
    printf("No flag to pick up!\n");
    return 0;
  }

  return 1;
}

extern int player_pickup_shovel(Player *p) {

  if (GameState.objects[2].x == p->x &&
      GameState.objects[2].y == p->y) {
    p->shovel = 1;
  }

  else if (GameState.objects[3].x == p->x &&
	   GameState.objects[3].y == p->y) {
    p->shovel = 2;
  }

  else {
    printf("No shovel to pick up!\n");
    return 0;
  }

  return 1;
}

extern int player_drop_flag(Player* p) {
  p->state = PLAYER_NORMAL;
  return 1;
}

extern int player_drop_shovel(Player *p) {
  p->shovel = 0;
  return 1;
}
