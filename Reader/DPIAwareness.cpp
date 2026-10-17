#include "framework.h"
#include "DPIAwareness.h"

#define DEFAULT_DPI             96

static int g_lastDpix = DEFAULT_DPI;
static int g_lastDpiy = DEFAULT_DPI;

static void GetDPI(int *dpix, int *dpiy)
{
    HDC hdc;

    hdc = GetDC(NULL);
    *dpix = GetDeviceCaps(hdc, LOGPIXELSX);
    *dpiy = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
#if 0 // for test
    *dpix = 120;
    *dpiy = 120;
#endif
    g_lastDpix = *dpix;
    g_lastDpiy = *dpiy;
}

int GetCxScreenForDpi(void)
{
    int dpix = 0, dpiy = 0;
    GetDPI(&dpix, &dpiy);
    return MulDiv(GetSystemMetrics(SM_CXSCREEN), DEFAULT_DPI, dpix);
}

int GetCyScreenForDpi(void)
{
    int dpix = 0, dpiy = 0;
    GetDPI(&dpix, &dpiy);
    return MulDiv(GetSystemMetrics(SM_CYSCREEN), DEFAULT_DPI, dpiy);
}

int GetWidthForDpi(int w)
{
    int dpix = 0, dpiy = 0;

    GetDPI(&dpix, &dpiy);

    if (dpix == DEFAULT_DPI && dpiy == DEFAULT_DPI)
    {
        return w;
    }

    return MulDiv(w, dpix, DEFAULT_DPI);
}

int GetHeightForDpi(int h)
{
    int dpix = 0, dpiy = 0;

    GetDPI(&dpix, &dpiy);

    if (dpix == DEFAULT_DPI && dpiy == DEFAULT_DPI)
    {
        return h;
    }

    return MulDiv(h, dpiy, DEFAULT_DPI);
}

void UpdateRectForDpi(RECT *rect)
{
    int dpix = 0, dpiy = 0;
    int scaledx, scaledy, scaledw, scaledh;

    GetDPI(&dpix, &dpiy);

    if (dpix == DEFAULT_DPI && dpiy == DEFAULT_DPI)
    {
        return;
    }

    scaledx = MulDiv(rect->left, dpix, DEFAULT_DPI);
    scaledy = MulDiv(rect->top, dpiy, DEFAULT_DPI);
    scaledw = MulDiv(rect->right - rect->left, dpix, DEFAULT_DPI);
    scaledh = MulDiv(rect->bottom - rect->top, dpiy, DEFAULT_DPI);
    rect->left = scaledx;
    rect->top = scaledy;
    rect->right = scaledx + scaledw;
    rect->bottom = scaledy + scaledh;
}

void UpdateFontForDpi(LOGFONT *lf)
{
    int dpix, dpiy;

    GetDPI(&dpix, &dpiy);

    if (dpix == DEFAULT_DPI && dpiy == DEFAULT_DPI)
    {
        return;
    }

    lf->lfWidth = MulDiv(lf->lfWidth, dpix, DEFAULT_DPI);
    lf->lfHeight = MulDiv(lf->lfHeight, dpiy, DEFAULT_DPI);
}

void RestoreRectForDpi(RECT *rect)
{
    int dpix = 0, dpiy = 0;
    int scaledx, scaledy, scaledw, scaledh;

    GetDPI(&dpix, &dpiy);

    if (dpix == DEFAULT_DPI && dpiy == DEFAULT_DPI)
    {
        return;
    }

    scaledx = MulDiv(rect->left, DEFAULT_DPI, dpix);
    scaledy = MulDiv(rect->top, DEFAULT_DPI, dpiy);
    scaledw = MulDiv(rect->right-rect->left, DEFAULT_DPI, dpix);
    scaledh = MulDiv(rect->bottom-rect->top, DEFAULT_DPI, dpiy);
    rect->left = scaledx;
    rect->top = scaledy;
    rect->right = scaledx + scaledw;
    rect->bottom = scaledy + scaledh;
}

void RestoreFontForDpi(LOGFONT *lf)
{
    int dpix, dpiy;

    GetDPI(&dpix, &dpiy);

    if (dpix == DEFAULT_DPI && dpiy == DEFAULT_DPI)
    {
        return;
    }

    lf->lfWidth = MulDiv(lf->lfWidth, DEFAULT_DPI, dpix);
    lf->lfHeight = MulDiv(lf->lfHeight, DEFAULT_DPI, dpiy);
}

void DpiChanged(HWND hWnd, LOGFONT *lf, RECT *rect, WPARAM newdpi, RECT *newRect)
{
    int newdpiy = HIWORD(newdpi);
    int newdpix = LOWORD(newdpi);

    // set font
    lf->lfWidth = MulDiv(lf->lfWidth, newdpix, g_lastDpix);
    lf->lfHeight = MulDiv(lf->lfHeight, newdpiy, g_lastDpiy);
    g_lastDpix = newdpix;
    g_lastDpiy = newdpiy;

    memcpy(rect, newRect, sizeof(RECT));
    SetWindowPos(hWnd, hWnd, newRect->left, newRect->top, newRect->right-newRect->left, newRect->bottom-newRect->top, SWP_NOZORDER | SWP_NOACTIVATE);
}

double GetDpiScaled(void)
{
    int dpix = 0, dpiy = 0;
    GetDPI(&dpix, &dpiy);
    return (((double)dpix)/((double)DEFAULT_DPI));
}

typedef struct tagDLGTEMPLATEEX_HEADER
{
    WORD dlgVer;
    WORD signature;
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    WORD cDlgItems;
    short x;
    short y;
    short cx;
    short cy;
} DLGTEMPLATEEX_HEADER;

static WORD *SkipSzOrOrd(WORD *p)
{
    if (*p == 0x0000)
        return p + 1;
    if (*p == 0xFFFF)
        return p + 2;
    while (*p)
        p++;
    return p + 1;
}

// Create a DPI-scaled modal dialog:
// - scale template pointsize by current DPI so DLU layout scales accordingly
// - replace DS_FIXEDSYS with DS_SETFONT so the template font really applies
//   (otherwise the font stays fixed at 8pt and never scales)
// fall back to plain DialogBoxParam at 96 DPI / non-EX template / named resource
INT_PTR ScaledDialogBoxParam(HINSTANCE hInstance, LPCTSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc)
{
    HRSRC hrsrc;
    HGLOBAL hglb;
    void *ptmpl, *pcopy;
    DLGTEMPLATEEX_HEADER *ph;
    WORD *pfont;
    int dpix, dpiy, size;
    INT_PTR ret;

    if (!IS_INTRESOURCE(lpTemplateName))
        return DialogBoxParam(hInstance, lpTemplateName, hWndParent, lpDialogFunc, 0);

    hrsrc = FindResource(hInstance, lpTemplateName, RT_DIALOG);
    if (!hrsrc)
        return DialogBoxParam(hInstance, lpTemplateName, hWndParent, lpDialogFunc, 0);

    hglb = LoadResource(hInstance, hrsrc);
    if (!hglb)
        return DialogBoxParam(hInstance, lpTemplateName, hWndParent, lpDialogFunc, 0);

    ptmpl = LockResource(hglb);
    if (!ptmpl)
        return DialogBoxParam(hInstance, lpTemplateName, hWndParent, lpDialogFunc, 0);

    ph = (DLGTEMPLATEEX_HEADER *)ptmpl;
    GetDPI(&dpix, &dpiy);
    if (ph->signature != 0xFFFF || (dpix == DEFAULT_DPI && dpiy == DEFAULT_DPI) ||
        0 == (ph->style & DS_FIXEDSYS))
    {
        return DialogBoxParam(hInstance, lpTemplateName, hWndParent, lpDialogFunc, 0);
    }

    size = SizeofResource(hInstance, hrsrc);
    pcopy = LocalAlloc(LMEM_FIXED, size);
    if (!pcopy)
        return DialogBoxParam(hInstance, lpTemplateName, hWndParent, lpDialogFunc, 0);
    memcpy(pcopy, ptmpl, size);

    // 1) DS_FIXEDSYS(0x0008) -> DS_SETFONT(0x0040)
    ph = (DLGTEMPLATEEX_HEADER *)pcopy;
    ph->style = (ph->style & ~DS_FIXEDSYS) | DS_SETFONT;

    // 2) locate pointsize: skip menu/windowClass/title after the 26-byte header
    //    (header size is even, so following elements are naturally WORD aligned)
    pfont = (WORD *)((BYTE *)pcopy + sizeof(DLGTEMPLATEEX_HEADER));
    pfont = SkipSzOrOrd(pfont);
    pfont = SkipSzOrOrd(pfont);
    pfont = SkipSzOrOrd(pfont);
    pfont = (WORD *)(((UINT_PTR)pfont + 1) & ~(UINT_PTR)1);

    // 3) scale pointsize: 8pt@96DPI -> 10pt@120 / 12pt@144 / 16pt@192
    *pfont = (WORD)MulDiv(*pfont, dpix, DEFAULT_DPI);

    ret = DialogBoxIndirectParam(hInstance, (LPCDLGTEMPLATE)pcopy, hWndParent, lpDialogFunc, 0);
    LocalFree(pcopy);
    return ret;
}
