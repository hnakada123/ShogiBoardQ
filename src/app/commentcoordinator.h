#ifndef COMMENTCOORDINATOR_H
#define COMMENTCOORDINATOR_H

/// @file commentcoordinator.h
/// @brief コメントコーディネータクラスの定義


#include <QObject>
#include <QString>
#include <QStringList>

class CommentEditorPanel;
class RecordPane;
class GameRecordModel;
class GameRecordPresenter;
class KifuRecordListModel;
class QStatusBar;

/**
 * @brief コメント処理コーディネーター
 *
 * MainWindowからコメント関連のロジックを分離したクラス。
 * コメントの表示更新、保存、同期処理を担当する。
 */
class CommentCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit CommentCoordinator(QObject* parent = nullptr);

    // 依存オブジェクトの設定
    void setCommentEditor(CommentEditorPanel* editor) { m_commentEditor = editor; }
    void setRecordPane(RecordPane* pane) { m_recordPane = pane; }
    void setGameRecordModel(GameRecordModel* model) { m_gameRecord = model; }
    void setRecordPresenter(GameRecordPresenter* presenter) { m_recordPresenter = presenter; }
    void setStatusBar(QStatusBar* bar) { m_statusBar = bar; }
    void setCurrentMoveIndex(int* index) { m_currentMoveIndex = index; }
    void setCommentsByRow(QStringList* comments) { m_commentsByRow = comments; }
    void setKifuRecordListModel(KifuRecordListModel* model) { m_kifuRecordModel = model; }

    /**
     * @brief コメントを解析タブと棋譜ペインに配信
     * @param text コメントテキスト
     * @param asHtml HTMLとして配信する場合はtrue
     */
    void broadcastComment(const QString& text, bool asHtml);

public slots:
    /**
     * @brief コメント更新スロット（UIからの更新）
     * @param moveIndex 手数インデックス
     * @param newComment 新しいコメント
     */
    void onCommentUpdated(int moveIndex, const QString& newComment);

    /**
     * @brief GameRecordModel::commentChanged スロット
     */
    void onGameRecordCommentChanged(int ply, const QString& comment);

    /**
     * @brief コメント更新コールバック（GameRecordModelから呼ばれる）
     * @param ply 手数
     * @param comment コメント
     */
    void onCommentUpdateCallback(int ply, const QString& comment);

    /**
     * @brief しおり編集リクエストスロット（RecordPaneのボタンから呼ばれる）
     */
    void onBookmarkEditRequested();

    /**
     * @brief しおり更新コールバック（GameRecordModelから呼ばれる）
     * @param ply 手数
     * @param bookmark しおり
     */
    void onBookmarkUpdateCallback(int ply, const QString& bookmark);

signals:
    /**
     * @brief GameRecordModelの初期化が必要な時に発行
     */
    void ensureGameRecordModelRequested();

private:
    CommentEditorPanel* m_commentEditor = nullptr;
    RecordPane* m_recordPane = nullptr;
    GameRecordModel* m_gameRecord = nullptr;
    GameRecordPresenter* m_recordPresenter = nullptr;
    QStatusBar* m_statusBar = nullptr;
    int* m_currentMoveIndex = nullptr;
    QStringList* m_commentsByRow = nullptr;
    KifuRecordListModel* m_kifuRecordModel = nullptr;
};

#endif // COMMENTCOORDINATOR_H
