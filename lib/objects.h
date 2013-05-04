#ifndef __DAGAME_OBJECTS_H__
#define __DAGAME_OBJECTS_H__

#include "net.h"
#include "protocol.h"
#include "protocol_session.h"

typedef enum {
  FLAG,
  SHOVEL
} Object_Type;


typedef struct {
  Object_Type type;
  int x;
  int y;
  int team;
  int visible;

} Object;

int object_marshall(Proto_Session *s, Object * object);
int object_unmarshall(Proto_Session *s, int offset, Object * object);
void object_dump(Object * object);

#endif
