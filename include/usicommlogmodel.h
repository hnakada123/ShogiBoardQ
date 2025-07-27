#ifndef USICOMMLOGMODEL_H
#define USICOMMLOGMODEL_H

#include <QObject>

// エンジン名、予想手、探索手、深さ、ノード数、局面探索数、ハッシュ使用率の更新に関するクラス
class UsiCommLogModel : public QObject
{
    // Qtのメタオブジェクトシステムを使用するためのマクロ
    Q_OBJECT

    // エンジン名のプロパティを定義。エンジン名を読み取る関数と設定する関数、および変更時のシグナルを指定。
    Q_PROPERTY(QString engineName READ engineName WRITE setEngineName NOTIFY engineNameChanged)

    // 予想手のプロパティを定義。予想手を読み取る関数と設定する関数、および変更時のシグナルを指定。
    Q_PROPERTY(QString predictiveMove READ predictiveMove WRITE setPredictiveMove NOTIFY predictiveMoveChanged)

    // 探索手のプロパティを定義。探索手を読み取る関数と設定する関数、および変更時のシグナルを指定。
    Q_PROPERTY(QString searchedMove READ searchedMove WRITE setSearchedMove NOTIFY searchedMoveChanged)

    // 探索深さのプロパティを定義。探索深さを読み取る関数と設定する関数、および変更時のシグナルを指定。
    Q_PROPERTY(QString searchDepth READ searchDepth WRITE setSearchDepth NOTIFY searchDepthChanged)

    // ノード数のプロパティを定義。ノード数を読み取る関数と設定する関数、および変更時のシグナルを指定。
    Q_PROPERTY(QString nodeCount READ nodeCount WRITE setNodeCount NOTIFY nodeCountChanged)

    // 探索局面数（NPS: Nodes Per Second）のプロパティを定義。NPSを読み取る関数と設定する関数、および変更時のシグナルを指定。
    Q_PROPERTY(QString nodesPerSecond READ nodesPerSecond WRITE setNodesPerSecond NOTIFY nodesPerSecondChanged)

    // ハッシュ使用率のプロパティを定義。ハッシュ使用率を読み取る関数と設定する関数、および変更時のシグナルを指定。
    Q_PROPERTY(QString hashUsage READ hashUsage WRITE setHashUsage NOTIFY hashUsageChanged)

    // USIプロトコル通信コマンド行のプロパティを定義。読み取り専用で、変更時のシグナルを指定。
    Q_PROPERTY(QString usiCommLog READ usiCommLog NOTIFY usiCommLogChanged)

public:
    // コンストラクタ
    explicit UsiCommLogModel(QObject *parent = nullptr);

    // 将棋エンジン名を取得する。
    QString engineName() const;

    // 予想手を取得する。
    QString predictiveMove() const;

    // 探索手を取得する。
    QString searchedMove() const;

    // 深さを取得する。
    QString searchDepth() const;

    // ノード数を取得する。
    QString nodeCount() const;

    // 探索局面数を取得する。
    QString nodesPerSecond() const;

    // ハッシュ使用率を取得する。
    QString hashUsage() const;

    QString usiCommLog() const;

    // 将棋GUIと将棋エンジン間のUSIプロトコル通信コマンド行をセットし、GUIに追記する。
    void appendUsiCommLog(const QString& usiCommLog);

public slots:
    // 将棋エンジン名をセットし、GUIの表示を更新する。
    void setEngineName(const QString& engineName);

    // 予想手をセットし、GUIの表示を更新する。
    void setPredictiveMove(const QString& predictiveMove);

    // 探索手をセットし、GUIの表示を更新する。
    void setSearchedMove(const QString& searchedMove);

    // 深さをセットし、GUIの表示を更新する。
    void setSearchDepth(const QString& searchDepth);

    // ノード数をセットし、GUIの表示を更新する。
    void setNodeCount(const QString& nodeCount);

    // 探索局面数をセットし、GUIの表示を更新する。
    void setNodesPerSecond(const QString& nodesPerSecond);

    // ハッシュ使用率をセットし、GUIの表示を更新する。
    void setHashUsage(const QString& hashUsage);

signals:
    // 将棋エンジン名が変更されたときに発生するシグナル
    void engineNameChanged();

    // 予想手が変更されたときに発生するシグナル
    void predictiveMoveChanged();

    // 探索手が変更されたときに発生するシグナル
    void searchedMoveChanged();

    // 深さが変更されたときに発生するシグナル
    void searchDepthChanged();

    // ノード数が変更されたときに発生するシグナル
    void nodeCountChanged();

    // 探索局面数が変更されたときに発生するシグナル
    void nodesPerSecondChanged();

    // ハッシュ使用率が変更されたときに発生するシグナル
    void hashUsageChanged();

    // 将棋GUIと将棋エンジン間のUSIプロトコル通信コマンド行を追加したときに発生するシグナル
    void usiCommLogChanged();

private:
    // 将棋エンジン名
    QString m_engineName;

    // 予想手
    QString m_predictiveMove;

    // 探索手
    QString m_searchedMove;

    // 深さ
    QString m_searchDepth;

    // ノード数
    QString m_nodeCount;

    // 探索局面数
    QString m_nodesPerSecond;

    // ハッシュ使用率
    QString m_hashUsage;

    // 将棋GUIと将棋エンジン間のUSIプロトコル通信コマンド行
    QString m_usiCommLog;
};

#endif // USICOMMLOGMODEL_H
