#ifndef POKEHEARTGOLD_ENCOUNTER_H
#define POKEHEARTGOLD_ENCOUNTER_H

#include "battle_setup.h"
#include "field_player_avatar.h"
#include "task.h"

typedef struct EncounterWork {
    u32 *winFlag;
    int effect;
    int bgm;
    int unkC;
    BATTLE_SETUP *setup;
} ENCOUNTER;

typedef struct WildEncounterWork {
    int state;
    int effect;
    int bgm;
    int *winFlag;
    BATTLE_SETUP *setup;
} WILD_ENCOUNTER;

void sub_02050B08(FieldSystem *fsys, BATTLE_SETUP *setup);
void sub_02051428(TaskManager *taskManager, void *a1, int a2);
void SetupAndStartWildBattle(TaskManager *taskManager, u16 species, u8 level, u32 *winFlag, BOOL canRun, BOOL shiny);
void sub_02051090(TaskManager *taskManager, u16 species, u8 level, u32 *winFlag, BOOL canRun);
void sub_02051228(TaskManager *taskManager, u16 species, u8 level);
void SetupAndStartTutorialBattle(TaskManager *taskManager);
void SetupAndStartTrainerBattle(TaskManager *taskManager, u32 opponentTrainer1, u32 opponentTrainer2, u32 followerTrainerNum, u32 a4, u32 a5, HeapID heapId, u32 *winFlag);
void sub_02050B90(FieldSystem *fsys, TaskManager *taskManager, BATTLE_SETUP *setup);
void sub_0205239C(BATTLE_SETUP *setup, FieldSystem *fsys);
void sub_02050AAC(TaskManager *man, BATTLE_SETUP *setup, int effect, int bgm, u32 *winFlag);
void sub_020511F8(FieldSystem *fsys, BATTLE_SETUP *setup);
void sub_020515FC(FieldSystem *fsys, PARTY *party, int battleFlags);
void sub_02051598(FieldSystem *fsys, void *a1, int battleFlags);
void sub_020514A4(TaskManager *man, int target, int maxLevel, int flag);

#endif //POKEHEARTGOLD_ENCOUNTER_H
