#include <QtTest>
#include <QTranslator>
#include "kifupresentation.h"
#include "sfenpositiontracer.h"

using namespace KifuPresentation;

class TestKifuPresentation : public QObject
{
    Q_OBJECT
private slots:
    void cleanup() { configure(QStringLiteral("ja_JP"), QStringLiteral("auto"), false); }

    void languageAndCoordinates()
    {
        QCOMPARE(resolveLanguage("system", "zh_Hant_HK"), QStringLiteral("zh_TW"));
        QCOMPARE(resolveLanguage("system", "zh_CN"), QStringLiteral("zh_CN"));
        QCOMPARE(resolveLanguage("system", "en_US"), QStringLiteral("en"));
        QCOMPARE(resolveLanguage("system", "fr_FR"), QStringLiteral("en"));
        configure("en", "auto", false);
        QCOMPARE(rankLabel(1), QStringLiteral("a"));
        QCOMPARE(rankLabel(9), QStringLiteral("i"));
        configure("zh_CN", "auto", false);
        QCOMPARE(rankLabel(6), QStringLiteral("六"));
        configure("ja_JP", "western", false);
        QCOMPARE(rankLabel(6), QStringLiteral("f"));
        configure("en", "japanese", false);
        QCOMPARE(rankLabel(6), QStringLiteral("六"));
    }

    void westernMoves()
    {
        const Options compact{Notation::Western, false};
        const Options full{Notation::Western, true};
        SfenPositionTracer board;
        QCOMPARE(move(board.toSfenString(), "7g7f", compact), QStringLiteral("▲P-7f"));
        QCOMPARE(move(board.toSfenString(), "7g7f", full), QStringLiteral("▲P7g-7f"));
        QVERIFY(board.applyUsiMove("7g7f"));
        QCOMPARE(move(board.toSfenString(), "3c3d", compact), QStringLiteral("△P-3d"));
        QVERIFY(board.applyUsiMove("3c3d"));
        QCOMPARE(move(board.toSfenString(), "8h2b+", compact), QStringLiteral("▲Bx2b+"));
        QCOMPARE(move(board.toSfenString(), "8h2b", compact), QStringLiteral("▲Bx2b="));
        const QString promoted = "4k4/9/9/4+B4/9/9/9/9/4K4 b P 1";
        QCOMPARE(move(promoted, "5d4d", compact), QStringLiteral("▲+B-4d"));
        QCOMPARE(move(promoted, "P*7f", compact), QStringLiteral("▲P*7f"));
        const QString ambiguous = "4k4/9/9/9/9/9/9/9/3GKG3 b - 1";
        QCOMPARE(move(ambiguous, "6i5h", compact), QStringLiteral("▲G6i-5h"));
    }

    void handicapPvAndCanonicalPreservation()
    {
        configure("en", "auto", false);
        SfenPositionTracer board;
        QString whiteStart = board.toSfenString();
        whiteStart.replace(" b ", " w ");
        QCOMPARE(pv(whiteStart, "3c3d 7g7f", "△３四歩(33)▲７六歩(77)"), QStringLiteral("△P-3d ▲P-7f"));
        QCOMPARE(label("   1 ▲７六歩(77)+", board.toSfenString(), "7g7f"), QStringLiteral("   1 ▲P-7f [+]"));
        const auto moves = SfenPositionTracer::buildGameMoves(board.toSfenString(), {"7g7f", "3c3d", "8h2b+"});
        QCOMPARE(usiMove(moves.at(2)), QStringLiteral("8h2b+"));
        configure("zh_CN", "auto", false);
        const QString canonical = QStringLiteral("▲同　飛成(28)");
        QCOMPARE(label(canonical, board.toSfenString(), "2h2b+"), canonical);
    }

    void metadataPreservesUserContent()
    {
        QCOMPARE(infoKey("独自タグ"), QStringLiteral("独自タグ"));
        QCOMPARE(infoValue("先手", "佐藤 一郎"), QStringLiteral("佐藤 一郎"));
        QCOMPARE(infoValue("棋戦", "平手"), QStringLiteral("平手"));
        QCOMPARE(infoValue("手合割", "独自ルール"), QStringLiteral("独自ルール"));
        QCOMPARE(infoValue("持ち時間", "10:00+10"), QStringLiteral("10:00+10"));
        QCOMPARE(infoValue("持ち時間", "05:00+10秒加算"), QStringLiteral("05:00+10秒加算"));
        QCOMPARE(infoValue("持ち時間", "各10分"), QStringLiteral("各10分"));
        QCOMPARE(infoValue("持ち時間", "下手 01:00+2 / 上手 05:00+10秒加算"),
                 QStringLiteral("下手 01:00+2 / 上手 05:00+10秒加算"));
        QCOMPARE(status("投了についてのコメント"), QStringLiteral("投了についてのコメント"));
    }
};
QTEST_GUILESS_MAIN(TestKifuPresentation)
#include "tst_kifupresentation.moc"
