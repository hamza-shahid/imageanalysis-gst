#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>


typedef enum
{
	INTENSITY = 0,
	MEAN = 1,
	HISTOGRAM = 2,
	NONE = 3
} AnalysisType;

typedef enum
{
	BLACK_ALL,
	BLACK_AOI,
	BLACK_NONE
} BlackoutType;

typedef enum
{
	GRAY_ALL,
	GRAY_AOI,
	GRAY_NONE
} GrayscaleType;

typedef struct AnalysisOpts
{
	AnalysisType	analysisType;
	guint			aoiHeight;
	guint			aoiPartitions;
	gboolean		connectValues;
	BlackoutType	blackoutType;
	GrayscaleType	grayscaleType;
} AnalysisOpts;

typedef struct _ImageAnalysis ImageAnalysis;

struct _ImageAnalysis
{
	AnalysisOpts	opts;
	int				iPrevPartitions;
	int				iImageWidth;
	int				iImageHeight;

	void (*init) (ImageAnalysis* pImageAnalysis, AnalysisOpts *opts, int iImageWidth, int iImageHeight);
	void (*deinit) (ImageAnalysis* pImageAnalysis);
	void (*analyze) (ImageAnalysis* pImageAnalysis, GstVideoFrame* frame);
};

#define GST_IMAGE_ANALYSIS(obj) ((ImageAnalysis*) obj)

double NormalizeValue(double fValue, double fOrigRange, double fMinOrig, double fNewRange, double fMinNew);
void UpdatePrintAnalysisOpts(ImageAnalysis* pImageAnalysis, AnalysisOpts* pOpts);