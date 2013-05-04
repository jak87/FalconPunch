#include "protocol_session.h"
#include "objects.h"

extern int object_marshall(Proto_Session *s, Object * object)
{
  int rc = proto_session_body_marshall_int(s, object->type);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, object->x);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, object->y);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, object->team);
  if (rc != 1) return rc;

  rc = proto_session_body_marshall_int(s, object->visible);
  return rc;
}

extern int object_unmarshall(Proto_Session *s, int offset, Object * object)
{
  int type,x,y,team,visible;

  offset = proto_session_body_unmarshall_int(s, offset, &type);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &x);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &y);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &team);
  if (offset < 0) return offset;

  offset = proto_session_body_unmarshall_int(s, offset, &visible);
  if (offset < 0) return offset;

  object->type = (Object_Type) type;
  object->x = x;
  object->y = y;
  object->team = team;
  object->visible = visible;

//  printf("Unmarshalled ");
//  object_dump(object);

  return offset;
}

extern void object_dump(Object * object)
{
  if (object->type == FLAG)
  {
    printf("Flag, team %d: (%d, %d)\n", object->team, object->x, object->y);
  }
  else
  {
    printf("Shovel, team %d: (%d, %d)\n", object->team, object->x, object->y);
  }
}
