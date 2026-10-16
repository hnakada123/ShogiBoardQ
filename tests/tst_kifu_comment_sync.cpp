/// @file tst_kifu_comment_sync.cpp
/// @brief 棋譜コメント・しおりのテスト
///
/// コメント・しおりは分岐ツリー（棋譜の正本）のノードにだけ入る。書き換えはノードを指定して行い、
/// 書き出しはノードから読む。書き換えは文書（KifuDocument）を通し、変わったときだけ未保存にして、ノード ID 付きで通知する。

#include <QtTest>
#include <QSignalSpy>

#include "kifudocument.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "recordline.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestKifuCommentSync : public QObject
{
    Q_OBJECT

private:
    /// 7手の本譜を作る
    static void setupBasicTree(KifuBranchTree& tree)
    {
        tree.setRootSfen(kHirateSfen);
        const QStringList moves = {
            QStringLiteral("▲７六歩"), QStringLiteral("△３四歩"),
            QStringLiteral("▲２六歩"), QStringLiteral("△８四歩"),
            QStringLiteral("▲２五歩"), QStringLiteral("△８五歩"),
            QStringLiteral("▲７八金")
        };
        KifuBranchNode* current = tree.root();
        for (qsizetype i = 0; i < moves.size(); ++i) {
            current = tree.addMove(current, ShogiMove(), moves.at(i), QStringLiteral("sfen%1").arg(i + 1));
        }
    }

private slots:
    /// 本譜のノードに書いたコメント・しおりは、そのノードと本譜の書き出しに入る
    void setComment_writesNodeAndExport()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);

        document.setComment(tree.mainLine().at(1), QStringLiteral("初手のコメント"));
        document.setBookmark(tree.mainLine().at(2), QStringLiteral("重要局面"));

        QCOMPARE(tree.mainLine().at(1)->comment(), QStringLiteral("初手のコメント"));
        QCOMPARE(tree.mainLine().at(2)->bookmark(), QStringLiteral("重要局面"));
        const RecordLine exported = tree.exportLine(0);
        QCOMPARE(exported.entries.at(0).comment, QStringLiteral("初手のコメント"));
        QCOMPARE(exported.entries.at(1).bookmark, QStringLiteral("重要局面"));
    }

    /// 同じ手数でも、分岐のノードと本譜のノードの注釈は別々に持つ
    void branchCommentsAndBookmarksStayOnTheirNodes()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);
        KifuBranchNode* main = tree.mainLine().at(2);
        KifuBranchNode* branch = tree.addMove(tree.mainLine().at(1), ShogiMove(),
                                              QStringLiteral("△８四歩"), QStringLiteral("branch"));
        QVERIFY(branch);

        document.setComment(main, QStringLiteral("本譜コメント"));
        document.setBookmark(main, QStringLiteral("本譜しおり"));
        document.setComment(branch, QStringLiteral("分岐コメント"));
        document.setBookmark(branch, QStringLiteral("分岐しおり"));
        QCOMPARE(branch->comment(), QStringLiteral("分岐コメント"));
        QCOMPARE(branch->bookmark(), QStringLiteral("分岐しおり"));
        QCOMPARE(main->comment(), QStringLiteral("本譜コメント"));
        QCOMPARE(tree.exportLine(0).entries.at(1).comment, QStringLiteral("本譜コメント"));
        QCOMPARE(tree.exportLine(0).entries.at(1).bookmark, QStringLiteral("本譜しおり"));

        document.setComment(main, {});
        document.setBookmark(main, {});
        QCOMPARE(branch->comment(), QStringLiteral("分岐コメント"));
        QVERIFY(tree.exportLine(0).entries.at(1).comment.isEmpty());
        QVERIFY(tree.exportLine(0).entries.at(1).bookmark.isEmpty());

        // 分岐の手前の共通の手は本譜の書き出しにも入る
        document.setComment(tree.mainLine().at(1), QStringLiteral("共通の親"));
        QCOMPARE(tree.exportLine(0).entries.at(0).comment, QStringLiteral("共通の親"));
    }

    /// 追加・変更・空文字での削除
    void setComment_addModifyAndDelete()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);
        KifuBranchNode* node = tree.mainLine().at(3);

        document.setComment(node, QStringLiteral("最初"));
        QCOMPARE(node->comment(), QStringLiteral("最初"));
        document.setComment(node, QStringLiteral("変更後"));
        QCOMPARE(node->comment(), QStringLiteral("変更後"));
        document.setComment(node, QString());
        QVERIFY(node->comment().isEmpty());

        document.setBookmark(node, QStringLiteral("しおり"));
        QCOMPARE(node->bookmark(), QStringLiteral("しおり"));
        document.setBookmark(node, QString());
        QVERIFY(node->bookmark().isEmpty());
    }

    /// 変わったときだけノード ID 付きで通知し、未保存にする
    void changesEmitNodeIdAndMarkDirty()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);
        KifuBranchNode* node = tree.mainLine().at(4);
        QSignalSpy comments(&document, &KifuDocument::commentChanged);
        QSignalSpy bookmarks(&document, &KifuDocument::bookmarkChanged);
        QVERIFY(!document.isModified());

        document.setComment(node, QStringLiteral("コメント"));
        QCOMPARE(comments.count(), 1);
        QCOMPARE(comments.last().at(0).toInt(), node->nodeId());
        QCOMPARE(comments.last().at(1).toString(), QStringLiteral("コメント"));
        QVERIFY(document.isModified());

        document.markSaved(QString());
        document.setComment(node, QStringLiteral("コメント"));
        QCOMPARE(comments.count(), 1);
        QVERIFY(!document.isModified());

        document.setBookmark(node, QStringLiteral("しおり"));
        QCOMPARE(bookmarks.count(), 1);
        QCOMPARE(bookmarks.last().at(0).toInt(), node->nodeId());
        QVERIFY(document.isModified());
        document.markSaved(QString());
        document.setBookmark(node, QStringLiteral("しおり"));
        QCOMPARE(bookmarks.count(), 1);
        QVERIFY(!document.isModified());
        QCOMPARE(comments.count(), 1);
    }

    /// 別の手の注釈、同じ手のコメントとしおりは互いに影響しない
    void annotationsAreIndependent()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);
        const QList<KifuBranchNode*> line = tree.mainLine();

        document.setComment(line.at(1), QStringLiteral("1手目"));
        document.setComment(line.at(3), QStringLiteral("3手目"));
        document.setComment(line.at(7), QStringLiteral("7手目"));
        document.setBookmark(line.at(3), QStringLiteral("しおり"));
        document.setComment(line.at(3), QString());

        QCOMPARE(line.at(1)->comment(), QStringLiteral("1手目"));
        QVERIFY(line.at(3)->comment().isEmpty());
        QCOMPARE(line.at(3)->bookmark(), QStringLiteral("しおり"));
        QCOMPARE(line.at(7)->comment(), QStringLiteral("7手目"));
        QVERIFY(line.at(2)->comment().isEmpty());
    }

    /// ノードが無ければ何もしない（未保存にもしない）
    void nullNodeIsIgnored()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);
        QSignalSpy comments(&document, &KifuDocument::commentChanged);
        document.setComment(nullptr, QStringLiteral("x"));
        document.setBookmark(nullptr, QStringLiteral("x"));
        QCOMPARE(comments.count(), 0);
        QVERIFY(!document.isModified());
    }
};

QTEST_MAIN(TestKifuCommentSync)
#include "tst_kifu_comment_sync.moc"
