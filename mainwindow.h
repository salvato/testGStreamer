#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <gst/gst.h>


/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *source;
  GstElement *audioconvert;
  GstElement *videoconvert;
  GstElement *audiosink;
  GstElement *videosink;
} CustomData;


namespace Ui {
  class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0, int argc=0, char *argv[]=NULL);
  ~MainWindow();

private slots:
  void on_pushButton_clicked();

private:
  Ui::MainWindow *ui;
  CustomData data;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  gboolean terminate;
};

#endif // MAINWINDOW_H
