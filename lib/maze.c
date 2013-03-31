#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "net.h"
#include "protocol.h"
#include "protocol_session.h"
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

extern int
maze_marshall_cell(Proto_Session *s, Cell *cell)
{
  int rc = 1;
  rc = proto_session_body_marshall_int(s, cell->x);
  fprintf(stderr, "\n\nMarshalling cell->x: %d", cell->x);
  if (rc != 1) return rc;
  rc = proto_session_body_marshall_int(s, cell->y);
  fprintf(stderr, "\n\nMarshalling cell->y: %d", cell->y);
  if (rc != 1) return rc;
  rc = proto_session_body_marshall_char(s, cell->type);
  fprintf(stderr, "\n\nMarshalling cell->type: %c", cell->type);
  if (rc != 1) return rc;
  rc = proto_session_body_marshall_int(s, cell->team);
  fprintf(stderr, "\n\nMarshalling cell->team: %d\n\n", cell->team);
  return rc;
}

extern int
maze_unmarshall_cell(Proto_Session *s, int offset, Cell *cell)
{
  offset = proto_session_body_unmarshall_int(s, offset, &(cell->x));
  fprintf(stderr, "\n\nUnmarshalling to cell->x: %d", cell->x);
  if (offset < 0) return offset;
  offset = proto_session_body_unmarshall_int(s, offset, &(cell->y));
  fprintf(stderr, "\n\nUnmarshalling to cell->y: %d", cell->y);
  if (offset < 0) return offset;
  offset = proto_session_body_unmarshall_char(s, offset, &(cell->type));
  fprintf(stderr, "\n\nUnmarshalling to cell->type: %c", cell->type);
  if (offset < 0) return offset;
  offset = proto_session_body_unmarshall_int(s, offset, &(cell->team));
  fprintf(stderr, "\n\nUnmarshalling to cell->team: %d\n\n", cell->team);
  return offset;
}

extern int
maze_marshall_board(Proto_Session *s)
{
  fprintf(stderr, "\n\nAbout to marshall board\n\n");
  return maze_marshall_cell(s, Board.cells[0][0]);
}

extern int
maze_unmarshall_board(Proto_Session *s, int offset)
{
  return maze_unmarshall_cell(s, offset, &(Board.cells[0][0]));
}
