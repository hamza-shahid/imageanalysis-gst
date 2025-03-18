#include <gst/video/video.h>

#include "printanalysis-gst.h"
#include "imageanalysis-rgb.h"
#include "imageanalysis-yuy2.h"

GST_DEBUG_CATEGORY_STATIC (printanalysis_debug);
#define GST_CAT_DEFAULT (printanalysis_debug)

enum
{
	PROP_0,
	PROP_ANALYSIS_TYPE,
	PROP_PARTITIONS_JSON,
	PROP_AOI_HEIGHT,
	PROP_PARTITIONS,
	PROP_CONNECT_VALUES,
	PROP_BLACKOUT_TYPE,
	PROP_GRAYSCALE_TYPE,
	PROP_LAST
};

enum {
	AOI_TOTAL_SIGNAL,
	NUM_SIGNALS
};

static guint gst_print_analysis_signals[NUM_SIGNALS] = { 0 };


#define gst_print_analysis_parent_class parent_class
G_DEFINE_TYPE (GstPrintAnalysis, gst_print_analysis, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE (printanalysis, "printanalysis",
    GST_RANK_NONE, gst_print_analysis_get_type ());

#define CAPS_STR GST_VIDEO_CAPS_MAKE ("{ " \
    "ARGB, BGRA, ABGR, RGBA, xRGB, BGRx, xBGR, RGBx, RGB, BGR, AYUV, YUY2 }")

static GstStaticPadTemplate gst_print_analysis_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static GstStaticPadTemplate gst_print_analysis_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STR)
    );

static gboolean gst_print_analysis_set_info (GstVideoFilter * vfilter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
	GstPrintAnalysis* filter = GST_PRINT_ANALYSIS(vfilter);
	AnalysisOpts opts;

	GST_DEBUG_OBJECT(filter,
		"in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

	filter->pImageAnalysis = NULL;

	filter->format = GST_VIDEO_INFO_FORMAT(in_info);
	filter->width = GST_VIDEO_INFO_WIDTH(in_info);
	filter->height = GST_VIDEO_INFO_HEIGHT(in_info);

	opts.analysisType = filter->analysisType;
	opts.aoiHeight = filter->aoiHeight;
	opts.aoiPartitions = filter->partitions;
	opts.connectValues = filter->connectValues;
	opts.blackoutType = filter->blackoutType;
	opts.grayscaleType = filter->grayscaleType;

	GST_OBJECT_LOCK(filter);

	switch (filter->format) {
	case GST_VIDEO_FORMAT_BGRx:
		filter->pImageAnalysis = calloc(1, sizeof(ImageAnalysisRGB));
		filter->pImageAnalysis->init = init_rgb;
		filter->pImageAnalysis->deinit = deinit_rgb;
		filter->pImageAnalysis->analyze = analyize_rgb;
		filter->pImageAnalysis->pGdiObj = filter->gdiObj;

		// initialize image analysis
		filter->pImageAnalysis->init(filter->pImageAnalysis, &opts, filter->width, filter->height);
		break;

	case GST_VIDEO_FORMAT_YUY2:
		filter->pImageAnalysis = calloc(1, sizeof(ImageAnalysisYUY2));
		filter->pImageAnalysis->init = init_yuy2;
		filter->pImageAnalysis->deinit = deinit_yuy2;
		filter->pImageAnalysis->analyze = analyize_yuy2;
		filter->pImageAnalysis->pGdiObj = filter->gdiObj;

		// initialize image analysis
		filter->pImageAnalysis->init(filter->pImageAnalysis, &opts, filter->width, filter->height);
		break;

	default:
		break;
	}

	GST_OBJECT_UNLOCK(filter);

	return filter->pImageAnalysis != NULL;
}

static GstFlowReturn gst_print_analysis_transform_frame_ip (GstVideoFilter * vfilter, GstVideoFrame * out)
{
	GstPrintAnalysis *filter = GST_PRINT_ANALYSIS (vfilter);

	if (!filter->pImageAnalysis)
		goto not_negotiated;

	time_t currentTime;
	time(&currentTime);

	GST_OBJECT_LOCK (filter);
	
	filter->pImageAnalysis->iStride = filter->stride = GST_VIDEO_FRAME_PLANE_STRIDE(out, 0);

	filter->pImageAnalysis->analyze (filter->pImageAnalysis, out);

	if (filter->analysisType == TOTAL && filter->pImageAnalysis->bPartitionsReady)
	{
		gchar* pPartitionsJsonStr = PartitionsArrayToJsonStr(filter->pImageAnalysis);
		
		if (pPartitionsJsonStr)
			g_signal_emit(GST_PRINT_ANALYSIS(vfilter), gst_print_analysis_signals[AOI_TOTAL_SIGNAL], 0, pPartitionsJsonStr);

		filter->pImageAnalysis->bPartitionsReady = FALSE;
		filter->prevSingalEmitTime = currentTime;
	}

	GST_OBJECT_UNLOCK (filter);

	return GST_FLOW_OK;

not_negotiated:
	GST_ERROR_OBJECT (filter, "Not negotiated yet");
	return GST_FLOW_NOT_NEGOTIATED;
}

static void gst_print_analysis_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
	GstPrintAnalysis *filter = GST_PRINT_ANALYSIS (object);

	GST_OBJECT_LOCK(filter);

	switch (prop_id)
	{
	case PROP_ANALYSIS_TYPE:
		filter->analysisType = (AnalysisType) g_value_get_uint(value);
		break;

	case PROP_PARTITIONS_JSON:
		if (filter->pImageAnalysis)
		{
			ParsePartitionsFromString(filter->pImageAnalysis, g_value_get_string(value));

			if (filter->pImageAnalysis->nPartitions)
				filter->pImageAnalysis->bPartitionsReady = TRUE;
		}
		break;
	
	case PROP_AOI_HEIGHT:
		filter->aoiHeight = g_value_get_uint(value);
		break;

	case PROP_PARTITIONS:
		filter->partitions = g_value_get_uint(value);
		break;

	case PROP_CONNECT_VALUES:
		filter->connectValues = g_value_get_boolean(value);
		break;

	case PROP_BLACKOUT_TYPE:
		filter->blackoutType = g_value_get_uint(value);
		break;

	case PROP_GRAYSCALE_TYPE:
		filter->grayscaleType = g_value_get_uint(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	AnalysisOpts opts;
	
	opts.analysisType = filter->analysisType;
	opts.aoiHeight = filter->aoiHeight;
	opts.aoiPartitions = filter->partitions;
	opts.connectValues = filter->connectValues;
	opts.blackoutType = filter->blackoutType;
	opts.grayscaleType = filter->grayscaleType;
	
	if (filter->pImageAnalysis)
		UpdatePrintAnalysisOpts(filter->pImageAnalysis, &opts);

	GST_OBJECT_UNLOCK(filter);
}

static void gst_print_analysis_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
	GstPrintAnalysis* filter = GST_PRINT_ANALYSIS(object);

	GST_OBJECT_LOCK(filter);

	switch (prop_id) 
	{
	case PROP_ANALYSIS_TYPE:
		g_value_set_uint(value, filter->analysisType);
		break;
	
	case PROP_PARTITIONS_JSON:
		g_value_set_string(value, "");
		break;

	case PROP_AOI_HEIGHT:
		g_value_set_uint(value, filter->aoiHeight);
		break;

	case PROP_PARTITIONS:
		g_value_set_uint(value, filter->partitions);
		break;

	case PROP_CONNECT_VALUES:
		g_value_set_boolean(value, filter->connectValues);
		break;

	case PROP_BLACKOUT_TYPE:
		g_value_set_uint(value, filter->blackoutType);
		g_value_set_enum(value, filter->blackoutType);
		break;

	case PROP_GRAYSCALE_TYPE:
		g_value_set_uint(value, filter->grayscaleType);
		break;
	
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}

	GST_OBJECT_UNLOCK(filter);
}

static void gst_print_analysis_finalize(GObject* object)
{
	GstPrintAnalysis* filter = GST_PRINT_ANALYSIS(object);
	
	if (filter->pImageAnalysis)
	{
		filter->pImageAnalysis->deinit(filter->pImageAnalysis);
		free(filter->pImageAnalysis);
		filter->pImageAnalysis = NULL;
	}

	if (filter->gdiObj)
		gdiplus_shutdown(filter->gdiObj);

	// Chain up to the parent class's finalize method
	G_OBJECT_CLASS(gst_print_analysis_parent_class)->finalize(object);
}

static void gst_print_analysis_class_init (GstPrintAnalysisClass * klass)
{
	GObjectClass* gobject_class = (GObjectClass*)klass;
	GstElementClass* element_class = (GstElementClass*)klass;
	GstVideoFilterClass* vfilter_class = (GstVideoFilterClass*)klass;

	GST_DEBUG_CATEGORY_INIT(printanalysis_debug, "printanalysis", 0, "printanalysis");

	gobject_class->set_property = gst_print_analysis_set_property;
	gobject_class->get_property = gst_print_analysis_get_property;

	g_object_class_install_property(
		gobject_class,
		PROP_ANALYSIS_TYPE,
		g_param_spec_uint(
			"analysis-type",
			"Analysis Type",
			"Type of analysis to perform",
			INTENSITY,
			NONE,
			NONE,
			G_PARAM_READWRITE));
	
	g_object_class_install_property(
		gobject_class,
		PROP_PARTITIONS_JSON,
		g_param_spec_string(
			"partitions-json",
			"Partitions Json",
			"Partitions (AOIs) of image to be analyzed",
			NULL,
			G_PARAM_READWRITE));
	
	g_object_class_install_property(
		gobject_class,
		PROP_AOI_HEIGHT,
		g_param_spec_uint(
			"aoi-height",
			"AOI Height",
			"Height of Area of Interest",
			0,
			UINT_MAX,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class,
		PROP_PARTITIONS,
		g_param_spec_uint(
			"aoi-partitions",
			"AOI Partitions",
			"Partitions in Area of Interest",
			1,
			UINT_MAX,
			1,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class,
		PROP_CONNECT_VALUES,
		g_param_spec_boolean(
			"connect-values",
			"Connect Values",
			"Connect graph values",
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class,
		PROP_BLACKOUT_TYPE,
		g_param_spec_uint(
			"blackout-type",
			"Blackout Type",
			"Type of blackout to perform",
			BLACK_ALL,
			BLACK_NONE,
			BLACK_NONE,
			G_PARAM_READWRITE));

	g_object_class_install_property(
		gobject_class,
		PROP_GRAYSCALE_TYPE,
		g_param_spec_uint(
			"grayscale-type",
			"Grayscale Type",
			"Type of grayscale to perform",
			GRAY_ALL,
			GRAY_NONE,
			GRAY_NONE,
			G_PARAM_READWRITE));
	
	gst_print_analysis_signals[AOI_TOTAL_SIGNAL] = g_signal_new(
		"aoi-total-signal",                 // Signal name
		G_TYPE_FROM_CLASS(klass),           // Signal owner type
		G_SIGNAL_RUN_LAST,                  // Signal flags
		0,									// Default handler offset
		NULL,								// Accumulator
		NULL,								// Accumulator data
		NULL,								// Custom marshaller
		G_TYPE_NONE,						// Return type
		1,									// Number of parameters
		G_TYPE_STRING					    // Parameter type: String
	);

	vfilter_class->set_info = GST_DEBUG_FUNCPTR(gst_print_analysis_set_info);
	vfilter_class->transform_frame_ip =
		GST_DEBUG_FUNCPTR(gst_print_analysis_transform_frame_ip);

	gst_element_class_set_static_metadata(element_class,
		"Print Analysis filter", "Filter/Effect/Video",
		"Print Analysis filter",
		"Hamza Shahid <hamza@mayartech.com>");

	gst_element_class_add_static_pad_template(element_class,
		&gst_print_analysis_sink_template);
	gst_element_class_add_static_pad_template(element_class,
		&gst_print_analysis_src_template);

	//gst_type_mark_as_plugin_api(GST_TYPE_PRINT_ANALYSIS_PRESET, 0);
	gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_print_analysis_finalize);
}

static void gst_print_analysis_init(GstPrintAnalysis* filter)
{
	GObjectClass* gobject_class = G_OBJECT_CLASS(filter);
	//gobject_class->finalize = gst_print_analysis_finalize;

	filter->prevSingalEmitTime = 0;
	
	filter->gdiObj = gdiplus_startup();

	gdiplus_setfont(filter->gdiObj, L"Arial", 12, (color_c) {255, 255, 255});
}
