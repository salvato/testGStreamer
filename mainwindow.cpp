#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QThread>


/* Handler for the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *pad, CustomData *data);


MainWindow::MainWindow(QWidget *parent, int argc, char *argv[])
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , terminate(FALSE)
{
    const gchar *nano_str;
    guint major, minor, micro, nano;

    ui->setupUi(this);
    gst_init (&argc, &argv);

    gst_version (&major, &minor, &micro, &nano);

    if (nano == 1)
        nano_str = "(CVS)";
    else if (nano == 2)
        nano_str = "(Prerelease)";
    else
        nano_str = "";

    ui->label->setText(tr("This program is linked against GStreamer %1.%2.%3")
                       .arg(major)
                       .arg(minor)
                       .arg(micro)
                       +QString(nano_str));
}


MainWindow::~MainWindow() {
    delete ui;
}


void
MainWindow::on_pushButton_clicked() {
    /* Build the pipeline */
//    QString sURI = QString("https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm");
#ifdef Q_PROCESSOR_ARM
    QString sURI = QString("file:///home/pi/spot/Campionesse.mp4");
#else
    QString sURI = QString("file:///home/gabriele/spot/Campionesse.mp4");
#endif
//    QString sCommand = QString("playbin uri=");


    /* Create the elements */
    data.source       = gst_element_factory_make ("uridecodebin",  "source");
    data.audioconvert = gst_element_factory_make ("audioconvert",  "audioconvert");
    data.videoconvert = gst_element_factory_make ("videoconvert",  "videoconvert");
    data.audiosink    = gst_element_factory_make ("autoaudiosink", "audiosink");
    data.videosink    = gst_element_factory_make ("autovideosink", "videosink");

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new ("test-pipeline");

    if (!data.pipeline ||
        !data.source ||
        !data.audioconvert ||
        !data.videoconvert ||
        !data.audiosink ||
        !data.videosink) {
      g_printerr ("Not all elements could be created.\n");
      return;
    }

    /* Build the pipeline.
     * Note that we are NOT linking the source at this point.
     * We will do it later. */
    gst_bin_add_many(GST_BIN (data.pipeline),
                     data.source,
                     data.audioconvert,
                     data.videoconvert,
                     data.audiosink,
                     data.videosink,
                     NULL);

    if (!gst_element_link (data.audioconvert, data.audiosink)) {
      g_printerr ("Elements could not be linked.\n");
      gst_object_unref (data.pipeline);
      return;
    }

    if (!gst_element_link (data.videoconvert, data.videosink)) {
      g_printerr ("Elements could not be linked.\n");
      gst_object_unref (data.pipeline);
      return;
    }

    /* Set the URI to play */
    g_object_set(data.source, "uri", sURI.toLatin1().constData(), NULL);

    /* Connect to the pad-added signal */
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);

    /* Start playing */
    ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      g_printerr ("Unable to set the pipeline to the playing state.\n");
      gst_object_unref (data.pipeline);
      return;
    }

    /* Listen to the bus */
    bus = gst_element_get_bus(data.pipeline);
    do {
      msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
          GstMessageType(GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

      /* Parse message */
      if (msg != NULL) {
        GError *err;
        gchar *debug_info;

        switch (GST_MESSAGE_TYPE (msg)) {
          case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
            g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free (debug_info);
            terminate = TRUE;
            break;
          case GST_MESSAGE_EOS:
            g_print("End-Of-Stream reached.\n");
            terminate = TRUE;
            break;
          case GST_MESSAGE_STATE_CHANGED:
            /* We are only interested in state-changed messages from the pipeline */
            if(GST_MESSAGE_SRC (msg) == GST_OBJECT(data.pipeline)) {
              GstState old_state, new_state, pending_state;
              gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
              g_print("Pipeline state changed from %s to %s:\n",
                  gst_element_state_get_name(old_state), gst_element_state_get_name (new_state));
            }
            break;
          default:
            /* We should not reach here */
            g_printerr("Unexpected message received.\n");
            break;
        }
        gst_message_unref(msg);
      }
    } while (!terminate);

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
}



/* This function will be called by the pad-added signal */
static void
pad_added_handler (GstElement *src, GstPad *new_pad, CustomData *data) {
    GstPad *audiosink_pad = gst_element_get_static_pad(data->audioconvert, "sink");
    GstPad *videosink_pad = gst_element_get_static_pad(data->videoconvert, "sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

    /* Check the new pad's type */
    new_pad_caps = gst_pad_query_caps(new_pad, NULL);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    if (g_str_has_prefix(new_pad_type, "video/x-raw")) {
        /* If our converter is already linked, we have nothing to do here */
        if (gst_pad_is_linked(videosink_pad)) {
            g_print ("  We are already linked. Ignoring.\n");
            goto exit;
        }
        /* Attempt the link */
        ret = gst_pad_link(new_pad, videosink_pad);
        if (GST_PAD_LINK_FAILED (ret)) {
            g_print ("  Type is '%s' but link failed.\n", new_pad_type);
        } else {
            g_print ("  Link succeeded (type '%s').\n", new_pad_type);
        }
    }
    else if (g_str_has_prefix(new_pad_type, "audio/x-raw")) {
        if (gst_pad_is_linked(audiosink_pad)) {
            g_print ("  We are already linked. Ignoring.\n");
            goto exit;
        }
        /* Attempt the link */
        ret = gst_pad_link(new_pad, audiosink_pad);
        if (GST_PAD_LINK_FAILED (ret)) {
            g_print ("  Type is '%s' but link failed.\n", new_pad_type);
        } else {
            g_print ("  Link succeeded (type '%s').\n", new_pad_type);
        }
    }



exit:
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL)
        gst_caps_unref (new_pad_caps);

    /* Unreference the sink pads */
    gst_object_unref (audiosink_pad);
    gst_object_unref (videosink_pad);
}
