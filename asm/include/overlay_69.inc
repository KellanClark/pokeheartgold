#include <nitro/fs/overlay.h>
#pragma once
.public ListMenuInit
.public ListMenu_ProcessInput
.public DestroyListMenu
.public Std_CreateYesNoMenu
.public Handle2dMenuInput_DeleteOnFinish
.public TextFlags_SetCanABSpeedUpPrint
.public sub_02002B50
.public sub_02002B8C
.public FontID_Alloc
.public FontID_Release
.public LoadFontPal0
.public PlaySE
.public OverlayManager_CreateAndGetData
.public OverlayManager_GetData
.public OverlayManager_FreeData
.public OverlayManager_GetArgs
.public NARC_New
.public NARC_Delete
.public NARC_AllocAndReadWholeMember
.public GfGfxLoader_LoadCharDataFromOpenNarc
.public GfGfxLoader_LoadScrnDataFromOpenNarc
.public GfGfxLoader_GXLoadPalFromOpenNarc
.public GfGfxLoader_LoadFromOpenNarc_GetSizeOut
.public NewMsgDataFromNarc
.public DestroyMsgData
.public ReadMsgDataIntoString
.public MessageFormat_New_Custom
.public MessageFormat_Delete
.public BufferCountryName
.public BufferCityName
.public StringExpandPlaceholders
.public LoadUserFrameGfx1
.public DrawFrameAndWindow1
.public sub_0200E5D4
.public LoadUserFrameGfx2
.public DrawFrameAndWindow2
.public BeginNormalPaletteFade
.public IsPaletteFadeFinished
.public ListMenuItems_New
.public ListMenuItems_Delete
.public ListMenuItems_AppendFromMsgData
.public Main_SetVBlankIntrCB
.public Main_SetHBlankIntrCB
.public SetKeyRepeatTimers
.public CreateHeap
.public DestroyHeap
.public FreeToHeap
.public BgConfig_Alloc
.public SetBothScreensModesAndDisable
.public InitBgFromTemplate
.public FreeBgTilemapBuffer
.public BG_ClearCharDataRange
.public BG_LoadPlttData
.public BG_SetMaskColor
.public BgClearTilemapBufferAndCommit
.public AddWindow
.public RemoveWindow
.public CopyWindowToVram
.public FillWindowPixelRect
.public sub_0201F51C
.public Draw3dModel
.public sub_0201F590
.public sub_0201F63C
.public GF_DegreeToSinCosIdxNoWrap
.public ResetAllTextPrinters
.public TextPrinterCheckActive
.public AddTextPrinterParameterized
.public GX_SetBanks
.public GX_DisableEngineALayers
.public GX_EngineAToggleLayers
.public GX_DisableEngineBLayers
.public GX_EngineBToggleLayers
.public GX_BothDispOn
.public GX_SwapDisplay
.public Camera_Create
.public sub_02023120
.public Camera_RegisterToStaticPtr
.public sub_02023154
.public Camera_SetClipBounds
.public sub_02023308
.public sub_020233D8
.public Camera_SetDistance
.public Camera_GetDistance
.public System_GetTouchNew
.public String_New
.public String_Delete
.public Thunk_G3X_Reset
.public sub_02026E50
.public Save_PlayerData_GetOptionsAddr
.public Options_GetTextFrameDelay
.public Options_GetFrame
.public sub_0202CA44
.public sub_0202CA58
.public sub_0202CA8C
.public sub_0202CA90
.public sub_0202CA94
.public sub_0202CB5C
.public LocationGmmDatCountGet
.public LocationGmmDatIndexGetByCountryMsgNo
.public LocationGmmDatGetGmmNo
.public LocationGmmDatGetCountryMsgNo
.public LocationGmmDatGetEarthPlaceDatId
.public LocationGmmDatGetDistrictNameMsgIdsPtr
.public LocationGmmDatGetDistrictCount
.public NNS_G3dGlbLightVector
.public MTX_RotX33_
.public MTX_RotY33_
.public MTX_RotZ33_
.public MTX_Concat33
.public FX_Sqrt
.public memset
.public _s32_div_f
.public _u32_div_f
.public gGameLanguage
.public FX_SinCosTable_
.public gSystem
.public ov69_021E5900
.public ov69_021E5A34
.public ov69_021E6080
