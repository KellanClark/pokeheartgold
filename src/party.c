#include "pokemon.h"
#include "heap.h"
#include "save.h"
#include "party.h"

#define PARTY_ASSERT_SLOT(party, slot) ({             \
    GF_ASSERT(slot >= 0);                             \
    GF_ASSERT(slot < (party)->core.curCount);        \
    GF_ASSERT(slot < (party)->core.maxCount);        \
})

u32 SaveArray_Party_sizeof(void) {
    return sizeof(PARTY);
}

u32 PartyCore_sizeof(void) {
    return sizeof(PARTY_CORE);
}

PARTY * SaveArray_Party_Alloc(HeapID heapId) {
    PARTY *ret = AllocFromHeap(heapId, sizeof(PARTY));
    SaveArray_Party_Init(ret);
    return ret;
}

void SaveArray_Party_Init(PARTY *party) {
    InitPartyWithMaxSize(party, PARTY_SIZE);
}

void InitPartyWithMaxSize(PARTY *party, int maxSize) {
    int i;

    GF_ASSERT(maxSize <= PARTY_SIZE);
    memset(party, 0, sizeof(PARTY));
    party->core.curCount = 0;
    party->core.maxCount = maxSize;
    for (i = 0; i < PARTY_SIZE; i++) {
        ZeroMonData(&party->core.mons[i]);
    }
    MI_CpuClear8(&party->extra, 5 * party->core.maxCount);
}

BOOL AddMonToParty(PARTY *party, const Pokemon *mon) {
    if (party->core.curCount >= party->core.maxCount) {
        return FALSE;
    }
    party->core.mons[party->core.curCount] = *mon;
    MI_CpuClear8(&party->extra.unk_00[party->core.curCount], sizeof(PARTY_EXTRA_SUB));
    party->core.curCount++;
    return TRUE;
}

BOOL RemoveMonFromParty(PARTY *party, int slot) {
    PARTY_ASSERT_SLOT(party, slot);
    GF_ASSERT(party->core.curCount > 0);
    for (; slot < party->core.curCount - 1; slot++) {
        party->core.mons[slot] = party->core.mons[slot + 1];
        party->extra.unk_00[slot] = party->extra.unk_00[slot + 1];
    }
    ZeroMonData(&party->core.mons[slot]);
    MI_CpuClear8(&party->extra.unk_00[slot], sizeof(PARTY_EXTRA_SUB));
    party->core.curCount--;
    return TRUE;
}

int GetPartyMaxCount(const PARTY *party) {
    return party->core.maxCount;
}

int GetPartyCount(const PARTY *party) {
    return party->core.curCount;
}

Pokemon *GetPartyMonByIndex(PARTY *party, int slot) {
    PARTY_ASSERT_SLOT(party, slot);
    return &party->core.mons[slot];
}

void Party_GetUnkSubSlot(const PARTY *party, PARTY_EXTRA_SUB *dest, int slot) {
    PARTY_ASSERT_SLOT(party, slot);
    *dest = party->extra.unk_00[slot];
}

void Party_SetUnkSubSlot(PARTY *party, const PARTY_EXTRA_SUB *src, int slot) {
    PARTY_ASSERT_SLOT(party, slot);
    party->extra.unk_00[slot] = *src;
}

void Party_ResetUnkSubSlot(PARTY *party, int slot) {
    PARTY_ASSERT_SLOT(party, slot);
    MI_CpuClear8(&party->extra.unk_00[slot], sizeof(PARTY_EXTRA_SUB));
}

void Party_SafeCopyMonToSlot_ResetUnkSub(PARTY *party, int slot, Pokemon *src) {
    PARTY_ASSERT_SLOT(party, slot);
    {
        BOOL valid = GetMonData(&party->core.mons[slot], MON_DATA_SPECIES_EXISTS, NULL) - GetMonData(src, MON_DATA_SPECIES_EXISTS, NULL);
        party->core.mons[slot] = *src;
        MI_CpuClear8(&party->extra.unk_00[slot], sizeof(PARTY_EXTRA_SUB));
        party->core.curCount += valid;
    }
}

BOOL Party_SwapSlots(PARTY *party, int slotA, int slotB) {
    PARTY_EXTRA_SUB tmp_PARTY_EXTRA_SUB;
    Pokemon *tmp_POKEMON;

    PARTY_ASSERT_SLOT(party, slotA);
    PARTY_ASSERT_SLOT(party, slotB);
    tmp_POKEMON = AllocFromHeap(HEAP_ID_0, sizeof(Pokemon));
    *tmp_POKEMON = party->core.mons[slotA];
    party->core.mons[slotA] = party->core.mons[slotB];
    party->core.mons[slotB] = *tmp_POKEMON;
    FreeToHeap(tmp_POKEMON);

    tmp_PARTY_EXTRA_SUB = party->extra.unk_00[slotA];
    party->extra.unk_00[slotA] = party->extra.unk_00[slotB];
    party->extra.unk_00[slotB] = tmp_PARTY_EXTRA_SUB;
    return FALSE;
}

void Party_Copy(const PARTY *src, PARTY *dest) {
    *dest = *src;
}

BOOL PartyHasMon(PARTY *party, u16 species) {
    int i;

    for (i = 0; i < party->core.curCount; i++) {
        if (species == GetMonData(&party->core.mons[i], MON_DATA_SPECIES2, NULL)) {
            break;
        }
    }

    return (i != party->core.curCount);
}

PARTY *SaveArray_PlayerParty_Get(SaveData *saveData) {
    return (PARTY *) SaveArray_Get(saveData, SAVE_PARTY);
}
