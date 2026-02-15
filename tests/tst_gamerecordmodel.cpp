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
};

QTEST_MAIN(TestGameRecordModel)
#include "tst_gamerecordmodel.moc"
