#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "printanalysis-gst.h"


#define VERSION "1.0"
#define PACKAGE "printanalysis-filter"


static gboolean plugin_init (GstPlugin * plugin)
{
    gboolean ret = FALSE;

    ret |= GST_ELEMENT_REGISTER (printanalysis, plugin);
    return ret;
}

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    printanalysis,
    "Print Analysis filter",
    plugin_init, VERSION, "LGPL", "GStreamer", "https://gstreamer.freedesktop.org/")
