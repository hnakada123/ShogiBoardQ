#ifndef USICOMMLOGMODEL_H
#define USICOMMLOGMODEL_H

/// @file usicommlogmodel.h
/// @brief USI通信ログ・エンジン情報表示モデルの定義


#include <QObject>

/**
 * @brief USIエンジンの思考情報（エンジン名・探索状況等）を保持し、GUIに通知するモデル
 *
 * Q_PROPERTYによるバインディングで、EngineInfoWidgetやEngineAnalysisTabと接続される。
 * 各プロパティの変更時にChangedシグナルを発火し、GUIラベルが自動更新される。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class UsiCommLogModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString engineName READ engineName WRITE setEngineName NOTIFY engineNameChanged)
    Q_PROPERTY(QString predictiveMove READ predictiveMove WRITE setPredictiveMove NOTIFY predictiveMoveChanged)
    Q_PROPERTY(QString searchedMove READ searchedMove WRITE setSearchedMove NOTIFY searchedMoveChanged)
    Q_PROPERTY(QString searchDepth READ searchDepth WRITE setSearchDepth NOTIFY searchDepthChanged)
    Q_PROPERTY(QString nodeCount READ nodeCount WRITE setNodeCount NOTIFY nodeCountChanged)
    Q_PROPERTY(QString nodesPerSecond READ nodesPerSecond WRITE setNodesPerSecond NOTIFY nodesPerSecondChanged)
    Q_PROPERTY(QString hashUsage READ hashUsage WRITE setHashUsage NOTIFY hashUsageChanged)
    Q_PROPERTY(QString usiCommLog READ usiCommLog NOTIFY usiCommLogChanged)

public:
    explicit UsiCommLogModel(QObject *parent = nullptr);

    // --- プロパティ getter ---

    QString engineName() const;
    QString predictiveMove() const;
    QString searchedMove() const;
    QString searchDepth() const;
    QString nodeCount() const;
    QString nodesPerSecond() const;
    QString hashUsage() const;
    QString usiCommLog() const;

    /// USIプロトコルの通信ログ行を追加する
    void appendUsiCommLog(const QString& usiCommLog);

    /// 全プロパティをクリアし、変更シグナルを発火する
    void clear();

public slots:
    // --- プロパティ setter ---

    void setEngineName(const QString& engineName);
    void setPredictiveMove(const QString& predictiveMove);
    void setSearchedMove(const QString& searchedMove);
    void setSearchDepth(const QString& searchDepth);
    void setNodeCount(const QString& nodeCount);
    void setNodesPerSecond(const QString& nodesPerSecond);
    void setHashUsage(const QString& hashUsage);

signals:
    // --- プロパティ変更通知（→ EngineInfoWidget, EngineAnalysisTab） ---

    void engineNameChanged();
    void predictiveMoveChanged();
    void searchedMoveChanged();
    void searchDepthChanged();
    void nodeCountChanged();
    void nodesPerSecondChanged();
    void hashUsageChanged();
    void usiCommLogChanged();

private:
    QString m_engineName;       ///< エンジン名
    QString m_predictiveMove;   ///< 予想手（ponder手）
    QString m_searchedMove;     ///< 探索手
    QString m_searchDepth;      ///< 探索深さ
    QString m_nodeCount;        ///< ノード数
    QString m_nodesPerSecond;   ///< 探索局面数（NPS）
    QString m_hashUsage;        ///< ハッシュ使用率
    QString m_usiCommLog;       ///< USIプロトコル通信ログ行
};

#endif // USICOMMLOGMODEL_H
