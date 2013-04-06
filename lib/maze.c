#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "net.h"
#include "protocol.h"
#include "protocol_session.h"
#include "maze.h"

void dump(){
  printf("Board size: %i\n", Board.size);
  int x, y;

  for (y = 0; y < Board.size; y++)
  {
    for (x = 0; x < Board.size; x++)
    {
      printf("%c", Board.cells[y][x]->type);
    }
    printf("\n");
  }
}

extern int
maze_marshall_cell(Proto_Session *s, Cell *cell)
{
  int rc = proto_session_body_marshall_int(s, cell->x);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, cell->y);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_char(s, cell->type);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, cell->team);
  return rc;
}

extern int
maze_unmarshall_cell(Proto_Session *s, int offset, Cell *cell)
{
  int x, y, team;
  char type;

  offset = proto_session_body_unmarshall_int(s, offset, &x);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &y);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_char(s, offset, &type);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &team);
  if (offset < 0) return offset;

  // After all the primitives are unmarshalled correctly,
  // allocate a new cell, and put it's pointer in the board
  Board.cells[x][y] = (Cell *) malloc(sizeof(Cell));

  // Populate the values
  Board.cells[x][y]->x = x;
  Board.cells[x][y]->y = y;
  Board.cells[x][y]->type = type;
  Board.cells[x][y]->team = team;

  return offset;
}

extern int
maze_marshall_board(Proto_Session *s)
{
  int rc = proto_session_body_marshall_int(s, Board.size);
  if (rc != 1) return rc;

  int x,y;
  for (y = 0; y < Board.size; y++)
  {
    for (x = 0; x < Board.size; x++)
    {
      rc = maze_marshall_cell(s, Board.cells[y][x]);
      if (rc != 1) return rc;
    }
  }

  return rc;
}

extern int
maze_unmarshall_board(Proto_Session *s, int offset)
{
  offset = proto_session_body_unmarshall_int(s, offset, &(Board.size));
  if (offset < 0) return offset;

  int x,y;
  for (y = 0; y < Board.size; y++)
  {
    for (x = 0; x < Board.size; x++)
    {
      offset = maze_unmarshall_cell(s, offset, Board.cells[y][x]);
      if (offset < 0) return offset;
    }
  }
  return offset;
}
