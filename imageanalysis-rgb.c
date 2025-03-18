#include <limits.h>
#include <gst/gst.h>
#include <stdio.h>

#include "imageanalysis-rgb.h"


#define ROW(pImage, width, y) &pImage[width * sizeof(RGBQUAD) * y]
#define ROWCOL(pImage, width, x, y) &pImage[(width * sizeof(RGBQUAD) * y) + (x*sizeof(RGBQUAD))]

#define RGB_BLACK ((RGBQUAD){0, 0, 0, 0})
#define RGB_WHITE ((RGBQUAD){255, 255, 255, 0})
#define RGB_RED ((RGBQUAD){0, 0, 255, 0})
#define RGB_GREEN ((RGBQUAD){0, 255, 0, 0})
#define RGB_BLUE ((RGBQUAD){255, 0, 0, 0})

    
static inline void AdjustMinMax(INTRGBTRIPLE newMin, INTRGBTRIPLE newMax, INTRGBTRIPLE* min, INTRGBTRIPLE* max)
{
    if (newMax.red > max->red) max->red = newMax.red;
    if (newMin.red < min->red) min->red = newMin.red;

    if (newMax.green > max->green) max->green = newMax.green;
    if (newMin.green < min->green) min->green = newMin.green;

    if (newMax.blue > max->blue) max->blue = newMax.blue;
    if (newMin.blue < min->blue) min->blue = newMin.blue;
}
    
static void ComputeMinMax(INTRGBTRIPLE* pValues, int iNumValues, INTRGBTRIPLE* min, INTRGBTRIPLE* max)
{
    min->red = min->green = min->blue = INT_MAX;
    max->red = max->green = max->blue = 0;

    for (int i = 0; i < iNumValues; i++)
        AdjustMinMax(pValues[i], pValues[i], min, max);
}

static void NormalizeRGB(INTRGBTRIPLE* iValue, int iOrigRange, int iMinOrig, int iNewRange, int iMinNew)
{
    iValue->red = (int) NormalizeValue(iValue->red, iOrigRange, iMinOrig, iNewRange, iMinNew);
    iValue->green = (int)NormalizeValue(iValue->green, iOrigRange, iMinOrig, iNewRange, iMinNew);
    iValue->blue = (int)NormalizeValue(iValue->blue, iOrigRange, iMinOrig, iNewRange, iMinNew);
}

static void Normalize(ImageAnalysisRGB* pImageAnalysisRgb, int iOrigMin, int iOrigMax, int iRangeMin, int iRangeMax)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);
    //int iNewRange = iRangeMax - iRangeMin;
    int iNewRange = iRangeMin - iRangeMax;

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        for (int j = 0; j < pImageAnalysisRgb->piNumResults[i]; j++)
            NormalizeRGB(&pImageAnalysisRgb->ppResults[i][j], iOrigMax - iOrigMin, iOrigMin, iNewRange, iRangeMax);
    }
}

static void ScaleGraph(const INTRGBTRIPLE* input, int inSize, INTRGBTRIPLE* output, int outSize)
{
    float fScaleFactor = (float)inSize / outSize;

    for (int i = 0; i < outSize; i++)
    {
        float fPos = i * fScaleFactor;
        int idx = (int)fPos;
        float fFraction = fPos - idx;

        // Handle boundary conditions
        int next_idx = (idx < inSize - 1) ? idx + 1 : idx;

        // Linear interpolation
        output[i].red = (int)((1 - fFraction) * input[idx].red + fFraction * input[next_idx].red);
        output[i].green = (int)((1 - fFraction) * input[idx].green + fFraction * input[next_idx].green);
        output[i].blue = (int)((1 - fFraction) * input[idx].blue + fFraction * input[next_idx].blue);
    }
}

static void Blackout(ImageAnalysisRGB* pImageAnalysisRgb, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);

    if (pImageAnalysis->opts.blackoutType == BLACK_ALL)
    {
        int iNumPixels = pImageAnalysis->iImageWidth * pImageAnalysis->iImageHeight;

        for (int i = 0; i < iNumPixels; i++)
            ((RGBQUAD*)pImage)[i] = RGB_BLACK;
    }
    else if (pImageAnalysis->opts.blackoutType == BLACK_AOI)
    {
        int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
        int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;

        for (int y = iAoiMinY; y < iAoiMaxY; y++)
        {
            RGBQUAD* pRGB = (RGBQUAD*)ROW(pImage, pImageAnalysis->iImageWidth, y);

            for (int x = 0; x < pImageAnalysis->iImageWidth; x++)
                pRGB[x] = RGB_BLACK;
        }
    }
}

static void DrawAOI(ImageAnalysisRGB* pImageAnalysisRgb, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);
    int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
    int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;
    RGBQUAD* pRgb;
    RGBQUAD aoiColor;

    if (pImageAnalysis->opts.blackoutType == BLACK_NONE)
    {
        aoiColor = RGB_BLACK;
    }
    else
    {
        Blackout(pImageAnalysisRgb, pImage);
        aoiColor = RGB_WHITE;
    }

    // Draw the bottom line of our area of interest
    pRgb = (RGBQUAD*)ROW(pImage, pImageAnalysis->iImageWidth, iAoiMinY);

    for (int x = 0; x < pImageAnalysis->iImageWidth; x++)
        pRgb[x] = aoiColor;

    // Draw the top line of our area of interest
    pRgb = (RGBQUAD*)ROW(pImage, pImageAnalysis->iImageWidth, iAoiMaxY);

    for (int x = 0; x < pImageAnalysis->iImageWidth; x++)
        pRgb[x] = aoiColor;

    // draw the partition lines
    for (guint i = 1; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        int x = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);

        for (int y = iAoiMinY; y < iAoiMaxY; y++)
            ((RGBQUAD*)ROW(pImage, pImageAnalysis->iImageWidth, y))[x] = aoiColor;
    }
}

static void DrawLine(ImageAnalysisRGB* pImageAnalysisRgb, guint8* pImage, int x0, int y0, int x1, int y1, RGBQUAD color)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1)
    {
        // Set the current pixel to red
        RGBQUAD* pixel = (RGBQUAD*)(ROWCOL(pImage, pImageAnalysis->iImageWidth, x0, y0));
        *pixel = color;

        // Check if we've reached the end point
        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static void PlotValues(ImageAnalysisRGB* pImageAnalysisRgb, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        int xStart = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);

        for (int j = 0; j < pImageAnalysisRgb->piNumResults[i]; j++)
        {
            ((RGBQUAD*)ROW((pImage), pImageAnalysis->iImageWidth, pImageAnalysisRgb->ppResults[i][j].red))[j + xStart] = RGB_RED;
            ((RGBQUAD*)ROW((pImage), pImageAnalysis->iImageWidth, pImageAnalysisRgb->ppResults[i][j].green))[j + xStart] = RGB_GREEN;
            ((RGBQUAD*)ROW((pImage), pImageAnalysis->iImageWidth, pImageAnalysisRgb->ppResults[i][j].blue))[j + xStart] = RGB_BLUE;

            if (pImageAnalysis->opts.connectValues && j > 0)
            {
                DrawLine(pImageAnalysisRgb, pImage + xStart * sizeof(RGBQUAD), j - 1, pImageAnalysisRgb->ppResults[i][j - 1].red, j, pImageAnalysisRgb->ppResults[i][j].red, RGB_RED);
                DrawLine(pImageAnalysisRgb, pImage + xStart * sizeof(RGBQUAD), j - 1, pImageAnalysisRgb->ppResults[i][j - 1].green, j, pImageAnalysisRgb->ppResults[i][j].green, RGB_GREEN);
                DrawLine(pImageAnalysisRgb, pImage + xStart * sizeof(RGBQUAD), j - 1, pImageAnalysisRgb->ppResults[i][j - 1].blue, j, pImageAnalysisRgb->ppResults[i][j].blue, RGB_BLUE);
            }
        }
    }
}

static void CheckAllocatedMemory(ImageAnalysisRGB* pImageAnalysisRgb)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);

    if (pImageAnalysis->iPrevPartitions != pImageAnalysis->opts.aoiPartitions)
    {
        if (pImageAnalysisRgb->ppResults)
        {
            for (int i = 0; i < pImageAnalysis->iPrevPartitions; i++)
                free(pImageAnalysisRgb->ppResults[i]);

            free(pImageAnalysisRgb->ppResults);
            free(pImageAnalysisRgb->piNumResults);
        }

        pImageAnalysisRgb->ppResults = calloc(pImageAnalysis->opts.aoiPartitions, sizeof(INTRGBTRIPLE));
        pImageAnalysisRgb->piNumResults = calloc(pImageAnalysis->opts.aoiPartitions, sizeof(int));

        for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
        {
            int xStart = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);
            int xEnd = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * (i + 1));

            pImageAnalysisRgb->piNumResults[i] = xEnd - xStart;
            pImageAnalysisRgb->ppResults[i] = calloc(pImageAnalysisRgb->piNumResults[i], sizeof(INTRGBTRIPLE));
        }
    }

    pImageAnalysis->iPrevPartitions = pImageAnalysis->opts.aoiPartitions;
}

static void ComputeIntensityActual(ImageAnalysisRGB* pImageAnalysisRgb, guint8* pImage, int iAoiMinY, int iAoiMaxY)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);
    RGBQUAD* pRGB = NULL;

    for (int y = iAoiMinY; y < iAoiMaxY; y++)
    {
        pRGB = (RGBQUAD*)ROW(pImage, pImageAnalysis->iImageWidth, y);

        for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
        {
            int xStart = (int) ((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);

            for (int j = 0; j < pImageAnalysisRgb->piNumResults[i]; j++)
            {
                pImageAnalysisRgb->ppResults[i][j].red += pRGB[j + xStart].rgbRed;
                pImageAnalysisRgb->ppResults[i][j].green += pRGB[j + xStart].rgbGreen;
                pImageAnalysisRgb->ppResults[i][j].blue += pRGB[j + xStart].rgbBlue;
            }
        }
    }
}

static void ComputeIntensity(ImageAnalysisRGB* pImageAnalysisRgb, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);
    int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
    int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;

    CheckAllocatedMemory(pImageAnalysisRgb);
    ComputeIntensityActual(pImageAnalysisRgb, pImage, iAoiMinY, iAoiMaxY);
    DrawAOI(pImageAnalysisRgb, pImage);
    Normalize(pImageAnalysisRgb, 0, UCHAR_MAX * pImageAnalysis->opts.aoiHeight, iAoiMinY, iAoiMaxY);
    PlotValues(pImageAnalysisRgb, pImage);
}

void ComputeAverageActual(ImageAnalysisRGB* pImageAnalysisRgb, guint8* pImage, int iAoiMinY, int iAoiMaxY)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);
    int iNumValues = iAoiMaxY - iAoiMinY;

    ComputeIntensityActual(pImageAnalysisRgb, pImage, iAoiMinY, iAoiMaxY);

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        for (int j = 0; j < pImageAnalysisRgb->piNumResults[i]; j++)
        {
            pImageAnalysisRgb->ppResults[i][j].red /= iNumValues;
            pImageAnalysisRgb->ppResults[i][j].green /= iNumValues;
            pImageAnalysisRgb->ppResults[i][j].blue /= iNumValues;
        }
    }
}

void ComputeAverage(ImageAnalysisRGB* pImageAnalysisRgb, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);
    int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
    int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;

    CheckAllocatedMemory(pImageAnalysisRgb);
    ComputeAverageActual(pImageAnalysisRgb, pImage, iAoiMinY, iAoiMaxY);
    DrawAOI(pImageAnalysisRgb, pImage);
    Normalize(pImageAnalysisRgb, 0, UCHAR_MAX, iAoiMinY, iAoiMaxY);
    PlotValues(pImageAnalysisRgb, pImage);
}

void ComputeHistogram(ImageAnalysisRGB* pImageAnalysisRgb, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);
    RGBQUAD* pRGB = NULL;
    int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
    int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;

    CheckAllocatedMemory(pImageAnalysisRgb);

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        int xStart = (int) ((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);
        int xEnd = (int) ((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * (i + 1));

        memset(pImageAnalysisRgb->piHistogram, 0, (UCHAR_MAX+1) * sizeof(INTRGBTRIPLE));

        for (int y = iAoiMinY; y < iAoiMaxY; y++)
        {
            pRGB = (RGBQUAD*)ROW(pImage, pImageAnalysis->iImageWidth, y);

            for (int x = xStart; x < xEnd; x++)
            {
                pImageAnalysisRgb->piHistogram[pRGB[x].rgbRed].red += 1;
                pImageAnalysisRgb->piHistogram[pRGB[x].rgbGreen].green += 1;
                pImageAnalysisRgb->piHistogram[pRGB[x].rgbBlue].blue += 1;
            }
        }

        INTRGBTRIPLE min, max;
        ComputeMinMax(pImageAnalysisRgb->piHistogram, UCHAR_MAX + 1, &min, &max);

        // normalize
        for (int j = 0; j < (UCHAR_MAX + 1); j++)
        {
            pImageAnalysisRgb->piHistogram[j].red    = (int)NormalizeValue(pImageAnalysisRgb->piHistogram[j].red, max.red - min.red, min.red, iAoiMinY - iAoiMaxY, iAoiMaxY);
            pImageAnalysisRgb->piHistogram[j].green  = (int)NormalizeValue(pImageAnalysisRgb->piHistogram[j].green, max.green - min.green, min.green, iAoiMinY - iAoiMaxY, iAoiMaxY);
            pImageAnalysisRgb->piHistogram[j].blue   = (int)NormalizeValue(pImageAnalysisRgb->piHistogram[j].blue, max.blue - min.blue, min.blue, iAoiMinY - iAoiMaxY, iAoiMaxY);
        }

        ScaleGraph(pImageAnalysisRgb->piHistogram, UCHAR_MAX + 1, pImageAnalysisRgb->ppResults[i], pImageAnalysisRgb->piNumResults[i]);
    }

    DrawAOI(pImageAnalysisRgb, pImage);
    PlotValues(pImageAnalysisRgb, pImage);
}

static void ComputePartitionTotal(ImageAnalysis* pImageAnalysis, guint8* pImage, PrintPartition* pPartition)
{
    int nStartX = pPartition->centerX - pPartition->width / 2;
    int nEndX = nStartX + pPartition->width;
    int nStartY = pPartition->centerY - pPartition->height / 2;
    int nEndY = nStartY + pPartition->height;
	gint pW = pPartition->width, pH = pPartition->height;
	double bg_r = pPartition->bg.rgb.r*pH, bg_g = pPartition->bg.rgb.g*pH, bg_b = pPartition->bg.rgb.b*pH;
	double bg_k = min(bg_r, min(bg_g, bg_b));
    double satR, satG, satB, satK;
    double satRmin, satGmin, satBmin, satKmin;
    double satRmax, satGmax, satBmax, satKmax;
    double satRavg, satGavg, satBavg, satKavg;
    double satRtot, satGtot, satBtot, satKtot;
    pPartition->total = (Pixel) { 0, 0, 0 };
    pPartition->nonUniformity = (Pixel){ 0, 0, 0 };

    if (pPartition->colTotal)
        free(pPartition->colTotal);

    pPartition->colTotal = calloc(pPartition->width, sizeof(Pixel));

    for (int y = nStartY; y < nEndY; y++)
    {
        RGBQUAD* pRGB = (RGBQUAD*)ROW(pImage, pImageAnalysis->iImageWidth, y);

        for (int x = nStartX; x < nEndX; x++)
        {
            pPartition->colTotal[x - nStartX].rgb.r += pRGB[x].rgbRed;
            pPartition->colTotal[x - nStartX].rgb.g += pRGB[x].rgbGreen;
            pPartition->colTotal[x - nStartX].rgb.b += pRGB[x].rgbBlue;
            
            pPartition->total.rgb.r += pRGB[x].rgbRed;
            pPartition->total.rgb.g += pRGB[x].rgbGreen;
            pPartition->total.rgb.b += pRGB[x].rgbBlue;
        }
    }

    pPartition->avg.rgb.r = pPartition->total.rgb.r / pPartition->width;
    pPartition->avg.rgb.g = pPartition->total.rgb.g / pPartition->width;
    pPartition->avg.rgb.b = pPartition->total.rgb.b / pPartition->width;

    pPartition->min.rgb.r = pPartition->min.rgb.g = pPartition->min.rgb.b = INT_MAX;
    pPartition->max.rgb.r = pPartition->max.rgb.g = pPartition->max.rgb.b = 0;

	satRmin = satGmin = satBmin = satKmin = DBL_MAX;
	satRmax = satGmax = satBmax = satKmax = 0.0;
	satRtot = satGtot = satBtot = satKtot = 0.0;

    for (int i = 0; i < pPartition->width; i++)
    {
		Pixel* pCol = &pPartition->colTotal[i];
		gint R = pCol->rgb.r / pH, G = pCol->rgb.g / pH, B = pCol->rgb.b / pH;
		gint Rbg = bg_r / pH, Gbg = bg_g / pH, Bbg = bg_b / pH;
		satR = 1.0 - ((double)pCol->rgb.r / bg_r); 
		satG = 1.0 - ((double)pCol->rgb.g / bg_g);
		satB = 1.0 - ((double)pCol->rgb.b / bg_b);
		pCol->rgb.k = min(pCol->rgb.r, min(pCol->rgb.g, pCol->rgb.b));
        satK = 1.0 - ((double)pCol->rgb.k / bg_k);

        satR = max(satR, 0.0); satG = max(satG, 0.0);
        satB = max(satB, 0.0); satK = max(satK, 0.0);
		satRmin = min(satR, satRmin); satGmin = min(satG, satGmin); 
        satBmin = min(satB, satBmin); satKmin = min(satK, satKmin);
		satRmax = max(satR, satRmax); satGmax = max(satG, satGmax);
		satBmax = max(satB, satBmax); satKmax = max(satK, satKmax);
        satRtot += satR; satGtot += satG; satBtot += satB; satKtot += satK;

        pPartition->min.rgb.r = pPartition->colTotal[i].rgb.r < pPartition->min.rgb.r ? pPartition->colTotal[i].rgb.r : pPartition->min.rgb.r;
        pPartition->min.rgb.g = pPartition->colTotal[i].rgb.g < pPartition->min.rgb.g ? pPartition->colTotal[i].rgb.g : pPartition->min.rgb.g;
        pPartition->min.rgb.b = pPartition->colTotal[i].rgb.b < pPartition->min.rgb.b ? pPartition->colTotal[i].rgb.b : pPartition->min.rgb.b;

        pPartition->max.rgb.r = pPartition->colTotal[i].rgb.r > pPartition->max.rgb.r ? pPartition->colTotal[i].rgb.r : pPartition->max.rgb.r;
        pPartition->max.rgb.g = pPartition->colTotal[i].rgb.g > pPartition->max.rgb.g ? pPartition->colTotal[i].rgb.g : pPartition->max.rgb.g;
        pPartition->max.rgb.b = pPartition->colTotal[i].rgb.b > pPartition->max.rgb.b ? pPartition->colTotal[i].rgb.b : pPartition->max.rgb.b;

        pPartition->nonUniformity.rgb.r += abs(pPartition->colTotal[i].rgb.r - pPartition->avg.rgb.r);
        pPartition->nonUniformity.rgb.g += abs(pPartition->colTotal[i].rgb.g - pPartition->avg.rgb.g);
        pPartition->nonUniformity.rgb.b += abs(pPartition->colTotal[i].rgb.b - pPartition->avg.rgb.b);
    }
    satRavg = satRtot / pW;
	satGavg = satGtot / pW;
	satBavg = satBtot / pW;
	satKavg = satKtot / pW;
	pPartition->minSat.rgb.r = (gint)(satRmin*1000.0);
	pPartition->minSat.rgb.g = (gint)(satGmin*1000.0);
	pPartition->minSat.rgb.b = (gint)(satBmin*1000.0);
    pPartition->minSat.rgb.k = (gint)(satKmin*1000.0);
    pPartition->maxSat.rgb.r = (gint)(satRmax*1000.0);
    pPartition->maxSat.rgb.g = (gint)(satGmax*1000.0);
    pPartition->maxSat.rgb.b = (gint)(satBmax*1000.0);
    pPartition->maxSat.rgb.k = (gint)(satKmax*1000.0);
    pPartition->avgSat.rgb.r = (gint)(satRavg*1000.0);
    pPartition->avgSat.rgb.g = (gint)(satGavg*1000.0);
    pPartition->avgSat.rgb.b = (gint)(satBavg*1000.0);
    pPartition->avgSat.rgb.k = (gint)(satKavg*1000.0);
	pPartition->total.rgb.r /= (pPartition->width * pPartition->height);
	pPartition->total.rgb.g /= (pPartition->width * pPartition->height);
	pPartition->total.rgb.b /= (pPartition->width * pPartition->height);
	pPartition->nonUniformity.rgb.r /= pPartition->width;
	pPartition->nonUniformity.rgb.g /= pPartition->width;
	pPartition->nonUniformity.rgb.b /= pPartition->width;
}

static void DrawPartition(ImageAnalysis* pImageAnalysis, guint8* pImage, PrintPartition* pPartition)
{
    int x0 = pPartition->centerX - pPartition->width / 2;
    int x1 = x0 + pPartition->width;
    int y0 = pPartition->centerY - pPartition->height / 2;
    int y1 = y0 + pPartition->height;

    RGBQUAD* pRgb0 = (RGBQUAD*)ROW(pImage, pImageAnalysis->iImageWidth, y0);
    RGBQUAD* pRgb1 = (RGBQUAD*)ROW(pImage, pImageAnalysis->iImageWidth, y1);

    // draw the horizontal lines
    for (int x = x0; x < x1; x++)
        pRgb0[x] = pRgb1[x] = RGB_BLACK;

    // draw the vertical lines
    for (int y = y0; y < y1; y++)
    {
        RGBQUAD* pRGB = (RGBQUAD*)ROW(pImage, pImageAnalysis->iImageWidth, y);
        pRGB[x0] = pRGB[x1] = RGB_BLACK;
    }

    gdiplus_draw_rgb(pImageAnalysis->pGdiObj, pPartition->total.rgb.r, pPartition->total.rgb.g, pPartition->total.rgb.b, x0, y0);
}

static void ComputeTotal(ImageAnalysisRGB* pImageAnalysisRgb, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisRgb);

    if (pImageAnalysis->bPartitionsReady)
    {
        for (int i = 0; i < pImageAnalysis->nPartitions; i++)
            ComputePartitionTotal(pImageAnalysis, pImage, &pImageAnalysis->pPartitions[i]);
    }

    gdiplus_init_context(pImageAnalysis->pGdiObj, pImage, pImageAnalysis->iImageWidth, pImageAnalysis->iImageHeight, pImageAnalysis->iStride);
    
    for (int i = 0; i < pImageAnalysis->nPartitions; i++)
        DrawPartition(pImageAnalysis, pImage, &pImageAnalysis->pPartitions[i]);
}

void init_rgb(ImageAnalysis* pImageAnalysis, AnalysisOpts* opts, int iImageWidth, int iImageHeight)
{
    ImageAnalysisRGB* pImageAnalysisRgb = GST_IMAGE_ANALYSIS_RGB(pImageAnalysis);

    pImageAnalysis->opts = *opts;
    pImageAnalysis->iImageWidth = iImageWidth;
    pImageAnalysis->iImageHeight = iImageHeight;
    pImageAnalysis->iPrevPartitions = 0;

    CheckAllocatedMemory(pImageAnalysisRgb);

    pImageAnalysisRgb->piHistogram = calloc(UCHAR_MAX + 1, sizeof(INTRGBTRIPLE));
}

void deinit_rgb(ImageAnalysis* pImageAnalysis)
{
    ImageAnalysisRGB* pImageAnalysisRgb = GST_IMAGE_ANALYSIS_RGB(pImageAnalysis);

    if (pImageAnalysisRgb->piHistogram)
        free(pImageAnalysisRgb->piHistogram);

    if (pImageAnalysisRgb->ppResults)
    {
        for (int i = 0; i < pImageAnalysis->iPrevPartitions; i++)
            free(pImageAnalysisRgb->ppResults[i]);

        free(pImageAnalysisRgb->ppResults);
        free(pImageAnalysisRgb->piNumResults);
    }
}

void analyize_rgb(ImageAnalysis* pImageAnalysis, GstVideoFrame* frame)
{
    ImageAnalysisRGB* pImageAnalysisRgb = GST_IMAGE_ANALYSIS_RGB(pImageAnalysis);
    guint8* pImage = GST_VIDEO_FRAME_PLANE_DATA(frame, 0);

    switch (pImageAnalysis->opts.analysisType)
    {
    case INTENSITY:
        ComputeIntensity(pImageAnalysisRgb, pImage);
        break;

    case MEAN:
        ComputeAverage(pImageAnalysisRgb, pImage);
        break;

    case HISTOGRAM:
        ComputeHistogram(pImageAnalysisRgb, pImage);
        break;

    case TOTAL:
        ComputeTotal(pImageAnalysisRgb, pImage);
        break;

    default:
        break;
    }
}
