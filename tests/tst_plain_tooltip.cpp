/// @file tst_plain_tooltip.cpp
/// @brief 外から来た文字列をツールチップに文字のまま出すことのテスト（監査 2026-11-01 M12・セキュリティ監査 S12）
///
/// Qt のツールチップは、タグらしいものがあると HTML として描く。棋譜のコメント・対局情報・
/// エンジン名などは plainToolTip() で包み、HTML として読ませず、空白と改行もそのまま出す。

#include <QtTest>
#include <QAction>
#include <QComboBox>
#include <QLabel>
#include <QJsonArray>
#include <QJsonObject>
#include <QTableWidget>
#include <QTemporaryDir>

#include "automationwidgets.h"
#include "branchnodeitem.h"
#include "branchtree_test_util.h"
#include "elidelabel.h"
#include "engineinfowidget.h"
#include "engineprofilestore.h"
#include "engineselector.h"
#include "gameinfoeditmodel.h"
#include "globaltooltip.h"
#include "josekitablemodel.h"
#include "kifubranchtree.h"
#include "kifurecordlistmodel.h"
#include "menubuttonwidget.h"
#include "plaintooltip.h"
#include "sfenutils.h"
#include "usicommlogmodel.h"

namespace {

/// タグ・記号・連続した空白・空行を含む、外から来た文字列
const QString kExternal = QStringLiteral("<img src=\"/dev/zero\"><b>太字</b> A & B\n  二行目  a   b\n\n四行目");

/// 利用者に見えるツールチップの文字（Qt と同じく、タグらしいものがあれば HTML として描く）
QString shownText(const QString& toolTip)
{
    return shownToolTipText(toolTip);
}

} // namespace

class TestPlainToolTip : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        QCoreApplication::setApplicationName(QStringLiteral("PlainToolTipTest"));
    }

    /// 見える文字: タグの無い文字はそのまま、タグらしいものがあれば HTML として描いた文字（画像は U+FFFC）
    void shownToolTipText_rendersLikeQt()
    {
        QCOMPARE(shownToolTipText(QStringLiteral("A &amp; B")), QStringLiteral("A &amp; B"));
        QCOMPARE(shownToolTipText(QStringLiteral("<b>A</b> &amp; B")), QStringLiteral("A & B"));
        QCOMPARE(shownToolTipText(QStringLiteral("<img src=\"/dev/zero\">x")), QString(QChar::ObjectReplacementCharacter) + QLatin1Char('x'));
    }

    /// 文字はエスケープし、空白と改行もそのまま出す。空なら空（ツールチップを出さない）
    void plainToolTip_showsTextAsIs()
    {
        const QString tip = plainToolTip(kExternal);
        QVERIFY(Qt::mightBeRichText(tip));   // 「&lt;」が見えないよう、必ず HTML として読ませる
        QCOMPARE(shownText(tip), kExternal);
        QCOMPARE(shownText(plainToolTip(QStringLiteral("A&B"))), QStringLiteral("A&B"));
        QVERIFY(plainToolTip(QString()).isEmpty());
        QVERIFY(plainToolTip(QStringList{QString(), QString()}).isEmpty());
    }

    /// 段は区切り線で分け、空の段は飛ばす
    void plainToolTip_joinsSectionsWithRule()
    {
        const QString tip = plainToolTip(QStringList{QStringLiteral("▲７六歩"), QString(), kExternal});
        QCOMPARE(tip.count(QStringLiteral("<hr/>")), 1);
        // 区切り線は、文字に直すと空の1行になる
        QCOMPARE(shownText(tip), QStringLiteral("▲７六歩\n\n") + kExternal);
        QCOMPARE(plainToolTip(QStringList{QString(), QStringLiteral("a")}), plainToolTip(QStringLiteral("a")));
    }

    /// 棋譜欄のコメントの列（棋譜のコメント）
    void kifuComment()
    {
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        TestTree::record(tree.root()).comment = kExternal;
        KifuRecordListModel rows(&tree);
        rows.setPath(tree.mainLine());
        QCOMPARE(shownText(rows.data(rows.index(0, KifuRecordListModel::CommentColumn), Qt::ToolTipRole).toString()),
                 kExternal);
    }

    /// 対局情報の値（棋譜の対局情報）
    void gameInfoValue()
    {
        GameInfoEditModel model;
        model.setItems({{QStringLiteral("棋戦"), kExternal}});
        QCOMPARE(shownText(model.data(model.index(0, GameInfoEditModel::ValueColumn), Qt::ToolTipRole).toString()),
                 kExternal);
    }

    /// 定跡ファイルのコメント
    void josekiComment()
    {
        JosekiMove move;
        move.move = QStringLiteral("7g7f");
        move.comment = kExternal;
        JosekiTableModel model;
        model.setMoves(SfenUtils::hirateSfen(), {move}, true);
        QCOMPARE(shownText(model.index(0, JosekiTableModel::Comment).data(Qt::ToolTipRole).toString()), kExternal);
    }

    /// 分岐ツリーの手のコメント（手の説明と区切り線で分ける）
    void branchNodeComment()
    {
        BranchNodeItem::Content content;
        content.label = QStringLiteral("７六歩");
        content.tooltip = QStringLiteral("▲７六歩(77)");
        BranchNodeItem item(1, 0, 1, content, QPointF());
        item.setComment(QStringLiteral("  ") + kExternal);   // 前後の空白はコメントの規則で除く
        QCOMPARE(shownText(item.toolTip()), QStringLiteral("▲７六歩(77)\n\n") + kExternal);
        // 折りたたんだ手は、最後の段の次の行に展開の案内を足す
        item.setCollapsedBadge(3, Qt::black);
        QCOMPARE(shownText(item.toolTip()), QStringLiteral("▲７六歩(77)\n\n") + kExternal
                                                + QStringLiteral("\n（ダブルクリックで展開）"));
    }

    /// エンジン名（登録したエンジンが名乗る名前）: 選ぶコンボ・情報欄・省略表示のラベル
    void engineNames()
    {
        EngineProfileStore::saveEngines({{kExternal, QStringLiteral("/engines/evil"), QStringLiteral("Author")}});
        QComboBox combo;
        EngineSelector selector({&combo, nullptr, nullptr}, EngineSelector::Options{}, nullptr);
        QCOMPARE(combo.count(), 1);
        QCOMPARE(shownText(combo.itemData(0, Qt::ToolTipRole).toString()), kExternal);
        QCOMPARE(shownText(combo.toolTip()), kExternal);

        UsiCommLogModel log;
        EngineInfoWidget info;
        info.setModel(&log);
        log.setEngineName(kExternal);
        auto* name = info.findChild<ElideLabel*>(QStringLiteral("engineInfoName"));
        QVERIFY(name);
        QCOMPARE(shownText(name->toolTip()), kExternal);

        ElideLabel label;
        label.setFullText(kExternal);
        QCOMPARE(shownText(label.toolTip()), kExternal);
    }

    /// メニューウィンドウのボタン（項目名と説明を改行で分ける）
    void menuButton()
    {
        QAction action(QStringLiteral("A && B <局面>"));   // && は表示用の &
        action.setToolTip(QStringLiteral("説明  2つの空白"));
        MenuButtonWidget button(&action);
        QCOMPARE(shownText(button.toolTip()), QStringLiteral("A & B <局面>\n説明  2つの空白"));
    }

    /// 自動化 API の部品の一覧は、ツールチップを利用者に見える文字で返す（HTML で包んだ形を返さない）
    void automationReturnsShownToolTip()
    {
        QWidget root;
        auto* label = new ElideLabel(&root);
        label->setObjectName(QStringLiteral("engineName"));
        label->setFullText(kExternal);
        const QJsonArray widgets = AutomationWidgets::describeWidgets(&root, QStringLiteral("engineName"), 10);
        QCOMPARE(widgets.size(), 1);
        QCOMPARE(widgets.at(0).toObject().value(QStringLiteral("tooltip")).toString(), kExternal);
    }

    /// 対局者名のツールチップ（GlobalToolTip は PlainText のラベルで、エスケープした文字を見せない）
    void globalToolTipShowsNameAsIs()
    {
        GlobalToolTip tip;
        tip.showText(QPoint(0, 0), kExternal);
        auto* label = tip.findChild<QLabel*>();
        QVERIFY(label);
        QCOMPARE(label->textFormat(), Qt::PlainText);
        QCOMPARE(label->text(), kExternal);
    }
};

QTEST_MAIN(TestPlainToolTip)
#include "tst_plain_tooltip.moc"
