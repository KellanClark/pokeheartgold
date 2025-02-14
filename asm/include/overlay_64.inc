#include <nitro/fs/overlay.h>
#pragma once
.public FontID_Alloc
.public FontID_Release
.public FontID_String_GetWidth
.public LoadFontPal0
.public PlaySE
.public OverlayManager_CreateAndGetData
.public OverlayManager_GetData
.public OverlayManager_FreeData
.public OverlayManager_GetArgs
.public ReadWholeNarcMemberByIdPair
.public AllocAndReadWholeNarcMemberByIdPair
.public NARC_New
.public NARC_Delete
.public GfGfxLoader_GXLoadPal
.public GfGfxLoader_LoadCharDataFromOpenNarc
.public GfGfxLoader_LoadScrnDataFromOpenNarc
.public GfGfxLoader_GXLoadPalFromOpenNarc
.public G2dRenderer_SetSubSurfaceCoords
.public NewMsgDataFromNarc
.public DestroyMsgData
.public ReadMsgDataIntoString
.public MessageFormat_New
.public MessageFormat_Delete
.public BufferIntegerAsString
.public BufferMonthNameAbbr
.public StringExpandPlaceholders
.public sub_0200CF18
.public sub_0200CF38
.public sub_0200CF6C
.public sub_0200CF70
.public sub_0200CFF4
.public sub_0200D020
.public sub_0200D034
.public sub_0200D108
.public sub_0200D3F8
.public sub_0200D504
.public sub_0200D5D4
.public sub_0200D6EC
.public sub_0200D71C
.public sub_0200D734
.public sub_0200D740
.public sub_0200D958
.public sub_0200D998
.public sub_0200D9DC
.public sub_0200DC18
.public sub_0200DC4C
.public sub_0200DCC0
.public sub_0200DCE8
.public sub_0200DD10
.public sub_0200DDB8
.public sub_0200DDF4
.public BeginNormalPaletteFade
.public IsPaletteFadeFinished
.public sub_02014510
.public sub_020145B4
.public sub_02019BA4
.public sub_02019BDC
.public sub_02019D18
.public sub_02019F74
.public sub_0201A018
.public Main_SetVBlankIntrCB
.public HBlankInterruptDisable
.public CreateHeap
.public DestroyHeap
.public AllocFromHeap
.public AllocFromHeapAtEnd
.public FreeToHeap
.public BgConfig_Alloc
.public SetBothScreensModesAndDisable
.public InitBgFromTemplate
.public FreeBgTilemapBuffer
.public BG_ClearCharDataRange
.public BgClearTilemapBufferAndCommit
.public BgFillTilemapBufferAndSchedule
.public AddWindow
.public RemoveWindow
.public ScheduleWindowCopyToVram
.public CopyWindowPixelsToVram_TextMode
.public FillWindowPixelBuffer
.public BgConfig_HandleScheduledScrollAndTransferOps
.public AddTextPrinterParameterized2
.public sub_020210BC
.public sub_02021148
.public sub_02021238
.public GX_SetBanks
.public GX_EngineAToggleLayers
.public GX_EngineASetLayers
.public GX_EngineBToggleLayers
.public GX_EngineBSetLayers
.public sub_02024B1C
.public sub_02024B34
.public sub_02025224
.public String_New
.public String_Delete
.public Save_HOF_GetNumRecords
.public Save_HOF_TranslateRecordIdx
.public Save_HOF_RecordCountMons
.public Save_HOF_GetMonStatsByIndexPair
.public Save_HOF_GetClearDate
.public SpeciesToOverworldModelIndexOffset
.public OverworldModelLookupFormeCount
.public OverworldModelLookupHasFemaleForme
.public GetGenderBySpeciesAndPersonality
.public CalcShininessByOtIdAndPersonality
.public GetMonSpriteCharAndPlttNarcIdsEx
.public sub_020708D8
.public sub_020729D8
.public NNS_G2dGetImageLocation
.public NNS_G2dGetImagePaletteLocation
.public NNS_G3dGetTex
.public GX_LoadOBJPltt
.public GX_LoadOBJ
.public GXS_LoadOBJ
.public DC_FlushRange
.public MI_CpuFill8
.public gSystem
.public ov64_021E5900
.public ov64_021E59C8
.public ov64_021E5A18
