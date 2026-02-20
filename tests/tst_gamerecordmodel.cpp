#include <QtTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QTemporaryFile>

#include "gamerecordmodel.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifdisplayitem.h"
#include "kiftosfenconverter.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestGameRecordModel : public QObject
{
    Q_OBJECT

private:
    void setupBasicModel(GameRecordModel& model, KifuBranchTree& tree,
                         KifuNavigationState& navState, QList<KifDisplayItem>& disp)
    {
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* current = tree.root();

        QStringList moves = {
            QStringLiteral("▲７六歩"), QStringLiteral("△３四歩"),
            QStringLiteral("▲２六歩"), QStringLiteral("△８四歩"),
            QStringLiteral("▲２五歩"), QStringLiteral("△８五歩"),
            QStringLiteral("▲７八金")
        };

        for (int i = 0; i < moves.size(); ++i) {
            current = tree.addMove(current, move, moves[i],
                                   QStringLiteral("sfen%1").arg(i + 1));
        }

        disp.clear();
        disp.append(KifDisplayItem(QStringLiteral("開始局面")));
        for (int i = 0; i < moves.size(); ++i) {
            disp.append(KifDisplayItem(moves[i], QString(), QString(), i + 1));
        }

        navState.setTree(&tree);
        navState.goToRoot();

        model.setBranchTree(&tree);
        model.setNavigationState(&navState);
        model.bind(&disp);
        model.initializeFromDisplayItems(disp, static_cast<int>(disp.size()));
    }

private slots:
    // === Comment Management ===

    void setComment_roundTrip()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(1, QStringLiteral("初手のコメント"));
        QCOMPARE(model.comment(1), QStringLiteral("初手のコメント"));
    }

    void commentChanged_signal()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        QSignalSpy spy(&model, &GameRecordModel::commentChanged);
        QVERIFY(spy.isValid());

        model.setComment(2, QStringLiteral("テストコメント"));
        QCOMPARE(spy.count(), 1);
    }

    // === Bookmark Management ===

    void setBookmark_roundTrip()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setBookmark(3, QStringLiteral("重要局面"));
        QCOMPARE(model.bookmark(3), QStringLiteral("重要局面"));
    }

    // === Initialize / Clear ===

    void clear_resetsAll()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(1, QStringLiteral("test"));
        model.clear();

        QCOMPARE(model.commentCount(), 0);
        QVERIFY(!model.isDirty());
    }

    // === Dirty Flag ===

    void dirty_flag()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        QVERIFY(!model.isDirty());
        model.setComment(1, QStringLiteral("comment"));
        QVERIFY(model.isDirty());
        model.clearDirty();
        QVERIFY(!model.isDirty());
    }

    // === Export: KIF ===

    void toKifLines()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = model.toKifLines(ctx);
        QVERIFY(!lines.isEmpty());

        // Should contain "手合割：平手"
        bool foundHandicap = false;
        for (const auto& line : std::as_const(lines)) {
            if (line.contains(QStringLiteral("手合割"))) {
                foundHandicap = true;
                break;
            }
        }
        QVERIFY(foundHandicap);
    }

    // === Export: KI2 ===

    void toKi2Lines()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = model.toKi2Lines(ctx);
        QVERIFY(!lines.isEmpty());

        // Should contain ▲ or △ markers
        bool foundMarker = false;
        for (const auto& line : std::as_const(lines)) {
            if (line.contains(QStringLiteral("▲")) || line.contains(QStringLiteral("△"))) {
                foundMarker = true;
                break;
            }
        }
        QVERIFY(foundMarker);
    }

    // === Export: JKF ===

    void toJkfLines()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = model.toJkfLines(ctx);
        QVERIFY(!lines.isEmpty());

        // Should be valid JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(lines.join(QString()).toUtf8(), &parseError);
        QVERIFY2(parseError.error == QJsonParseError::NoError,
                 qPrintable(parseError.errorString()));
        QVERIFY(doc.isObject());
    }

    // === Export: USEN ===

    void toUsenLines()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        QStringList usiMoves = {
            QStringLiteral("7g7f"), QStringLiteral("3c3d"),
            QStringLiteral("2g2f"), QStringLiteral("8c8d"),
            QStringLiteral("2f2e"), QStringLiteral("8d8e"),
            QStringLiteral("6i7h")
        };
        auto lines = model.toUsenLines(ctx, usiMoves);
        QVERIFY(!lines.isEmpty());
        // USEN output typically starts with "~0."
        QVERIFY(lines.first().startsWith(QStringLiteral("~")));
    }

    // === Branch '+' marker (Kifu for Windows compatible) ===

    void toKifLines_branchPlusMarker()
    {
        // 分岐ツリー構造（ユーザー提示の棋譜を再現）:
        //
        //   root → ▲７六歩 → △３四歩 → ▲２六歩 → △８四歩 → ▲２五歩 → △投了
        //                              ├→ ▲１六歩 → △９四歩 → ▲４六歩 → △７四歩
        //                              │                      └→ ▲５六歩 → △５二金 → ▲５五歩
        //                              └→ ▲７七桂 → △９二香
        //                                                     └→ ▲１六歩 → △８五歩 → ▲投了
        //
        // Kifu for Windows準拠の '+' マーク期待値:
        //   本譜: ply3(▲２六歩)='+', ply5(▲２五歩)='+'
        //   変化5手(▲１六歩): 先頭='+なし' (最後の兄弟)
        //   変化3手(▲１六歩): ply3='+' (▲７七桂が後続), ply5(▲４六歩)='+' (▲５六歩が後続)
        //   変化5手(▲５六歩): 先頭='+なし' (最後の兄弟)
        //   変化3手(▲７七桂): 先頭='+なし' (最後の兄弟)

        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;

        // 本譜
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("▲７六歩(77)"), QStringLiteral("s1"),
                                QStringLiteral("( 0:01/00:00:01)"));
        auto* n2 = tree.addMove(n1, move, QStringLiteral("△３四歩(33)"), QStringLiteral("s2"),
                                QStringLiteral("( 0:02/00:00:02)"));
        auto* n3 = tree.addMove(n2, move, QStringLiteral("▲２六歩(27)"), QStringLiteral("s3"),
                                QStringLiteral("( 0:02/00:00:03)"));
        auto* n4 = tree.addMove(n3, move, QStringLiteral("△８四歩(83)"), QStringLiteral("s4"),
                                QStringLiteral("( 0:01/00:00:03)"));
        auto* n5 = tree.addMove(n4, move, QStringLiteral("▲２五歩(26)"), QStringLiteral("s5"),
                                QStringLiteral("( 0:01/00:00:04)"));
        tree.addTerminalMove(n5, TerminalType::Resign, QStringLiteral("△投了"),
                             QStringLiteral("( 0:03/00:00:06)"));

        // 変化：5手（本譜の▲２五歩の兄弟）
        auto* v5 = tree.addMove(n4, move, QStringLiteral("▲１六歩(17)"), QStringLiteral("v5"),
                                QStringLiteral("( 0:05/00:00:08)"));
        auto* v5b = tree.addMove(v5, move, QStringLiteral("△８五歩(84)"), QStringLiteral("v5b"),
                                 QStringLiteral("( 0:04/00:00:07)"));
        tree.addTerminalMove(v5b, TerminalType::Resign, QStringLiteral("▲投了"),
                             QStringLiteral("( 0:02/00:00:10)"));

        // 変化：3手（本譜の▲２六歩の兄弟1: ▲１六歩）
        auto* v3a = tree.addMove(n2, move, QStringLiteral("▲１六歩(17)"), QStringLiteral("v3a"),
                                 QStringLiteral("( 0:00/00:00:01)"));
        auto* v3a4 = tree.addMove(v3a, move, QStringLiteral("△９四歩(93)"), QStringLiteral("v3a4"),
                                  QStringLiteral("( 0:00/00:00:02)"));
        auto* v3a5 = tree.addMove(v3a4, move, QStringLiteral("▲４六歩(47)"), QStringLiteral("v3a5"),
                                  QStringLiteral("( 0:00/00:00:01)"));
        tree.addMove(v3a5, move, QStringLiteral("△７四歩(73)"), QStringLiteral("v3a6"),
                     QStringLiteral("( 0:00/00:00:02)"));

        // 変化：5手（▲１六歩ラインの▲４六歩の兄弟: ▲５六歩）
        auto* v3a5b = tree.addMove(v3a4, move, QStringLiteral("▲５六歩(57)"), QStringLiteral("v3a5b"),
                                   QStringLiteral("( 0:00/00:00:01)"));
        auto* v3a5b6 = tree.addMove(v3a5b, move, QStringLiteral("△５二金(61)"), QStringLiteral("v3a5b6"),
                                    QStringLiteral("( 0:00/00:00:02)"));
        tree.addMove(v3a5b6, move, QStringLiteral("▲５五歩(56)"), QStringLiteral("v3a5b7"),
                     QStringLiteral("( 0:00/00:00:01)"));

        // 変化：3手（本譜の▲２六歩の兄弟2: ▲７七桂）
        auto* v3b = tree.addMove(n2, move, QStringLiteral("▲７七桂(89)"), QStringLiteral("v3b"),
                                 QStringLiteral("( 0:00/00:00:01)"));
        tree.addMove(v3b, move, QStringLiteral("△９二香(91)"), QStringLiteral("v3b4"),
                     QStringLiteral("( 0:00/00:00:02)"));

        // モデルセットアップ
        GameRecordModel model;
        KifuNavigationState navState;
        navState.setTree(&tree);
        navState.goToRoot();
        model.setBranchTree(&tree);
        model.setNavigationState(&navState);

        QList<KifDisplayItem> disp;
        disp.append(KifDisplayItem(QStringLiteral("開始局面")));
        auto mainNodes = tree.allLines().at(0).nodes;
        for (int i = 1; i < mainNodes.size(); ++i) {
            auto* node = mainNodes.at(i);
            disp.append(KifDisplayItem(node->displayText(), node->timeText(), QString(), node->ply()));
        }
        model.bind(&disp);
        model.initializeFromDisplayItems(disp, static_cast<int>(disp.size()));

        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = model.toKifLines(ctx);

        // '+' マーク付き行と '+' マークなし行を検証するヘルパー
        auto findLine = [&lines](const QString& keyword) -> QString {
            for (const auto& line : std::as_const(lines)) {
                if (line.contains(keyword)) return line;
            }
            return {};
        };

        // 本譜: ply3(▲２六歩) は '+' あり（▲１六歩,▲７七桂が後続）
        QString mainPly3 = findLine(QStringLiteral("２六歩(27)"));
        QVERIFY2(mainPly3.endsWith(QStringLiteral("+")),
                 qPrintable(QStringLiteral("Main ply3 should have '+': ") + mainPly3));

        // 本譜: ply5(▲２五歩) は '+' あり（▲１六歩が後続）
        QString mainPly5 = findLine(QStringLiteral("２五歩(26)"));
        QVERIFY2(mainPly5.endsWith(QStringLiteral("+")),
                 qPrintable(QStringLiteral("Main ply5 should have '+': ") + mainPly5));

        // 変化5手の▲１六歩(17): '+' なし（最後の兄弟）
        // ▲１六歩(17) は2箇所あるので、「変化：5手」の直後の行を探す
        bool foundVar5 = false;
        for (int i = 0; i < lines.size() - 1; ++i) {
            if (lines.at(i) == QStringLiteral("変化：5手")) {
                const QString& nextLine = lines.at(i + 1);
                if (nextLine.contains(QStringLiteral("１六歩(17)"))) {
                    QVERIFY2(!nextLine.endsWith(QStringLiteral("+")),
                             qPrintable(QStringLiteral("Var5 16fu should NOT have '+': ") + nextLine));
                    foundVar5 = true;
                    break;
                }
            }
        }
        QVERIFY2(foundVar5, "Should find variation at ply 5 with 16fu");

        // 変化3手の▲１六歩(17): '+' あり（▲７七桂が後続兄弟）
        bool foundVar3a = false;
        for (int i = 0; i < lines.size() - 1; ++i) {
            if (lines.at(i) == QStringLiteral("変化：3手")) {
                const QString& nextLine = lines.at(i + 1);
                if (nextLine.contains(QStringLiteral("１六歩(17)"))) {
                    QVERIFY2(nextLine.endsWith(QStringLiteral("+")),
                             qPrintable(QStringLiteral("Var3a 16fu should have '+': ") + nextLine));
                    foundVar3a = true;
                    break;
                }
            }
        }
        QVERIFY2(foundVar3a, "Should find variation at ply 3 with 16fu");

        // 変化3手の▲７七桂(89): '+' なし（最後の兄弟）
        bool foundVar3b = false;
        for (int i = 0; i < lines.size() - 1; ++i) {
            if (lines.at(i) == QStringLiteral("変化：3手")) {
                const QString& nextLine = lines.at(i + 1);
                if (nextLine.contains(QStringLiteral("７七桂(89)"))) {
                    QVERIFY2(!nextLine.endsWith(QStringLiteral("+")),
                             qPrintable(QStringLiteral("Var3b 77kei should NOT have '+': ") + nextLine));
                    foundVar3b = true;
                    break;
                }
            }
        }
        QVERIFY2(foundVar3b, "Should find variation at ply 3 with 77kei");

        // 変化5手の▲５六歩(57): '+' なし（最後の兄弟）
        bool foundVar5b = false;
        for (int i = 0; i < lines.size() - 1; ++i) {
            if (lines.at(i) == QStringLiteral("変化：5手")) {
                const QString& nextLine = lines.at(i + 1);
                if (nextLine.contains(QStringLiteral("５六歩(57)"))) {
                    QVERIFY2(!nextLine.endsWith(QStringLiteral("+")),
                             qPrintable(QStringLiteral("Var5b 56fu should NOT have '+': ") + nextLine));
                    foundVar5b = true;
                    break;
                }
            }
        }
        QVERIFY2(foundVar5b, "Should find variation at ply 5 with 56fu");
    }
};

QTEST_MAIN(TestGameRecordModel)
#include "tst_gamerecordmodel.moc"
