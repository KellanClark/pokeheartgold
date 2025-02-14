#ifndef POKEHEARTGOLD_MAIL_H
#define POKEHEARTGOLD_MAIL_H

#include "mail_message.h"
#include "heap.h"
#include "save.h"
#include "pokemon_types_def.h"
#include "pm_string.h"

#define MAILBOX_MSG_COUNT       (20)

typedef struct Mailbox {
    MAIL msgs[MAILBOX_MSG_COUNT];
} MAILBOX;

MAIL *Mail_New(HeapID heap_id);
void Mail_Copy(const MAIL *src, MAIL *dest);
void Mail_Init(MAIL *mail);
BOOL Mail_TypeIsValid(MAIL *mail);
BOOL Mail_Compare(const MAIL *a, const MAIL *b);
void Mail_SetNewMessageDetails(MAIL *mail, u8 mailType, u8 mon_no, SaveData *saveData);
u32 Mail_GetOTID(const MAIL *mail);
u16 *Mail_GetAuthorNamePtr(MAIL *mail);
u8 Mail_GetAuthorGender(const MAIL *mail);
u8 Mail_GetType(const MAIL *mail);
void Mail_SetType(MAIL *mail, u8 mailType);
u8 Mail_GetLanguage(const MAIL *mail);
u8 Mail_GetVersion(const MAIL *mail);
u16 sub_0202B404(MAIL *mail, u8 r1, u8 r4, u16 r3);
u16 sub_0202B4E4(const MAIL *mail);
MAIL_MESSAGE *Mail_GetUnk20Array(MAIL *mail, int i);
void Mail_SetMessage(MAIL *mail, const MAIL_MESSAGE *src, int i);
MAILBOX *Save_Mailbox_Get(SaveData *saveData);
u32 Save_Mailbox_sizeof(void);
void Save_Mailbox_Init(MAILBOX *mailbox);
MAIL *Mailbox_AllocAndFetchMailI(MAIL *msgs, int n, int i, HeapID heapId);
int Mailbox_GetFirstEmptySlotIdx(MAILBOX *mailbox);
void Mailbox_DeleteSlotI(MAIL *msgs, int n, int i);
void Mailbox_CopyMailToSlotI(MAIL *msgs, int n, int i, const MAIL *src);
u32 Mailbox_CountMessages(MAILBOX *mailbox, int unused);
void Mailbox_FetchMailToBuffer(MAIL *msgs, int n, int i, MAIL *dest);
int MailArray_GetFirstEmptySlotIdx(MAIL *msgs, int n);
u32 MailArray_CountMessages(MAIL *msgs, int n);
MAIL *CreateKenyaMail(Pokemon *mon, u8 mailType, u8 gender, STRING *name, u8 otId);

#endif //POKEHEARTGOLD_MAIL_H
