#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

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

    // ここでアプリ全体のツールチップ見た目を設定
    a.setStyleSheet(
        "QToolTip { background: #FFF9C4; color: #333; "
        "border: 1px solid #C49B00; padding: 6px; font-size: 12pt; }"
        );

    MainWindow w;

    w.show();

    return a.exec();
}
