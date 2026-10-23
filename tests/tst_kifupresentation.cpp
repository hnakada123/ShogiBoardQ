#include <QtTest>
#include <QTranslator>
#include "kifupresentation.h"
#include "pvtext.h"
#include "recordentry.h"
#include "sfenreplay.h"
#include "sfenutils.h"
#include "test_replay.h"

using namespace KifuPresentation;

class TestKifuPresentation : public QObject
{
    Q_OBJECT
private slots:
    void cleanup() { NotationStyle::configure(QStringLiteral("ja_JP"), QStringLiteral("auto"), false); }

    void languageAndCoordinates()
    {
        QCOMPARE(NotationStyle::resolveLanguage("system", "zh_Hant_HK"), QStringLiteral("zh_TW"));
        QCOMPARE(NotationStyle::resolveLanguage("system", "zh_CN"), QStringLiteral("zh_CN"));
        QCOMPARE(NotationStyle::resolveLanguage("system", "en_US"), QStringLiteral("en"));
        QCOMPARE(NotationStyle::resolveLanguage("system", "fr_FR"), QStringLiteral("en"));
        NotationStyle::configure("en", "auto", false);
        QCOMPARE(rankLabel(1), QStringLiteral("a"));
        QCOMPARE(rankLabel(9), QStringLiteral("i"));
        NotationStyle::configure("zh_CN", "auto", false);
        QCOMPARE(rankLabel(6), QStringLiteral("六"));
        NotationStyle::configure("ja_JP", "western", false);
        QCOMPARE(rankLabel(6), QStringLiteral("f"));
        NotationStyle::configure("en", "japanese", false);
        QCOMPARE(rankLabel(6), QStringLiteral("六"));
    }

    void westernMoves()
    {
        const Options compact{Notation::Western, false};
        const Options full{Notation::Western, true};
        QString sfen = SfenUtils::hirateSfen();
        QCOMPARE(move(sfen, "7g7f", compact), QStringLiteral("▲P-7f"));
        QCOMPARE(move(sfen, "7g7f", full), QStringLiteral("▲P7g-7f"));
        sfen = SfenReplay::positionAfter(sfen, "7g7f");
        QCOMPARE(move(sfen, "3c3d", compact), QStringLiteral("△P-3d"));
        sfen = SfenReplay::positionAfter(sfen, "3c3d");
        QCOMPARE(move(sfen, "8h2b+", compact), QStringLiteral("▲Bx2b+"));
        QCOMPARE(move(sfen, "8h2b", compact), QStringLiteral("▲Bx2b="));
        const QString promoted = "4k4/9/9/4+B4/9/9/9/9/4K4 b P 1";
        QCOMPARE(move(promoted, "5d4d", compact), QStringLiteral("▲+B-4d"));
        QCOMPARE(move(promoted, "P*7f", compact), QStringLiteral("▲P*7f"));
        const QString ambiguous = "4k4/9/9/9/9/9/9/9/3GKG3 b - 1";
        QCOMPARE(move(ambiguous, "6i5h", compact), QStringLiteral("▲G6i-5h"));
        // 移動元の要否は KI2 の修飾語と同じく合法手で決める。ピンされて動けない金は数えない
        const QString pinned = "4r3k/9/9/9/9/9/5G3/4G4/4K4 b - 1";
        QCOMPARE(move(pinned, "4g4h", compact), QStringLiteral("▲G-4h"));
        // 日本語表記は棋譜と同じ KifMoveText（手番の印は includeSide で付け外しする）
        const Options japanese{Notation::Japanese, false};
        QCOMPARE(move(SfenUtils::hirateSfen(), "7g7f", japanese), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(move(SfenUtils::hirateSfen(), "7g7f", japanese, false), QStringLiteral("７六歩(77)"));
    }

    void handicapPvAndCanonicalPreservation()
    {
        NotationStyle::configure("en", "auto", false);
        const QString hirate = SfenUtils::hirateSfen();
        QString whiteStart = hirate;
        whiteStart.replace(" b ", " w ");
        QCOMPARE(pv(PvText::play(whiteStart, QStringLiteral("3c3d 7g7f"))), QStringLiteral("△P-3d ▲P-7f"));
        QCOMPARE(numberedLabel(1, moveLabel("▲７六歩(77)", hirate, "7g7f"), true), QStringLiteral("   1 ▲P-7f [+]"));
        // 棋譜の1行は値から作る（手・終局の種類・指した側。表示の文字列を読み直さない）
        const auto moves = TestReplay::buildGameMoves(hirate, {"7g7f", "3c3d", "8h2b+"});
        const RecordEntry first = RecordEntry::played(moves.at(0), QStringLiteral("▲７六歩(77)"), QString());
        QCOMPARE(entryLabel(first, hirate), QStringLiteral("▲P-7f"));
        QCOMPARE(entryLabel(RecordEntry::ending(TerminalType::Resign, Turn::White), hirate), QStringLiteral("△投了"));
        NotationStyle::configure("zh_CN", "auto", false);
        const QString canonical = QStringLiteral("▲同　飛成(28)");
        QCOMPARE(moveLabel(canonical, hirate, "2h2b+"), canonical);
        QCOMPARE(entryLabel(first, hirate), QStringLiteral("▲７六歩(77)"));
        // 日本語表記で棋譜の表記が無ければ指し手から作る
        QCOMPARE(entryLabel(RecordEntry::played(moves.at(0), QString(), QString()), hirate), QStringLiteral("▲７六歩(77)"));
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
    }
};
QTEST_GUILESS_MAIN(TestKifuPresentation)
#include "tst_kifupresentation.moc"
