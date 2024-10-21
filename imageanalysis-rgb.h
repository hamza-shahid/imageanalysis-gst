#pragma once

#include "imageanalysis.h"

typedef struct INTRGBTRIPLE
{
	int red;
	int green;
	int blue;
} INTRGBTRIPLE;

typedef struct tagRGBQUAD {
	guint8    rgbBlue;
	guint8    rgbGreen;
	guint8    rgbRed;
	guint8    rgbReserved;
} RGBQUAD;

typedef struct ImageAnalysisRGB
{
	ImageAnalysis	imageAnalysis;

	INTRGBTRIPLE*	piHistogram;
	INTRGBTRIPLE**	ppResults;
	int*			piNumResults;
} ImageAnalysisRGB;

#define GST_IMAGE_ANALYSIS_RGB(obj) ((ImageAnalysisRGB*) obj) 


void init_rgb(ImageAnalysis* pImageAnalysis, AnalysisOpts* opts, int iImageWidth, int iImageHeight);
void deinit_rgb(ImageAnalysis* pImageAnalysis);
void analyize_rgb(ImageAnalysis* pImageAnalysis, GstVideoFrame* frame);
