#ifndef POKEHEARTGOLD_BAG_VIEW_H
#define POKEHEARTGOLD_BAG_VIEW_H

#include "item.h"
#include "save.h"
#include "bag_cursor.h"
#include "player_data.h"

// Enum for argument "code" to GetItemUseErrorMessage
enum ItemUseError {
    ITEMUSEERROR_OKAY       = 0, // no error
    ITEMUSEERROR_NODISMOUNT = 1, // can't get off bike
    ITEMUSEERROR_NOFOLLOWER = 2, // have a companion
    ITEMUSEERROR_NOTNOW     = 3, // you're a member of team rocket

    ITEMUSEERROR_OAKSWORDS  = -1u,
};

/*
 * Item slot access for bag view
 */
typedef struct BAG_VIEW_POCKET {
    ItemSlot *slots;                  // Points into Bag
    u8 padding[4];
    u8 pocketId;                       // POCKET_XXX constant
    u8 padding2[3];
} BAG_VIEW_POCKET;

/*
 * Data relevant to drawing the bag on screen
 */
typedef struct BAG_VIEW {
    SaveData *saveData;                // Persistent game state
    BAG_VIEW_POCKET pockets[8];        // Pocket information
    u8 unk_64;
    u8 unk_65;
    u16 unk_66;
    u16 unk_68;
    u8 filler_6A[2];
    BAG_CURSOR *cursor;                // State of last selection
    void *unk_70;
    u8 unk_74;
    u8 unk_75;
    u16 unk_76_0:1;
    u16 unk_76_1:15;
    u32 unk_78;
} BAG_VIEW; // size: 0x7C

/*
 * BAG_VIEW *BagView_New(u8 heap_id)
 *
 * Create a new BAG_VIEW
 *
 * @param heap_id: Heap to allocate from
 *
 * @returns: Pointer to new BAG_VIEW
 */
BAG_VIEW *BagView_New(u8 heap_id);

/*
 * u32 BagView_sizeof(void)
 *
 * Gets the size of BAG_VIEW
 *
 * @return: Size of BAG_VIEW
 */
u32 BagView_sizeof(void);
void sub_02077894(BAG_VIEW *a0, u8 a1);
void sub_0207789C(BAG_VIEW *a0, SaveData *a1, u8 a2, BAG_CURSOR *a3, u32 a4);

/*
 * void BagView_SetItem(BAG_VIEW *bagView, ItemSlot *slots, u8 pocketId, u8 position)
 *
 * Sets pocket view #pocketId to slots.
 * Called by BagViewCreate in bag.c
 * BUG: Meant to set pocket view #position.
 * See note in bag_view.c
 *
 * @param bagView:     The BAG_VIEW instance
 * @param slots:       ItemSlot array of pocket
 * @param pocketId:    Which pocket it is
 * @param position:    Unused
 */
void BagView_SetItem(BAG_VIEW *bagView, ItemSlot *slots, u8 pocketId, u8 position);
void sub_020778C8(BAG_VIEW *bagView);
void sub_020778DC(BAG_VIEW *bagView, void *a1);
void sub_020778E0(BAG_VIEW *bagView, u8 a1);
void sub_020778E8(BAG_VIEW *bagView, u16 a1);
u16 sub_02077904(BAG_VIEW *bagView);
u16 sub_0207790C(BAG_VIEW *bagView);
u8 sub_02077914(BAG_VIEW *bagView);
u8 sub_0207791C(BAG_VIEW *bagView);

/*
 * BOOL TryFormatRegisteredKeyItemUseMessage(SaveData *saveData, STRING *dest, u16 itemId, u32 heap_id)
 *
 * Some key items, when used, only print a non_npc_msg.
 * If the selected item is one of these, formats the
 * non_npc_msg into dest and returns TRUE. Otherwise,
 * returns FALSE and leaves dest untouched.
 *
 * @param saveData:    Persistent game state
 * @param dest:        Output STRING
 * @param itemId:      ID of item to check
 * @param heap_id:     Heap in which to allocate temp buffers
 *
 * @returns: TRUE if non_npc_msg formatted, else FALSE
 */
BOOL TryFormatRegisteredKeyItemUseMessage(SaveData *saveData, STRING *dest, u16 itemId, HeapID heap_id);

/*
 * void GetItemUseErrorMessage(PLAYERDATA *playerData, STRING *dest, u32 unused, enum ItemUseError code, u32 heap_id)
 *
 * If you can't use the item for whatever reason, this
 * routine is called to format the non_npc_msg.
 * In the default case, it's just Oak telling you off.
 * There are special messages for Cycling Road and when
 * you have an NPC companion.
 *
 * @param playerProfile:  Pointer to PLAYERPROFILE, used to get your
 *                        name in Oak's admonishment
 * @param dest:           Pointer to output STRING
 * @param itemId:         Unused
 * @param code:           Which non_npc_msg to output
 * @param heap_id:        Heap to allocate temp buffers from
 */
void GetItemUseErrorMessage(PLAYERPROFILE *playerProfile, STRING *dest, u16 itemId, enum ItemUseError code, HeapID heap_id);

#endif //POKEHEARTGOLD_BAG_VIEW_H
