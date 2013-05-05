#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "net.h"
#include "protocol.h"
#include "protocol_utils.h"
#include "protocol_server.h"
#include "maze.h"
#include "game_control.h"

// Updates
extern int do_send_players_state();
extern int do_gameover(int winner);

// Connection Handlers
extern int hello_handler(Proto_Session *s);
extern int goodbye_handler(Proto_Session *s);

// Player Creation Handlers
extern int new_player_handler(Proto_Session *s);
extern void remove_player(int fd_id);

// Player Actions
extern int game_move_handler(Proto_Session *s);
extern int pickup_flag_handler(Proto_Session *s);
extern int pickup_shovel_handler(Proto_Session *s);
extern int drop_flag_handler(Proto_Session *s);
extern int drop_shovel_handler(Proto_Session *s);

// Outdated Code
extern int game_maze_info_handler(Proto_Session *s);
extern int game_cell_info_handler(Proto_Session *s);
extern int game_dump_handler(Proto_Session *s);
