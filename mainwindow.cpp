#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QThread>

MainWindow::MainWindow(QWidget *parent, int argc, char *argv[]) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
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
    QString sURI = QString("file:///home/gabriele/spot/Unime_ti_apre_le_porte.mp4");
    QString sCommand = QString("playbin uri=");


    /* Create the elements */
    source = gst_element_factory_make("videotestsrc", "source");
    sink   = gst_element_factory_make("autovideosink", "sink");
    GstElement* vertigo =  gst_element_factory_make("vertigotv", "vertigo");
    GstElement* videoconvert =  gst_element_factory_make("videoconvert", "videoconvert");

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new("test-pipeline");

    if (!pipeline || !source || !sink || !vertigo || !videoconvert) {
        g_printerr("Not all elements could be created.\n");
        return;
    }

    /* Build the pipeline */
    gst_bin_add_many(GST_BIN (pipeline), source, vertigo, videoconvert, sink, NULL);
    if (gst_element_link(source, vertigo) != TRUE) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }
    if (gst_element_link(vertigo, videoconvert) != TRUE) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }
    if (gst_element_link(videoconvert, sink) != TRUE) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return;
    }
//for(int i=0; i<24; i++) {
    /* Modify the source's properties */
    g_object_set(source, "pattern", 0, NULL);

    /* Start playing */
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (pipeline);
        return;
    }
//    QThread::sleep(1);
//    ret = gst_element_set_state(pipeline, GST_STATE_PAUSED);
//    if (ret == GST_STATE_CHANGE_FAILURE) {
//        g_printerr("Unable to set the pipeline to the playing state.\n");
//        gst_object_unref (pipeline);
//        return;
//    }
//}
    /* Wait until error or EOS */
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    /* Parse message */
    if (msg != NULL) {
        GError *err;
        gchar *debug_info;

        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error (msg, &err, &debug_info);
            g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
            g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
            g_clear_error (&err);
            g_free (debug_info);
            break;
        case GST_MESSAGE_EOS:
            ui->label->setText("End-Of-Stream reached.");
            break;
        default:
            /* We should not reach here because we only asked for ERRORs and EOS */
            g_printerr("Unexpected message received.\n");
            break;
        }
        gst_message_unref(msg);
    }

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}
