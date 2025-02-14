#include "scrcmd.h"
#include "bag.h"
#include "field_map_object.h"
#include "get_egg.h"
#include "party.h"
#include "player_data.h"
#include "pokemon.h"

extern void ov01_021F9048(LocalMapObject* map_object);
extern void FollowPokeMapObjectSetParams(LocalMapObject *mapObject, u16 species, u8 forme, BOOL shiny);
extern u32 FollowingPokemon_GetSpriteID(int species, u16 forme, u32 gender);

static LocalMapObject* CreateDayCareMonSpriteInternal(MapObjectMan* object_man, u8 dc_mon_idx, u16 species, u8 forme, u32 gender, u32 direction, u32 x, u32 y, u32 map_no, BOOL shiny);

BOOL ScrCmd_BufferDayCareMonNicks(ScriptContext* ctx) {
    SaveData* savedata = ctx->fsys->savedata;
    MessageFormat** msg_fmt = FieldSysGetAttrAddr(ctx->fsys, SCRIPTENV_MESSAGE_FORMAT);
    DAYCARE* daycare = Save_DayCare_Get(savedata);

    Save_DayCare_BufferStoredMonNicks(daycare, *msg_fmt);

    return FALSE;
}

BOOL ScrCmd_GetDayCareState(ScriptContext* ctx) {
    SaveData* savedata = ctx->fsys->savedata;
    u16* ret_ptr = ScriptGetVarPointer(ctx);
    DAYCARE* daycare = SaveArray_Get(savedata, SAVE_DAYCARE);

    *ret_ptr = Save_DayCare_GetState(daycare);

    return FALSE;
}

BOOL ScrCmd_ResetDayCareEgg(ScriptContext* ctx) {
    DAYCARE* daycare = SaveArray_Get(ctx->fsys->savedata, SAVE_DAYCARE);

    Save_DayCare_ResetEggStats(daycare);

    return FALSE;
}

BOOL ScrCmd_GiveDayCareEgg(ScriptContext* ctx) {
    FieldSystem* fsys = ctx->fsys;
    DAYCARE* daycare = SaveArray_Get(fsys->savedata, SAVE_DAYCARE);
    PARTY* party = SaveArray_PlayerParty_Get(fsys->savedata);
    SaveData* savedata = FieldSys_GetSaveDataPtr(ctx->fsys);
    PLAYERPROFILE* profile = Save_PlayerData_GetProfileAddr(savedata);

    GiveEggToPlayer(daycare, party, profile);

    return 0;
}

BOOL ScrCmd_RetrieveDayCareMon(ScriptContext* ctx) {
    FieldSystem* fsys = ctx->fsys;
    MessageFormat** msg_fmt = FieldSysGetAttrAddr(fsys, SCRIPTENV_MESSAGE_FORMAT);
    SaveData* savedata = fsys->savedata;
    u16* ret_ptr = ScriptGetVarPointer(ctx);
    u16 daycare_mon_idx = ScriptGetVar(ctx);
    DAYCARE* daycare = SaveArray_Get(savedata, SAVE_DAYCARE);
    PARTY* party = SaveArray_PlayerParty_Get(fsys->savedata);

    *ret_ptr = Save_DayCare_RetrieveMon(party, *msg_fmt, daycare, (u8)daycare_mon_idx);

    return FALSE;
}

BOOL ScrCmd_BufferDayCareWithdrawCost(ScriptContext* ctx) {
    FieldSystem* fsys = ctx->fsys;
    MessageFormat** msg_fmt = FieldSysGetAttrAddr(fsys, SCRIPTENV_MESSAGE_FORMAT);
    SaveData* savedata = fsys->savedata;
    u16* ret_ptr = ScriptGetVarPointer(ctx);
    u16 daycare_mon_idx = ScriptGetVar(ctx);
    DAYCARE* daycare = SaveArray_Get(savedata, SAVE_DAYCARE);

    *ret_ptr = Save_DayCare_BufferMonNickAndRetrievalPrice(daycare, (u8)daycare_mon_idx, *msg_fmt);

    return FALSE;
}

BOOL ScrCmd_BufferDayCareMonGrowth(ScriptContext* ctx) {
    SaveData* savedata = ctx->fsys->savedata;
    MessageFormat** msg_fmt = FieldSysGetAttrAddr(ctx->fsys, SCRIPTENV_MESSAGE_FORMAT);
    u16* ret_ptr = ScriptGetVarPointer(ctx);
    u16 daycare_mon_idx = ScriptGetVar(ctx);
    DAYCARE* daycare = SaveArray_Get(savedata, SAVE_DAYCARE);

    *ret_ptr = Save_DayCare_BufferGrowthAndNick(daycare, daycare_mon_idx, *msg_fmt);

    return FALSE;
}

BOOL ScrCmd_GetTailDayCareMonSpeciesAndNick(ScriptContext* ctx) {
    FieldSystem* fsys = ctx->fsys;
    MessageFormat** msg_fmt = FieldSysGetAttrAddr(ctx->fsys, SCRIPTENV_MESSAGE_FORMAT);
    u16* ret_ptr = ScriptGetVarPointer(ctx);
    DAYCARE* daycare = Save_DayCare_Get(fsys->savedata);

    *ret_ptr = Save_DayCare_BufferTailMonNick(daycare, *msg_fmt);

    return FALSE;
}

BOOL ScrCmd_PutMonInDayCare(ScriptContext* ctx) {
    FieldSystem* fsys = ctx->fsys;
    SaveData* savedata = fsys->savedata;
    u16 slot = ScriptGetVar(ctx);
    PARTY* party = SaveArray_PlayerParty_Get(fsys->savedata);
    DAYCARE* daycare = SaveArray_Get(savedata, SAVE_DAYCARE);

    Save_DayCare_PutMonIn(party, (u8)slot, daycare, savedata);

    return FALSE;
}

BOOL ScrCmd_BufferDayCareMonStats(ScriptContext* ctx) {
    SaveData* savedata = ctx->fsys->savedata;
    MessageFormat** msg_fmt = FieldSysGetAttrAddr(ctx->fsys, SCRIPTENV_MESSAGE_FORMAT);
    u16 nickname_idx = ScriptGetVar(ctx);
    u16 level_idx = ScriptGetVar(ctx);
    u16 gender_idx = ScriptGetVar(ctx);
    u16 slot = ScriptGetVar(ctx);
    DAYCARE* daycare = Save_DayCare_Get(savedata);

    Save_DayCare_BufferMonStats(daycare, (u8)nickname_idx, (u8)level_idx, (u8)gender_idx, (u8)slot, *msg_fmt);

    return FALSE;
}

BOOL ScrCmd_GetDayCareCompatibility(ScriptContext* ctx) {
    SaveData* savedata = ctx->fsys->savedata;
    u16* ret_ptr = ScriptGetVarPointer(ctx);
    DAYCARE* daycare = SaveArray_Get(savedata, SAVE_DAYCARE);

    *ret_ptr = Save_DayCare_CalcCompatibility(daycare);

    return FALSE;
}

BOOL ScrCmd_CheckDayCareEgg(ScriptContext* ctx) {
    SaveData* savedata = ctx->fsys->savedata;
    u16* ret_ptr = ScriptGetVarPointer(ctx);
    DAYCARE* daycare = SaveArray_Get(savedata, SAVE_DAYCARE);

    *ret_ptr = Save_DayCare_HasEgg(daycare);

    return FALSE;
}

BOOL ScrCmd_UpdateDayCareMonObjects(ScriptContext* ctx) {
    DAYCARE* daycare;
    u8 forme;
    u16 species;

    FieldSystem* fsys = ctx->fsys;
    daycare = Save_DayCare_Get(fsys->savedata);

    for (s32 dc_mon_idx = 0, y = 5, x = 8; dc_mon_idx < 2; dc_mon_idx++, y += 4, x += 2) {
        LocalMapObject* mon_map_object = GetMapObjectByID(fsys->mapObjectMan, obj_daycare_poke_1 + dc_mon_idx);
        if (mon_map_object) {
            DeleteMapObject(mon_map_object);
        }

        BoxPokemon *boxMon = DayCareMon_GetBoxMon(Save_DayCare_GetMonX(daycare, dc_mon_idx));
        if (GetBoxMonData(boxMon, MON_DATA_SPECIES, NULL) == SPECIES_NONE) {
            continue;
        }

        forme = GetBoxMonData(boxMon, MON_DATA_FORME, NULL);
        species = GetBoxMonData(boxMon, MON_DATA_SPECIES, NULL);
        u32 gender = GetBoxMonData(boxMon, MON_DATA_GENDER, NULL);
        BOOL shiny = BoxMonIsShiny(boxMon);

        CreateDayCareMonSpriteInternal(fsys->mapObjectMan, (u8)dc_mon_idx, species, forme, gender, 1, x, y, fsys->location->mapId, shiny);
    }

    return FALSE;
}

static LocalMapObject* CreateDayCareMonSpriteInternal(MapObjectMan* object_man, u8 dc_mon_idx, u16 species, u8 forme, u32 gender, u32 direction, u32 x, u32 y, u32 map_no, BOOL shiny) {
    u32 sprite_id = FollowingPokemon_GetSpriteID(species, forme, gender);
    LocalMapObject* lmo = CreateSpecialFieldObject(object_man, x, y, direction, sprite_id, 11, map_no);
    GF_ASSERT(lmo != NULL);

    MapObject_SetID(lmo, obj_daycare_poke_1 + dc_mon_idx);
    MapObject_SetType(lmo, 0);
    MapObject_SetFlagID(lmo, 0);
    MapObject_SetScript(lmo, 0);
    MapObject_SetParam(lmo, 0, 2);
    FollowPokeMapObjectSetParams(lmo, species, (u32)forme, shiny);
    MapObject_SetXRange(lmo, -1);
    MapObject_SetYRange(lmo, -1);
    MapObject_SetFlagsBits(lmo, MAPOBJECTFLAG_UNK2);
    MapObject_ClearFlagsBits(lmo, 0);
    MapObject_SetFlag29(lmo, TRUE);
    ov01_021F9048(lmo);

    return lmo;
}

BOOL ScrCmd_DayCareSanitizeMon(ScriptContext* ctx) {
    Pokemon *mon;

    FieldSystem* fsys = ctx->fsys;
    u16 party_slot = ScriptGetVar(ctx);
    u16* ret_ptr = ScriptGetVarPointer(ctx);
    PARTY* party = SaveArray_PlayerParty_Get(fsys->savedata);
    mon = GetPartyMonByIndex(party, party_slot);

    *ret_ptr = 0;

    if (party_slot == 0xFF) {
        return FALSE;
    }

    u32 held_item = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);
    if (held_item == ITEM_GRISEOUS_ORB) {
        Bag* bag = SaveGetBag(fsys->savedata);
        if (!BagAddItem(bag, ITEM_GRISEOUS_ORB, 1, HEAP_ID_FIELD)) {
            *ret_ptr = 0xFF;
            return FALSE;
        }

        u32 no_item = ITEM_NONE;
        SetMonData(mon, MON_DATA_HELD_ITEM, &no_item);
    }

    s32 forme = GetMonData(mon, MON_DATA_FORME, NULL);
    if (forme > 0) {
        u32 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
        switch (species) {
        case SPECIES_GIRATINA:
            Mon_UpdateGiratinaForme(mon);
            break;
        case SPECIES_ROTOM:
            Mon_UpdateRotomForme(mon, 0, 0);
            break;
        case SPECIES_SHAYMIN:
            Mon_UpdateShayminForme(mon, 0);
            break;
        }
    }

    return FALSE;
}
