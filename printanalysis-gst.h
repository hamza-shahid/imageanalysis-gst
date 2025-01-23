#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include "imageanalysis.h"

G_BEGIN_DECLS
#define GST_TYPE_PRINT_ANALYSIS \
  (gst_print_analysis_get_type())
#define GST_PRINT_ANALYSIS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PRINT_ANALYSIS,GstPrintAnalysis))
#define GST_PRINT_ANALYSIS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PRINT_ANALYSIS,GstPrintAnalysisClass))
#define GST_IS_PRINT_ANALYSIS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PRINT_ANALYSIS))
#define GST_IS_PRINT_ANALYSIS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PRINT_ANALYSIS))
typedef struct _GstPrintAnalysis GstPrintAnalysis;
typedef struct _GstPrintAnalysisClass GstPrintAnalysisClass;

/**
 * GstPrintAnalysis:
 *
 * Opaque datastructure.
 */
struct _GstPrintAnalysis
{
	GstVideoFilter videofilter;

	/* < private > */
	/* video format */
	GstVideoFormat format;
	gint width;
	gint height;

	AnalysisType analysisType;
	guint aoiHeight;
	guint partitions;
	gboolean connectValues;
	BlackoutType blackoutType;
	GrayscaleType grayscaleType;

	ImageAnalysis* pImageAnalysis;

	time_t prevSingalEmitTime;
	//void (*process) (GstPrintAnalysis* filter, GstVideoFrame * frame);
};

struct _GstPrintAnalysisClass
{
  GstVideoFilterClass parent_class;
};

GType gst_print_analysis_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (printanalysis);

G_END_DECLS
