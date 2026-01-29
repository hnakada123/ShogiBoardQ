#ifndef PLAYERINFOWIRING_H
#define PLAYERINFOWIRING_H

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>

#include "playmode.h"
#include "kifparsetypes.h"

class QTabWidget;
class GameInfoPaneController;
class PlayerInfoController;
class ShogiView;

/**
 * @brief 対局情報・プレイヤー情報関連のUI配線を担当するクラス
 *
 * 責務:
 * - GameInfoPaneControllerの初期化と管理
 * - PlayerInfoControllerの初期化と管理
 * - 対局情報タブの追加・更新
 * - プレイヤー名・エンジン名の設定フック処理
 *
 * 本クラスは対局情報関連の処理を一元管理し、
 * MainWindowの肥大化を防ぐ。
 */
class PlayerInfoWiring : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 依存オブジェクト構造体
     */
    struct Dependencies {
        QWidget* parentWidget = nullptr;
        QTabWidget* tabWidget = nullptr;
        ShogiView* shogiView = nullptr;
        PlayMode* playMode = nullptr;
        QString* humanName1 = nullptr;
        QString* humanName2 = nullptr;
        QString* engineName1 = nullptr;
        QString* engineName2 = nullptr;
    };

    /**
     * @brief コンストラクタ
     * @param deps 依存オブジェクト
     * @param parent 親オブジェクト
     */
    explicit PlayerInfoWiring(const Dependencies& deps, QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~PlayerInfoWiring() override = default;

    /**
     * @brief 起動時に対局情報タブを追加する
     */
    void addGameInfoTabAtStartup();

    /**
     * @brief 現在の対局に基づいて対局情報タブを更新
     */
    void updateGameInfoForCurrentMatch();

    /**
     * @brief 元の対局情報を保存（棋譜読み込み時）
     * @param items 対局情報リスト
     */
    void setOriginalGameInfo(const QList<KifGameInfoItem>& items);

    /**
     * @brief 対局情報タブの先手・後手名を更新
     * @param blackName 先手名
     * @param whiteName 後手名
     */
    void updateGameInfoPlayerNames(const QString& blackName, const QString& whiteName);

    /**
     * @brief 対局開始時の対局情報を設定（持ち時間を含む）
     * @param startDateTime 対局開始日時
     * @param blackName 先手名
     * @param whiteName 後手名
     * @param handicap 手合割
     * @param hasTimeControl 時間制御が有効か
     * @param baseTimeMs 持ち時間（ミリ秒）
     * @param byoyomiMs 秒読み（ミリ秒）
     * @param incrementMs フィッシャー加算（ミリ秒）
     */
    void setGameInfoForMatchStart(const QDateTime& startDateTime,
                                  const QString& blackName,
                                  const QString& whiteName,
                                  const QString& handicap,
                                  bool hasTimeControl,
                                  qint64 baseTimeMs,
                                  qint64 byoyomiMs,
                                  qint64 incrementMs);

    /**
     * @brief 対局終了時の終了日時を対局情報に追加
     * @param endDateTime 対局終了日時
     */
    void updateGameInfoWithEndTime(const QDateTime& endDateTime);

    /**
     * @brief GameInfoPaneControllerを取得
     * @return GameInfoPaneControllerへのポインタ
     */
    GameInfoPaneController* gameInfoController() const { return m_gameInfoController; }

    /**
     * @brief PlayerInfoControllerを取得
     * @return PlayerInfoControllerへのポインタ
     */
    PlayerInfoController* playerInfoController() const { return m_playerInfoController; }

    /**
     * @brief GameInfoPaneControllerを確保する（外部から呼び出し可能）
     */
    void ensureGameInfoController();

    /**
     * @brief タブウィジェットを設定する（遅延初期化用）
     * @param tabWidget タブウィジェット
     */
    void setTabWidget(QTabWidget* tabWidget);

public slots:
    /**
     * @brief 対局者名設定フック（将棋盤ラベル更新）
     * @param p1 先手名
     * @param p2 後手名
     */
    void onSetPlayersNames(const QString& p1, const QString& p2);

    /**
     * @brief エンジン名設定フック
     * @param e1 エンジン1名
     * @param e2 エンジン2名
     */
    void onSetEngineNames(const QString& e1, const QString& e2);

    /**
     * @brief 対局者名確定時の処理
     * @param human1 人間1名
     * @param human2 人間2名
     * @param engine1 エンジン1名
     * @param engine2 エンジン2名
     * @param playMode プレイモード
     */
    void onPlayerNamesResolved(const QString& human1, const QString& human2,
                               const QString& engine1, const QString& engine2,
                               int playMode);

signals:
    /**
     * @brief 対局情報が更新された
     * @param items 更新された対局情報
     */
    void gameInfoUpdated(const QList<KifGameInfoItem>& items);

    /**
     * @brief エンジン名が更新された
     * @param engine1 エンジン1名
     * @param engine2 エンジン2名
     */
    void engineNamesUpdated(const QString& engine1, const QString& engine2);

    /**
     * @brief プレイヤー名が確定した
     * @param human1 人間1名
     * @param human2 人間2名
     * @param engine1 エンジン1名
     * @param engine2 エンジン2名
     * @param playMode プレイモード
     */
    void playerNamesResolved(const QString& human1, const QString& human2,
                             const QString& engine1, const QString& engine2,
                             int playMode);

    /**
     * @brief タブ変更時
     * @param index タブインデックス
     */
    void tabCurrentChanged(int index);

private slots:
    /**
     * @brief GameInfoPaneControllerからの更新通知
     * @param items 対局情報リスト
     */
    void onGameInfoUpdated(const QList<KifGameInfoItem>& items);

private:
    /**
     * @brief PlayerInfoControllerを確保する
     */
    void ensurePlayerInfoController();

    /**
     * @brief デフォルトの対局情報を設定
     */
    void populateDefaultGameInfo();

    // 依存オブジェクト
    QWidget* m_parentWidget = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    ShogiView* m_shogiView = nullptr;
    PlayMode* m_playMode = nullptr;
    QString* m_humanName1 = nullptr;
    QString* m_humanName2 = nullptr;
    QString* m_engineName1 = nullptr;
    QString* m_engineName2 = nullptr;

    // 内部コントローラ
    GameInfoPaneController* m_gameInfoController = nullptr;
    PlayerInfoController* m_playerInfoController = nullptr;
};

#endif // PLAYERINFOWIRING_H
