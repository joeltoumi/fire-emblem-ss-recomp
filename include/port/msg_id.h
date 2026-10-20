#ifndef PORT_MSG_ID_H
#define PORT_MSG_ID_H

// Message ids in the game code are the US ones. The European version
// numbers its messages differently, so its build translates them at compile
// time; the message ids in ROM data are already right.
#if defined(PLATFORM_PC) && defined(REGION_EU)
#include "port/msg_ids_eu.h"
#define MSG_ID(id) MSG_EU_##id
#else
#define MSG_ID(id) (id)
#endif

#endif // PORT_MSG_ID_H
