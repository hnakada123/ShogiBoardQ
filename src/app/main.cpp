#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStyleFactory>
#include <QGuiApplication>
#include <QToolTip>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // アプリケーションアイコンを設定
    a.setWindowIcon(QIcon(":/shogiboardq.png"));

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
