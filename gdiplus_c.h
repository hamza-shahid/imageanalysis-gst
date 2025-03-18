#pragma once

typedef struct color_c
{
	unsigned char r, g, b;
} color_c;

#ifdef __cplusplus

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>


extern "C" {
#endif

typedef struct gdiplus_c gdiplus_c;


gdiplus_c* gdiplus_startup();
void gdiplus_shutdown(gdiplus_c* gdiObj);

void gdiplus_init_context(gdiplus_c* pGdiObj, unsigned char* pImage, int width, int height, int stride);
void gdiplus_setfont(gdiplus_c* pGdiObj, const wchar_t* fontFamily, int fontSize, color_c fontColor);

void gdiplus_draw_rgb(gdiplus_c* gdiObj, int r, int g, int b, int x, int y);


#ifdef __cplusplus
}
#endif
