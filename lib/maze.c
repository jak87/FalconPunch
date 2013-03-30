#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "maze.h"

////////////////////////////////////
//
//@param mapPath: location in filesystem of map
//@return: -1 on file open error, -2 on invalid char in map, 1 normally
////////////////////////////////////

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
