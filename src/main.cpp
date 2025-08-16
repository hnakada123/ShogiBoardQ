#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStyleFactory>
#include <QGuiApplication>
#include <QToolTip>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;

    const QStringList uiLanguages = QLocale::system().uiLanguages();

    for (const QString &locale : uiLanguages) {
        const QString baseName = "ShogiBoardQ_" + QLocale(locale).name();

        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // Creatorのような「Fusion」スタイルに統一する。
    a.setStyle(QStyleFactory::create("Fusion"));

    MainWindow w;

    w.show();

    return a.exec();
}
