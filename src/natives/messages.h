// src/natives/messages.h
#pragma once
#include "amx/amx.h"

namespace natives {
namespace messages {

cell AMX_NATIVE_CALL FindChannelByName(AMX* amx, cell* params);
cell AMX_NATIVE_CALL FindChannelById  (AMX* amx, cell* params);
cell AMX_NATIVE_CALL SendChannelMessage(AMX* amx, cell* params);
cell AMX_NATIVE_CALL DeleteMessage    (AMX* amx, cell* params);
cell AMX_NATIVE_CALL EditMessage      (AMX* amx, cell* params);
cell AMX_NATIVE_CALL GetMessageContent(AMX* amx, cell* params);

extern const AMX_NATIVE_INFO kNatives[];

} // namespace messages
} // namespace natives