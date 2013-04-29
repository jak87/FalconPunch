#include "protocol_session.h"
#include "player.h"

extern int player_marshall(Proto_Session *s, Player * player)
{
  int rc = proto_session_body_marshall_int(s, player->id);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->team);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->state);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->shovel);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->flag);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->fd);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->x);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->y);
  return rc;
}

extern int player_unmarshall(Proto_Session *s, int offset, Player * player)
{
  int id,team,state,x,y,fd,shovel,flag;

  offset = proto_session_body_unmarshall_int(s, offset, &id);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &team);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &state);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &shovel);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &flag);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &fd);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &x);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &y);
  if (offset < 0) return offset;

  //printf("New Player:\nID = %d\nTeam = %d\nState = %d\n(x,y) = (%d,%d)\n",
  //	 id,team,state,x,y);

  player->id = id;
  player->team = team;
  player->state = state;
  player->shovel = shovel;
  player->flag = flag;
  player->fd = fd;
  player->x = x;
  player->y = y;

  return offset;
}
