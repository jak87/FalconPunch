#include "net.h"
#include "protocol.h"
#include "protocol_session.h"
#include "maze.h"
#include "player.h"
#include "game_control.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>




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
	Board.cells[row][col]->type = ' ';
	Board.total_floor++;
	break;
	  case '#': //wall cell
	Board.cells[row][col]->type = '#';
	Board.total_wall++;
	break;
	  case 'h': //team1 home cell
	Board.cells[row][col]->type = 'h';
	Board.home_cells[0][Board.total_h] = Board.cells[row][col];
	Board.total_floor++;
	Board.total_h++;
	break;
	  case 'H': //team2 home cell
	Board.cells[row][col]->type = 'H';
	Board.home_cells[1][Board.total_H] = Board.cells[row][col];
	Board.total_floor++;
	Board.total_H++;
	break;
	  case 'j': //team1 jail cell
	Board.cells[row][col]->type = 'j';
	Board.jail_cells[0][Board.total_j] = Board.cells[row][col];
	Board.total_floor++;
	Board.total_j++;
	break;
	  case 'J': //team2 jail cell
	Board.cells[row][col]->type = 'J';
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
	  Board.cells[row][col]->team = (col<mid) ? 0 : 1;
	}
	row++;
  }
  fclose(map);

  // TODO: handle some error cases and return -1 if something went wrong
  return rc;
}

extern void game_init()
{
  bzero(&GameState, sizeof(GameState));

  pthread_mutex_init(&GameState.masterLock, 0);

  game_load_board();

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
}

/**
 * Set starting position of a newly created player
 */
void game_set_player_start_position(Player* p)
{
  int i,success = 0;
  srand(time(NULL));

  pthread_mutex_lock(&GameState.masterLock);

  while (success == 0)
  {
    // we'll be in trouble if no home cell is available...
    // assuming total_h = total_H
    i = rand() % Board.total_h;

    // Find an unoccupied home cell
    if (Board.home_cells[p->team][i]->occupant == NULL)
    {
      // update position pointers consistently, 3rd parameter is NULL because
      // this is a new player and wasn't in any cell before now.
      game_set_player_position(p, Board.home_cells[p->team][i], 0);
      success = 1;
      printf("Player position = Board.home_cells[%d][%d]\n\n",p->team,i);
    }
  }

  pthread_mutex_unlock(&GameState.masterLock);

}

void game_set_player_jail_position(Player* p)
{
  int i, success = 0;
  srand(time(NULL));

  while (success == 0)
  {
    // we'll be in trouble if no jail cell is available...
    // assuming total_j = total_J
    i = rand() % Board.total_j;

    if (Board.jail_cells[p->team][i]->occupant == NULL)
    {
      game_set_player_position(p, Board.jail_cells[p->team][i], 1);
      success = 1;
    }
  }
}

extern Player* game_create_player(int team)
{
  int i=0, playerTeam;

  Player *p = malloc(sizeof(Player));

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
      // slot i is available for the new player.
      GameState.players[playerTeam][i] = p;
      GameState.numPlayers[playerTeam]++;
      // assign id. 2 players can have the same id, as long as they
      // are on different teams
      p->id = i;
      p->team = playerTeam;
    }
  else {
    printf("ERROR, GAME FULL!\n");
    return p;
  }

  // Checking to make sure everything worked
  //printf("\nPlayer created:\nTeam = %d\nPlayer ID = %d",p->team,p->id);
  //printf("\nnumPlayers = %d\n", GameState.numPlayers[playerTeam]);

  //put player on the first unoccupied cell in its home territory
  game_set_player_start_position(p);

  return p;
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
  if (newCell->destructable && p->shovel != 0)
  {
    // TODO: change new cell type
    // TODO: return shovel to its place
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
      // TODO: handle other state (freed player cannot be tagged and cannot pick up stuff until it goes back)
      GameState.players[team][i]->state = PLAYER_FREE;
    }
  }
}

void game_jail_player(Player* p)
{
  game_set_player_jail_position(p);
  p->state = PLAYER_JAILED;
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

  pthread_mutex_lock(&GameState.masterLock);

  // calculate new position from current position and direction
  Cell* newCell = findCellForMove(p->x, p->y, direction);

//  player_dump(p);
  printf(" ..wants to move to cell : %d, %d = %c\n", newCell->x, newCell->y, newCell->type);

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
  // if a free player enters its team's jail
  else if (p->state != PLAYER_JAILED && ((p->team == 0 && newCell->type == 'j')
	                                  || (p->team == 1 && newCell->type == 'J')))
  {
    game_set_player_position(p, newCell, 1);
    game_free_jailed_players(p->team);
    didMove = 1;
  }
  // if a player is jailed and tries to move outside its team's jail
  else if (p->state == PLAYER_JAILED && !((p->team == 0 && newCell->type == 'j')
                                       || (p->team == 1 && newCell->type == 'J')))
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

  pthread_mutex_unlock(&GameState.masterLock);
  return didMove;
}
