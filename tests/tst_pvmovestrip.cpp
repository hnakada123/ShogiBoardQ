/// @file tst_pvmovestrip.cpp
/// @brief 読み筋盤の帯 PvMoveStrip と、帯と盤の結び付きのテスト

#include <QtTest>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>

#include "boardviewerwidget.h"
#include "notationstyle.h"
#include "pvboarddialog.h"
#include "pvline.h"
#include "pvmovestrip.h"
#include "pvtext.h"
#include "sfenutils.h"

class TestPvMoveStrip : public QObject
{
    Q_OBJECT

private:
    static QAbstractButton* word(const QWidget& root, int index)
    {
        return root.findChild<QAbstractButton*>(QStringLiteral("pvMove%1").arg(index));
    }
    static int checkedWords(const QWidget& root)
    {
        int count = 0;
        const auto buttons = root.findChildren<QAbstractButton*>(QRegularExpression(QStringLiteral("^pvMove\\d+$")));
        for (const QAbstractButton* button : buttons) count += button->isChecked() ? 1 : 0;
        return count;
    }
    static QStringList moves()
    {
        return {QStringLiteral("開始局面"), QStringLiteral("▲７六歩(77)"), QStringLiteral("△３四歩(33)"),
                QStringLiteral("▲２六歩(27)")};
    }
    static PvText pvOf(const QString& usi) { return PvText::play(SfenUtils::hirateSfen(), usi); }

private slots:
    void init() { NotationStyle::configure(QStringLiteral("ja_JP"), QStringLiteral("japanese"), false); }

    /// 局面ごとに語が1つあり、始めは開始局面の語が今の語
    void oneWordPerPositionStartingAtTheBeginning()
    {
        const PvMoveStrip strip(moves(), QString());
        QCOMPARE(strip.findChildren<QAbstractButton*>().size(), 4);
        QCOMPARE(word(strip, 0)->text(), QStringLiteral("開始局面"));
        QVERIFY(word(strip, 0)->isChecked());
        QCOMPARE(checkedWords(strip), 1);
    }

    /// 語を押すと局面を頼むだけで、今の語は呼び出し側の setCurrent で決まる
    void clickingWordRequestsThatPosition()
    {
        PvMoveStrip strip(moves(), QString());
        strip.show();
        QVERIFY(QTest::qWaitForWindowExposed(&strip));
        QSignalSpy requested(&strip, &PvMoveStrip::positionRequested);
        QTest::mouseClick(word(strip, 2), Qt::LeftButton);
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.first().first().toInt(), 2);
    }

    /// 今の語は常に1つだけで、範囲外の番号では変わらない
    void setCurrentKeepsExactlyOneWord()
    {
        PvMoveStrip strip(moves(), QString());
        strip.setCurrent(3);
        QVERIFY(word(strip, 3)->isChecked());
        QCOMPARE(checkedWords(strip), 1);
        strip.setCurrent(1);
        QVERIFY(word(strip, 1)->isChecked());
        QCOMPARE(checkedWords(strip), 1);
        strip.setCurrent(99);
        QVERIFY(word(strip, 1)->isChecked());
        QCOMPARE(checkedWords(strip), 1);
    }

    /// 手のない語は押せず、HTML として読まない。局面と合わない読み筋の語は1語ずつ折り返せる
    void endingWordsArePlainLabels()
    {
        const PvMoveStrip strip({QStringLiteral("開始局面")}, QStringLiteral("<b>a</b> b"));
        QCOMPARE(strip.findChildren<QAbstractButton*>().size(), 1);
        const auto labels = strip.findChildren<QLabel*>(QStringLiteral("pvMoveEnding"));
        QCOMPARE(labels.size(), 2);
        QCOMPARE(labels.first()->text(), QStringLiteral("<b>a</b>"));
        QCOMPARE(labels.first()->textFormat(), Qt::PlainText);
    }

    /// 幅が足りなければ語の途中ではなく語の間で折り返し、どの語も帯の中に収まる
    void wrapsBetweenWordsInsideTheStrip()
    {
        QStringList labels = moves();
        for (int i = 0; i < 20; ++i) labels.append(QStringLiteral("▲２六歩(27)"));
        PvMoveStrip strip(labels, QString());
        strip.show();
        QVERIFY(QTest::qWaitForWindowExposed(&strip));
        const int oneWordHeight = word(strip, 1)->sizeHint().height();

        strip.resize(320, strip.heightForWidth(320));
        QTRY_VERIFY(word(strip, 23)->y() > oneWordHeight);
        QVERIFY(strip.heightForWidth(320) > strip.heightForWidth(4000));
        for (int i = 0; i < labels.size(); ++i) {
            const QAbstractButton* button = word(strip, i);
            QCOMPARE(button->height(), oneWordHeight);   // 1行で描く
            QVERIFY2(strip.rect().contains(button->geometry()), qPrintable(QString::number(i)));
        }
    }

    /// 帯を押すと盤がその局面に移り、局面送りでも帯の今の語が付いてくる
    void dialogKeepsStripAndBoardTogether()
    {
        const PvText pv = pvOf(QStringLiteral("7g7f 3c3d 2g2f"));
        const PvLine line(pv);
        PvBoardDialog dialog(pv);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* viewer = dialog.findChild<BoardViewerWidget*>();
        QVERIFY(viewer);
        QVERIFY(word(dialog, 0)->isChecked());

        QTest::mouseClick(word(dialog, 2), Qt::LeftButton);
        QCOMPARE(viewer->currentIndex(), 2);
        QCOMPARE(viewer->currentSfen(), line.positions().at(2));
        QVERIFY(word(dialog, 2)->isChecked());
        QCOMPARE(checkedWords(dialog), 1);

        viewer->goForward();
        QVERIFY(word(dialog, 3)->isChecked());
        QCOMPARE(checkedWords(dialog), 1);

        QTest::mouseClick(word(dialog, 0), Qt::LeftButton);
        QCOMPARE(viewer->currentIndex(), 0);
        QVERIFY(word(dialog, 0)->isChecked());
    }

    /// 盤を小さくして帯の折り返しが増えても、ウィンドウは盤全体が入る高さになる（スクロールバーを出さない）
    void windowFitsBoardWhenStripWrapsAfterNarrowing()
    {
        PvBoardDialog dialog(pvOf(QStringLiteral("7g7f 3c3d 2g2f 8c8d 2f2e 8d8e 6i7h 4a3b 3i3h 7a7b 9g9f 9c9d 1g1f 1c1d")));
        dialog.setSquareSize(90);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QTest::qWait(50);
        const auto* strip = dialog.findChild<PvMoveStrip*>();
        const int wideHeight = strip->height();

        dialog.setSquareSize(30);
        QTest::qWait(50);
        QVERIFY2(strip->height() > wideHeight, "the narrower window should wrap the strip onto more lines");
        const auto* scroll = dialog.findChild<QScrollArea*>(QStringLiteral("boardScrollArea"));
        QVERIFY(scroll);
        QVERIFY(!scroll->verticalScrollBar()->isVisible());
        QVERIFY(!scroll->horizontalScrollBar()->isVisible());
    }

    /// 注記・投了は語にしても局面を増やさない
    void dialogEndingAddsNoPosition()
    {
        const PvBoardDialog noted(pvOf(QStringLiteral("7g7f (57.54%)")));
        QCOMPARE(noted.findChild<BoardViewerWidget*>()->count(), 2);
        QVERIFY(word(noted, 1));
        QVERIFY(!word(noted, 2));
        QCOMPARE(noted.findChild<QLabel*>(QStringLiteral("pvMoveEnding"))->text(), QStringLiteral("(57.54%)"));

        const PvBoardDialog resigned(pvOf(QStringLiteral("7g7f resign")));
        QCOMPARE(resigned.findChild<QLabel*>(QStringLiteral("pvMoveEnding"))->text(), QStringLiteral("△投了"));
    }
};

QTEST_MAIN(TestPvMoveStrip)

#include "tst_pvmovestrip.moc"
