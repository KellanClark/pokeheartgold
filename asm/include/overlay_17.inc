#include <nitro/fs/overlay.h>
#pragma once
.public FontID_Alloc
.public FontID_Release
.public FontID_String_GetWidth
.public LoadFontPal1
.public sub_02004EC4
.public PlaySE
.public StopSE
.public PlayFanfare
.public IsFanfarePlaying
.public OverlayManager_CreateAndGetData
.public OverlayManager_GetData
.public OverlayManager_FreeData
.public OverlayManager_GetArgs
.public NARC_New
.public NARC_Delete
.public GfGfxLoader_LoadCharDataFromOpenNarc
.public GfGfxLoader_LoadScrnDataFromOpenNarc
.public GfGfxLoader_GXLoadPalFromOpenNarc
.public sub_0200B2E0
.public sub_0200B2E8
.public NewMsgDataFromNarc
.public DestroyMsgData
.public NewString_ReadMsgData
.public MessageFormat_New_Custom
.public MessageFormat_Delete
.public BufferString
.public BufferIntegerAsString
.public BufferItemName
.public StringExpandPlaceholders
.public sub_0200CF18
.public sub_0200CF38
.public sub_0200CF70
.public sub_0200CFF4
.public sub_0200D020
.public sub_0200D034
.public sub_0200D0E4
.public sub_0200D108
.public sub_0200D2A4
.public sub_0200D2B4
.public sub_0200D3F8
.public sub_0200D504
.public sub_0200D5D4
.public sub_0200D6EC
.public sub_0200D71C
.public sub_0200D734
.public sub_0200D998
.public sub_0200D9DC
.public sub_0200DD88
.public sub_0200DE00
.public sub_0200DEA0
.public sub_0200DED0
.public sub_0200E2B0
.public sub_0200E2B4
.public sub_0200E2B8
.public sub_0200E2EC
.public CreateSysTask
.public DestroySysTask
.public LoadUserFrameGfx2
.public DrawFrameAndWindow2
.public BeginNormalPaletteFade
.public IsPaletteFadeFinished
.public sub_0200FBDC
.public sub_0200FBF4
.public GF_RTC_CopyDateTime
.public ListMenuItems_New
.public ListMenuItems_Delete
.public ListMenuItems_AppendFromMsgData
.public sub_0201660C
.public sub_02016624
.public sub_020183F0
.public sub_02018410
.public sub_02018424
.public sub_02018474
.public sub_020185FC
.public sub_02018674
.public sub_02018680
.public sub_020186A4
.public Main_SetVBlankIntrCB
.public HBlankInterruptDisable
.public CreateHeap
.public DestroyHeap
.public AllocFromHeap
.public FreeToHeap
.public BgConfig_Alloc
.public SetBothScreensModesAndDisable
.public InitBgFromTemplate
.public FreeBgTilemapBuffer
.public BG_ClearCharDataRange
.public FillBgTilemapRect
.public BgClearTilemapBufferAndCommit
.public AddWindow
.public RemoveWindow
.public ScheduleWindowCopyToVram
.public ClearWindowTilemapAndCopyToVram
.public FillWindowPixelBuffer
.public BgConfig_HandleScheduledScrollAndTransferOps
.public AddTextPrinterParameterized
.public AddTextPrinterParameterized2
.public GF_CreateVramTransferManager
.public GF_DestroyVramTransferManager
.public sub_020210BC
.public sub_02021148
.public sub_02021238
.public GX_SetBanks
.public GX_DisableEngineALayers
.public GX_EngineAToggleLayers
.public GX_DisableEngineBLayers
.public Sprite_Delete
.public Set2dSpriteVisibleFlag
.public Set2dSpriteAnimActiveFlag
.public Set2dSpriteAnimSeqNo
.public sub_02024964
.public sub_020249D4
.public sub_02025320
.public System_GetTouchNew
.public String_New
.public String_Delete
.public String_Copy
.public Save_PlayerData_GetOptionsAddr
.public Save_BerryPots_Get
.public Save_BerryPotRTC_Get
.public Options_GetTextFrameDelay
.public Options_GetFrame
.public Save_GameStats_Get
.public GameStats_Inc
.public GameStats_AddSpecial
.public GetItemIndexMapping
.public GetItemIconCell
.public GetItemIconAnim
.public ItemIdIsBerry
.public GetNutName
.public BagHasSpaceForItem
.public BagAddItem
.public BagTakeItem
.public SaveGetBag
.public NNS_GfdDoVramTransfer
.public MI_CpuFill8
.public MI_CpuCopy8
.public RTC_ConvertDateTimeToSecond
.public _ll_sdiv
.public _s32_div_f
.public gSystem
.public ov16_022014A0
.public ov16_022015B0
.public ov16_022015BC
.public ov16_022015C4
.public ov16_022015E0
.public ov16_022015EC
.public ov16_022015F8
.public ov16_02201604
.public ov16_02201610
.public ov16_02201644
.public ov16_02201650
.public ov16_0220165C
.public ov16_02201668
.public ov16_02201674
.public ov16_02201760
.public ov16_022017FC
.public ov16_02201820
.public ov16_022018B4
.public ov16_022018C4
.public ov16_022018D4
.public ov16_022018E4
.public ov16_022018F0
.public ov16_0220191C
.public ov17_02201BC0
.public ov17_02201C10
.public ov17_02201C78
.public ov17_02201D04
