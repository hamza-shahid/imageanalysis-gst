#include "gdiplus_c.h"

#include <stdio.h>
#include <string>

using namespace Gdiplus;


struct gdiplus_c
{
	ULONG_PTR gdiplusToken;
	GdiplusStartupInput gdiInput;

	Bitmap* pBitmap;
	Graphics* pGraphics;
	FontFamily* pFontFamily;
	Font* pFont;
	SolidBrush* pBrush;
	int fontSize;
};


static void gdiplus_cleanup_context(gdiplus_c* pGdiObj)
{
	if (pGdiObj->pBitmap)
		delete pGdiObj->pBitmap;

	if (pGdiObj->pGraphics)
		delete pGdiObj->pGraphics;
}

static void gdiplus_cleanup_font(gdiplus_c* pGdiObj)
{
	if (pGdiObj->pFontFamily)
		delete pGdiObj->pFontFamily;

	if (pGdiObj->pFont)
		delete pGdiObj->pFont;

	if (pGdiObj->pBrush)
		delete pGdiObj->pBrush;
}

gdiplus_c* gdiplus_startup()
{
	gdiplus_c* gdiObj = new gdiplus_c;

	memset(gdiObj, 0, sizeof(gdiplus_c));

	GdiplusStartup(&gdiObj->gdiplusToken, &gdiObj->gdiInput, NULL);

	return gdiObj;
}

void gdiplus_shutdown(gdiplus_c* pGdiObj)
{
	if (pGdiObj)
	{
		gdiplus_cleanup_font(pGdiObj);
		gdiplus_cleanup_context(pGdiObj);

		if (pGdiObj)
			GdiplusShutdown(pGdiObj->gdiplusToken);
	}
}

void gdiplus_setfont(gdiplus_c* pGdiObj, const wchar_t* fontFamily, int fontSize, color_c fontColor)
{
	if (!pGdiObj)
		return;

	gdiplus_cleanup_font(pGdiObj);

	pGdiObj->fontSize = fontSize;

	pGdiObj->pFontFamily = new FontFamily(fontFamily);
	pGdiObj->pFont = new Font(pGdiObj->pFontFamily, pGdiObj->fontSize, FontStyleRegular, UnitPoint);
	pGdiObj->pBrush = new SolidBrush(Color(fontColor.r, fontColor.g, fontColor.b));
}

void gdiplus_init_context(gdiplus_c* pGdiObj, unsigned char* pImage, int width, int height, int stride)
{
	gdiplus_cleanup_context(pGdiObj);

	pGdiObj->pBitmap = new Bitmap(width, height, stride, PixelFormat32bppRGB, pImage);
	pGdiObj->pGraphics = new Graphics(pGdiObj->pBitmap);
}

void gdiplus_draw_rgb(gdiplus_c* gdiObj, int r, int g, int b, int x, int y)
{
	std::wstring R = std::to_wstring(r);
	std::wstring G = std::to_wstring(g);
	std::wstring B = std::to_wstring(g);

	int offsetY = gdiObj->fontSize  + 5;
	
	gdiObj->pGraphics->DrawString(B.c_str(), -1, gdiObj->pFont, PointF(x, y - 1 * offsetY), gdiObj->pBrush);
	gdiObj->pGraphics->DrawString(G.c_str(), -1, gdiObj->pFont, PointF(x, y - 2 * offsetY), gdiObj->pBrush);
	gdiObj->pGraphics->DrawString(R.c_str(), -1, gdiObj->pFont, PointF(x, y - 3 * offsetY), gdiObj->pBrush);
}
