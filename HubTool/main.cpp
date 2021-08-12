#include "hubtool.h"
#include <QApplication>
#include <QFontDatabase>

#include "BrainStem2/aVersion.h"

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    QString title = "HubTool: v";
    title += QString::fromUtf8(aVersion_GetString());

    HubTool w;
    w.setWindowTitle(title);
    w.show();

#if !defined(_WIN32) && !defined(__APPLE__)
    QString fontPath = ":/fonts/TitilliumWeb-Regular.ttf";
    int fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId != -1)
    {
        qDebug() << "Loaded Font";
        a.setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).at(0)));
        //QApplication::setFont(QFont(":/fonts/fonts/Andale Mono.ttf")); //This doesn't work...
    }
    else {
        qDebug() << "Did not load font";
    }
#endif


    return a.exec();
}
