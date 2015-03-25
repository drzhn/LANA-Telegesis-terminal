#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QString>
#include <QTimer>
#include <QMessageBox>
#include <QStringList>
#include <QGraphicsScene>
#include <fstream>
#include <QFileDialog>

namespace Ui {
class MainWindow;
}

class readThread : public QThread
{
    Q_OBJECT
public:
    void run();
signals:
    void readyToWrite(QString);
    void howMuchBytes(QString);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    readThread *mThread;
    QTimer *timer,*delay;
    QGraphicsScene *plot;
private slots:

    void on_connectButton_clicked();
    void sendCommand();
    void _sendString();
    void on_sendButton_clicked();
    void commandProccesing(QString);
    void on_pushButton_clicked();
    void requestTemp();
    void requestLight();
    void on_pushButton_2_clicked();
    void plotValue(int);
    void recievedBytesCounter(QString);
    void on_tempSensor_clicked();

    void on_lightSensor_clicked();

    void on_LED1_clicked();

    void on_LED2_clicked();

    void on_LED3_clicked();

    void on_LED4_clicked();

    void on_clearStats_clicked();

    void on_pushButton_5_clicked();
    void saveFilename(QString);

private:
    Ui::MainWindow *ui;

};



#endif // MAINWINDOW_H
