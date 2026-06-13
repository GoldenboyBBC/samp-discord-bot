#pragma once
#include "amx/amx.h"

namespace natives {
namespace bot {

cell AMX_NATIVE_CALL SetBotPresenceStatus(AMX* amx, cell* params);
cell AMX_NATIVE_CALL SetBotActivity(AMX* amx, cell* params);
cell AMX_NATIVE_CALL GetBotPresenceStatus(AMX* amx, cell* params);

extern const AMX_NATIVE_INFO kNatives[];

} // namespace bot
} // namespace natives