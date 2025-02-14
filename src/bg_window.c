#include "window.h"
#include "global.h"
#include "math_util.h"

static u8 TranslateGFBgModePairToGXScreenSize(enum GFBgScreenSize size, enum GFBgType type);
static void GetBgScreenDimensions(u32 screenSize, u8 *width_p, u8 *height_p);
static void Bg_SetPosText(BG *bg, enum BgPosAdjustOp op, fx32 value);
static void BgAffineReset(BGCONFIG *bgConfig, u8 layer);
static void CopyOrUncompressTilemapData(const void *src, void *dest, u32 size);
static void CopyTilesToVram(u8 layer, const void *data, u32 offset, u32 size);
static void BG_LoadCharPixelData(BGCONFIG *bgConfig, u8 layer, const void *buffer, u32 size, u32 offset);
static void LoadBgVramChar(u8 layer, const void *data, u32 offset, u32 size);
static u16 GetTileMapIndexFromCoords(u8 x, u8 y, u8 size, u8 mode);
static u16 GetSrcTileMapIndexFromCoords(u8 dstX, u8 dstY, u8 width, u8 height);
static void CopyToBgTilemapRectText(BG *bg, u8 destX, u8 destY, u8 destWidth, u8 destHeight, const u16 *buf, u8 srcX, u8 srcY, u8 srcWidth, u8 srcHeight, u8 mode);
static void CopyBgTilemapRectAffine(BG *bg, u8 destX, u8 destY, u8 destWidth, u8 destHeight, const u8 *buf, u8 srcX, u8 srcY, u8 srcWidth, u8 srcHeight, u8 mode);
static void FillBgTilemapRectText(BG *bg, u16 value, u8 x, u8 y, u8 width, u8 height, u8 mode);
static void FillBgTilemapRectAffine(BG *bg, u8 value, u8 x, u8 y, u8 width, u8 height);
static void BgClearTilemapBufferAndSchedule(BGCONFIG *bgConfig, u8 layer);
static void Convert4bppTo8bppInternal(u8 *src4bpp, u32 size, u8 *dest8bpp, u8 paletteNum);
static fx32 GetBgVOffset(BGCONFIG *bgConfig, u8 layer);
static u16 GetBgRotation(BGCONFIG *bgConfig, u8 layer);
static void BlitBitmapRect8bit(const BITMAP *src, const BITMAP *dst, u16 srcX, u16 srcY, u16 dstX, u16 dstY, u16 width, u16 height, u16 colorKey);
static void FillBitmapRect4bit(const BITMAP *surface, u16 x, u16 y, u16 width, u16 height, u8 fillValue);
static void FillBitmapRect8bit(const BITMAP *surface, u16 x, u16 y, u16 width, u16 height, u8 fillValue);
static void PutWindowTilemap_TextMode(WINDOW *window);
static void PutWindowTilemap_AffineMode(WINDOW *window);
static void ClearWindowTilemapText(WINDOW *window);
static void ClearWindowTilemapAffine(WINDOW *window);
static void CopyWindowToVram_TextMode(WINDOW *window);
static void ScheduleWindowCopyToVram_TextMode(WINDOW *window);
static void CopyWindowToVram_AffineMode(WINDOW *window);
static void ScheduleWindowCopyToVram_AffineMode(WINDOW *window);
static void ClearWindowTilemapAndCopyToVram_TextMode(WINDOW *window);
static void ClearWindowTilemapAndScheduleTransfer_TextMode(WINDOW *window);
static void ClearWindowTilemapAndCopyToVram_AffineMode(WINDOW *window);
static void ClearWindowTilemapAndScheduleTransfer_AffineMode(WINDOW *window);
static void ScrollWindow_Text(WINDOW *window, u8 direction, u8 y, u8 fillValue);
static void ScrollWindow_Affine(WINDOW *window, u8 direction, u8 y, u8 fillValue);
static void BgConfig_HandleScheduledBufferTransfers(BGCONFIG *bgConfig);
static void BgConfig_HandleScheduledScrolls(BGCONFIG *bgConfig);
static void Bg_SetAffineScale(BG *bg, enum BgPosAdjustOp op, fx32 value);
static void ApplyFlipFlagsToTile(BGCONFIG *bgConfig, u8 flags, u8 *tile);

static const u8 sTilemapWidthByBufferSize[] = {
    16, // GF_BG_SCR_SIZE_128x128
    32, // GF_BG_SCR_SIZE_256x256
    32, // GF_BG_SCR_SIZE_256x512
    32, // GF_BG_SCR_SIZE_512x256
    32, // GF_BG_SCR_SIZE_512x512
    32, // GF_BG_SCR_SIZE_1024x1024
};

static void (*const sScheduleWindowCopyToVramFuncs[GF_BG_TYPE_MAX])(WINDOW *window) = {
    ScheduleWindowCopyToVram_TextMode,
    ScheduleWindowCopyToVram_AffineMode,
    ScheduleWindowCopyToVram_TextMode,
};

static void (*const sClearWindowTilemapAndCopyToVramFuncs[])(WINDOW *) = {
    ClearWindowTilemapAndCopyToVram_TextMode,
    ClearWindowTilemapAndCopyToVram_AffineMode,
    ClearWindowTilemapAndCopyToVram_TextMode,
};

static void (*const sClearWindowTilemapAndScheduleTransferFuncs[])(WINDOW *) = {
    ClearWindowTilemapAndScheduleTransfer_TextMode,
    ClearWindowTilemapAndScheduleTransfer_AffineMode,
    ClearWindowTilemapAndScheduleTransfer_TextMode,
};

static void (*const sPutWindowTilemapFuncs[GF_BG_TYPE_MAX])(WINDOW *window) = {
    PutWindowTilemap_TextMode,
    PutWindowTilemap_AffineMode,
    PutWindowTilemap_TextMode,
};

static void (*const sCopyWindowToVramFuncs[GF_BG_TYPE_MAX])(WINDOW *window) = {
    CopyWindowToVram_TextMode,
    CopyWindowToVram_AffineMode,
    CopyWindowToVram_TextMode,
};

static void (*const sClearWindowTilemapFuncs[GF_BG_TYPE_MAX])(WINDOW *window) = {
    ClearWindowTilemapText,
    ClearWindowTilemapAffine,
    ClearWindowTilemapText,
};

// Make a new BGCONFIG object, which manages the
// eight background layers (four on each screen).
BGCONFIG *BgConfig_Alloc(HeapID heapId) {
    BGCONFIG *ret = AllocFromHeap(heapId, sizeof(BGCONFIG));
    memset(ret, 0, sizeof(BGCONFIG));
    ret->heap_id = heapId;
    ret->scrollScheduled = 0;         // redundant to above memset
    ret->bufferTransferScheduled = 0; // redundant to above memset
    return ret;
}

HeapID BgConfig_GetHeapId(BGCONFIG *bgConfig) {
    return bgConfig->heap_id;
}

void SetBothScreensModesAndDisable(const struct GFBgModeSet *modeSet) {
    GX_SetGraphicsMode(modeSet->dispMode, modeSet->bgModeMain, modeSet->_2d3dSwitch);
    GXS_SetGraphicsMode(modeSet->bgModeSub);
    GX_SetBGScrOffset(GX_BGSCROFFSET_0x00000);
    GX_SetBGCharOffset(GX_BGCHAROFFSET_0x00000);
    GX_DisableEngineALayers();
    GX_DisableEngineBLayers();
}

void SetScreenModeAndDisable(const struct GFBgModeSet *modeSet, enum GFScreen screen) {
    if (screen == SCREEN_MAIN) {
        GX_SetGraphicsMode(modeSet->dispMode, modeSet->bgModeMain, modeSet->_2d3dSwitch);
        GX_DisableEngineALayers();
    } else {
        GXS_SetGraphicsMode(modeSet->bgModeSub);
        GX_DisableEngineBLayers();
    }
}

void InitBgFromTemplateEx(BGCONFIG *bgConfig, u8 bgId, const BGTEMPLATE *template, u8 bgMode, GX_LayerToggle enable) {
    u8 screenSize = TranslateGFBgModePairToGXScreenSize((enum GFBgScreenSize)template->size, (enum GFBgType)bgMode);

    switch (bgId) {
    case GF_BG_LYR_MAIN_0:
        GX_EngineAToggleLayers(GF_BG_LYR_MAIN_0_F, enable);
        G2_SetBG0Control((GXBGScrSizeText)screenSize, (GXBGColorMode)template->colorMode, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase, (GXBGExtPltt)template->bgExtPltt);
        G2_SetBG0Priority(template->priority);
        G2_BG0Mosaic(template->mosaic);
        break;

    case GF_BG_LYR_MAIN_1:
        GX_EngineAToggleLayers(GF_BG_LYR_MAIN_1_F, enable);
        G2_SetBG1Control((GXBGScrSizeText)screenSize, (GXBGColorMode)template->colorMode, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase, (GXBGExtPltt)template->bgExtPltt);
        G2_SetBG1Priority(template->priority);
        G2_BG1Mosaic(template->mosaic);
        break;

    case GF_BG_LYR_MAIN_2:
        GX_EngineAToggleLayers(GF_BG_LYR_MAIN_2_F, enable);
        switch (bgMode) {
        default:
        case GF_BG_TYPE_TEXT:
            G2_SetBG2ControlText((GXBGScrSizeText)screenSize, (GXBGColorMode)template->colorMode, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        case GF_BG_TYPE_AFFINE:
            G2_SetBG2ControlAffine((GXBGScrSizeAffine)screenSize, (GXBGAreaOver)template->areaOver, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        case GF_BG_TYPE_256x16PLTT:
            G2_SetBG2Control256x16Pltt((GXBGScrSize256x16Pltt)screenSize, (GXBGAreaOver)template->areaOver, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        }
        G2_SetBG2Priority(template->priority);
        G2_BG2Mosaic(template->mosaic);
        break;

    case GF_BG_LYR_MAIN_3:
        GX_EngineAToggleLayers(GF_BG_LYR_MAIN_3_F, enable);
        switch (bgMode) {
        default:
        case GF_BG_TYPE_TEXT:
            G2_SetBG3ControlText((GXBGScrSizeText)screenSize, (GXBGColorMode)template->colorMode, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        case GF_BG_TYPE_AFFINE:
            G2_SetBG3ControlAffine((GXBGScrSizeAffine)screenSize, (GXBGAreaOver)template->areaOver, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        case GF_BG_TYPE_256x16PLTT:
            G2_SetBG3Control256x16Pltt((GXBGScrSize256x16Pltt)screenSize, (GXBGAreaOver)template->areaOver, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        }
        G2_SetBG3Priority(template->priority);
        G2_BG3Mosaic(template->mosaic);
        break;

    case GF_BG_LYR_SUB_0:
        GX_EngineBToggleLayers(GF_BG_LYR_SUB_0_F, enable);
        G2S_SetBG0Control((GXBGScrSizeText)screenSize, (GXBGColorMode)template->colorMode, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase, (GXBGExtPltt)template->bgExtPltt);
        G2S_SetBG0Priority(template->priority);
        G2S_BG0Mosaic(template->mosaic);
        break;

    case GF_BG_LYR_SUB_1:
        GX_EngineBToggleLayers(GF_BG_LYR_SUB_1_F, enable);
        G2S_SetBG1Control((GXBGScrSizeText)screenSize, (GXBGColorMode)template->colorMode, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase, (GXBGExtPltt)template->bgExtPltt);
        G2S_SetBG1Priority(template->priority);
        G2S_BG1Mosaic(template->mosaic);
        break;

    case GF_BG_LYR_SUB_2:
        GX_EngineBToggleLayers(GF_BG_LYR_SUB_2_F, enable);
        switch (bgMode) {
        default:
        case GF_BG_TYPE_TEXT:
            G2S_SetBG2ControlText((GXBGScrSizeText)screenSize, (GXBGColorMode)template->colorMode, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        case GF_BG_TYPE_AFFINE:
            G2S_SetBG2ControlAffine((GXBGScrSizeAffine)screenSize, (GXBGAreaOver)template->areaOver, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        case GF_BG_TYPE_256x16PLTT:
            G2S_SetBG2Control256x16Pltt((GXBGScrSize256x16Pltt)screenSize, (GXBGAreaOver)template->areaOver, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        }
        G2S_SetBG2Priority(template->priority);
        G2S_BG2Mosaic(template->mosaic);
        break;

    case GF_BG_LYR_SUB_3:
        GX_EngineBToggleLayers(GF_BG_LYR_SUB_3_F, enable);
        switch (bgMode) {
        default:
        case GF_BG_TYPE_TEXT:
            G2S_SetBG3ControlText((GXBGScrSizeText)screenSize, (GXBGColorMode)template->colorMode, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        case GF_BG_TYPE_AFFINE:
            G2S_SetBG3ControlAffine((GXBGScrSizeAffine)screenSize, (GXBGAreaOver)template->areaOver, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        case GF_BG_TYPE_256x16PLTT:
            G2S_SetBG3Control256x16Pltt((GXBGScrSize256x16Pltt)screenSize, (GXBGAreaOver)template->areaOver, (GXBGScrBase)template->screenBase, (GXBGCharBase)template->charBase);
            break;
        }
        G2S_SetBG3Priority(template->priority);
        G2S_BG3Mosaic(template->mosaic);
        break;
    }

    bgConfig->bgs[bgId].rotation = 0;
    bgConfig->bgs[bgId].xScale = FX32_ONE;
    bgConfig->bgs[bgId].yScale = FX32_ONE;
    bgConfig->bgs[bgId].centerX = 0;
    bgConfig->bgs[bgId].centerY = 0;

    if (template->bufferSize != 0) {
        bgConfig->bgs[bgId].tilemapBuffer = AllocFromHeap(bgConfig->heap_id, template->bufferSize);

        MI_CpuClear16(bgConfig->bgs[bgId].tilemapBuffer, template->bufferSize);

        bgConfig->bgs[bgId].bufferSize = template->bufferSize;
        bgConfig->bgs[bgId].baseTile = template->baseTile;
    } else {
        bgConfig->bgs[bgId].tilemapBuffer = NULL;
        bgConfig->bgs[bgId].bufferSize = 0;
        bgConfig->bgs[bgId].baseTile = 0;
    }

    bgConfig->bgs[bgId].size = template->size;
    bgConfig->bgs[bgId].mode = bgMode;
    bgConfig->bgs[bgId].colorMode = template->colorMode;

    if (bgMode == GF_BG_TYPE_TEXT && template->colorMode == GX_BG_COLORMODE_16) {
        bgConfig->bgs[bgId].tileSize = 0x20;
    } else {
        bgConfig->bgs[bgId].tileSize = 0x40;
    }

    BgSetPosTextAndCommit(bgConfig, bgId, BG_POS_OP_SET_X, template->x);
    BgSetPosTextAndCommit(bgConfig, bgId, BG_POS_OP_SET_Y, template->y);
}

void InitBgFromTemplate(BGCONFIG *bgConfig, u8 bgId, const BGTEMPLATE *template, u8 bgMode) {
    InitBgFromTemplateEx(bgConfig, bgId, template, bgMode, GX_LAYER_TOGGLE_ON);
}

void SetBgControlParam(BGCONFIG *config, u8 bgId, enum GFBgCntSet attr, u8 value) {
    u16 screenSize;
    if (attr == GF_BG_CNT_SET_COLOR_MODE) {
        config->bgs[bgId].colorMode = value;
        if (config->bgs[bgId].mode == GF_BG_TYPE_TEXT && config->bgs[bgId].colorMode == GX_BG_COLORMODE_16) {
            config->bgs[bgId].tileSize = 0x20;
        } else {
            config->bgs[bgId].tileSize = 0x40;
        }
    } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
        config->bgs[bgId].mode = value;
        screenSize = TranslateGFBgModePairToGXScreenSize((enum GFBgScreenSize)config->bgs[bgId].size, (enum GFBgType)value);
        if (value == GF_BG_TYPE_TEXT && config->bgs[bgId].colorMode == GX_BG_COLORMODE_16) {
            config->bgs[bgId].tileSize = 0x20;
        } else {
            config->bgs[bgId].tileSize = 0x40;
        }
    }

    switch (bgId) {
    case GF_BG_LYR_MAIN_0:
        GXBg01Control bg0cnt = G2_GetBG0Control();
        if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
            bg0cnt.screenBase = value;
        } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
            bg0cnt.charBase = value;
        } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
            bg0cnt.screenSize = screenSize;
        }

        G2_SetBG0Control((GXBGScrSizeText)bg0cnt.screenSize, (GXBGColorMode)config->bgs[bgId].colorMode, (GXBGScrBase)bg0cnt.screenBase, (GXBGCharBase)bg0cnt.charBase, (GXBGExtPltt)bg0cnt.bgExtPltt);
        break;
    case GF_BG_LYR_MAIN_1:
        GXBg01Control bg1cnt = G2_GetBG1Control();
        if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
            bg1cnt.screenBase = value;
        } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
            bg1cnt.charBase = value;
        } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
            bg1cnt.screenSize = screenSize;
        }

        G2_SetBG1Control((GXBGScrSizeText)bg1cnt.screenSize, (GXBGColorMode)config->bgs[bgId].colorMode, (GXBGScrBase)bg1cnt.screenBase, (GXBGCharBase)bg1cnt.charBase, (GXBGExtPltt)bg1cnt.bgExtPltt);
        break;
    case GF_BG_LYR_MAIN_2:
        switch (config->bgs[bgId].mode) {
        default:
        case GF_BG_TYPE_TEXT:
            GXBg23ControlText bg2cnt_tx = G2_GetBG2ControlText();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg2cnt_tx.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg2cnt_tx.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg2cnt_tx.screenSize = screenSize;
            }

            G2_SetBG2ControlText((GXBGScrSizeText)bg2cnt_tx.screenSize, (GXBGColorMode)config->bgs[bgId].colorMode, (GXBGScrBase)bg2cnt_tx.screenBase, (GXBGCharBase)bg2cnt_tx.charBase);
            break;
        case GF_BG_TYPE_AFFINE:
            GXBg23ControlAffine bg2cnt_aff = G2_GetBG2ControlAffine();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg2cnt_aff.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg2cnt_aff.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg2cnt_aff.screenSize = screenSize;
            }

            G2_SetBG2ControlAffine((GXBGScrSizeAffine)bg2cnt_aff.screenSize, (GXBGAreaOver)bg2cnt_aff.areaOver, (GXBGScrBase)bg2cnt_aff.screenBase, (GXBGCharBase)bg2cnt_aff.charBase);
            break;
        case GF_BG_TYPE_256x16PLTT:
            GXBg23Control256x16Pltt bg2cnt_256x16pltt = G2_GetBG2Control256x16Pltt();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg2cnt_256x16pltt.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg2cnt_256x16pltt.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg2cnt_256x16pltt.screenSize = screenSize;
            }

            G2_SetBG2Control256x16Pltt((GXBGScrSize256x16Pltt)bg2cnt_256x16pltt.screenSize, (GXBGAreaOver)bg2cnt_256x16pltt.areaOver, (GXBGScrBase)bg2cnt_256x16pltt.screenBase, (GXBGCharBase)bg2cnt_256x16pltt.charBase);
            break;
        }
        break;
    case GF_BG_LYR_MAIN_3:
        switch (config->bgs[bgId].mode) {
        default:
        case GF_BG_TYPE_TEXT:
            GXBg23ControlText bg3cnt_tx = G2_GetBG3ControlText();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg3cnt_tx.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg3cnt_tx.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg3cnt_tx.screenSize = screenSize;
            }

            G2_SetBG3ControlText((GXBGScrSizeText)bg3cnt_tx.screenSize, (GXBGColorMode)config->bgs[bgId].colorMode, (GXBGScrBase)bg3cnt_tx.screenBase, (GXBGCharBase)bg3cnt_tx.charBase);
            break;
        case GF_BG_TYPE_AFFINE:
            GXBg23ControlAffine bg3cnt_aff = G2_GetBG3ControlAffine();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg3cnt_aff.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg3cnt_aff.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg3cnt_aff.screenSize = screenSize;
            }

            G2_SetBG3ControlAffine((GXBGScrSizeAffine)bg3cnt_aff.screenSize, (GXBGAreaOver)bg3cnt_aff.areaOver, (GXBGScrBase)bg3cnt_aff.screenBase, (GXBGCharBase)bg3cnt_aff.charBase);
            break;
        case GF_BG_TYPE_256x16PLTT:
            GXBg23Control256x16Pltt bg3cnt_256x16pltt = G2_GetBG3Control256x16Pltt();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg3cnt_256x16pltt.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg3cnt_256x16pltt.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg3cnt_256x16pltt.screenSize = screenSize;
            }

            G2_SetBG3Control256x16Pltt((GXBGScrSize256x16Pltt)bg3cnt_256x16pltt.screenSize, (GXBGAreaOver)bg3cnt_256x16pltt.areaOver, (GXBGScrBase)bg3cnt_256x16pltt.screenBase, (GXBGCharBase)bg3cnt_256x16pltt.charBase);
            break;
        }
        break;
    case GF_BG_LYR_SUB_0:
        GXBg01Control bg0cntsub = G2S_GetBG0Control();
        if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
            bg0cntsub.screenBase = value;
        } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
            bg0cntsub.charBase = value;
        } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
            bg0cntsub.screenSize = screenSize;
        }

        G2S_SetBG0Control((GXBGScrSizeText)bg0cntsub.screenSize, (GXBGColorMode)config->bgs[bgId].colorMode, (GXBGScrBase)bg0cntsub.screenBase, (GXBGCharBase)bg0cntsub.charBase, (GXBGExtPltt)bg0cntsub.bgExtPltt);
        break;
    case GF_BG_LYR_SUB_1:
        GXBg01Control bg1cntsub = G2S_GetBG1Control();
        if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
            bg1cntsub.screenBase = value;
        } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
            bg1cntsub.charBase = value;
        } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
            bg1cntsub.screenSize = screenSize;
        }

        G2S_SetBG1Control((GXBGScrSizeText)bg1cntsub.screenSize, (GXBGColorMode)config->bgs[bgId].colorMode, (GXBGScrBase)bg1cntsub.screenBase, (GXBGCharBase)bg1cntsub.charBase, (GXBGExtPltt)bg1cntsub.bgExtPltt);
        break;
    case GF_BG_LYR_SUB_2:
        switch (config->bgs[bgId].mode) {
        default:
        case GF_BG_TYPE_TEXT:
            GXBg23ControlText bg2cntsub_tx = G2S_GetBG2ControlText();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg2cntsub_tx.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg2cntsub_tx.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg2cntsub_tx.screenSize = screenSize;
            }

            G2S_SetBG2ControlText((GXBGScrSizeText)bg2cntsub_tx.screenSize, (GXBGColorMode)config->bgs[bgId].colorMode, (GXBGScrBase)bg2cntsub_tx.screenBase, (GXBGCharBase)bg2cntsub_tx.charBase);
            break;
        case GF_BG_TYPE_AFFINE:
            GXBg23ControlAffine bg2cntsub_aff = G2S_GetBG2ControlAffine();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg2cntsub_aff.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg2cntsub_aff.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg2cntsub_aff.screenSize = screenSize;
            }

            G2S_SetBG2ControlAffine((GXBGScrSizeAffine)bg2cntsub_aff.screenSize, (GXBGAreaOver)bg2cntsub_aff.areaOver, (GXBGScrBase)bg2cntsub_aff.screenBase, (GXBGCharBase)bg2cntsub_aff.charBase);
            break;
        case GF_BG_TYPE_256x16PLTT:
            GXBg23Control256x16Pltt bg2cntsub_256x16pltt = G2S_GetBG2Control256x16Pltt();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg2cntsub_256x16pltt.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg2cntsub_256x16pltt.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg2cntsub_256x16pltt.screenSize = screenSize;
            }

            G2S_SetBG2Control256x16Pltt((GXBGScrSize256x16Pltt)bg2cntsub_256x16pltt.screenSize, (GXBGAreaOver)bg2cntsub_256x16pltt.areaOver, (GXBGScrBase)bg2cntsub_256x16pltt.screenBase, (GXBGCharBase)bg2cntsub_256x16pltt.charBase);
            break;
        }
        break;
    case GF_BG_LYR_SUB_3:
        switch (config->bgs[bgId].mode) {
        default:
        case GF_BG_TYPE_TEXT:
            GXBg23ControlText bg3cntsub_tx = G2S_GetBG3ControlText();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg3cntsub_tx.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg3cntsub_tx.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg3cntsub_tx.screenSize = screenSize;
            }

            G2S_SetBG3ControlText((GXBGScrSizeText)bg3cntsub_tx.screenSize, (GXBGColorMode)config->bgs[bgId].colorMode, (GXBGScrBase)bg3cntsub_tx.screenBase, (GXBGCharBase)bg3cntsub_tx.charBase);
            break;
        case GF_BG_TYPE_AFFINE:
            GXBg23ControlAffine bg3cntsub_aff = G2S_GetBG3ControlAffine();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg3cntsub_aff.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg3cntsub_aff.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg3cntsub_aff.screenSize = screenSize;
            }

            G2S_SetBG3ControlAffine((GXBGScrSizeAffine)bg3cntsub_aff.screenSize, (GXBGAreaOver)bg3cntsub_aff.areaOver, (GXBGScrBase)bg3cntsub_aff.screenBase, (GXBGCharBase)bg3cntsub_aff.charBase);
            break;
        case GF_BG_TYPE_256x16PLTT:
            GXBg23Control256x16Pltt bg3cntsub_256x16pltt = G2S_GetBG3Control256x16Pltt();
            if (attr == GF_BG_CNT_SET_SCREEN_BASE) {
                bg3cntsub_256x16pltt.screenBase = value;
            } else if (attr == GF_BG_CNT_SET_CHAR_BASE) {
                bg3cntsub_256x16pltt.charBase = value;
            } else if (attr == GF_BG_CNT_SET_SCREEN_SIZE) {
                bg3cntsub_256x16pltt.screenSize = screenSize;
            }

            G2S_SetBG3Control256x16Pltt((GXBGScrSize256x16Pltt)bg3cntsub_256x16pltt.screenSize, (GXBGAreaOver)bg3cntsub_256x16pltt.areaOver, (GXBGScrBase)bg3cntsub_256x16pltt.screenBase, (GXBGCharBase)bg3cntsub_256x16pltt.charBase);
            break;
        }
        break;
    }
}

static u8 TranslateGFBgModePairToGXScreenSize(enum GFBgScreenSize size, enum GFBgType type) {
    switch (type) {
    case GF_BG_TYPE_TEXT:

        if (size == GF_BG_SCR_SIZE_256x256) {
            return GX_BG_SCRSIZE_TEXT_256x256;
        } else if (size == GF_BG_SCR_SIZE_256x512) {
            return GX_BG_SCRSIZE_TEXT_256x512;
        } else if (size == GF_BG_SCR_SIZE_512x256) {
            return GX_BG_SCRSIZE_TEXT_512x256;
        } else if (size == GF_BG_SCR_SIZE_512x512) {
            return GX_BG_SCRSIZE_TEXT_512x512;
        }
        break;

    case GF_BG_TYPE_AFFINE:

        if (size == GF_BG_SCR_SIZE_128x128) {
            return GX_BG_SCRSIZE_AFFINE_128x128;
        } else if (size == GF_BG_SCR_SIZE_256x256) {
            return GX_BG_SCRSIZE_AFFINE_256x256;
        } else if (size == GF_BG_SCR_SIZE_512x512) {
            return GX_BG_SCRSIZE_AFFINE_512x512;
        } else if (size == GF_BG_SCR_SIZE_1024x1024) {
            return GX_BG_SCRSIZE_AFFINE_1024x1024;
        }
        break;

    case GF_BG_TYPE_256x16PLTT:

        if (size == GF_BG_SCR_SIZE_128x128) {
            return GX_BG_SCRSIZE_256x16PLTT_128x128;
        } else if (size == GF_BG_SCR_SIZE_256x256) {
            return GX_BG_SCRSIZE_256x16PLTT_256x256;
        } else if (size == GF_BG_SCR_SIZE_512x512) {
            return GX_BG_SCRSIZE_256x16PLTT_512x512;
        } else if (size == GF_BG_SCR_SIZE_1024x1024) {
            return GX_BG_SCRSIZE_256x16PLTT_1024x1024;
        }
        break;
    }

    return GX_BG_SCRSIZE_TEXT_256x256; // GX_BG_SCRSIZE_AFFINE_128x128; GX_BG_SCRSIZE_256x16PLTT_128x128;
}

static void GetBgScreenDimensions(u32 screenSize, u8 *width_p, u8 *height_p) {
    switch (screenSize) {
    case GF_BG_SCR_SIZE_128x128:
        *width_p = 0x10;
        *height_p = 0x10;
        break;
    case GF_BG_SCR_SIZE_256x256:
        *width_p = 0x20;
        *height_p = 0x20;
        break;
    case GF_BG_SCR_SIZE_256x512:
        *width_p = 0x20;
        *height_p = 0x40;
        break;
    case GF_BG_SCR_SIZE_512x256:
        *width_p = 0x40;
        *height_p = 0x20;
        break;
    case GF_BG_SCR_SIZE_512x512:
        *width_p = 0x40;
        *height_p = 0x40;
        break;
    case GF_BG_SCR_SIZE_1024x1024:
        *width_p = 0x80;
        *height_p = 0x80;
        break;
    }
}

void FreeBgTilemapBuffer(BGCONFIG *bgConfig, u8 layer) {
    if (bgConfig->bgs[layer].tilemapBuffer != NULL) {
        FreeToHeap(bgConfig->bgs[layer].tilemapBuffer);
        bgConfig->bgs[layer].tilemapBuffer = NULL;
    }
}

void SetBgPriority(u8 layer, int priority) {
    switch (layer) {
    case GF_BG_LYR_MAIN_0:
        G2_SetBG0Priority(priority);
        break;
    case GF_BG_LYR_MAIN_1:
        G2_SetBG1Priority(priority);
        break;
    case GF_BG_LYR_MAIN_2:
        G2_SetBG2Priority(priority);
        break;
    case GF_BG_LYR_MAIN_3:
        G2_SetBG3Priority(priority);
        break;
    case GF_BG_LYR_SUB_0:
        G2S_SetBG0Priority(priority);
        break;
    case GF_BG_LYR_SUB_1:
        G2S_SetBG1Priority(priority);
        break;
    case GF_BG_LYR_SUB_2:
        G2S_SetBG2Priority(priority);
        break;
    case GF_BG_LYR_SUB_3:
        G2S_SetBG3Priority(priority);
        break;
    }
}

void ToggleBgLayer(u8 layer, u8 toggle) {
    switch (layer) {
    case GF_BG_LYR_MAIN_0:
        GX_EngineAToggleLayers(GF_BG_LYR_MAIN_0_F, (GX_LayerToggle)toggle);
        break;
    case GF_BG_LYR_MAIN_1:
        GX_EngineAToggleLayers(GF_BG_LYR_MAIN_1_F, (GX_LayerToggle)toggle);
        break;
    case GF_BG_LYR_MAIN_2:
        GX_EngineAToggleLayers(GF_BG_LYR_MAIN_2_F, (GX_LayerToggle)toggle);
        break;
    case GF_BG_LYR_MAIN_3:
        GX_EngineAToggleLayers(GF_BG_LYR_MAIN_3_F, (GX_LayerToggle)toggle);
        break;
    case GF_BG_LYR_SUB_0:
        GX_EngineBToggleLayers(GF_BG_LYR_SUB_0_F, (GX_LayerToggle)toggle);
        break;
    case GF_BG_LYR_SUB_1:
        GX_EngineBToggleLayers(GF_BG_LYR_SUB_1_F, (GX_LayerToggle)toggle);
        break;
    case GF_BG_LYR_SUB_2:
        GX_EngineBToggleLayers(GF_BG_LYR_SUB_2_F, (GX_LayerToggle)toggle);
        break;
    case GF_BG_LYR_SUB_3:
        GX_EngineBToggleLayers(GF_BG_LYR_SUB_3_F, (GX_LayerToggle)toggle);
        break;
    }
}

void BgSetPosTextAndCommit(BGCONFIG *bgConfig, u8 bgId, enum BgPosAdjustOp op, fx32 val) {
    Bg_SetPosText(&bgConfig->bgs[bgId], op, val);

    u32 x = (u32)bgConfig->bgs[bgId].hOffset;
    u32 y = (u32)bgConfig->bgs[bgId].vOffset;
    switch (bgId) {
    case GF_BG_LYR_MAIN_0:
        G2_SetBG0Offset(x, y);
        break;
    case GF_BG_LYR_MAIN_1:
        G2_SetBG1Offset(x, y);
        break;
    case GF_BG_LYR_MAIN_2:
        if (bgConfig->bgs[GF_BG_LYR_MAIN_2].mode == 0) {
            G2_SetBG2Offset(x, y);
        } else {
            BgAffineReset(bgConfig, GF_BG_LYR_MAIN_2);
        }

        break;
    case GF_BG_LYR_MAIN_3:
        if (bgConfig->bgs[GF_BG_LYR_MAIN_3].mode == 0) {
            G2_SetBG3Offset(x, y);
        } else {
            BgAffineReset(bgConfig, GF_BG_LYR_MAIN_3);
        }
        break;
    case GF_BG_LYR_SUB_0:
        G2S_SetBG0Offset(x, y);
        break;
    case GF_BG_LYR_SUB_1:
        G2S_SetBG1Offset(x, y);
        break;
    case GF_BG_LYR_SUB_2:
        if (bgConfig->bgs[GF_BG_LYR_SUB_2].mode == 0) {
            G2S_SetBG2Offset(x, y);
        } else {
            BgAffineReset(bgConfig, GF_BG_LYR_SUB_2);
        }
        break;
    case GF_BG_LYR_SUB_3:
        if (bgConfig->bgs[GF_BG_LYR_SUB_3].mode == 0) {
            G2S_SetBG3Offset(x, y);
        } else {
            BgAffineReset(bgConfig, GF_BG_LYR_SUB_3);
        }
        break;
    }
}

fx32 Bg_GetXpos(const BGCONFIG *bgConfig, u8 layer) {
    return bgConfig->bgs[layer].hOffset;
}

fx32 Bg_GetYpos(const BGCONFIG *bgConfig, u8 layer) {
    return bgConfig->bgs[layer].vOffset;
}

void Bg_SetTextDimAndAffineParams(BGCONFIG *bgConfig, u8 layer, enum BgPosAdjustOp op, fx32 value, MtxFx22 *mtx, fx32 centerX, fx32 centerY) {
    Bg_SetPosText(&bgConfig->bgs[layer], op, value);
    SetBgAffine(bgConfig, layer, mtx, centerX, centerY);
}

static void Bg_SetPosText(BG *bg, enum BgPosAdjustOp op, fx32 value) {
    switch (op) {
    case BG_POS_OP_SET_X:
        bg->hOffset = value;
        break;
    case BG_POS_OP_ADD_X:
        bg->hOffset += value;
        break;
    case BG_POS_OP_SUB_X:
        bg->hOffset -= value;
        break;
    case BG_POS_OP_SET_Y:
        bg->vOffset = value;
        break;
    case BG_POS_OP_ADD_Y:
        bg->vOffset += value;
        break;
    case BG_POS_OP_SUB_Y:
        bg->vOffset -= value;
        break;
    }
}

void SetBgAffine(BGCONFIG *bgConfig, u8 layer, MtxFx22 *mtx, fx32 centerX, fx32 centerY) {
    switch (layer) {
    case GF_BG_LYR_MAIN_0:
        break;
    case GF_BG_LYR_MAIN_1:
        break;
    case GF_BG_LYR_MAIN_2:
        G2_SetBG2Affine(mtx, centerX, centerY, bgConfig->bgs[layer].hOffset, bgConfig->bgs[layer].vOffset);
        break;
    case GF_BG_LYR_MAIN_3:
        G2_SetBG3Affine(mtx, centerX, centerY, bgConfig->bgs[layer].hOffset, bgConfig->bgs[layer].vOffset);
        break;
    case GF_BG_LYR_SUB_0:
        break;
    case GF_BG_LYR_SUB_1:
        break;
    case GF_BG_LYR_SUB_2:
        G2S_SetBG2Affine(mtx, centerX, centerY, bgConfig->bgs[layer].hOffset, bgConfig->bgs[layer].vOffset);
        break;
    case GF_BG_LYR_SUB_3:
        G2S_SetBG3Affine(mtx, centerX, centerY, bgConfig->bgs[layer].hOffset, bgConfig->bgs[layer].vOffset);
        break;
    }
}

static void BgAffineReset(BGCONFIG *bgConfig, u8 layer) {
    MtxFx22 mtx;
    MTX22_2DAffine(&mtx, 0, FX32_ONE, FX32_ONE, 0);
    SetBgAffine(bgConfig, layer, &mtx, 0, 0);
}

static void CopyOrUncompressTilemapData(const void *src, void *dest, u32 size) {
    if (size == 0) {
        MI_UncompressLZ8(src, dest);
    } else if ((((u32)src) % 4) == 0 && (((u32)dest) % 4) == 0 && (((u16)size) % 4) == 0) {
        MI_CpuCopy32(src, dest, size);
    } else {
        MI_CpuCopy16(src, dest, size);
    }
}

void BgCommitTilemapBufferToVram(BGCONFIG *bgConfig, u8 layer) {
    BgCopyOrUncompressTilemapBufferRangeToVram(bgConfig, layer, bgConfig->bgs[layer].tilemapBuffer, bgConfig->bgs[layer].bufferSize, bgConfig->bgs[layer].baseTile);
}

void BgCopyOrUncompressTilemapBufferRangeToVram(BGCONFIG *bgConfig, u8 layer, const void *buffer, u32 bufferSize, u32 baseTile) {
    void *dest;
    u32 uncompSize;
    void *ptr;
    if (bufferSize == 0) {
        dest = bgConfig->bgs[layer].tilemapBuffer;
        if (dest != NULL) {
            CopyOrUncompressTilemapData(buffer, dest, bufferSize);
            CopyTilesToVram(layer, dest, bgConfig->bgs[layer].baseTile * 2, bgConfig->bgs[layer].bufferSize);
        } else {
            uncompSize = MI_GetUncompressedSize(buffer);
            ptr = AllocFromHeapAtEnd(bgConfig->heap_id, uncompSize);
            CopyOrUncompressTilemapData(buffer, ptr, bufferSize);
            CopyTilesToVram(layer, ptr, baseTile * 2, uncompSize);
            FreeToHeap(ptr);
        }
    } else {
        CopyTilesToVram(layer, buffer, baseTile * 2, bufferSize);
    }
}

static void CopyTilesToVram(u8 layer, const void *data, u32 offset, u32 size) {
    DC_FlushRange(data, size);
    switch (layer) {
    case GF_BG_LYR_MAIN_0:
        GX_LoadBG0Scr(data, offset, size);
        break;
    case GF_BG_LYR_MAIN_1:
        GX_LoadBG1Scr(data, offset, size);
        break;
    case GF_BG_LYR_MAIN_2:
        GX_LoadBG2Scr(data, offset, size);
        break;
    case GF_BG_LYR_MAIN_3:
        GX_LoadBG3Scr(data, offset, size);
        break;
    case GF_BG_LYR_SUB_0:
        GXS_LoadBG0Scr(data, offset, size);
        break;
    case GF_BG_LYR_SUB_1:
        GXS_LoadBG1Scr(data, offset, size);
        break;
    case GF_BG_LYR_SUB_2:
        GXS_LoadBG2Scr(data, offset, size);
        break;
    case GF_BG_LYR_SUB_3:
        GXS_LoadBG3Scr(data, offset, size);
        break;
    }
}

void BG_LoadScreenTilemapData(BGCONFIG *bgConfig, u8 layer, const void *data, u32 size) {
    CopyOrUncompressTilemapData(data, bgConfig->bgs[layer].tilemapBuffer, size);
}

void BG_LoadCharTilesData(BGCONFIG *bgConfig, u8 layer, const void *data, u32 size, u32 tileStart) {
    if (bgConfig->bgs[layer].colorMode == GF_BG_CLR_4BPP) {
        BG_LoadCharPixelData(bgConfig, layer, data, size, tileStart * TILE_SIZE_4BPP);
    } else {
        BG_LoadCharPixelData(bgConfig, layer, data, size, tileStart * TILE_SIZE_8BPP);
    }
}

static void BG_LoadCharPixelData(BGCONFIG *bgConfig, u8 layer, const void *buffer, u32 size, u32 offset) {
    u32 uncompressedSize;
    void *uncompressedBuffer;
    if (size == 0) {
        uncompressedSize = MI_GetUncompressedSize(buffer);
        uncompressedBuffer = AllocFromHeapAtEnd(bgConfig->heap_id, uncompressedSize);
        CopyOrUncompressTilemapData(buffer, uncompressedBuffer, size);
        LoadBgVramChar(layer, uncompressedBuffer, offset, uncompressedSize);
        FreeToHeap(uncompressedBuffer);
    } else {
        LoadBgVramChar(layer, buffer, offset, size);
    }
}

static void LoadBgVramChar(u8 layer, const void *data, u32 offset, u32 size) {
    DC_FlushRange(data, size);
    switch (layer) {
    case GF_BG_LYR_MAIN_0:
        GX_LoadBG0Char(data, offset, size);
        break;
    case GF_BG_LYR_MAIN_1:
        GX_LoadBG1Char(data, offset, size);
        break;
    case GF_BG_LYR_MAIN_2:
        GX_LoadBG2Char(data, offset, size);
        break;
    case GF_BG_LYR_MAIN_3:
        GX_LoadBG3Char(data, offset, size);
        break;
    case GF_BG_LYR_SUB_0:
        GXS_LoadBG0Char(data, offset, size);
        break;
    case GF_BG_LYR_SUB_1:
        GXS_LoadBG1Char(data, offset, size);
        break;
    case GF_BG_LYR_SUB_2:
        GXS_LoadBG2Char(data, offset, size);
        break;
    case GF_BG_LYR_SUB_3:
        GXS_LoadBG3Char(data, offset, size);
        break;
    }
}

void BG_ClearCharDataRange(u8 layer, u32 size, u32 offset, HeapID heapId) {
    void *buffer;
    buffer = AllocFromHeapAtEnd(heapId, size);
    memset(buffer, 0, size);
    LoadBgVramChar(layer, buffer, offset, size);
    FreeToHeapExplicit(heapId, buffer);
}

void BG_FillCharDataRange(BGCONFIG *bgConfig, u32 layer, u8 fillValue, u32 ntiles, u32 offset) {
    void *buffer;
    u32 size;
    u32 value;

    size = ntiles * bgConfig->bgs[layer].tileSize;
    value = fillValue;
    buffer = AllocFromHeapAtEnd(bgConfig->heap_id, size);
    if (bgConfig->bgs[layer].tileSize == TILE_SIZE_4BPP) {
        value = (value << 12) | (value << 8) | (value << 4) | (value << 0);
        value |= value << 16;
    } else {
        value = (value << 24) | (value << 16) | (value << 8) | (value << 0);
    }
    MI_CpuFillFast(buffer, value, size);
    LoadBgVramChar(layer, buffer, bgConfig->bgs[layer].tileSize * offset, size);
    FreeToHeap(buffer);
}

void BG_LoadPlttData(u8 layer, const void *data, u32 size, u32 offset) {
    DC_FlushRange(data, size);
    if (layer < GF_BG_LYR_MAIN_CNT) {
        GX_LoadBGPltt(data, offset, size);
    } else {
        GXS_LoadBGPltt(data, offset, size);
    }
}

void BG_LoadBlankPltt(u8 layer, u32 size, u32 offset, HeapID heapId) {
    void *data;

    data = AllocFromHeapAtEnd(heapId, size);
    memset(data, 0, size);
    DC_FlushRange(data, size);
    if (layer < GF_BG_LYR_MAIN_CNT) {
        GX_LoadBGPltt(data, offset, size);
    } else {
        GXS_LoadBGPltt(data, offset, size);
    }
    FreeToHeapExplicit(heapId, data);
}

void BG_SetMaskColor(u8 layer, u16 value) {
    BG_LoadPlttData(layer, &value, sizeof(u16), 0);
}

static u16 GetTileMapIndexFromCoords(u8 x, u8 y, u8 size, u8 mode) {
    u16 ret;
    switch (size) {
    case GF_BG_SCR_SIZE_128x128:
        GF_ASSERT(x < 16);
        GF_ASSERT(y < 16);
        ret = y * 16 + x;
        break;
    case GF_BG_SCR_SIZE_256x256:
        GF_ASSERT(x < 32);
        GF_ASSERT(y < 32);
        ret = y * 32 + x;
        break;
    case GF_BG_SCR_SIZE_256x512:
        GF_ASSERT(x < 32);
        GF_ASSERT(y < 64);
        ret = y * 32 + x;
        break;
    case GF_BG_SCR_SIZE_512x256:
        GF_ASSERT(x < 64);
        GF_ASSERT(y < 32);
        ret = ((x >> 5) * 32 + y) * 32 + (x & 0x1F);
        break;
    case GF_BG_SCR_SIZE_512x512:
        GF_ASSERT(x < 64);
        GF_ASSERT(y < 64);
        if (mode == GF_BG_TYPE_TEXT) {
            ret = (x >> 5) + (y >> 5) * 2;
            ret *= 1024;
            ret += (y & 0x1F) * 32 + (x & 0x1F);
        } else {
            ret = x + 64 * y;
        }
        break;
    case GF_BG_SCR_SIZE_1024x1024:
        GF_ASSERT(x < 128);
        GF_ASSERT(y < 128);
        ret = y * 128 + x;
        break;
    }
    return ret;
}

static u16 GetSrcTileMapIndexFromCoords(u8 dstX, u8 dstY, u8 width, u8 height) {
    u8 r2 = 0;
    u16 r3 = 0;
    s16 r4 = width - 32;
    s16 r7 = height - 32;
    if (dstX / 32) {
        r2 += 1;
    }
    if (dstY / 32) {
        r2 += 2;
    }
    switch (r2) {
    case 0:
        if (r4 >= 0) {
            r3 += dstY * 32 + dstX;
        } else {
            r3 += dstY * width + dstX;
        }
        break;
    case 1:
        if (r7 >= 0) {
            r3 += 1024;
        } else {
            r3 += height * 32;
        }
        r3 += dstY * r4 + (dstX & 0x1F);
        break;
    case 2:
        r3 += width * 32;
        if (r4 >= 0) {
            r3 += (dstY & 0x1F) * 32 + dstX;
        } else {
            r3 += (dstY & 0x1F) * width + dstX;
        }
        break;
    case 3:
        r3 += width * 32 + r7 * 32;
        r3 += (dstY & 0x1F) * r4 + (dstX & 0x1F);
        break;
    }
    return r3;
}

void LoadRectToBgTilemapRect(BGCONFIG *bgConfig, u8 layer, const void *buf, u8 destX, u8 destY, u8 width, u8 height) {
    CopyToBgTilemapRect(bgConfig, layer, destX, destY, width, height, buf, 0, 0, width, height);
}

void CopyToBgTilemapRect(BGCONFIG *bgConfig, u8 layer, u8 destX, u8 destY, u8 destWidth, u8 destHeight, const void *buf, u8 srcX, u8 srcY, u8 srcWidth, u8 srcHeight) {
    if (bgConfig->bgs[layer].mode != GF_BG_TYPE_AFFINE) {
        CopyToBgTilemapRectText(&bgConfig->bgs[layer], destX, destY, destWidth, destHeight, buf, srcX, srcY, srcWidth, srcHeight, TILEMAP_COPY_SRC_FLAT);
    } else {
        CopyBgTilemapRectAffine(&bgConfig->bgs[layer], destX, destY, destWidth, destHeight, buf, srcX, srcY, srcWidth, srcHeight, TILEMAP_COPY_SRC_FLAT);
    }
}

void CopyRectToBgTilemapRect(BGCONFIG *bgConfig, u8 layer, u8 destX, u8 destY, u8 destWidth, u8 destHeight, const void *buf, u8 srcX, u8 srcY, u8 srcWidth, u8 srcHeight) {
    if (bgConfig->bgs[layer].mode != GF_BG_TYPE_AFFINE) {
        CopyToBgTilemapRectText(&bgConfig->bgs[layer], destX, destY, destWidth, destHeight, buf, srcX, srcY, srcWidth, srcHeight, TILEMAP_COPY_SRC_RECT);
    } else {
        CopyBgTilemapRectAffine(&bgConfig->bgs[layer], destX, destY, destWidth, destHeight, buf, srcX, srcY, srcWidth, srcHeight, TILEMAP_COPY_SRC_RECT);
    }
}

static void CopyToBgTilemapRectText(BG *bg, u8 destX, u8 destY, u8 destWidth, u8 destHeight, const u16 *buf, u8 srcX, u8 srcY, u8 srcWidth, u8 srcHeight, u8 mode) {
    u16 *buffer;
    u8 screenWidth, screenHeight;
    u8 i, j;
    if (bg->tilemapBuffer == NULL) {
        return;
    }
    buffer = bg->tilemapBuffer;
    GetBgScreenDimensions(bg->size, &screenWidth, &screenHeight);
    if (mode == TILEMAP_COPY_SRC_FLAT) {
        for (i = 0; i < destHeight; i++) {
            if (destY + i >= screenHeight || srcY + i >= srcHeight) {
                break;
            }
            for (j = 0; j < destWidth; j++) {
                if (destX + j >= screenWidth || srcX + j >= srcWidth) {
                    break;
                }
                buffer[GetTileMapIndexFromCoords(destX + j, destY + i, bg->size, bg->mode)] = buf[(srcY + i) * srcWidth + srcX + j];
            }
        }
    } else {
        for (i = 0; i < destHeight; i++) {
            if (destY + i >= screenHeight || srcY + i >= srcHeight) {
                break;
            }
            for (j = 0; j < destWidth; j++) {
                if (destX + j >= screenWidth || srcX + j >= srcWidth) {
                    break;
                }
                buffer[GetTileMapIndexFromCoords(destX + j, destY + i, bg->size, bg->mode)] = buf[GetSrcTileMapIndexFromCoords(srcX + j, srcY + i, srcWidth, srcHeight)];
            }
        }
    }
}

static void CopyBgTilemapRectAffine(BG *bg, u8 destX, u8 destY, u8 destWidth, u8 destHeight, const u8 *buf, u8 srcX, u8 srcY, u8 srcWidth, u8 srcHeight, u8 mode) {
    u8 *buffer;
    u8 screenWidth, screenHeight;
    u8 i, j;
    if (bg->tilemapBuffer == NULL) {
        return;
    }
    buffer = bg->tilemapBuffer;
    GetBgScreenDimensions(bg->size, &screenWidth, &screenHeight);
    if (mode == TILEMAP_COPY_SRC_FLAT) {
        for (i = 0; i < destHeight; i++) {
            if (destY + i >= screenHeight || srcY + i >= srcHeight) {
                break;
            }
            for (j = 0; j < destWidth; j++) {
                if (destX + j >= screenWidth || srcX + j >= srcWidth) {
                    break;
                }
                buffer[GetTileMapIndexFromCoords(destX + j, destY + i, bg->size, bg->mode)] = buf[(srcY + i) * srcWidth + srcX + j];
            }
        }
    } else {
        for (i = 0; i < destHeight; i++) {
            if (destY + i >= screenHeight || srcY + i >= srcHeight) {
                break;
            }
            for (j = 0; j < destWidth; j++) {
                if (destX + j >= screenWidth || srcX + j >= srcWidth) {
                    break;
                }
                buffer[GetTileMapIndexFromCoords(destX + j, destY + i, bg->size, bg->mode)] = buf[GetSrcTileMapIndexFromCoords(srcX + j, srcY + i, srcWidth, srcHeight)];
            }
        }
    }
}

void FillBgTilemapRect(BGCONFIG *bgConfig, u8 layer, u16 value, u8 x, u8 y, u8 width, u8 height, u8 mode) {
    if (bgConfig->bgs[layer].mode != GF_BG_TYPE_AFFINE) {
        FillBgTilemapRectText(&bgConfig->bgs[layer], value, x, y, width, height, mode);
    } else {
        FillBgTilemapRectAffine(&bgConfig->bgs[layer], value, x, y, width, height);
    }
}

static void FillBgTilemapRectText(BG *bg, u16 value, u8 x, u8 y, u8 width, u8 height, u8 mode) {
    u16 *buffer;
    u8 screenWidth, screenHeight;
    u8 i, j;
    u16 pos;
    if (bg->tilemapBuffer == NULL) {
        return;
    }
    buffer = bg->tilemapBuffer;
    GetBgScreenDimensions(bg->size, &screenWidth, &screenHeight);
    for (i = y; i < y + height; i++) {
        if (i >= screenHeight) {
            break;
        }
        for (j = x; j < x + width; j++) {
            if (j >= screenWidth) {
                break;
            }
            pos = GetTileMapIndexFromCoords(j, i, bg->size, bg->mode);
            if (mode == TILEMAP_FILL_OVWT_PAL) {
                buffer[pos] = value;
            } else if (mode == TILEMAP_FILL_KEEP_PAL) {
                buffer[pos] = (buffer[pos] & 0xF000) + (value & 0xFFF);
            } else {
                buffer[pos] = (mode << 12) + (value & 0xFFF);
            }
        }
    }
}

static void FillBgTilemapRectAffine(BG *bg, u8 value, u8 x, u8 y, u8 width, u8 height) {
    u8 *buffer;
    u8 screenWidth, screenHeight;
    u8 i, j;
    u16 pos;
    if (bg->tilemapBuffer == NULL) {
        return;
    }
    buffer = bg->tilemapBuffer;
    GetBgScreenDimensions(bg->size, &screenWidth, &screenHeight);
    for (i = y; i < y + height; i++) {
        if (i >= screenHeight) {
            break;
        }
        for (j = x; j < x + width; j++) {
            if (j >= screenWidth) {
                break;
            }
            pos = GetTileMapIndexFromCoords(j, i, bg->size, bg->mode);
            buffer[pos] = value;
        }
    }
}

void BgTilemapRectChangePalette(BGCONFIG *bgConfig, u8 layer, u8 x, u8 y, u8 width, u8 height, u8 palette) {
    u16 *buffer;
    u8 screenWidth, screenHeight;
    u8 i, j;
    u16 pos;
    if (bgConfig->bgs[layer].tilemapBuffer == NULL) {
        return;
    }
    buffer = bgConfig->bgs[layer].tilemapBuffer;
    GetBgScreenDimensions(bgConfig->bgs[layer].size, &screenWidth, &screenHeight);
    for (i = y; i < y + height; i++) {
        if (i >= screenHeight) {
            break;
        }
        for (j = x; j < x + width; j++) {
            if (j >= screenWidth) {
                break;
            }
            pos = GetTileMapIndexFromCoords(j, i, bgConfig->bgs[layer].size, bgConfig->bgs[layer].mode);
            buffer[pos] = (buffer[pos] & 0xFFF) | (palette << 12);
        }
    }
}

void BgClearTilemapBufferAndCommit(BGCONFIG *bgConfig, u8 layer) {
    if (bgConfig->bgs[layer].tilemapBuffer != NULL) {
        MI_CpuClear16(bgConfig->bgs[layer].tilemapBuffer, bgConfig->bgs[layer].bufferSize);
        BgCommitTilemapBufferToVram(bgConfig, layer);
    }
}

static void BgClearTilemapBufferAndSchedule(BGCONFIG *bgConfig, u8 layer) {
    if (bgConfig->bgs[layer].tilemapBuffer != NULL) {
        MI_CpuClear16(bgConfig->bgs[layer].tilemapBuffer, bgConfig->bgs[layer].bufferSize);
        ScheduleBgTilemapBufferTransfer(bgConfig, layer);
    }
}

void BgFillTilemapBufferAndCommit(BGCONFIG *bgConfig, u8 layer, u16 value) {
    if (bgConfig->bgs[layer].tilemapBuffer != NULL) {
        MI_CpuFill16(bgConfig->bgs[layer].tilemapBuffer, value, bgConfig->bgs[layer].bufferSize);
        BgCommitTilemapBufferToVram(bgConfig, layer);
    }
}

void BgFillTilemapBufferAndSchedule(BGCONFIG *bgConfig, u8 layer, u16 value) {
    if (bgConfig->bgs[layer].tilemapBuffer != NULL) {
        MI_CpuFill16(bgConfig->bgs[layer].tilemapBuffer, value, bgConfig->bgs[layer].bufferSize);
        ScheduleBgTilemapBufferTransfer(bgConfig, layer);
    }
}

void *BgGetCharPtr(u8 layer) {
    switch (layer) {
    case GF_BG_LYR_MAIN_0:
        return G2_GetBG0CharPtr();
    case GF_BG_LYR_MAIN_1:
        return G2_GetBG1CharPtr();
    case GF_BG_LYR_MAIN_2:
        return G2_GetBG2CharPtr();
    case GF_BG_LYR_MAIN_3:
        return G2_GetBG3CharPtr();
    case GF_BG_LYR_SUB_0:
        return G2S_GetBG0CharPtr();
    case GF_BG_LYR_SUB_1:
        return G2S_GetBG1CharPtr();
    case GF_BG_LYR_SUB_2:
        return G2S_GetBG2CharPtr();
    case GF_BG_LYR_SUB_3:
        return G2S_GetBG3CharPtr();
    }

    return NULL;
}

static void Convert4bppTo8bppInternal(u8 *src4bpp, u32 size, u8 *dest8bpp, u8 paletteNum) {
    paletteNum <<= 4;
    for (u32 i = 0; i < size; i++) {
        dest8bpp[i * 2 + 0] = (u8)(src4bpp[i] & 0xf);
        if (dest8bpp[i * 2 + 0] != 0) {
            dest8bpp[i * 2 + 0] += paletteNum;
        }

        dest8bpp[i * 2 + 1] = (u8)((src4bpp[i] >> 4) & 0xf);
        if (dest8bpp[i * 2 + 1] != 0) {
            dest8bpp[i * 2 + 1] += paletteNum;
        }
    }
}

u8 *Convert4bppTo8bpp(u8 *src4Bpp, u32 size, u8 paletteNum, HeapID heap_id) {
    u8 *ptr = (u8*)AllocFromHeap(heap_id, size * 2);

    Convert4bppTo8bppInternal(src4Bpp, size, ptr, paletteNum);

    return ptr;
}

void *GetBgTilemapBuffer(BGCONFIG *bgConfig, u8 layer) {
    return bgConfig->bgs[layer].tilemapBuffer;
}

fx32 GetBgHOffset(BGCONFIG *bgConfig, u8 layer) {
    return bgConfig->bgs[layer].hOffset;
}

static fx32 GetBgVOffset(BGCONFIG *bgConfig, u8 layer) {
    return bgConfig->bgs[layer].vOffset;
}

static u16 GetBgRotation(BGCONFIG *bgConfig, u8 layer) {
    return bgConfig->bgs[layer].rotation;
}

u8 GetBgColorMode(BGCONFIG *bgConfig, u8 layer) {
    return bgConfig->bgs[layer].colorMode;
}

u16 GetBgPriority(BGCONFIG *bgConfig, u8 layer) {
    switch (layer) {
    case GF_BG_LYR_MAIN_0: {
        GXBg01Control control = G2_GetBG0Control();
        return (u8)control.priority;
    }
    case GF_BG_LYR_MAIN_1: {
        GXBg01Control control = G2_GetBG1Control();
        return (u8)control.priority;
    }
    case GF_BG_LYR_MAIN_2:
        switch (bgConfig->bgs[layer].mode) {
        default:
        case GF_BG_TYPE_TEXT: {
            GXBg23ControlText control = G2_GetBG2ControlText();
            return (u8)control.priority;
        }
        case GF_BG_TYPE_AFFINE: {
            GXBg23ControlAffine control = G2_GetBG2ControlAffine();
            return (u8)control.priority;
        }
        case GF_BG_TYPE_256x16PLTT: {
            GXBg23Control256x16Pltt control = G2_GetBG2Control256x16Pltt();
            return (u8)control.priority;
        }
        }
    case GF_BG_LYR_MAIN_3:
        switch (bgConfig->bgs[layer].mode) {
        default:
        case GF_BG_TYPE_TEXT: {
            GXBg23ControlText control = G2_GetBG3ControlText();
            return (u8)control.priority;
        }
        case GF_BG_TYPE_AFFINE: {
            GXBg23ControlAffine control = G2_GetBG3ControlAffine();
            return (u8)control.priority;
        }
        case GF_BG_TYPE_256x16PLTT: {
            GXBg23Control256x16Pltt control = G2_GetBG3Control256x16Pltt();
            return (u8)control.priority;
        }
        }
    case GF_BG_LYR_SUB_0: {
        GXBg01Control control = G2S_GetBG0Control();
        return (u8)control.priority;
    }
    case GF_BG_LYR_SUB_1: {
        GXBg01Control control = G2S_GetBG1Control();
        return (u8)control.priority;
    }
    case GF_BG_LYR_SUB_2:
        switch (bgConfig->bgs[layer].mode) {
        default:
        case GF_BG_TYPE_TEXT: {
            GXBg23ControlText control = G2S_GetBG2ControlText();
            return (u8)control.priority;
        }
        case GF_BG_TYPE_AFFINE: {
            GXBg23ControlAffine control = G2S_GetBG2ControlAffine();
            return (u8)control.priority;
        }
        case GF_BG_TYPE_256x16PLTT: {
            GXBg23Control256x16Pltt control = G2S_GetBG2Control256x16Pltt();
            return (u8)control.priority;
        }
        }
    case GF_BG_LYR_SUB_3:
        switch (bgConfig->bgs[layer].mode) {
        default:
        case GF_BG_TYPE_TEXT: {
            GXBg23ControlText control = G2S_GetBG3ControlText();
            return (u8)control.priority;
        }
        case GF_BG_TYPE_AFFINE: {
            GXBg23ControlAffine control = G2S_GetBG3ControlAffine();
            return (u8)control.priority;
        }
        case GF_BG_TYPE_256x16PLTT: {
            GXBg23Control256x16Pltt control = G2S_GetBG3Control256x16Pltt();
            return (u8)control.priority;
        }
        }
    }
    return 0;
}

#define GetPixelAddressFromBlit4bpp(ptr, x, y, width) ((u8 *)((ptr) + (((x) >> 1) & 3) + (((x) << 2) & 0x3FE0) + ((((y) << 2) & 0x3FE0) * (width)) + (((u32)(((y) << 2) & 0x1C)))))
#define GetPixelAddressFromBlit8bpp(ptr, x, y, width) ((u8 *)((ptr) + ((x) & 7) + (((x) << 3) & 0x7FC0) + ((((y) << 3) & 0x7FC0) * (width)) + (((u32)(((y) << 3) & 0x38)))))

#define ConvertPixelsToTiles(x)    (((x) + ((x) & 7)) >> 3)

void BlitBitmapRect4Bit(const BITMAP *src, const BITMAP *dst, u16 srcX, u16 srcY, u16 dstX, u16 dstY, u16 width, u16 height, u16 colorKey) {
    int xEnd, yEnd;
    int multiplierSrcY, multiplierDstY;
    int loopSrcY, loopDstY;
    int loopSrcX, loopDstX;
    int toOrr, toShift;
    u8 * pixelsSrc, * pixelsDst;

    if (dst->width - dstX < width) {
        xEnd = dst->width - dstX + srcX;
    } else {
        xEnd = width + srcX;
    }
    if (dst->height - dstY < height) {
        yEnd = dst->height - dstY + srcY;
    } else {
        yEnd = height + srcY;
    }
    multiplierSrcY = ConvertPixelsToTiles(src->width);
    multiplierDstY = ConvertPixelsToTiles(dst->width);

    if (colorKey == 0xFFFF) {
        for (loopSrcY = srcY, loopDstY = dstY; loopSrcY < yEnd; loopSrcY++, loopDstY++) {
            for (loopSrcX = srcX, loopDstX = dstX; loopSrcX < xEnd; loopSrcX++, loopDstX++) {
                pixelsSrc = GetPixelAddressFromBlit4bpp(src->pixels, loopSrcX, loopSrcY, multiplierSrcY);
                pixelsDst = GetPixelAddressFromBlit4bpp(dst->pixels, loopDstX, loopDstY, multiplierDstY);

                toOrr = (*pixelsSrc >> ((loopSrcX & 1) * 4)) & 0xF;
                toShift = (loopDstX & 1) * 4;
                *pixelsDst = ((toOrr << toShift) | (*pixelsDst & (0xF0 >> toShift)));
            }
        }
    } else {
        for (loopSrcY = srcY, loopDstY = dstY; loopSrcY < yEnd; loopSrcY++, loopDstY++) {
            for (loopSrcX = srcX, loopDstX = dstX; loopSrcX < xEnd; loopSrcX++, loopDstX++) {
                pixelsSrc = GetPixelAddressFromBlit4bpp(src->pixels, loopSrcX, loopSrcY, multiplierSrcY);
                pixelsDst = GetPixelAddressFromBlit4bpp(dst->pixels, loopDstX, loopDstY, multiplierDstY);

                toOrr = (*pixelsSrc >> ((loopSrcX & 1) * 4)) & 0xF;
                if (toOrr != colorKey) {
                    toShift = (loopDstX & 1) * 4;
                    *pixelsDst = (u8) ((toOrr << toShift) | (*pixelsDst & (0xF0 >> toShift)));
                }
            }
        }
    }
}

static void BlitBitmapRect8bit(const BITMAP *src, const BITMAP *dst, u16 srcX, u16 srcY, u16 dstX, u16 dstY, u16 width, u16 height, u16 colorKey) {
    int xEnd, yEnd;
    int multiplierSrcY, multiplierDstY;
    int loopSrcY, loopDstY;
    int loopSrcX, loopDstX;
    u8 * pixelsSrc, * pixelsDst;

    if (dst->width - dstX < width) {
        xEnd = dst->width - dstX + srcX;
    } else {
        xEnd = width + srcX;
    }
    if (dst->height - dstY < height) {
        yEnd = dst->height - dstY + srcY;
    } else {
        yEnd = height + srcY;
    }
    multiplierSrcY = ConvertPixelsToTiles(src->width);
    multiplierDstY = ConvertPixelsToTiles(dst->width);

    if (colorKey == 0xFFFF) {
        for (loopSrcY = srcY, loopDstY = dstY; loopSrcY < yEnd; loopSrcY++, loopDstY++) {
            for (loopSrcX = srcX, loopDstX = dstX; loopSrcX < xEnd; loopSrcX++, loopDstX++) {
                pixelsSrc = GetPixelAddressFromBlit8bpp(src->pixels, loopSrcX, loopSrcY, multiplierSrcY);
                pixelsDst = GetPixelAddressFromBlit8bpp(dst->pixels, loopDstX, loopDstY, multiplierDstY);

                *pixelsDst = *pixelsSrc;
            }
        }
    } else {
        for (loopSrcY = srcY, loopDstY = dstY; loopSrcY < yEnd; loopSrcY++, loopDstY++) {
            for (loopSrcX = srcX, loopDstX = dstX; loopSrcX < xEnd; loopSrcX++, loopDstX++) {
                pixelsSrc = GetPixelAddressFromBlit8bpp(src->pixels, loopSrcX, loopSrcY, multiplierSrcY);
                pixelsDst = GetPixelAddressFromBlit8bpp(dst->pixels, loopDstX, loopDstY, multiplierDstY);

                if (*pixelsSrc != colorKey) {
                    *pixelsDst = *pixelsSrc;
                }
            }
        }
    }
}

static void FillBitmapRect4bit(const BITMAP *surface, u16 x, u16 y, u16 width, u16 height, u8 fillValue) {

    int j;
    int i;
    int xEnd;
    int yEnd;
    int blitWidth;
    u8 *pixels;

    xEnd = x + width;
    if (xEnd > surface->width) {
        xEnd = surface->width;
    }

    yEnd = y + height;
    if (yEnd > surface->height) {
        yEnd = surface->height;
    }

    blitWidth = ConvertPixelsToTiles(surface->width);

    for (i = y; i < yEnd; i++) {
        for (j = x; j < xEnd; j++) {
            pixels = GetPixelAddressFromBlit4bpp(surface->pixels, j, i, blitWidth);

            if ((j & 1) != 0) {
                *pixels &= 0xf;
                *pixels |= (fillValue << 4);
            } else {
                *pixels &= 0xf0;
                *pixels |= fillValue;
            }
        }
    }
}

static void FillBitmapRect8bit(const BITMAP *surface, u16 x, u16 y, u16 width, u16 height, u8 fillValue) {
    int j;
    int i;
    int xEnd;
    int yEnd;
    int blitWidth;
    u8 *pixels;

    xEnd = x + width;
    if (xEnd > surface->width) {
        xEnd = surface->width;
    }

    yEnd = y + height;
    if (yEnd > surface->height) {
        yEnd = surface->height;
    }

    blitWidth = ConvertPixelsToTiles(surface->width);

    for (i = y; i < yEnd; i++) {
        for (j = x; j < xEnd; j++) {
            pixels = GetPixelAddressFromBlit8bpp(surface->pixels, j, i, blitWidth);
            *pixels = fillValue;
        }
    }
}

WINDOW *AllocWindows(HeapID heapId, int num) {
    WINDOW *ret;
    u16 i;

    ret = AllocFromHeap(heapId, num * sizeof(WINDOW));
    for (i = 0; i < num; i++) {
        InitWindow(&ret[i]);
    }
    return ret;
}

void InitWindow(WINDOW *window) {
    window->bgConfig = NULL;
    window->bgId = GF_BG_LYR_UNALLOC;
    window->tilemapLeft = 0;
    window->tilemapTop = 0;
    window->width = 0;
    window->height = 0;
    window->paletteNum = 0;
    window->baseTile = 0;
    window->pixelBuffer = NULL;
    window->colorMode = GF_BG_CLR_4BPP;
}

BOOL WindowIsInUse(const WINDOW *window) {
    if (window->bgConfig == NULL || window->bgId == 0xFF || window->pixelBuffer == NULL) {
        return FALSE;
    } else {
        return TRUE;
    }
}

void AddWindowParameterized(BGCONFIG *bgConfig, WINDOW *window, u8 layer, u8 x, u8 y, u8 width, u8 height, u8 paletteNum, u16 baseTile) {
    void *buffer;
    if (bgConfig->bgs[layer].tilemapBuffer != NULL) {
        buffer = AllocFromHeap(bgConfig->heap_id, width * height * bgConfig->bgs[layer].tileSize);
        if (buffer != NULL) {
            window->bgConfig = bgConfig;
            window->bgId = layer;
            window->tilemapLeft = x;
            window->tilemapTop = y;
            window->width = width;
            window->height = height;
            window->paletteNum = paletteNum;
            window->baseTile = baseTile;
            window->pixelBuffer = buffer;
            window->colorMode = (bgConfig->bgs[layer].colorMode == GX_BG_COLORMODE_16) ? GF_BG_CLR_4BPP : GF_BG_CLR_8BPP;
        }
    }
}

void AddTextWindowTopLeftCorner(BGCONFIG *bgConfig, WINDOW *window, u8 width, u8 height, u16 baseTile, u8 paletteNum) {
    u32 size;
    void *ptr;

    size = width * height * 32;
    ptr = AllocFromHeap(bgConfig->heap_id, size);
    paletteNum |= (paletteNum << 4);
    memset(ptr, paletteNum, size); // could cause a data protection abort if below is true
    if (ptr != NULL) {
        window->bgConfig = bgConfig;
        window->width = width;
        window->height = height;
        window->baseTile = baseTile;
        window->pixelBuffer = ptr;
        window->colorMode = GF_BG_CLR_4BPP;
    }
}

void AddWindow(BGCONFIG *bgConfig, WINDOW *window, const WINDOWTEMPLATE *template) {
    AddWindowParameterized(bgConfig, window, template->bgId, template->left, template->top, template->width, template->height, template->palette, template->baseBlock);
}

void RemoveWindow(WINDOW* window) {
    FreeToHeap(window->pixelBuffer);
    window->bgConfig = NULL;
    window->bgId = GF_BG_LYR_UNALLOC;
    window->tilemapLeft = 0;
    window->tilemapTop = 0;
    window->width = 0;
    window->height = 0;
    window->paletteNum = 0;
    window->baseTile = 0;
    window->pixelBuffer = NULL;
}

void WindowArray_Delete(WINDOW *window, int num) {
    u16 i;
    for (i = 0; i < num; i++) {
        if (window[i].pixelBuffer != NULL) {
            FreeToHeap(window[i].pixelBuffer);
        }
    }
    FreeToHeap(window);
}

void CopyWindowToVram(WINDOW *window) {
    GF_ASSERT(window != NULL);
    GF_ASSERT(window->bgConfig != NULL);
    GF_ASSERT(window->bgId < GF_BG_LYR_MAX);
    GF_ASSERT(window->bgConfig->bgs[window->bgId].mode < GF_BG_TYPE_MAX);
    sCopyWindowToVramFuncs[window->bgConfig->bgs[window->bgId].mode](window);
}

void ScheduleWindowCopyToVram(WINDOW *window) {
    GF_ASSERT(window != NULL);
    GF_ASSERT(window->bgConfig != NULL);
    GF_ASSERT(window->bgId < GF_BG_LYR_MAX);
    GF_ASSERT(window->bgConfig->bgs[window->bgId].mode < GF_BG_TYPE_MAX);
    sScheduleWindowCopyToVramFuncs[window->bgConfig->bgs[window->bgId].mode](window);
}

void PutWindowTilemap(WINDOW *window) {
    sPutWindowTilemapFuncs[window->bgConfig->bgs[window->bgId].mode](window);
}

void ClearWindowTilemap(WINDOW *window) {
    sClearWindowTilemapFuncs[window->bgConfig->bgs[window->bgId].mode](window);
}

static void PutWindowTilemap_TextMode(WINDOW *window) {
    u32 i, j, idx, tile;
    u32 tilemapBottom, tilemapRight;
    u16 *tilemap;

    tilemap = window->bgConfig->bgs[window->bgId].tilemapBuffer;
    if (tilemap != NULL) {
        tile = window->baseTile;
        tilemapRight = window->tilemapLeft + window->width;
        tilemapBottom = window->tilemapTop + window->height;
        for (i = window->tilemapTop; i < tilemapBottom; i++) {
            for (j = window->tilemapLeft; j < tilemapRight; j++) {
                idx = GetTileMapIndexFromCoords(j, i, window->bgConfig->bgs[window->bgId].size, window->bgConfig->bgs[window->bgId].mode);
                tilemap[idx] = tile | (window->paletteNum << 12);
                tile++;
            }
        }
    }
}

static void PutWindowTilemap_AffineMode(WINDOW *window) {
    int j, i, tile, tilemapWidth;
    u8 *tilemap;

    if (window->bgConfig->bgs[window->bgId].tilemapBuffer != NULL) {
        tilemapWidth = sTilemapWidthByBufferSize[window->bgConfig->bgs[window->bgId].size];
        tilemap = (u8 *)(window->bgConfig->bgs[window->bgId].tilemapBuffer) + window->tilemapTop * tilemapWidth + window->tilemapLeft;
        tile = window->baseTile;
        for (i = 0; i < window->height; i++) {
            for (j = 0; j < window->width; j++) {
                tilemap[j] = tile++;
            }
            tilemap += tilemapWidth;
        }
    }
}

static void ClearWindowTilemapText(WINDOW *window) {
    u32 i, j, idx;
    u32 tilemapBottom, tilemapRight;
    u16 *tilemap;

    tilemap = window->bgConfig->bgs[window->bgId].tilemapBuffer;
    if (tilemap != NULL) {
        tilemapRight = window->tilemapLeft + window->width;
        tilemapBottom = window->tilemapTop + window->height;
        for (i = window->tilemapTop; i < tilemapBottom; i++) {
            for (j = window->tilemapLeft; j < tilemapRight; j++) {
                idx = GetTileMapIndexFromCoords(j, i, window->bgConfig->bgs[window->bgId].size, window->bgConfig->bgs[window->bgId].mode);
                tilemap[idx] = 0;
            }
        }
    }
}

static void ClearWindowTilemapAffine(WINDOW *window) {
    int j, i, tilemapWidth;
    u8 *tilemap;

    if (window->bgConfig->bgs[window->bgId].tilemapBuffer != NULL) {
        tilemapWidth = sTilemapWidthByBufferSize[window->bgConfig->bgs[window->bgId].size];
        tilemap = (u8 *)(window->bgConfig->bgs[window->bgId].tilemapBuffer) + window->tilemapTop * tilemapWidth + window->tilemapLeft;
        for (i = 0; i < window->height; i++) {
            for (j = 0; j < window->width; j++) {
                tilemap[j] = 0;
            }
            tilemap += tilemapWidth;
        }
    }
}

static void CopyWindowToVram_TextMode(WINDOW *window) {
    PutWindowTilemap_TextMode(window);
    CopyWindowPixelsToVram_TextMode(window);
    BgCopyOrUncompressTilemapBufferRangeToVram(window->bgConfig, window->bgId, window->bgConfig->bgs[window->bgId].tilemapBuffer, window->bgConfig->bgs[window->bgId].bufferSize, window->bgConfig->bgs[window->bgId].baseTile);
}

static void ScheduleWindowCopyToVram_TextMode(WINDOW *window) {
    PutWindowTilemap_TextMode(window);
    ScheduleBgTilemapBufferTransfer(window->bgConfig, window->bgId);
    CopyWindowPixelsToVram_TextMode(window);
}

static void CopyWindowToVram_AffineMode(WINDOW *window) {
    PutWindowTilemap_AffineMode(window);
    BgCopyOrUncompressTilemapBufferRangeToVram(window->bgConfig, window->bgId, window->bgConfig->bgs[window->bgId].tilemapBuffer, window->bgConfig->bgs[window->bgId].bufferSize, window->bgConfig->bgs[window->bgId].baseTile);
    BG_LoadCharTilesData(window->bgConfig, window->bgId, window->pixelBuffer, window->width * window->height * TILE_SIZE_8BPP, window->baseTile);
}

static void ScheduleWindowCopyToVram_AffineMode(WINDOW *window) {
    PutWindowTilemap_AffineMode(window);
    ScheduleBgTilemapBufferTransfer(window->bgConfig, window->bgId);
    BG_LoadCharTilesData(window->bgConfig, window->bgId, window->pixelBuffer, window->width * window->height * TILE_SIZE_8BPP, window->baseTile);
}

void CopyWindowPixelsToVram_TextMode(WINDOW *window) {
    BG_LoadCharTilesData(window->bgConfig, window->bgId, window->pixelBuffer, window->width * window->height * window->bgConfig->bgs[window->bgId].tileSize, window->baseTile);
}

void ClearWindowTilemapAndCopyToVram(WINDOW *window) {
    sClearWindowTilemapAndCopyToVramFuncs[window->bgConfig->bgs[window->bgId].mode](window);
}

void ClearWindowTilemapAndScheduleTransfer(WINDOW *window) {
    sClearWindowTilemapAndScheduleTransferFuncs[window->bgConfig->bgs[window->bgId].mode](window);
}

static void ClearWindowTilemapAndCopyToVram_TextMode(WINDOW *window) {
    ClearWindowTilemapText(window);
    BgCopyOrUncompressTilemapBufferRangeToVram(window->bgConfig, window->bgId, window->bgConfig->bgs[window->bgId].tilemapBuffer, window->bgConfig->bgs[window->bgId].bufferSize, window->bgConfig->bgs[window->bgId].baseTile);
}

static void ClearWindowTilemapAndScheduleTransfer_TextMode(WINDOW *window) {
    ClearWindowTilemapText(window);
    ScheduleBgTilemapBufferTransfer(window->bgConfig, window->bgId);
}

static void ClearWindowTilemapAndCopyToVram_AffineMode(WINDOW *window) {
    ClearWindowTilemapAffine(window);
    BgCopyOrUncompressTilemapBufferRangeToVram(window->bgConfig, window->bgId, window->bgConfig->bgs[window->bgId].tilemapBuffer, window->bgConfig->bgs[window->bgId].bufferSize, window->bgConfig->bgs[window->bgId].baseTile);
}

static void ClearWindowTilemapAndScheduleTransfer_AffineMode(WINDOW *window) {
    ClearWindowTilemapAffine(window);
    ScheduleBgTilemapBufferTransfer(window->bgConfig, window->bgId);
}

void FillWindowPixelBuffer(WINDOW *window, u8 fillValue) {
    u32 fillWord;
    if (window->bgConfig->bgs[window->bgId].tileSize == 32) {
        fillValue |= fillValue << 4;
    }
    fillWord = (fillValue << 24) | (fillValue << 16) | (fillValue << 8) | (fillValue << 0);
    MI_CpuFillFast(window->pixelBuffer, fillWord, window->bgConfig->bgs[window->bgId].tileSize * window->width * window->height);
}

void FillWindowPixelBufferText_AssumeTileSize32(WINDOW *window, u8 fillValue) {
    u32 fillWord;
    fillValue |= fillValue << 4;
    fillWord = (fillValue << 24) | (fillValue << 16) | (fillValue << 8) | (fillValue << 0);
    MI_CpuFillFast(window->pixelBuffer, fillWord, 32 * window->width * window->height);
}

void BlitBitmapRectToWindow(WINDOW *window, void *src, u16 srcX, u16 srcY, u16 srcWidth, u16 srcHeight, u16 destX, u16 destY, u16 destWidth, u16 destHeight) {
    BlitBitmapRect(window, src, srcX, srcY, srcWidth, srcHeight, destX, destY, destWidth, destHeight, 0);
}

void BlitBitmapRect(WINDOW *window, void *src, u16 srcX, u16 srcY, u16 srcWidth, u16 srcHeight, u16 destX, u16 destY, u16 destWidth, u16 destHeight, u16 colorKey) {
    BITMAP bmpSrc, bmpDest;

    bmpSrc.pixels = src;
    bmpSrc.width = srcWidth;
    bmpSrc.height = srcHeight;

    bmpDest.pixels = window->pixelBuffer;
    bmpDest.width = window->width * 8;
    bmpDest.height = window->height * 8;

    if (window->bgConfig->bgs[window->bgId].colorMode == GF_BG_CLR_4BPP) {
        BlitBitmapRect4Bit(&bmpSrc, &bmpDest, srcX, srcY, destX, destY, destWidth, destHeight, colorKey);
    } else {
        BlitBitmapRect8bit(&bmpSrc, &bmpDest, srcX, srcY, destX, destY, destWidth, destHeight, colorKey);
    }
}

void FillWindowPixelRect(WINDOW *window, u8 fillValue, u16 x, u16 y, u16 width, u16 height) {
    BITMAP bmp;

    bmp.pixels = window->pixelBuffer;
    bmp.width = window->width * 8;
    bmp.height = window->height * 8;

    if (window->bgConfig->bgs[window->bgId].colorMode == GF_BG_CLR_4BPP) {
        FillBitmapRect4bit(&bmp, x, y, width, height, fillValue);
    } else {
        FillBitmapRect8bit(&bmp, x, y, width, height, fillValue);
    }
}

#define GLYPH_COPY_4BPP(glyphPixels, srcX, srcY, srcWidth, srcHeight, windowPixels, destX, destY, destWidth, table) { \
    int srcJ, dstJ, srcI, dstI, bits;                                                                                 \
    u8 toOrr;                                                                                                         \
    u8 tableFlag;                                                                                                     \
    u8 tableBit;                                                                                                      \
    u8 *dest;                                                                                                         \
    const u8 *src;                                                                                                    \
    u32 pixelData;                                                                                                    \
                                                                                                                      \
    src = glyphPixels + (srcY / 8 * 64) + (srcX / 8 * 32);                                                            \
    if (srcY == 0) {                                                                                                  \
        dstI = destY + srcY;                                                                                          \
        tableBit = table & 0xFF;                                                                                      \
    } else {                                                                                                          \
        dstI = destY + srcY;                                                                                          \
        for (srcI = 0; srcI < 8; srcI++) {                                                                            \
            if (((table >> srcI) & 1) != 0) {                                                                         \
                dstI++;                                                                                               \
            }                                                                                                         \
        }                                                                                                             \
        tableBit = table >> 8;                                                                                        \
    }                                                                                                                 \
    for (srcI = 0; srcI < srcHeight; srcI++) {                                                                        \
        pixelData = *(u32 *)src;                                                                                      \
        tableFlag = (tableBit >> srcI) & 1;                                                                           \
        for (srcJ = 0, dstJ = destX + srcX; srcJ < srcWidth; srcJ++, dstJ++) {                                        \
            dest = GetPixelAddressFromBlit4bpp(windowPixels, dstJ, dstI, destWidth);                                  \
            toOrr = (pixelData >> (srcJ * 4)) & 0xF;                                                                  \
            if (toOrr != 0) {                                                                                         \
                bits = (dstJ & 1) * 4;                                                                                \
                toOrr = (toOrr << bits) | (*dest & (0xF0 >> bits));                                                   \
                *dest = toOrr;                                                                                        \
                if (tableFlag) {                                                                                      \
                    dest = GetPixelAddressFromBlit4bpp(windowPixels, dstJ, dstI + 1, destWidth);                      \
                    *dest = toOrr;                                                                                    \
                }                                                                                                     \
            }                                                                                                         \
        }                                                                                                             \
        if (tableFlag) {                                                                                              \
            dstI += 2;                                                                                                \
        } else {                                                                                                      \
            dstI += 1;                                                                                                \
        }                                                                                                             \
        src += 4;                                                                                                     \
    }                                                                                                                 \
}

#define GLYPH_COPY_8BPP(glyphPixels, srcX, srcY, srcWidth, srcHeight, windowPixels, destX, destY, destWidth, table) { \
    int srcJ, dstJ, srcI, dstI;                                                                                       \
    u8 toOrr;                                                                                                         \
    u8 tableFlag;                                                                                                     \
    u8 tableBit;                                                                                                      \
    u8 *dest;                                                                                                         \
    const u8 *src;                                                                                                    \
    u8 *pixelData;                                                                                                    \
                                                                                                                      \
    src = glyphPixels + (srcY / 8 * 128) + (srcX / 8 * 64);                                                           \
    if (srcY == 0) {                                                                                                  \
        dstI = destY + srcY;                                                                                          \
        tableBit = table & 0xFF;                                                                                      \
    } else {                                                                                                          \
        dstI = destY + srcY;                                                                                          \
        for (srcI = 0; srcI < 8; srcI++) {                                                                            \
            if (((table >> srcI) & 1) != 0) {                                                                         \
                dstI++;                                                                                               \
            }                                                                                                         \
        }                                                                                                             \
        tableBit = table >> 8;                                                                                        \
    }                                                                                                                 \
    for (srcI = 0; srcI < srcHeight; srcI++) {                                                                        \
        pixelData = (u8 *)src;                                                                                        \
        tableFlag = (tableBit >> srcI) & 1;                                                                           \
        for (srcJ = 0, dstJ = destX + srcX; srcJ < srcWidth; srcJ++, dstJ++) {                                        \
            dest = GetPixelAddressFromBlit8bpp(windowPixels, dstJ, dstI, destWidth);                                  \
            toOrr = pixelData[srcJ];                                                                                  \
            if (toOrr != 0) {                                                                                         \
                *dest = toOrr;                                                                                        \
                if (tableFlag) {                                                                                      \
                    dest = GetPixelAddressFromBlit8bpp(windowPixels, dstJ, dstI + 1, destWidth);                      \
                    *dest = toOrr;                                                                                    \
                }                                                                                                     \
            }                                                                                                         \
        }                                                                                                             \
        if (tableFlag) {                                                                                              \
            dstI += 2;                                                                                                \
        } else {                                                                                                      \
            dstI += 1;                                                                                                \
        }                                                                                                             \
        src += 8;                                                                                                     \
    }                                                                                                                 \
}

void CopyGlyphToWindow(WINDOW *window, u8 *glyphPixels, u16 srcWidth, u16 srcHeight, u16 dstX, u16 dstY, u16 table) {
    u8 *windowPixels;
    u16 destWidth, destHeight;
    int srcRight, srcBottom;
    u8 glyphSizeParam;

    windowPixels = (u8 *)window->pixelBuffer;
    destWidth = (u16)(window->width * 8);
    destHeight = (u16)(window->height * 8);

    // Don't overflow the window
    if (destWidth - dstX < srcWidth) {
        srcRight = destWidth - dstX;
    } else {
        srcRight = srcWidth;
    }
    if (destHeight - dstY < srcHeight) {
        srcBottom = destHeight - dstY;
    } else {
        srcBottom = srcHeight;
    }

    // Get the max glyph dimensions
    // Default: 1x1
    glyphSizeParam = 0;
    if (srcRight > 8) {
        glyphSizeParam |= 1; // 2 wide
    }
    if (srcBottom > 8) {
        glyphSizeParam |= 2; // 2 high
    }

    if (window->colorMode == GF_BG_CLR_4BPP) {
        switch (glyphSizeParam) {
        case 0: // 1x1
            GLYPH_COPY_4BPP(glyphPixels, 0, 0, srcRight, srcBottom, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            return;
        case 1: // 2x1
            GLYPH_COPY_4BPP(glyphPixels, 0, 0, 8, srcBottom, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            GLYPH_COPY_4BPP(glyphPixels, 8, 0, srcRight - 8, srcBottom, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            return;
        case 2: // 1x2
            GLYPH_COPY_4BPP(glyphPixels, 0, 0, srcRight, 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            GLYPH_COPY_4BPP(glyphPixels, 0, 8, srcRight, srcBottom - 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            return;
        case 3: // 2x2
            GLYPH_COPY_4BPP(glyphPixels, 0, 0, 8, 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            GLYPH_COPY_4BPP(glyphPixels, 8, 0, srcRight - 8, 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            GLYPH_COPY_4BPP(glyphPixels, 0, 8, 8, srcBottom - 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            GLYPH_COPY_4BPP(glyphPixels, 8, 8, srcRight - 8, srcBottom - 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            return;
        }
    } else { // 8bpp
        u8 *convertedSrc;
        convertedSrc = Convert4bppTo8bpp(glyphPixels, srcWidth * 4 * srcHeight * 8, window->paletteNum, window->bgConfig->heap_id);
        switch (glyphSizeParam) {
        case 0: // 1x1
            GLYPH_COPY_8BPP(convertedSrc, 0, 0, srcRight, srcBottom, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            break;
        case 1: // 2x1
            GLYPH_COPY_8BPP(convertedSrc, 0, 0, 8, srcBottom, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            GLYPH_COPY_8BPP(convertedSrc, 8, 0, srcRight - 8, srcBottom, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            break;
        case 2: // 1x2
            GLYPH_COPY_8BPP(convertedSrc, 0, 0, srcRight, 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            GLYPH_COPY_8BPP(convertedSrc, 0, 8, srcRight, srcBottom - 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            break;
        case 3: // 2x2
            GLYPH_COPY_8BPP(convertedSrc, 0, 0, 8, 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            GLYPH_COPY_8BPP(convertedSrc, 8, 0, srcRight - 8, 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            GLYPH_COPY_8BPP(convertedSrc, 0, 8, 8, srcBottom - 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            GLYPH_COPY_8BPP(convertedSrc, 8, 8, srcRight - 8, srcBottom - 8, windowPixels, dstX, dstY, ConvertPixelsToTiles(destWidth), table);
            break;
        }
        FreeToHeap(convertedSrc);
    }
}

void ScrollWindow(WINDOW *window, u8 direction, u8 y, u8 fillValue) {
    if (window->bgConfig->bgs[window->bgId].colorMode == GF_BG_CLR_4BPP) {
        ScrollWindow_Text(window, direction, y, fillValue);
    } else {
        ScrollWindow_Affine(window, direction, y, fillValue);
    }
}

static void ScrollWindow_Text(WINDOW *window, u8 direction, u8 y, u8 fillValue) {
    u8 *pixelBuffer;
    int y0, y1, y2;
    int fillWord, size;
    u32 width;
    int i, j;

    pixelBuffer = window->pixelBuffer;
    fillWord = (fillValue << 24) | (fillValue << 16) | (fillValue << 8) | (fillValue << 0);
    size = window->height * window->width * TILE_SIZE_4BPP;
    width = window->width;

    switch (direction) {
    case 0: // up
        for (i = 0; i < size; i += TILE_SIZE_4BPP) {
            y0 = y;
            for (j = 0; j < 8; j++) {
                y1 = i + (j << 2);
                y2 = i + (((width * (y0 & ~7)) | (y0 & 7)) << 2);
                if (y2 < size) {
                    *(u32 *)(pixelBuffer + y1) = *(u32 *)(pixelBuffer + y2);
                } else {
                    *(u32 *)(pixelBuffer + y1) = fillWord;
                }
                y0++;
            }
        }
        break;
    case 1: // down
        pixelBuffer += size - 4;
        for (i = 0; i < size; i += TILE_SIZE_4BPP) {
            y0 = y;
            for (j = 0; j < 8; j++) {
                y1 = i + (j << 2);
                y2 = i + (((width * (y0 & ~7)) | (y0 & 7)) << 2);
                if (y2 < size) {
                    *(u32 *)(pixelBuffer - y1) = *(u32 *)(pixelBuffer - y2);
                } else {
                    *(u32 *)(pixelBuffer - y1) = fillWord;
                }
                y0++;
            }
        }
        break;
    case 2: // left
        break;
    case 3: // right
        break;
    }
}

static void ScrollWindow_Affine(WINDOW *window, u8 direction, u8 y, u8 fillValue) {
    u8 *pixelBuffer;
    int y0, y1, y2;
    int fillWord, size;
    u32 width;
    int i, j;

    pixelBuffer = window->pixelBuffer;
    fillWord = (fillValue << 24) | (fillValue << 16) | (fillValue << 8) | (fillValue << 0);
    size = window->height * window->width * TILE_SIZE_8BPP;
    width = window->width;

    switch (direction) {
    case 0: // up
        for (i = 0; i < size; i += TILE_SIZE_8BPP) {
            y0 = y;
            for (j = 0; j < 8; j++) {
                y1 = i + (j << 3);
                y2 = i + (((width * (y0 & ~7)) | (y0 & 7)) << 3);
                if (y2 < size) {
                    *(u32 *)(pixelBuffer + y1) = *(u32 *)(pixelBuffer + y2);
                } else {
                    *(u32 *)(pixelBuffer + y1) = fillWord;
                }
                y1 += 4;
                y2 += 4;
                if (y2 < size + 4) {
                    *(u32 *)(pixelBuffer + y1) = *(u32 *)(pixelBuffer + y2);
                } else {
                    *(u32 *)(pixelBuffer + y1) = fillWord;
                }
                y0++;
            }
        }
        break;
    case 1: // down
        pixelBuffer += size - 8;
        for (i = 0; i < size; i += TILE_SIZE_8BPP) {
            y0 = y;
            for (j = 0; j < 8; j++) {
                y1 = i + (j << 3);
                y2 = i + (((width * (y0 & ~7)) | (y0 & 7)) << 3);
                if (y2 < size) {
                    *(u32 *)(pixelBuffer - y1) = *(u32 *)(pixelBuffer - y2);
                } else {
                    *(u32 *)(pixelBuffer - y1) = fillWord;
                }
                y1 -= 4;
                y2 -= 4;
                if (y2 < size - 4) {
                    *(u32 *)(pixelBuffer - y1) = *(u32 *)(pixelBuffer - y2);
                } else {
                    *(u32 *)(pixelBuffer - y1) = fillWord;
                }
                y0++;
            }
        }
        break;
    case 2: // left
        break;
    case 3: // right
        break;
    }
}

BGCONFIG *GetWindowBgConfig(WINDOW *window) {
    return window->bgConfig;
}

u8 GetWindowBgId(WINDOW *window) {
    return window->bgId;
}

u8 GetWindowWidth(WINDOW *window) {
    return window->width;
}

u8 GetWindowHeight(WINDOW *window) {
    return window->height;
}

u8 GetWindowX(WINDOW *window) {
    return window->tilemapLeft;
}

u8 GetWindowY(WINDOW *window) {
    return window->tilemapTop;
}

u16 GetWindowBaseTile(WINDOW *window) {
    return window->baseTile;
}

void SetWindowX(WINDOW *window, u8 x) {
    window->tilemapLeft = x;
}

void SetWindowY(WINDOW *window, u8 y) {
    window->tilemapTop = y;
}

void SetWindowPaletteNum(WINDOW *window, u8 paletteNum) {
    window->paletteNum = paletteNum;
}

void BgConfig_HandleScheduledScrollAndTransferOps(BGCONFIG *bgConfig) {
    BgConfig_HandleScheduledScrolls(bgConfig);
    BgConfig_HandleScheduledBufferTransfers(bgConfig);
    bgConfig->scrollScheduled = 0;
    bgConfig->bufferTransferScheduled = 0;
}

static void BgConfig_HandleScheduledBufferTransfers(BGCONFIG *bgConfig) {
    if (bgConfig->bufferTransferScheduled & (1 << GF_BG_LYR_MAIN_0)) {
        CopyTilesToVram(GF_BG_LYR_MAIN_0, bgConfig->bgs[GF_BG_LYR_MAIN_0].tilemapBuffer, bgConfig->bgs[GF_BG_LYR_MAIN_0].baseTile * 2, bgConfig->bgs[GF_BG_LYR_MAIN_0].bufferSize);
    }
    if (bgConfig->bufferTransferScheduled & (1 << GF_BG_LYR_MAIN_1)) {
        CopyTilesToVram(GF_BG_LYR_MAIN_1, bgConfig->bgs[GF_BG_LYR_MAIN_1].tilemapBuffer, bgConfig->bgs[GF_BG_LYR_MAIN_1].baseTile * 2, bgConfig->bgs[GF_BG_LYR_MAIN_1].bufferSize);
    }
    if (bgConfig->bufferTransferScheduled & (1 << GF_BG_LYR_MAIN_2)) {
        CopyTilesToVram(GF_BG_LYR_MAIN_2, bgConfig->bgs[GF_BG_LYR_MAIN_2].tilemapBuffer, bgConfig->bgs[GF_BG_LYR_MAIN_2].baseTile * 2, bgConfig->bgs[GF_BG_LYR_MAIN_2].bufferSize);
    }
    if (bgConfig->bufferTransferScheduled & (1 << GF_BG_LYR_MAIN_3)) {
        CopyTilesToVram(GF_BG_LYR_MAIN_3, bgConfig->bgs[GF_BG_LYR_MAIN_3].tilemapBuffer, bgConfig->bgs[GF_BG_LYR_MAIN_3].baseTile * 2, bgConfig->bgs[GF_BG_LYR_MAIN_3].bufferSize);
    }
    if (bgConfig->bufferTransferScheduled & (1 << GF_BG_LYR_SUB_0)) {
        CopyTilesToVram(GF_BG_LYR_SUB_0, bgConfig->bgs[GF_BG_LYR_SUB_0].tilemapBuffer, bgConfig->bgs[GF_BG_LYR_SUB_0].baseTile * 2, bgConfig->bgs[GF_BG_LYR_SUB_0].bufferSize);
    }
    if (bgConfig->bufferTransferScheduled & (1 << GF_BG_LYR_SUB_1)) {
        CopyTilesToVram(GF_BG_LYR_SUB_1, bgConfig->bgs[GF_BG_LYR_SUB_1].tilemapBuffer, bgConfig->bgs[GF_BG_LYR_SUB_1].baseTile * 2, bgConfig->bgs[GF_BG_LYR_SUB_1].bufferSize);
    }
    if (bgConfig->bufferTransferScheduled & (1 << GF_BG_LYR_SUB_2)) {
        CopyTilesToVram(GF_BG_LYR_SUB_2, bgConfig->bgs[GF_BG_LYR_SUB_2].tilemapBuffer, bgConfig->bgs[GF_BG_LYR_SUB_2].baseTile * 2, bgConfig->bgs[GF_BG_LYR_SUB_2].bufferSize);
    }
    if (bgConfig->bufferTransferScheduled & (1 << GF_BG_LYR_SUB_3)) {
        CopyTilesToVram(GF_BG_LYR_SUB_3, bgConfig->bgs[GF_BG_LYR_SUB_3].tilemapBuffer, bgConfig->bgs[GF_BG_LYR_SUB_3].baseTile * 2, bgConfig->bgs[GF_BG_LYR_SUB_3].bufferSize);
    }
}

void ScheduleBgTilemapBufferTransfer(BGCONFIG *bgConfig, u8 layer) {
    bgConfig->bufferTransferScheduled |= (1 << layer);
}

static void BgConfig_HandleScheduledScrolls(BGCONFIG *bgConfig) {
    if (bgConfig->scrollScheduled & (1 << GF_BG_LYR_MAIN_0)) {
        G2_SetBG0Offset(bgConfig->bgs[GF_BG_LYR_MAIN_0].hOffset, bgConfig->bgs[GF_BG_LYR_MAIN_0].vOffset);
    }
    if (bgConfig->scrollScheduled & (1 << GF_BG_LYR_MAIN_1)) {
        G2_SetBG1Offset(bgConfig->bgs[GF_BG_LYR_MAIN_1].hOffset, bgConfig->bgs[GF_BG_LYR_MAIN_1].vOffset);
    }
    if (bgConfig->scrollScheduled & (1 << GF_BG_LYR_MAIN_2)) {
        if (bgConfig->bgs[GF_BG_LYR_MAIN_2].mode == GF_BG_TYPE_TEXT) {
            G2_SetBG2Offset(bgConfig->bgs[GF_BG_LYR_MAIN_2].hOffset, bgConfig->bgs[GF_BG_LYR_MAIN_2].vOffset);
        } else {
            MtxFx22 mtx;
            MTX22_2DAffine(&mtx, bgConfig->bgs[GF_BG_LYR_MAIN_2].rotation, bgConfig->bgs[GF_BG_LYR_MAIN_2].xScale, bgConfig->bgs[GF_BG_LYR_MAIN_2].yScale, 2);
            G2_SetBG2Affine(&mtx, bgConfig->bgs[GF_BG_LYR_MAIN_2].centerX, bgConfig->bgs[GF_BG_LYR_MAIN_2].centerY, bgConfig->bgs[GF_BG_LYR_MAIN_2].hOffset, bgConfig->bgs[GF_BG_LYR_MAIN_2].vOffset);
        }
    }
    if (bgConfig->scrollScheduled & (1 << GF_BG_LYR_MAIN_3)) {
        if (bgConfig->bgs[GF_BG_LYR_MAIN_3].mode == GF_BG_TYPE_TEXT) {
            G2_SetBG3Offset(bgConfig->bgs[GF_BG_LYR_MAIN_3].hOffset, bgConfig->bgs[GF_BG_LYR_MAIN_3].vOffset);
        } else {
            MtxFx22 mtx;
            MTX22_2DAffine(&mtx, bgConfig->bgs[GF_BG_LYR_MAIN_3].rotation, bgConfig->bgs[GF_BG_LYR_MAIN_3].xScale, bgConfig->bgs[GF_BG_LYR_MAIN_3].yScale, 2);
            G2_SetBG3Affine(&mtx, bgConfig->bgs[GF_BG_LYR_MAIN_3].centerX, bgConfig->bgs[GF_BG_LYR_MAIN_3].centerY, bgConfig->bgs[GF_BG_LYR_MAIN_3].hOffset, bgConfig->bgs[GF_BG_LYR_MAIN_3].vOffset);
        }
    }
    if (bgConfig->scrollScheduled & (1 << GF_BG_LYR_SUB_0)) {
        G2S_SetBG0Offset(bgConfig->bgs[GF_BG_LYR_SUB_0].hOffset, bgConfig->bgs[GF_BG_LYR_SUB_0].vOffset);
    }
    if (bgConfig->scrollScheduled & (1 << GF_BG_LYR_SUB_1)) {
        G2S_SetBG1Offset(bgConfig->bgs[GF_BG_LYR_SUB_1].hOffset, bgConfig->bgs[GF_BG_LYR_SUB_1].vOffset);
    }
    if (bgConfig->scrollScheduled & (1 << GF_BG_LYR_SUB_2)) {
        if (bgConfig->bgs[GF_BG_LYR_SUB_2].mode == GF_BG_TYPE_TEXT) {
            G2S_SetBG2Offset(bgConfig->bgs[GF_BG_LYR_SUB_2].hOffset, bgConfig->bgs[GF_BG_LYR_SUB_2].vOffset);
        } else {
            MtxFx22 mtx;
            MTX22_2DAffine(&mtx, bgConfig->bgs[GF_BG_LYR_SUB_2].rotation, bgConfig->bgs[GF_BG_LYR_SUB_2].xScale, bgConfig->bgs[GF_BG_LYR_SUB_2].yScale, 2);
            G2S_SetBG2Affine(&mtx, bgConfig->bgs[GF_BG_LYR_SUB_2].centerX, bgConfig->bgs[GF_BG_LYR_SUB_2].centerY, bgConfig->bgs[GF_BG_LYR_SUB_2].hOffset, bgConfig->bgs[GF_BG_LYR_SUB_2].vOffset);
        }
    }
    if (bgConfig->scrollScheduled & (1 << GF_BG_LYR_SUB_3)) {
        if (bgConfig->bgs[GF_BG_LYR_SUB_3].mode == GF_BG_TYPE_TEXT) {
            G2S_SetBG3Offset(bgConfig->bgs[GF_BG_LYR_SUB_3].hOffset, bgConfig->bgs[GF_BG_LYR_SUB_3].vOffset);
        } else {
            MtxFx22 mtx;
            MTX22_2DAffine(&mtx, bgConfig->bgs[GF_BG_LYR_SUB_3].rotation, bgConfig->bgs[GF_BG_LYR_SUB_3].xScale, bgConfig->bgs[GF_BG_LYR_SUB_3].yScale, 2);
            G2S_SetBG3Affine(&mtx, bgConfig->bgs[GF_BG_LYR_SUB_3].centerX, bgConfig->bgs[GF_BG_LYR_SUB_3].centerY, bgConfig->bgs[GF_BG_LYR_SUB_3].hOffset, bgConfig->bgs[GF_BG_LYR_SUB_3].vOffset);
        }
    }
}

void ScheduleSetBgPosText(BGCONFIG *bgConfig, u8 layer, enum BgPosAdjustOp op, fx32 value) {
    Bg_SetPosText(&bgConfig->bgs[layer], op, value);
    bgConfig->scrollScheduled |= 1 << layer;
}

void ScheduleSetBgAffineScale(BGCONFIG *bgConfig, u8 layer, enum BgPosAdjustOp op, fx32 value) {
    Bg_SetAffineScale(&bgConfig->bgs[layer], op, value);
    bgConfig->scrollScheduled |= 1 << layer;
}

static void Bg_SetAffineScale(BG *bg, enum BgPosAdjustOp op, fx32 value) {
    switch (op) {
    case BG_POS_OP_SET_XSCALE:
        bg->xScale = value;
        break;
    case BG_POS_OP_ADD_XSCALE:
        bg->xScale += value;
        break;
    case BG_POS_OP_SUB_XSCALE:
        bg->xScale -= value;
        break;
    case BG_POS_OP_SET_YSCALE:
        bg->yScale = value;
        break;
    case BG_POS_OP_ADD_YSCALE:
        bg->yScale += value;
        break;
    case BG_POS_OP_SUB_YSCALE:
        bg->yScale -= value;
        break;
    }
}

BOOL DoesPixelAtScreenXYMatchPtrVal(BGCONFIG *bgConfig, u8 layer, u8 x, u8 y, u16 *src) {
    u8 *bgCharPtr;
    u16 tilemapIdx;
    u8 xPixOffs;
    u8 yPixOffs;
    u8 pixelValue;
    u8 i;
    if (bgConfig->bgs[layer].tilemapBuffer == NULL) {
        return FALSE;
    }

    tilemapIdx = GetTileMapIndexFromCoords(x >> 3, y >> 3, bgConfig->bgs[layer].size, bgConfig->bgs[layer].mode);
    bgCharPtr = BgGetCharPtr(layer);
    xPixOffs = x & 7;
    yPixOffs = y & 7;
    if (bgConfig->bgs[layer].colorMode == GX_BG_COLORMODE_16) {
        u16 *tilemapBuffer;
        u8 *tile;

        tilemapBuffer = bgConfig->bgs[layer].tilemapBuffer;
        tile = AllocFromHeapAtEnd(bgConfig->heap_id, 0x40);
        bgCharPtr += (tilemapBuffer[tilemapIdx] & 0x3FF) * TILE_SIZE_4BPP;
        for (i = 0; i < TILE_SIZE_4BPP; i++) {
            tile[2 * i]     = bgCharPtr[i] & 0xF;
            tile[2 * i + 1] = bgCharPtr[i] >> 4;
        }
        ApplyFlipFlagsToTile(bgConfig, (tilemapBuffer[tilemapIdx] >> 10) & 3, tile);
        pixelValue = tile[xPixOffs + 8 * yPixOffs];
        FreeToHeap(tile);
        if (((*src) & (1 << pixelValue)) != 0) {
            return 1;
        }
    } else {
        if (bgConfig->bgs[layer].mode != GF_BG_TYPE_AFFINE) {
            u16 *tilemapBuffer;
            u8 *tile;
            tilemapBuffer = bgConfig->bgs[layer].tilemapBuffer;
            tile = AllocFromHeapAtEnd(bgConfig->heap_id, 0x40);
            memcpy(tile, bgCharPtr + (tilemapBuffer[tilemapIdx] & 0x3FF) * TILE_SIZE_8BPP, TILE_SIZE_8BPP);
            ApplyFlipFlagsToTile(bgConfig, (tilemapBuffer[tilemapIdx] >> 10) & 3, tile);
            pixelValue = tile[xPixOffs + 8 * yPixOffs];
            FreeToHeap(tile);
        } else {
            pixelValue = bgCharPtr[((u8 *)bgConfig->bgs[layer].tilemapBuffer)[tilemapIdx] * TILE_SIZE_8BPP + xPixOffs + 8 * yPixOffs];
        }
        // BUG: Infinite loop
        while (1) {
            if (*src == 0xFFFF) {
                break;
            }
            if (pixelValue == (u8)*src) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static void ApplyFlipFlagsToTile(BGCONFIG *bgConfig, u8 flags, u8 *tile) {
    u8 i, j;
    u8 *buffer;
    if (flags != 0) {
        buffer = AllocFromHeapAtEnd(bgConfig->heap_id, 0x40);
        if ((flags & 1) != 0) { // hflip
            for (i = 0; i < 8; i++) {
                for (j = 0; j < 8; j++) {
                    buffer[i * 8 + j] = tile[i * 8 + (7 - j)];
                }
            }
            memcpy(tile, buffer, 0x40);
        }
        if ((flags & 2) != 0) { // vflip
            for (i = 0; i < 8; i++) {
                memcpy(&buffer[i * 8], &tile[(7 - i) * 8], 8);
            }
            memcpy(tile, buffer, 0x40);
        }
        FreeToHeap(buffer);
    }
}
