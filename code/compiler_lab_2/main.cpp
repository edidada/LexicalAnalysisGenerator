#include "widget.h"

#include <QApplication>
#include <QTextCodec>
#pragma execution_character_set("utf-8")

int main(int argc, char *argv[])
{
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForLocale(codec);

    QApplication a(argc, argv);
    Widget w;
    w.show();
    return a.exec();
}
