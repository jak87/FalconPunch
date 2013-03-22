#define MAX_BOARD_SIZE 202

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
  int total_jail;
  int total_home;
} Board;

int loadBoard();

void dump();
