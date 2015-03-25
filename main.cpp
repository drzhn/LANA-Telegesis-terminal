#include "mainwindow.h"
#include <QApplication>

extern QString PORTNAME, Qcommand;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
