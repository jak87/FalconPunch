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
  int i = 0;
  int j = 0;
  while(fgets(buf, MAX_BOARD_SIZE, map) != NULL) {
	int mid = strlen(buf)/2;
	Board.size = mid*2;
	for (j=0; buf[j]; j++) {
	  Board.cells[i][j] = (Cell *) malloc(sizeof(Cell));
	  switch(buf[j]) {
	  case ' ': //floor cell
	Board.cells[i][j]->type = ' ';
	Board.total_floor++;
	break;
	  case '#': //wall cell
	Board.cells[i][j]->type = '#';
	Board.total_wall++;
	break;
	  case 'h': //team1 home cell
	Board.cells[i][j]->type = 'h';
	Board.home_cells[0][Board.total_h] = Board.cells[i][j];
	Board.total_floor++;
	Board.total_h++;
	break;
	  case 'H': //team2 home cell
	Board.cells[i][j]->type = 'H';
	Board.home_cells[1][Board.total_H] = Board.cells[i][j];
	Board.total_floor++;
	Board.total_H++;
	break;
	  case 'j': //team1 jail cell
	Board.cells[i][j]->type = 'j';
	Board.total_floor++;
	Board.total_j++;
	break;
	  case 'J': //team2 jail cell
	Board.cells[i][j]->type = 'J';
	Board.total_floor++;
	Board.total_J++;
	break;
	  case '\n':
	continue;
	  default: //unknown
	fclose(map);
	return -2;
	  }
	  Board.cells[i][j]->x = i;
	  Board.cells[i][j]->y = j;
	  Board.cells[i][j]->team = (j<mid) ? 1 : 2;
	}
	i++;
  }
  fclose(map);

  // TODO: handle some error cases and return -1 if something went wrong
  return rc;
}

extern void game_init()
{
  bzero(&GameState, sizeof(GameState));

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
void game_set_player_position(Player* p, Cell* c)
{
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

  while (success == 0)
  {
    // we'll be in trouble if no home cell is available...
    // assuming total_h = total_H
    i = rand() % Board.total_h;

    // TODO: lock stuff
    // Should be if == -1 or something, since 0 can be a player ID???
    // Actually, shouldn't this be if == NULL since the occupant 
    // is a player struct, not an int?
    //if (Board.home_cells[p->team][i]->occupant == 0)
    if (Board.home_cells[p->team][i]->occupant == NULL)
    {
      game_set_player_position(p, Board.home_cells[p->team][i]);
      success = 1;
      printf("Player position = Board.home_cells[%d][%d]\n\n",p->team,i);
    }
  }

}

extern Player* game_create_player(int team)
{
  int i=0, playerTeam;
  Player* p = malloc(sizeof(Player));

  if (team < 2)
	  playerTeam = team;
  else
	  // if team 1 has less players, set playerTeam to 1. 0 otherwise
	  playerTeam = GameState.numPlayers[1] < GameState.numPlayers[0];

  /*
  for (i = 0; i < MAX_NUM_PLAYERS; i++)
  {
    if (GameState.players[playerTeam][i] == 0)
    {
      // slot i is available for the new player.
      GameState.players[playerTeam][i] = p;
      GameState.numPlayers[playerTeam]++;
      // assign id. 2 players can have the same id, as long as they
      // are on different teams
      p->id = i;
      p->team = playerTeam;
    }
  }*/

  i = 0;
  while (GameState.players[playerTeam][i] == 1 && i < MAX_NUM_PLAYERS)
    i++;

  if (GameState.players[playerTeam][i] == 0)
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

  printf("\nPlayer created:\nTeam = %d\nPlayer ID = %d",p->team,p->id);
  printf("\nnumPlayers = %d\n", GameState.numPlayers[playerTeam]);

  //put player on the first unoccupied cell in its home territory
  game_set_player_start_position(p);

  return p;
}

extern int game_move_player(Player * p, int direction)
{
  return 0;
}
