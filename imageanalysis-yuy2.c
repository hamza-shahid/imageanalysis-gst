#include "imageanalysis-yuy2.h"


#define ROW(pImage, width, y) &pImage[width * sizeof(YUY2PIXEL) * y]
#define ROWCOL(pImage, width, x, y) &pImage[(width * sizeof(YUY2PIXEL) * y) + (x*sizeof(YUY2PIXEL))]

#define YUY2_BLACK ((YUY2PIXEL){0, 128})
#define YUY2_WHITE ((YUY2PIXEL){255, 128})
#define YUY2_RED_BLUE ((YUY2PIXEL){80, 255})

static inline void AdjustMinMax(INTYUVPIXEL newMin, INTYUVPIXEL newMax, INTYUVPIXEL* min, INTYUVPIXEL* max)
{
    if (newMax.luma > max->luma) max->luma = newMax.luma;
    if (newMin.luma < min->luma) min->luma = newMin.luma;

    if (newMax.Cr > max->Cr) max->Cr = newMax.Cr;
    if (newMin.Cr < min->Cr) min->Cr = newMin.Cr;

    if (newMax.Cb > max->Cb) max->Cb = newMax.Cb;
    if (newMin.Cb < min->Cb) min->Cb = newMin.Cb;
}

static void ComputeMinMax(INTYUVPIXEL* pValues, int iNumValues, INTYUVPIXEL* min, INTYUVPIXEL* max)
{
    min->luma = min->Cr = min->Cb = INT_MAX;
    max->luma = max->Cr = max->Cb = 0;

    for (int i = 0; i < iNumValues; i++)
        AdjustMinMax(pValues[i], pValues[i], min, max);
}

static void NormalizeYUY2(INTYUY2PIXEL* iValue, int iOrigRange, int iMinOrig, int iNewRange, int iMinNew, GrayscaleType eGrayScaleType)
{
    iValue->luma = (int)NormalizeValue(iValue->luma, iOrigRange, iMinOrig, iNewRange, iMinNew);
        
    if (eGrayScaleType == GRAY_NONE)
        iValue->chroma = (int)NormalizeValue(iValue->chroma, iOrigRange, iMinOrig, iNewRange, iMinNew);
}

static void Normalize(ImageAnalysisYUY2* pImageAnalysisYuy2, int iOrigMin, int iOrigMax, int iRangeMin, int iRangeMax)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);
    int iNewRange = iRangeMin - iRangeMax;

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        for (int j = 0; j < pImageAnalysisYuy2->piNumResults[i]; j++)
            NormalizeYUY2(&pImageAnalysisYuy2->ppResults[i][j], iOrigMax - iOrigMin, iOrigMin, iNewRange, iRangeMax, pImageAnalysis->opts.grayscaleType);
    }
}

static void CheckAllocatedMemory(ImageAnalysisYUY2* pImageAnalysisYuy2)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);

    if (pImageAnalysis->iPrevPartitions != pImageAnalysis->opts.aoiPartitions)
    {
        if (pImageAnalysisYuy2->ppResults)
        {
            for (int i = 0; i < pImageAnalysis->iPrevPartitions; i++)
                free(pImageAnalysisYuy2->ppResults[i]);

            free(pImageAnalysisYuy2->ppResults);
            free(pImageAnalysisYuy2->piNumResults);
        }

        pImageAnalysisYuy2->ppResults = calloc(pImageAnalysis->opts.aoiPartitions, sizeof(INTYUY2PIXEL*));
        pImageAnalysisYuy2->piNumResults = calloc(pImageAnalysis->opts.aoiPartitions, sizeof(int));

        for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
        {
            int xStart = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);
            int xEnd = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * (i + 1));

            // make multiple to 2
            xStart = (xStart >> 1) << 1;
            xEnd = (xEnd >> 1) << 1;

            pImageAnalysisYuy2->piNumResults[i] = xEnd - xStart;
            pImageAnalysisYuy2->ppResults[i] = calloc(pImageAnalysisYuy2->piNumResults[i], sizeof(INTYUY2PIXEL));
        }
    }

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
        memset(pImageAnalysisYuy2->ppResults[i], 0, pImageAnalysisYuy2->piNumResults[i] * sizeof(INTYUY2PIXEL));

    pImageAnalysis->iPrevPartitions = pImageAnalysis->opts.aoiPartitions;
}

static void DrawLine(guint8* pImage, int iImageWidth, int x0, int y0, int x1, int y1, YUY2PIXEL color)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 2 : -2;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1)
    {
        // Set the current pixel to red
        YUY2PIXEL* pixel = (YUY2PIXEL*)(ROWCOL(pImage, iImageWidth, x0, y0));
        *pixel = color;

        // Check if we've reached the end point
        if (x0 >= x1 && y0 == y1) break;

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

static void PlotValues(ImageAnalysisYUY2* pImageAnalysisYuy2, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        int xStart = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);
        xStart = (xStart >> 1) << 1;

        for (int j = 0; j < pImageAnalysisYuy2->piNumResults[i]; j++)
        {
            ((YUY2PIXEL*)ROW((pImage), pImageAnalysis->iImageWidth, pImageAnalysisYuy2->ppResults[i][j].luma))[j + xStart] = YUY2_WHITE;

            if (pImageAnalysis->opts.grayscaleType == GRAY_NONE)
                ((YUY2PIXEL*)ROW((pImage), pImageAnalysis->iImageWidth, pImageAnalysisYuy2->ppResults[i][j].chroma))[j + xStart] = YUY2_RED_BLUE;

            if (pImageAnalysis->opts.connectValues)
            {
                if (j > 0)
                    DrawLine(pImage + xStart * sizeof(YUY2PIXEL), pImageAnalysis->iImageWidth, j - 1, pImageAnalysisYuy2->ppResults[i][j - 1].luma, j, pImageAnalysisYuy2->ppResults[i][j].luma, YUY2_WHITE);

                if (j > 1 && pImageAnalysis->opts.grayscaleType == GRAY_NONE)
                    DrawLine(pImage + xStart * sizeof(YUY2PIXEL), pImageAnalysis->iImageWidth, j - 2, pImageAnalysisYuy2->ppResults[i][j - 2].chroma, j, pImageAnalysisYuy2->ppResults[i][j].chroma, YUY2_RED_BLUE);
            }
        }
    }
}

static void PlotValuesYUV(ImageAnalysisYUY2* pImageAnalysisYuy2, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        int xStart = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);
        xStart = (xStart >> 1) << 1;

        for (int j = 0; j < pImageAnalysisYuy2->piNumHistogramResults[i]; j++)
        {
            ((YUY2PIXEL*)ROW((pImage), pImageAnalysis->iImageWidth, pImageAnalysisYuy2->ppHistogram[i][j].luma))[j + xStart] = YUY2_WHITE;

            if (pImageAnalysis->opts.grayscaleType == GRAY_NONE)
            {
                ((YUY2PIXEL*)ROW((pImage), pImageAnalysis->iImageWidth, pImageAnalysisYuy2->ppHistogram[i][j].Cr))[j + xStart] = YUY2_RED_BLUE;
                ((YUY2PIXEL*)ROW((pImage), pImageAnalysis->iImageWidth, pImageAnalysisYuy2->ppHistogram[i][j].Cb))[j + xStart] = YUY2_RED_BLUE;
            }

            if (pImageAnalysis->opts.connectValues && j > 0)
            {
                DrawLine(pImage + xStart * sizeof(YUY2PIXEL), pImageAnalysis->iImageWidth, j - 1, pImageAnalysisYuy2->ppHistogram[i][j - 1].luma, j, pImageAnalysisYuy2->ppHistogram[i][j].luma, YUY2_WHITE);

                if (pImageAnalysis->opts.grayscaleType == GRAY_NONE)
                {
                    DrawLine(pImage + xStart * sizeof(YUY2PIXEL), pImageAnalysis->iImageWidth, j - 1, pImageAnalysisYuy2->ppHistogram[i][j - 1].Cr, j, pImageAnalysisYuy2->ppHistogram[i][j].Cr, YUY2_RED_BLUE);
                    DrawLine(pImage + xStart * sizeof(YUY2PIXEL), pImageAnalysis->iImageWidth, j - 1, pImageAnalysisYuy2->ppHistogram[i][j - 1].Cb, j, pImageAnalysisYuy2->ppHistogram[i][j].Cb, YUY2_RED_BLUE);
                }
            }
        }
    }
}

static void GrayScale(ImageAnalysisYUY2* pImageAnalysisYuy2, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);

    if (pImageAnalysis->opts.grayscaleType == GRAY_ALL)
    {
        int iNumPixels = pImageAnalysis->iImageWidth * pImageAnalysis->iImageHeight;

        for (int i = 0; i < iNumPixels; i++)
            ((YUY2PIXEL*)pImage)[i].chroma = 128;
    }
    else if (pImageAnalysis->opts.grayscaleType == GRAY_AOI)
    {
        int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
        int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;

        for (int y = iAoiMinY; y < iAoiMaxY; y++)
        {
            YUY2PIXEL* pYUV = (YUY2PIXEL*)ROW(pImage, pImageAnalysis->iImageWidth, y);

            for (int x = 0; x < pImageAnalysis->iImageWidth; x++)
                pYUV[x].chroma = 128;
        }
    }
}

static void Blackout(ImageAnalysisYUY2* pImageAnalysisYuy2, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);

    if (pImageAnalysis->opts.blackoutType == BLACK_ALL)
    {
        int iNumPixels = pImageAnalysis->iImageWidth * pImageAnalysis->iImageHeight;

        for (int i = 0; i < iNumPixels; i++)
            ((YUY2PIXEL*)pImage)[i] = YUY2_BLACK;
    }
    else if (pImageAnalysis->opts.blackoutType == BLACK_AOI)
    {
        int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
        int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;

        for (int y = iAoiMinY; y < iAoiMaxY; y++)
        {
            YUY2PIXEL* pYUV = (YUY2PIXEL*)ROW(pImage, pImageAnalysis->iImageWidth, y);

            for (int x = 0; x < pImageAnalysis->iImageWidth; x++)
                pYUV[x] = YUY2_BLACK;
        }
    }
}

static void ScaleGraph(const INTYUVPIXEL* input, int inSize, INTYUVPIXEL* output, int outSize)
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
        output[i].luma = (int)((1 - fFraction) * input[idx].luma + fFraction * input[next_idx].luma);
        output[i].Cr = (int)((1 - fFraction) * input[idx].Cr + fFraction * input[next_idx].Cr);
        output[i].Cb = (int)((1 - fFraction) * input[idx].Cb + fFraction * input[next_idx].Cb);
    }
}

static void DrawAOI(ImageAnalysisYUY2* pImageAnalysisYuy2, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);
    int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
    int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;
    YUY2PIXEL* pYuv;
    YUY2PIXEL aoiColor;

    if (pImageAnalysis->opts.blackoutType == BLACK_NONE)
    {
        aoiColor = YUY2_BLACK;
    }
    else
    {
        Blackout(pImageAnalysisYuy2, pImage);
        aoiColor = YUY2_WHITE;
    }

    // Draw the bottom line of our area of interest
    pYuv = (YUY2PIXEL*)ROW(pImage, pImageAnalysis->iImageWidth, iAoiMinY);

    for (int x = 0; x < pImageAnalysis->iImageWidth; x++)
        pYuv[x] = aoiColor;

    // Draw the top line of our area of interest
    pYuv = (YUY2PIXEL*)ROW(pImage, pImageAnalysis->iImageWidth, iAoiMaxY);

    for (int x = 0; x < pImageAnalysis->iImageWidth; x++)
        pYuv[x] = aoiColor;

    // draw the partition lines
    for (guint i = 1; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        int x = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);
        x = (x >> 1) << 1;

        for (int y = iAoiMinY; y < iAoiMaxY; y++)
            ((YUY2PIXEL*)ROW(pImage, pImageAnalysis->iImageWidth, y))[x] = aoiColor;
    }
}

static void CheckAllocatedMemoryHistogram(ImageAnalysisYUY2* pImageAnalysisYuy2)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);
    
    if (pImageAnalysisYuy2->iPrevHistPartitions != pImageAnalysis->opts.aoiPartitions)
    {
        if (pImageAnalysisYuy2->ppHistogram)
        {
            for (int i = 0; i < pImageAnalysis->iPrevPartitions; i++)
                free(pImageAnalysisYuy2->ppHistogram[i]);

            free(pImageAnalysisYuy2->ppHistogram);
            free(pImageAnalysisYuy2->piNumHistogramResults);
        }

        pImageAnalysisYuy2->ppHistogram = calloc(pImageAnalysis->opts.aoiPartitions, sizeof(INTYUVPIXEL*));
        pImageAnalysisYuy2->piNumHistogramResults = calloc(pImageAnalysis->opts.aoiPartitions, sizeof(int));

        for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
        {
            int xStart = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);
            int xEnd = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * (i + 1));

            // make multiple to 2
            xStart = (xStart >> 1) << 1;
            xEnd = (xEnd >> 1) << 1;

            pImageAnalysisYuy2->piNumHistogramResults[i] = xEnd - xStart;
            pImageAnalysisYuy2->ppHistogram[i] = calloc(pImageAnalysisYuy2->piNumHistogramResults[i], sizeof(INTYUVPIXEL));
        }
    }

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
        memset(pImageAnalysisYuy2->ppHistogram[i], 0, pImageAnalysisYuy2->piNumHistogramResults[i] * sizeof(INTYUVPIXEL));

    pImageAnalysisYuy2->iPrevHistPartitions = pImageAnalysis->opts.aoiPartitions;
}

static void ComputeIntensityActual(ImageAnalysisYUY2* pImageAnalysisYuy2, guint8* pImage, int iAoiMinY, int iAoiMaxY)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);
    YUY2PIXEL* pYUV = NULL;

    for (int y = iAoiMinY; y < iAoiMaxY; y++)
    {
        pYUV = (YUY2PIXEL*)ROW(pImage, pImageAnalysis->iImageWidth, y);

        for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
        {
            int xStart = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);

            for (int j = 0; j < pImageAnalysisYuy2->piNumResults[i]; j++)
            {
                pImageAnalysisYuy2->ppResults[i][j].luma += pYUV[j + xStart].luma;
                pImageAnalysisYuy2->ppResults[i][j].chroma += pYUV[j + xStart].chroma;
            }
        }
    }
}
    
static void ComputeIntensity(ImageAnalysisYUY2* pImageAnalysisYuy2, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);
    int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
    int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;

    CheckAllocatedMemory(pImageAnalysisYuy2);
    ComputeIntensityActual(pImageAnalysisYuy2, pImage, iAoiMinY, iAoiMaxY);
    GrayScale(pImageAnalysisYuy2, pImage);
    DrawAOI(pImageAnalysisYuy2, pImage);
    Normalize(pImageAnalysisYuy2, 0, (UCHAR_MAX) * pImageAnalysis->opts.aoiHeight, iAoiMinY, iAoiMaxY);
    PlotValues(pImageAnalysisYuy2, pImage);
}

static void ComputeAverageActual(ImageAnalysisYUY2* pImageAnalysisYuy2, guint8* pImage, int iAoiMinY, int iAoiMaxY)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);
    int iNumValues = iAoiMaxY - iAoiMinY;

    ComputeIntensityActual(pImageAnalysisYuy2, pImage, iAoiMinY, iAoiMaxY);

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        for (int j = 0; j < pImageAnalysisYuy2->piNumResults[i]; j++)
        {
            pImageAnalysisYuy2->ppResults[i][j].luma      /= iNumValues;
            pImageAnalysisYuy2->ppResults[i][j].chroma    /= iNumValues;
        }
    }
}

static void ComputeAverage(ImageAnalysisYUY2* pImageAnalysisYuy2, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);
    int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
    int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;

    CheckAllocatedMemory(pImageAnalysisYuy2);
    ComputeAverageActual(pImageAnalysisYuy2, pImage, iAoiMinY, iAoiMaxY);
    GrayScale(pImageAnalysisYuy2, pImage);
    DrawAOI(pImageAnalysisYuy2, pImage);
    Normalize(pImageAnalysisYuy2, 0, UCHAR_MAX, iAoiMinY, iAoiMaxY);
    PlotValues(pImageAnalysisYuy2, pImage);
}

static void ComputeHistogram(ImageAnalysisYUY2* pImageAnalysisYuy2, guint8* pImage)
{
    ImageAnalysis* pImageAnalysis = GST_IMAGE_ANALYSIS(pImageAnalysisYuy2);
    YUY2PIXEL* pYUV = NULL;
    int iAoiMinY = (pImageAnalysis->iImageHeight - pImageAnalysis->opts.aoiHeight) / 2;
    int iAoiMaxY = iAoiMinY + pImageAnalysis->opts.aoiHeight;

    CheckAllocatedMemoryHistogram(pImageAnalysisYuy2);

    for (guint i = 0; i < pImageAnalysis->opts.aoiPartitions; i++)
    {
        int xStart = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * i);
        int xEnd = (int)((float)pImageAnalysis->iImageWidth / pImageAnalysis->opts.aoiPartitions * (i + 1));

        // make multiple to 2
        xStart = (xStart >> 1) << 1;
        xEnd = (xEnd >> 1) << 1;

        memset(pImageAnalysisYuy2->piHistogram, 0, (UCHAR_MAX+1) * sizeof(INTYUVPIXEL));

        for (int y = iAoiMinY; y < iAoiMaxY; y++)
        {
            pYUV = (YUY2PIXEL*)ROW(pImage, pImageAnalysis->iImageWidth, y);

            for (int x = xStart; x < xEnd; x += 2)
            {
                pImageAnalysisYuy2->piHistogram[pYUV[x].luma].luma += 1;
                pImageAnalysisYuy2->piHistogram[pYUV[x].chroma].Cr += 1;
            }

            for (int x = xStart + 1; x < xEnd; x += 2)
            {
                pImageAnalysisYuy2->piHistogram[pYUV[x].luma].luma += 1;
                pImageAnalysisYuy2->piHistogram[pYUV[x].chroma].Cb += 1;
            }
        }

        INTYUVPIXEL min, max;
        ComputeMinMax(pImageAnalysisYuy2->piHistogram, UCHAR_MAX+1, &min, &max);

        // normalize
        for (int j = 0; j < UCHAR_MAX+1; j++)
        {
            pImageAnalysisYuy2->piHistogram[j].luma = (int)NormalizeValue(pImageAnalysisYuy2->piHistogram[j].luma, max.luma - min.luma, min.luma, iAoiMinY - iAoiMaxY, iAoiMaxY);
            pImageAnalysisYuy2->piHistogram[j].Cr = (int)NormalizeValue(pImageAnalysisYuy2->piHistogram[j].Cr, max.Cr - min.Cr, min.Cr, iAoiMinY - iAoiMaxY, iAoiMaxY);
            pImageAnalysisYuy2->piHistogram[j].Cb = (int)NormalizeValue(pImageAnalysisYuy2->piHistogram[j].Cb, max.Cb - min.Cb, min.Cb, iAoiMinY - iAoiMaxY, iAoiMaxY);
        }

        ScaleGraph(pImageAnalysisYuy2->piHistogram, UCHAR_MAX+1, pImageAnalysisYuy2->ppHistogram[i], pImageAnalysisYuy2->piNumHistogramResults[i]);
    }

    GrayScale(pImageAnalysisYuy2, pImage);
    DrawAOI(pImageAnalysisYuy2, pImage);
    PlotValuesYUV(pImageAnalysisYuy2, pImage);
}

void init_yuy2(ImageAnalysis* pImageAnalysis, AnalysisOpts* opts, int iImageWidth, int iImageHeight)
{
    ImageAnalysisYUY2* pImageAnalysisYuy2 = GST_IMAGE_ANALYSIS_YUY2(pImageAnalysis);

    pImageAnalysis->opts = *opts;
    pImageAnalysis->iImageWidth = iImageWidth;
    pImageAnalysis->iImageHeight = iImageHeight;
    pImageAnalysis->iPrevPartitions = 0;
    pImageAnalysisYuy2->iPrevHistPartitions = 0;

    CheckAllocatedMemory(pImageAnalysisYuy2);

    pImageAnalysisYuy2->piHistogram = calloc(UCHAR_MAX + 1, sizeof(INTYUVPIXEL));
}

void deinit_yuy2(ImageAnalysis* pImageAnalysis)
{
    ImageAnalysisYUY2* pImageAnalysisYuy2 = GST_IMAGE_ANALYSIS_YUY2(pImageAnalysis);

    if (pImageAnalysisYuy2->piHistogram)
        free(pImageAnalysisYuy2->piHistogram);

    if (pImageAnalysisYuy2->ppResults)
    {
        for (int i = 0; i < pImageAnalysis->iPrevPartitions; i++)
            free(pImageAnalysisYuy2->ppResults[i]);

        free(pImageAnalysisYuy2->ppResults);
        free(pImageAnalysisYuy2->piNumResults);
    }

    if (pImageAnalysisYuy2->ppHistogram)
    {
        for (int i = 0; i < pImageAnalysisYuy2->iPrevHistPartitions; i++)
            free(pImageAnalysisYuy2->ppHistogram[i]);

        free(pImageAnalysisYuy2->ppHistogram);
        free(pImageAnalysisYuy2->piNumHistogramResults);
    }
}

void analyize_yuy2(ImageAnalysis* pImageAnalysis, GstVideoFrame* frame)
{
    ImageAnalysisYUY2* pImageAnalysisYuy2 = GST_IMAGE_ANALYSIS_YUY2(pImageAnalysis);
    guint8* pImage = GST_VIDEO_FRAME_PLANE_DATA(frame, 0);

    switch (pImageAnalysis->opts.analysisType)
    {
    case INTENSITY:
        ComputeIntensity(pImageAnalysisYuy2, pImage);
        break;

    case MEAN:
        ComputeAverage(pImageAnalysisYuy2, pImage);
        break;

    case HISTOGRAM:
        ComputeHistogram(pImageAnalysisYuy2, pImage);
        break;

    default:
        break;
    }
}
