#include "net.h"
#include "protocol.h"
#include "protocol_session.h"
#include "maze.h"
#include "player.h"
#include "game_control.h"

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
	Board.h_cells[Board.total_h] = Board.cells[i][j];
	Board.total_floor++;
	Board.total_h++;
	break;
	  case 'H': //team2 home cell
	Board.cells[i][j]->type = 'H';
	Board.h_cells[Board.total_H] = Board.cells[i][j];
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
}

/**
 * Set player start position and update corresponding board
 */
void game_set_player_position(Player* p)
{

}

/**
 * Assign player to a team. Set starting position.
 */
void game_player_init(Player* p, int team)
{

}

extern Player* game_create_player(int team)
{
  int i;
  Player* p = malloc(sizeof(Player));

  for (i = 0; i < MAX_NUM_PLAYERS; i++)
  {
    if (GameState.players[0][i] == 0 && (team == 0 || team == 1))
    {
      // team 1 has slot i available
    }
  }

  //put player on the first unoccupied cell 'h'
  return p;
}

extern int game_move_player(Player * p, int direction)
{
  return 0;
}
