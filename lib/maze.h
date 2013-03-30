#ifndef __DAGAME_MAZE_H__
#define __DAGAME_MAZE_H__

#include "net.h"
#include "protocol.h"
#include "protocol_session.h"

#define MAX_BOARD_SIZE 202
#define MAP_PATH "../lib/daGame.map"

typedef struct {
  int x;
  int y;
  char type;
  short team;
} Cell;

struct {
  Cell* cells[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
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
