/// @file playerinfocontroller.cpp
/// @brief プレイヤー情報コントローラクラスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "playerinfocontroller.h"

#include <QDebug>

#include "shogiview.h"
#include "gameinfopanecontroller.h"
#include "evaluationgraphcontroller.h"
#include "usicommlogmodel.h"
#include "engineanalysistab.h"
#include "playernameservice.h"
#include "kifparsetypes.h"  // KifGameInfoItem

PlayerInfoController::PlayerInfoController(QObject* parent)
    : QObject(parent)
{
}

PlayerInfoController::~PlayerInfoController() = default;

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void PlayerInfoController::setShogiView(ShogiView* view)
{
    m_shogiView = view;
}

void PlayerInfoController::setGameInfoController(GameInfoPaneController* controller)
{
    m_gameInfoController = controller;
}

void PlayerInfoController::setEvalGraphController(EvaluationGraphController* controller)
{
    m_evalGraphController = controller;
}

void PlayerInfoController::setLogModels(UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    m_lineEditModel1 = log1;
    m_lineEditModel2 = log2;
}

void PlayerInfoController::setAnalysisTab(EngineAnalysisTab* tab)
{
    m_analysisTab = tab;
}

// --------------------------------------------------------
// 状態設定
// --------------------------------------------------------

void PlayerInfoController::setPlayMode(PlayMode mode)
{
    if (m_playMode != mode) {
        m_playMode = mode;
        Q_EMIT playModeChanged(mode);
    }
}

void PlayerInfoController::setHumanNames(const QString& name1, const QString& name2)
{
    m_humanName1 = name1;
    m_humanName2 = name2;
}

void PlayerInfoController::setEngineNames(const QString& name1, const QString& name2)
{
    m_engineName1 = name1;
    m_engineName2 = name2;
}

// --------------------------------------------------------
// 対局者名更新処理
// --------------------------------------------------------

void PlayerInfoController::applyPlayersNamesForMode()
{
    if (!m_shogiView) return;

    // PlayModeに応じて対局者名を決定
    QString blackName;
    QString whiteName;

    switch (m_playMode) {
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        blackName = m_humanName1.isEmpty() ? tr("先手") : m_humanName1;
        whiteName = m_engineName2.isEmpty() ? tr("後手") : m_engineName2;
        break;
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        blackName = m_engineName1.isEmpty() ? tr("先手") : m_engineName1;
        whiteName = m_humanName2.isEmpty() ? tr("後手") : m_humanName2;
        break;
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
        blackName = m_engineName1.isEmpty() ? tr("先手") : m_engineName1;
        whiteName = m_engineName2.isEmpty() ? tr("後手") : m_engineName2;
        break;
    case PlayMode::HumanVsHuman:
        blackName = m_humanName1.isEmpty() ? tr("先手") : m_humanName1;
        whiteName = m_humanName2.isEmpty() ? tr("後手") : m_humanName2;
        break;
    default:
        blackName = tr("先手");
        whiteName = tr("後手");
        break;
    }

    m_shogiView->setBlackPlayerName(blackName);
    m_shogiView->setWhitePlayerName(whiteName);

    Q_EMIT playerNamesUpdated(blackName, whiteName);
}

void PlayerInfoController::applyEngineNamesToLogModels()
{
    qDebug().noquote() << "[PlayerInfo] applyEngineNamesToLogModels: m_playMode=" << static_cast<int>(m_playMode)
                       << " m_engineName1=" << m_engineName1 << " m_engineName2=" << m_engineName2;

    const EngineNameMapping e =
        PlayerNameService::computeEngineModels(m_playMode, m_engineName1, m_engineName2);

    qDebug().noquote() << "[PlayerInfo] applyEngineNamesToLogModels: computed model1=" << e.model1 << " model2=" << e.model2;

    if (m_lineEditModel1) {
        qDebug().noquote() << "[PlayerInfo] setting m_lineEditModel1 to" << e.model1;
        m_lineEditModel1->setEngineName(e.model1);
    }
    if (m_lineEditModel2) {
        qDebug().noquote() << "[PlayerInfo] setting m_lineEditModel2 to" << e.model2;
        m_lineEditModel2->setEngineName(e.model2);
    }
}

void PlayerInfoController::updateSecondEngineVisibility()
{
    if (!m_analysisTab) return;

    // EvE対局の場合のみ2番目のエンジン情報を表示
    const bool isEvE =
        (m_playMode == PlayMode::EvenEngineVsEngine) ||
        (m_playMode == PlayMode::HandicapEngineVsEngine);

    m_analysisTab->setSecondEngineVisible(isEvE);
}

void PlayerInfoController::updateGameInfoPlayerNames(const QString& blackName, const QString& whiteName)
{
    if (m_gameInfoController) {
        m_gameInfoController->updatePlayerNames(blackName, whiteName);
    }
}

void PlayerInfoController::updateGameInfoForCurrentMatch()
{
    if (!m_gameInfoController) return;

    QString blackName;
    QString whiteName;

    switch (m_playMode) {
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        blackName = m_humanName1.isEmpty() ? tr("先手") : m_humanName1;
        whiteName = m_engineName2.isEmpty() ? tr("後手") : m_engineName2;
        break;
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        blackName = m_engineName1.isEmpty() ? tr("先手") : m_engineName1;
        whiteName = m_humanName2.isEmpty() ? tr("後手") : m_humanName2;
        break;
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
        blackName = m_engineName1.isEmpty() ? tr("先手") : m_engineName1;
        whiteName = m_engineName2.isEmpty() ? tr("後手") : m_engineName2;
        break;
    case PlayMode::HumanVsHuman:
        blackName = m_humanName1.isEmpty() ? tr("先手") : m_humanName1;
        whiteName = m_humanName2.isEmpty() ? tr("後手") : m_humanName2;
        break;
    default:
        blackName = tr("先手");
        whiteName = tr("後手");
        break;
    }

    updateGameInfoPlayerNames(blackName, whiteName);
}

void PlayerInfoController::setOriginalGameInfo(const QList<KifGameInfoItem>& items)
{
    if (m_gameInfoController) {
        m_gameInfoController->setGameInfo(items);
    }
}

// --------------------------------------------------------
// スロット
// --------------------------------------------------------

void PlayerInfoController::onSetPlayersNames(const QString& p1, const QString& p2)
{
    qDebug().noquote() << "[PlayerInfo] onSetPlayersNames: p1=" << p1 << " p2=" << p2;

    // 将棋盤の対局者名ラベルを更新
    if (m_shogiView) {
        m_shogiView->setBlackPlayerName(p1);
        m_shogiView->setWhitePlayerName(p2);
    }

    // 対局情報タブの先手・後手を更新
    updateGameInfoPlayerNames(p1, p2);

    Q_EMIT playerNamesUpdated(p1, p2);
}

void PlayerInfoController::onSetEngineNames(const QString& e1, const QString& e2)
{
    qDebug().noquote() << "[PlayerInfo] onSetEngineNames CALLED";
    qDebug().noquote() << "[PlayerInfo] onSetEngineNames: e1=" << e1 << " e2=" << e2;
    qDebug().noquote() << "[PlayerInfo] onSetEngineNames: current m_engineName1=" << m_engineName1 << " m_engineName2=" << m_engineName2;
    qDebug().noquote() << "[PlayerInfo] onSetEngineNames: current m_playMode=" << static_cast<int>(m_playMode);

    // メンバ変数に保存
    m_engineName1 = e1;
    m_engineName2 = e2;

    qDebug().noquote() << "[PlayerInfo] onSetEngineNames: after save m_engineName1=" << m_engineName1 << " m_engineName2=" << m_engineName2;

    // ログモデル名を更新
    applyEngineNamesToLogModels();

    // EvE対局時に2番目のエンジン情報を表示
    updateSecondEngineVisibility();

    // 将棋盤の対局者名ラベルを更新
    applyPlayersNamesForMode();

    // 対局情報タブも更新
    updateGameInfoForCurrentMatch();

    // 評価値グラフコントローラにもエンジン名を設定
    if (m_evalGraphController) {
        qDebug().noquote() << "[PlayerInfo] onSetEngineNames: m_evalGraphController is valid, calling setEngine1Name/setEngine2Name";
        m_evalGraphController->setEngine1Name(m_engineName1);
        m_evalGraphController->setEngine2Name(m_engineName2);
    } else {
        qDebug().noquote() << "[PlayerInfo] onSetEngineNames: m_evalGraphController is NULL!";
    }

    Q_EMIT engineNamesUpdated(e1, e2);

    qDebug().noquote() << "[PlayerInfo] onSetEngineNames END";
}

void PlayerInfoController::onPlayerNamesResolved(const QString& human1, const QString& human2,
                                                  const QString& engine1, const QString& engine2,
                                                  int playMode)
{
    qDebug().noquote() << "[PlayerInfo] onPlayerNamesResolved:"
                       << " human1=" << human1
                       << " human2=" << human2
                       << " engine1=" << engine1
                       << " engine2=" << engine2
                       << " playMode=" << playMode;

    // メンバ変数に保存
    m_humanName1 = human1;
    m_humanName2 = human2;
    m_engineName1 = engine1;
    m_engineName2 = engine2;
    m_playMode = static_cast<PlayMode>(playMode);

    // 将棋盤の対局者名ラベルを更新
    applyPlayersNamesForMode();

    // エンジン名をログモデルに反映
    applyEngineNamesToLogModels();

    // EvE対局時に2番目のエンジン情報を表示
    updateSecondEngineVisibility();

    // 対局情報タブを更新
    updateGameInfoForCurrentMatch();

    Q_EMIT playModeChanged(m_playMode);
}
