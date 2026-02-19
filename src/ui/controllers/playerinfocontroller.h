#ifndef PLAYERINFOCONTROLLER_H
#define PLAYERINFOCONTROLLER_H

/// @file playerinfocontroller.h
/// @brief プレイヤー情報コントローラクラスの定義


#include <QObject>
#include <QString>
#include <QList>

#include "mainwindow.h"  // PlayMode enum

class ShogiView;
class GameInfoPaneController;
class EvaluationGraphController;
class UsiCommLogModel;
class EngineAnalysisTab;
struct KifGameInfoItem;

/**
 * @brief PlayerInfoController - 対局者情報管理クラス
 *
 * MainWindowから対局者名・エンジン名の管理を分離したクラス。
 * 以下の責務を担当:
 * - 対局者名の設定・更新
 * - エンジン名の設定・更新
 * - 対局情報タブへの反映
 * - 評価値グラフへのエンジン名反映
 */
class PlayerInfoController : public QObject
{
    Q_OBJECT

public:
    explicit PlayerInfoController(QObject* parent = nullptr);
    ~PlayerInfoController() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------
    void setShogiView(ShogiView* view);
    void setGameInfoController(GameInfoPaneController* controller);
    void setEvalGraphController(EvaluationGraphController* controller);
    void setLogModels(UsiCommLogModel* log1, UsiCommLogModel* log2);
    void setAnalysisTab(EngineAnalysisTab* tab);

    // --------------------------------------------------------
    // 状態設定
    // --------------------------------------------------------
    void setPlayMode(PlayMode mode);
    PlayMode playMode() const { return m_playMode; }

    void setHumanNames(const QString& name1, const QString& name2);
    void setEngineNames(const QString& name1, const QString& name2);

    QString humanName1() const { return m_humanName1; }
    QString humanName2() const { return m_humanName2; }
    QString engineName1() const { return m_engineName1; }
    QString engineName2() const { return m_engineName2; }

    // --------------------------------------------------------
    // 対局者名更新処理
    // --------------------------------------------------------

    /**
     * @brief PlayModeに応じて将棋盤の対局者名ラベルを更新
     */
    void applyPlayersNamesForMode();

    /**
     * @brief エンジン名をログモデルに反映
     */
    void applyEngineNamesToLogModels();

    /**
     * @brief EvE対局時に2番目のエンジン情報の表示を切り替え
     */
    void updateSecondEngineVisibility();

    /**
     * @brief 対局情報タブの先手・後手名を更新
     */
    void updateGameInfoPlayerNames(const QString& blackName, const QString& whiteName);

    /**
     * @brief 現在の対局に基づいて対局情報タブを更新
     */
    void updateGameInfoForCurrentMatch();

    /**
     * @brief 元の対局情報を保存（棋譜読み込み時）
     */
    void setOriginalGameInfo(const QList<KifGameInfoItem>& items);

public Q_SLOTS:
    /**
     * @brief 対局者名設定フック（MatchCoordinatorから呼ばれる）
     */
    void onSetPlayersNames(const QString& p1, const QString& p2);

    /**
     * @brief エンジン名設定フック（MatchCoordinatorから呼ばれる）
     */
    void onSetEngineNames(const QString& e1, const QString& e2);

    /**
     * @brief 対局者名確定時のスロット
     */
    void onPlayerNamesResolved(const QString& human1, const QString& human2,
                               const QString& engine1, const QString& engine2,
                               int playMode);

Q_SIGNALS:
    /**
     * @brief エンジン名が更新された
     */
    void engineNamesUpdated(const QString& engine1, const QString& engine2);

    /**
     * @brief PlayModeが変更された
     */
    void playModeChanged(PlayMode mode);

private:
    ShogiView* m_shogiView = nullptr;
    GameInfoPaneController* m_gameInfoController = nullptr;
    EvaluationGraphController* m_evalGraphController = nullptr;
    UsiCommLogModel* m_lineEditModel1 = nullptr;
    UsiCommLogModel* m_lineEditModel2 = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;

    PlayMode m_playMode = PlayMode::NotStarted;
    QString m_humanName1;
    QString m_humanName2;
    QString m_engineName1;
    QString m_engineName2;
};

#endif // PLAYERINFOCONTROLLER_H
