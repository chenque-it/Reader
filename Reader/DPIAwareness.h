#ifndef __DPI_AWARENESS_H__
#define __DPI_AWARENESS_H__

#ifndef WM_DPICHANGED
#define WM_DPICHANGED   0x02E0
#endif

int GetCxScreenForDpi(void);
int GetCyScreenForDpi(void);

int GetWidthForDpi(int w);
int GetHeightForDpi(int h);

void UpdateRectForDpi(RECT *rect);
void UpdateFontForDpi(LOGFONT *lf);

void RestoreRectForDpi(RECT *rect);
void RestoreFontForDpi(LOGFONT *lf);

void DpiChanged(HWND hWnd, LOGFONT *lf, RECT *rect, WPARAM newdpi, RECT *newRect);

void SetDpiGlobal(WPARAM newdpi);

int GetDpiY(void);

double GetDpiScaled(void);

INT_PTR ScaledDialogBoxParam(HINSTANCE hInstance, LPCTSTR lpTemplateName, HWND hWndParent, DLGPROC lpDialogFunc);


#endif
