#include "protocol_session.h"
#include "player.h"

extern int player_marshall(Proto_Session *s, Player *player)
{
  int rc = proto_session_body_marshall_int(s, player->id);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->team);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->state);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->x);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, player->y);
  return rc;
}
