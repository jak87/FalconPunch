#ifndef __DAGAME_MAZE_H__
#define __DAGAME_MAZE_H__

#include "protocol_session.h"

#define MAX_BOARD_SIZE 202
#define MAP_PATH "daGame_small.map"

typedef struct {
  int x;
  int y;
  char type;
  int team;
} Cell;

struct {
  Cell* cells[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
  Cell* h_cells[MAX_BOARD_SIZE * MAX_BOARD_SIZE/2];
  Cell* H_cells[MAX_BOARD_SIZE * MAX_BOARD_SIZE/2];
  int total_wall;
  int total_floor;
  int total_j;
  int total_J;
  int total_h;
  int total_H;
  int size; // along 1 dimension
} Board;

int loadBoard();

void dump();

int maze_marshall_cell(Proto_Session *s, Cell *cell);
int maze_unmarshall_cell(Proto_Session *s, int offset, Cell *cell);

int maze_marshall_board(Proto_Session *s);
int maze_unmarshall_board(Proto_Session *s, int offset);

#endif
