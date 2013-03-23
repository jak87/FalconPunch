#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "maze.h"

#define MAP_PATH "../lib/daGame.map"

////////////////////////////////////
//
//@param mapPath: location in filesystem of map
//@return: -1 on file open error, -2 on invalid char in map, 1 normally
////////////////////////////////////
int loadBoard(){
  int rc = 1;
  FILE* map = fopen(MAP_PATH, "r");
  if (map==NULL){rc = -1; return rc;}
  char buf[MAX_BOARD_SIZE];
  int i = 0;
  int j = 0;
  while(fgets(buf, MAX_BOARD_SIZE, map) != NULL){
    int mid = strlen(buf)/2;
    Board.size = mid*2;
    for (j = 0; buf[j]; j++){
      Board.cells[i][j] = (Cell *) malloc(sizeof(Cell));
      switch(buf[j]){
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
        Board.total_h++;
	// Since it's also a floor cell
	Board.total_floor++;
        break;
      case 'H': //team 2 home cell
        Board.cells[i][j]->type = 'H';
        Board.total_H++;
	Board.total_floor++;
        break;
      case 'j': //team 1 jail cell
        Board.cells[i][j]->type = 'j';
        Board.total_j++;
	Board.total_floor++;
        break;
      case 'J': //team 2 jail cell
        Board.cells[i][j]->type = 'J';
        Board.total_J++;
	Board.total_floor++;
        break;
      case '\n':
	continue;
      default: //unknown character
	rc = -2;
	fclose(map);
	return rc;
      }
      Board.cells[i][j]->x = i;
      Board.cells[i][j]->y = j;
      Board.cells[i][j]->team = (j<mid) ? 1 : 2;
    }
    i++;
  }
  fclose(map);
  return rc;
}


/////////////////////////////////////
//
//
//
////////////////////////////////////
void dump(){
  int i=0;
  int j=0;
  char buf[MAX_BOARD_SIZE];
  char c = '\0';
  for (i=0;i<MAX_BOARD_SIZE && Board.cells[i][0];i++){
    memcpy(buf, &c, sizeof(buf));
    for (j=0; j<MAX_BOARD_SIZE && Board.cells[i][j];j++){
      buf[j] = Board.cells[i][j]->type;
    }
    printf("%s\n", buf);
  }
}
