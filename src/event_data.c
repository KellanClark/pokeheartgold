#include "event_data.h"

static u8 sTempFlags[NUM_TEMP_FLAGS / 8] = {0};

u32 SaveArray_Flags_sizeof(void) {
    return sizeof(ScriptState);
}

void SaveArray_Flags_Init(ScriptState *scriptState) {
    memset(scriptState, 0, sizeof(ScriptState));
}

ScriptState *SaveArray_Flags_Get(SaveData *saveData) {
    return SaveArray_Get(saveData, SAVE_FLAGS);
}

BOOL CheckFlagInArray(ScriptState *scriptState, u16 flagno) {
    u8 *flagAddr = GetFlagAddr(scriptState, flagno);
    if (flagAddr != NULL) {
        if (*flagAddr & (1 << (flagno % 8))) {
            return TRUE;
        }
    }
    return FALSE;
}

void SetFlagInArray(ScriptState *scriptState, u16 flagno) {
    u8 *flagAddr = GetFlagAddr(scriptState, flagno);
    if (flagAddr == NULL) {
        return;
    }
    *flagAddr |= 1 << (flagno % 8);
}

void ClearFlagInArray(ScriptState *scriptState, u16 flagno) {
    u8 *flagAddr = GetFlagAddr(scriptState, flagno);
    if (flagAddr == NULL) {
        return;
    }
    *flagAddr &= 0xFF ^ (1 << (flagno % 8));
}

u8 *GetFlagAddr(ScriptState *scriptState, u16 flagno) {
    if (flagno == 0) {
        return NULL;
    } else if (flagno < TEMP_FLAG_BASE) {
        GF_ASSERT((flagno / 8) < (NUM_FLAGS / 8));
        return &scriptState->flags[flagno / 8];
    } else {
        GF_ASSERT(((flagno - TEMP_FLAG_BASE) / 8) < (NUM_TEMP_FLAGS / 8));
        return &sTempFlags[(flagno - TEMP_FLAG_BASE) / 8];
    }
}

u16 *GetVarAddr(ScriptState *scriptState, u16 varno) {
    GF_ASSERT((varno - VAR_BASE) < NUM_VARS);
    return &scriptState->vars[varno - VAR_BASE];
}
