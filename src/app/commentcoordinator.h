#ifndef COMMENTCOORDINATOR_H
#define COMMENTCOORDINATOR_H

#include <QObject>
#include <QString>
#include <QStringList>

class EngineAnalysisTab;
class RecordPane;
class GameRecordModel;
class GameRecordPresenter;
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
    void setAnalysisTab(EngineAnalysisTab* tab) { m_analysisTab = tab; }
    void setRecordPane(RecordPane* pane) { m_recordPane = pane; }
    void setGameRecordModel(GameRecordModel* model) { m_gameRecord = model; }
    void setRecordPresenter(GameRecordPresenter* presenter) { m_recordPresenter = presenter; }
    void setStatusBar(QStatusBar* bar) { m_statusBar = bar; }
    void setCurrentMoveIndex(int* index) { m_currentMoveIndex = index; }
    void setCommentsByRow(QStringList* comments) { m_commentsByRow = comments; }

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

signals:
    /**
     * @brief GameRecordModelの初期化が必要な時に発行
     */
    void ensureGameRecordModelRequested();

private:
    EngineAnalysisTab* m_analysisTab = nullptr;
    RecordPane* m_recordPane = nullptr;
    GameRecordModel* m_gameRecord = nullptr;
    GameRecordPresenter* m_recordPresenter = nullptr;
    QStatusBar* m_statusBar = nullptr;
    int* m_currentMoveIndex = nullptr;
    QStringList* m_commentsByRow = nullptr;
};

#endif // COMMENTCOORDINATOR_H
