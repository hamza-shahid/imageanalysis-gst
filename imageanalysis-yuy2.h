#ifndef __IMAGE_ANALYSIS_YUY2_H__
#define __IMAGE_ANALYSIS_YUY2_H__

#include "imageanalysis.h"

typedef struct YUY2PIXEL
{
	guint8 luma;
	guint8 chroma;
} YUY2PIXEL;
	
typedef struct INTYUY2PIXEL
{
	int luma;
	int chroma;
} INTYUY2PIXEL;

typedef struct INTYUVPIXEL
{
	int luma;
	int Cr;
	int Cb;
} INTYUVPIXEL;

typedef struct ImageAnalysisYUY2
{
	ImageAnalysis	imageAnalysis;

	INTYUVPIXEL*	piHistogram;
	INTYUY2PIXEL**	ppResults;
	int*			piNumResults;

	int				iPrevHistPartitions;
	INTYUVPIXEL**	ppHistogram;
	int*			piNumHistogramResults;
} ImageAnalysisYUY2;

#define GST_IMAGE_ANALYSIS_YUY2(obj) ((ImageAnalysisYUY2*) obj) 


void init_yuy2(ImageAnalysis* pImageAnalysis, AnalysisOpts* opts, int iImageWidth, int iImageHeight);
void deinit_yuy2(ImageAnalysis* pImageAnalysis);
void analyize_yuy2(ImageAnalysis* pImageAnalysis, GstVideoFrame* frame);

#endif // __IMAGE_ANALYSIS_YUY2_H__