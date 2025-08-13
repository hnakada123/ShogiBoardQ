#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStyleFactory>
#include <QGuiApplication>

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

    // 1) Creatorっぽい「Fusion」スタイルに統一
    a.setStyle(QStyleFactory::create("Fusion"));

    // 2) フォントを小さめに（例: 10pt）
    QFont f = a.font();
    f.setPointSizeF(10.0);           // お好みで 9.0〜10.5 くらい
    a.setFont(f);

    // ここでアプリ全体のツールチップ見た目を設定
    a.setStyleSheet(R"(
        QToolTip { background: #FFF9C4; color: #333; border: 1px solid #C49B00; padding: 6px; font-size: 12pt; }
        QMenuBar { font-size: 10pt; padding:1px 4px; }
        QMenuBar::item { padding:2px 6px; }
        QMenu { font-size: 10pt; padding:2px; }
        QMenu::item { padding:2px 10px; } /* 行間を詰める */
        )");

    MainWindow w;

    w.show();

    return a.exec();
}
