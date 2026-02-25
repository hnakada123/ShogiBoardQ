/// @file tst_kifu_comment_sync.cpp
/// @brief 棋譜コメント・しおり同期のテスト
///
/// GameRecordModel と外部データストア（liveDisp / KifuBranchTree）の
/// 同期整合をテストし、リファクタリングによる回帰を検出する。

#include <QtTest>
#include <QSignalSpy>

#include "gamerecordmodel.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifdisplayitem.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestKifuCommentSync : public QObject
{
    Q_OBJECT

private:
    /// テスト用の基本セットアップ（7手の棋譜）
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

    // ================================================================
    // Scenario 1: Model と liveDisp の同期整合
    // ================================================================

    /// setComment で liveDisp にコメントが反映されること
    void setComment_syncsToLiveDisp()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(1, QStringLiteral("初手のコメント"));

        // liveDisp にも反映されている
        QCOMPARE(disp[1].comment, QStringLiteral("初手のコメント"));
        // Model 内部とも一致
        QCOMPARE(model.comment(1), QStringLiteral("初手のコメント"));
    }

    /// setBookmark で liveDisp にしおりが反映されること
    void setBookmark_syncsToLiveDisp()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setBookmark(2, QStringLiteral("重要局面"));

        // liveDisp にも反映されている
        QCOMPARE(disp[2].bookmark, QStringLiteral("重要局面"));
        // Model 内部とも一致
        QCOMPARE(model.bookmark(2), QStringLiteral("重要局面"));
    }

    /// setComment で KifuBranchTree ノードにもコメントが反映されること
    void setComment_syncsToBranchTree()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(3, QStringLiteral("分岐ノードのコメント"));

        // BranchTree のノードにも反映されている
        QVector<BranchLine> lines = tree.allLines();
        QVERIFY(!lines.isEmpty());
        const BranchLine& mainLine = lines.at(0);
        QVERIFY(mainLine.nodes.size() > 3);
        QCOMPARE(mainLine.nodes[3]->comment(), QStringLiteral("分岐ノードのコメント"));
    }

    /// setBookmark で KifuBranchTree ノードにもしおりが反映されること
    void setBookmark_syncsToBranchTree()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setBookmark(4, QStringLiteral("ブックマーク"));

        // BranchTree のノードにも反映されている
        QVector<BranchLine> lines = tree.allLines();
        QVERIFY(!lines.isEmpty());
        const BranchLine& mainLine = lines.at(0);
        QVERIFY(mainLine.nodes.size() > 4);
        QCOMPARE(mainLine.nodes[4]->bookmark(), QStringLiteral("ブックマーク"));
    }

    // ================================================================
    // Scenario 2: コメント更新の双方向反映
    // ================================================================

    /// コメントの追加・変更が正しく反映されること
    void setComment_addAndModify()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        // 追加
        model.setComment(0, QStringLiteral("開始局面のコメント"));
        QCOMPARE(model.comment(0), QStringLiteral("開始局面のコメント"));
        QCOMPARE(disp[0].comment, QStringLiteral("開始局面のコメント"));

        // 変更
        model.setComment(0, QStringLiteral("変更後のコメント"));
        QCOMPARE(model.comment(0), QStringLiteral("変更後のコメント"));
        QCOMPARE(disp[0].comment, QStringLiteral("変更後のコメント"));
    }

    /// コメントの削除（空文字設定）が正しく反映されること
    void setComment_deleteByEmptyString()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(1, QStringLiteral("一時的なコメント"));
        QCOMPARE(model.comment(1), QStringLiteral("一時的なコメント"));

        // 空文字でコメント削除
        model.setComment(1, QString());
        QCOMPARE(model.comment(1), QString());
        QCOMPARE(disp[1].comment, QString());
    }

    /// コメント変更時に commentChanged シグナルが発火すること
    void setComment_emitsSignalOnChange()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        QSignalSpy spy(&model, &GameRecordModel::commentChanged);
        QVERIFY(spy.isValid());

        model.setComment(2, QStringLiteral("新しいコメント"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("新しいコメント"));
    }

    /// 同じコメントを再設定した場合はシグナルが発火しないこと
    void setComment_noSignalOnSameValue()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(1, QStringLiteral("同じコメント"));

        QSignalSpy spy(&model, &GameRecordModel::commentChanged);
        QVERIFY(spy.isValid());

        // 同じ値を再設定
        model.setComment(1, QStringLiteral("同じコメント"));
        QCOMPARE(spy.count(), 0);
    }

    /// CommentUpdateCallback が変更時に呼ばれること
    void commentUpdateCallback_calledOnChange()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        int callbackPly = -1;
        QString callbackComment;
        model.setCommentUpdateCallback([&](int ply, const QString& comment) {
            callbackPly = ply;
            callbackComment = comment;
        });

        model.setComment(3, QStringLiteral("コールバックテスト"));
        QCOMPARE(callbackPly, 3);
        QCOMPARE(callbackComment, QStringLiteral("コールバックテスト"));
    }

    /// 同じ値の再設定時は CommentUpdateCallback が呼ばれないこと
    void commentUpdateCallback_notCalledOnSameValue()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(1, QStringLiteral("固定コメント"));

        int callCount = 0;
        model.setCommentUpdateCallback([&](int, const QString&) {
            ++callCount;
        });

        model.setComment(1, QStringLiteral("固定コメント"));
        QCOMPARE(callCount, 0);
    }

    /// 複数手のコメントが独立して管理されること
    void setComment_multipleMovesIndependent()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(0, QStringLiteral("開始局面"));
        model.setComment(1, QStringLiteral("1手目"));
        model.setComment(5, QStringLiteral("5手目"));

        QCOMPARE(model.comment(0), QStringLiteral("開始局面"));
        QCOMPARE(model.comment(1), QStringLiteral("1手目"));
        QCOMPARE(model.comment(2), QString());
        QCOMPARE(model.comment(5), QStringLiteral("5手目"));

        // liveDisp も同期
        QCOMPARE(disp[0].comment, QStringLiteral("開始局面"));
        QCOMPARE(disp[1].comment, QStringLiteral("1手目"));
        QCOMPARE(disp[2].comment, QString());
        QCOMPARE(disp[5].comment, QStringLiteral("5手目"));
    }

    // ================================================================
    // Scenario 3: しおり更新の双方向反映
    // ================================================================

    /// しおりの追加・変更が正しく反映されること
    void setBookmark_addAndModify()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        // 追加
        model.setBookmark(1, QStringLiteral("重要"));
        QCOMPARE(model.bookmark(1), QStringLiteral("重要"));
        QCOMPARE(disp[1].bookmark, QStringLiteral("重要"));

        // 変更
        model.setBookmark(1, QStringLiteral("非常に重要"));
        QCOMPARE(model.bookmark(1), QStringLiteral("非常に重要"));
        QCOMPARE(disp[1].bookmark, QStringLiteral("非常に重要"));
    }

    /// しおりの削除（空文字設定）が正しく反映されること
    void setBookmark_deleteByEmptyString()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setBookmark(2, QStringLiteral("一時マーク"));
        QCOMPARE(model.bookmark(2), QStringLiteral("一時マーク"));

        model.setBookmark(2, QString());
        QCOMPARE(model.bookmark(2), QString());
        QCOMPARE(disp[2].bookmark, QString());
    }

    /// BookmarkUpdateCallback が変更時に呼ばれること
    void bookmarkUpdateCallback_calledOnChange()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        int callbackPly = -1;
        QString callbackBookmark;
        model.setBookmarkUpdateCallback([&](int ply, const QString& bookmark) {
            callbackPly = ply;
            callbackBookmark = bookmark;
        });

        model.setBookmark(4, QStringLiteral("しおりテスト"));
        QCOMPARE(callbackPly, 4);
        QCOMPARE(callbackBookmark, QStringLiteral("しおりテスト"));
    }

    /// 同じ値の再設定時は BookmarkUpdateCallback が呼ばれないこと
    void bookmarkUpdateCallback_notCalledOnSameValue()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setBookmark(1, QStringLiteral("固定しおり"));

        int callCount = 0;
        model.setBookmarkUpdateCallback([&](int, const QString&) {
            ++callCount;
        });

        model.setBookmark(1, QStringLiteral("固定しおり"));
        QCOMPARE(callCount, 0);
    }

    /// しおりの追加が dirty フラグを立てること
    void setBookmark_setsDirtyFlag()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        QVERIFY(!model.isDirty());
        model.setBookmark(1, QStringLiteral("しおり"));
        QVERIFY(model.isDirty());
    }

    // ================================================================
    // Scenario 4: bind() の動作確認
    // ================================================================

    /// bind() でデータソースが正しくバインドされること
    void bind_setsDataSource()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        // bind 済みなので setComment で liveDisp が更新される
        model.setComment(1, QStringLiteral("バインド済み"));
        QCOMPARE(disp[1].comment, QStringLiteral("バインド済み"));
    }

    /// bind(nullptr) 後の setComment がクラッシュしないこと
    void bind_null_nocrash()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        // バインド解除
        model.bind(nullptr);

        // クラッシュしないことを確認
        model.setComment(1, QStringLiteral("バインド解除後"));

        // Model 内部は更新されている
        QCOMPARE(model.comment(1), QStringLiteral("バインド解除後"));

        // liveDisp は更新されていない（バインド解除済み）
        QCOMPARE(disp[1].comment, QString());
    }

    /// bind(nullptr) 後の setBookmark がクラッシュしないこと
    void bind_null_nocrash_bookmark()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.bind(nullptr);

        model.setBookmark(1, QStringLiteral("バインド解除後"));
        QCOMPARE(model.bookmark(1), QStringLiteral("バインド解除後"));
        QCOMPARE(disp[1].bookmark, QString());
    }

    /// 別の liveDisp に再バインドできること
    void bind_rebind_toNewDisp()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        // 新しい liveDisp を用意
        QList<KifDisplayItem> newDisp;
        for (int i = 0; i < 8; ++i) {
            newDisp.append(KifDisplayItem());
        }

        // 再バインド
        model.bind(&newDisp);

        model.setComment(2, QStringLiteral("新しいバインド先"));

        // 新しい liveDisp に反映されている
        QCOMPARE(newDisp[2].comment, QStringLiteral("新しいバインド先"));
        // 元の liveDisp は変更されない
        QCOMPARE(disp[2].comment, QString());
    }

    /// initializeFromDisplayItems がコメントを正しく読み込むこと
    void initializeFromDisplayItems_loadsComments()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;

        tree.setRootSfen(kHirateSfen);

        QList<KifDisplayItem> disp;
        disp.append(KifDisplayItem(QStringLiteral("開始局面"), QString(),
                                   QStringLiteral("開始時コメント")));
        KifDisplayItem item1(QStringLiteral("▲７六歩"), QString(),
                             QStringLiteral("1手目コメント"), 1);
        disp.append(item1);
        KifDisplayItem item2(QStringLiteral("△３四歩"), QString(), QString(), 2);
        disp.append(item2);

        navState.setTree(&tree);
        navState.goToRoot();
        model.setBranchTree(&tree);
        model.setNavigationState(&navState);
        model.bind(&disp);
        model.initializeFromDisplayItems(disp, static_cast<int>(disp.size()));

        QCOMPARE(model.comment(0), QStringLiteral("開始時コメント"));
        QCOMPARE(model.comment(1), QStringLiteral("1手目コメント"));
        QCOMPARE(model.comment(2), QString());
    }

    /// initializeFromDisplayItems がしおりを正しく読み込むこと
    void initializeFromDisplayItems_loadsBookmarks()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;

        tree.setRootSfen(kHirateSfen);

        QList<KifDisplayItem> disp;
        {
            KifDisplayItem item(QStringLiteral("開始局面"));
            item.bookmark = QStringLiteral("開始しおり");
            disp.append(item);
        }
        {
            KifDisplayItem item(QStringLiteral("▲７六歩"), QString(), QString(), 1);
            item.bookmark = QStringLiteral("1手目しおり");
            disp.append(item);
        }
        disp.append(KifDisplayItem(QStringLiteral("△３四歩"), QString(), QString(), 2));

        navState.setTree(&tree);
        navState.goToRoot();
        model.setBranchTree(&tree);
        model.setNavigationState(&navState);
        model.bind(&disp);
        model.initializeFromDisplayItems(disp, static_cast<int>(disp.size()));

        QCOMPARE(model.bookmark(0), QStringLiteral("開始しおり"));
        QCOMPARE(model.bookmark(1), QStringLiteral("1手目しおり"));
        QCOMPARE(model.bookmark(2), QString());
    }

    /// initializeFromDisplayItems 後は dirty フラグがクリアされていること
    void initializeFromDisplayItems_clearsDirtyFlag()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        // setupBasicModel 直後は dirty = false
        QVERIFY(!model.isDirty());

        // コメントを設定して dirty にする
        model.setComment(1, QStringLiteral("test"));
        QVERIFY(model.isDirty());

        // 再初期化で dirty がクリアされる
        model.initializeFromDisplayItems(disp, static_cast<int>(disp.size()));
        QVERIFY(!model.isDirty());
    }

    /// clear() 後に bind 済みの liveDisp へのアクセスがクラッシュしないこと
    void clear_thenSetComment_nocrash()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.clear();
        QCOMPARE(model.commentCount(), 0);

        // clear 後も bind は維持されている → setComment で容量拡張 + liveDisp 同期
        model.setComment(0, QStringLiteral("クリア後コメント"));
        QCOMPARE(model.comment(0), QStringLiteral("クリア後コメント"));
        QCOMPARE(disp[0].comment, QStringLiteral("クリア後コメント"));
    }

    /// ensureCommentCapacity で配列が正しく拡張されること
    void ensureCommentCapacity_extendsArray()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        // 初期サイズは 8（開始局面 + 7手）
        QCOMPARE(model.commentCount(), 8);

        // 範囲外にコメント設定 → 自動拡張
        model.setComment(15, QStringLiteral("遠い手のコメント"));
        QVERIFY(model.commentCount() > 15);
        QCOMPARE(model.comment(15), QStringLiteral("遠い手のコメント"));
    }

    /// ensureBookmarkCapacity で配列が正しく拡張されること
    void ensureBookmarkCapacity_extendsArray()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        // 範囲外にしおり設定 → 自動拡張
        model.setBookmark(20, QStringLiteral("遠い手のしおり"));
        QCOMPARE(model.bookmark(20), QStringLiteral("遠い手のしおり"));
    }

    /// 負の ply でコメント設定してもクラッシュしないこと
    void setComment_negativePly_nocrash()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        // 負の ply は無視される
        model.setComment(-1, QStringLiteral("不正な手数"));
        QCOMPARE(model.comment(-1), QString());
    }

    /// 負の ply でしおり設定してもクラッシュしないこと
    void setBookmark_negativePly_nocrash()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setBookmark(-1, QStringLiteral("不正な手数"));
        QCOMPARE(model.bookmark(-1), QString());
    }

    /// コメントとしおりが同一手で独立して管理されること
    void commentAndBookmark_independentOnSamePly()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(3, QStringLiteral("3手目コメント"));
        model.setBookmark(3, QStringLiteral("3手目しおり"));

        QCOMPARE(model.comment(3), QStringLiteral("3手目コメント"));
        QCOMPARE(model.bookmark(3), QStringLiteral("3手目しおり"));
        QCOMPARE(disp[3].comment, QStringLiteral("3手目コメント"));
        QCOMPARE(disp[3].bookmark, QStringLiteral("3手目しおり"));

        // コメント変更がしおりに影響しないこと
        model.setComment(3, QStringLiteral("変更後コメント"));
        QCOMPARE(model.bookmark(3), QStringLiteral("3手目しおり"));

        // しおり変更がコメントに影響しないこと
        model.setBookmark(3, QStringLiteral("変更後しおり"));
        QCOMPARE(model.comment(3), QStringLiteral("変更後コメント"));
    }
};

QTEST_MAIN(TestKifuCommentSync)
#include "tst_kifu_comment_sync.moc"
