#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>


typedef enum
{
	INTENSITY = 0,
	MEAN = 1,
	HISTOGRAM = 2,
	TOTAL = 3,
	NONE = 4
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

typedef union
{
	struct 
	{
		gint r;
		gint g;
		gint b;
	} rgb;

	struct {
		gint y;
		gint u;
		gint v;
	} yuv;
} Pixel;

typedef struct AnalysisOpts
{
	AnalysisType	analysisType;
	guint			aoiHeight;
	guint			aoiPartitions;
	gboolean		connectValues;
	BlackoutType	blackoutType;
	GrayscaleType	grayscaleType;
} AnalysisOpts;

typedef struct PrintPartition
{
	gint id;
	gint centerX;
	gint centerY;
	gint width;
	gint height;

	Pixel total;
} PrintPartition;

typedef struct _ImageAnalysis ImageAnalysis;

struct _ImageAnalysis
{
	AnalysisOpts	opts;
	int				iPrevPartitions;
	int				iImageWidth;
	int				iImageHeight;

	PrintPartition*	pPartitions;
	int				nPartitions;
	gboolean		bPartitionsReady;

	void (*init) (ImageAnalysis* pImageAnalysis, AnalysisOpts *opts, int iImageWidth, int iImageHeight);
	void (*deinit) (ImageAnalysis* pImageAnalysis);
	void (*analyze) (ImageAnalysis* pImageAnalysis, GstVideoFrame* frame);
};

#define GST_IMAGE_ANALYSIS(obj) ((ImageAnalysis*) obj)

double NormalizeValue(double fValue, double fOrigRange, double fMinOrig, double fNewRange, double fMinNew);
void UpdatePrintAnalysisOpts(ImageAnalysis* pImageAnalysis, AnalysisOpts* pOpts);

gboolean ParsePartitionsFromString(ImageAnalysis* pImageAnalysis, const gchar* pJsonStr);
char* PartitionsArrayToJsonStr(ImageAnalysis* pImageAnalysis);
