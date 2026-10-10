/// @file playerinfowiring.cpp
/// @brief プレイヤー情報配線クラスの実装

#include "playerinfowiring.h"
#include "kifuexportmetadata.h"

#include "logcategories.h"
#include <QTabWidget>

#include "gameinfopanecontroller.h"
#include "playerinfocontroller.h"
#include "shogiview.h"
#include "appsettings.h"
#include "engineanalysistab.h"
#include "engineinfowidget.h"
#include "timecontrolcontroller.h"
#include "gameinfokeys.h"

PlayerInfoWiring::PlayerInfoWiring(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_markGameRecordDirty(deps.markGameRecordDirty)
    , m_parentWidget(deps.parentWidget)
    , m_tabWidget(deps.tabWidget)
    , m_shogiView(deps.shogiView)
    , m_playMode(deps.playMode)
    , m_humanName1(deps.humanName1)
    , m_humanName2(deps.humanName2)
    , m_engineName1(deps.engineName1)
    , m_engineName2(deps.engineName2)
    , m_startSfenStr(deps.startSfenStr)
    , m_timeControllerRef(deps.timeControllerRef)
    , m_continuesExistingRecord(deps.continuesExistingRecord)
    , m_setContinuationNote(deps.setContinuationNote)
{
}

void PlayerInfoWiring::ensureGameInfoController()
{
    if (m_gameInfoController) return;

    m_gameInfoController = new GameInfoPaneController(m_parentWidget);
    connect(m_gameInfoController, &GameInfoPaneController::gameInfoUpdated,
            this, &PlayerInfoWiring::onGameInfoUpdated);

    qCDebug(lcUi) << "GameInfoPaneController created";
}

void PlayerInfoWiring::onGameInfoUpdated(const QList<KifGameInfoItem>& items)
{
    QString black, white, shitate, uwate;
    for (const auto& item : items) {
        if (item.key == GameInfoKeys::kBlackPlayer) black = item.value;
        else if (item.key == GameInfoKeys::kWhitePlayer) white = item.value;
        else if (item.key == GameInfoKeys::kShitatePlayer) shitate = item.value;
        else if (item.key == GameInfoKeys::kUwatePlayer) uwate = item.value;
    }
    if (m_shogiView) {
        if (black.isEmpty()) black = shitate.isEmpty() ? tr("先手") : shitate;
        if (white.isEmpty()) white = uwate.isEmpty() ? tr("後手") : uwate;
        m_shogiView->setBlackPlayerName(black);
        m_shogiView->setWhitePlayerName(white);
    }
    if (m_markGameRecordDirty) m_markGameRecordDirty();
}

void PlayerInfoWiring::setTabWidget(QTabWidget* tabWidget)
{
    m_tabWidget = tabWidget;
}

void PlayerInfoWiring::setAnalysisTab(EngineAnalysisTab* analysisTab)
{
    m_analysisTab = analysisTab;
    if (m_playerInfoController) {
        m_playerInfoController->setAnalysisTab(analysisTab);
        m_playerInfoController->updateSecondEngineVisibility();
    }
}

void PlayerInfoWiring::ensurePlayerInfoController()
{
    ensureGameInfoController();
    if (m_playerInfoController) return;

    m_playerInfoController = new PlayerInfoController(m_parentWidget);

    // 依存オブジェクトの設定
    m_playerInfoController->setShogiView(m_shogiView);
    m_playerInfoController->setGameInfoController(m_gameInfoController);
    setAnalysisTab(m_analysisTab);

    qCDebug(lcUi) << "PlayerInfoController created";
}

void PlayerInfoWiring::addGameInfoTabAtStartup()
{
    ensureGameInfoController();
    if (!m_gameInfoController) return;

    // 対局情報は分析ドックとして表示するため、起動時は初期データのみ準備する。
    populateDefaultGameInfo();

    if (!m_tabWidget) return;

    // 保存されたタブインデックスを復元
    int savedIndex = AppSettings::lastSelectedTabIndex();
    if (savedIndex >= 0 && savedIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(savedIndex);
    } else {
        m_tabWidget->setCurrentIndex(0);
    }

    // タブ変更時にシグナルを発行
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &PlayerInfoWiring::tabCurrentChanged,
            Qt::UniqueConnection);

    qCDebug(lcUi) << "addGameInfoTabAtStartup: current tab restored without game-info tab insertion";
}

void PlayerInfoWiring::populateDefaultGameInfo()
{
    if (!m_gameInfoController) return;

    m_gameInfoController->resetGameInfo();
}

void PlayerInfoWiring::applyPlayersNamesForMode()
{
    ensurePlayerInfoController();
    if (!m_playerInfoController) return;
    if (m_playMode) m_playerInfoController->setPlayMode(*m_playMode);
    if (m_humanName1 && m_humanName2)
        m_playerInfoController->setHumanNames(*m_humanName1, *m_humanName2);
    if (m_engineName1 && m_engineName2)
        m_playerInfoController->setEngineNames(*m_engineName1, *m_engineName2);
    m_playerInfoController->applyPlayersNamesForMode();
}

void PlayerInfoWiring::applyEngineNamesToLogModels()
{
    ensurePlayerInfoController();
    if (!m_playerInfoController) return;
    if (m_playMode) m_playerInfoController->setPlayMode(*m_playMode);
    if (m_engineName1 && m_engineName2)
        m_playerInfoController->setEngineNames(*m_engineName1, *m_engineName2);
    m_playerInfoController->applyEngineNamesToLogModels();
}

void PlayerInfoWiring::applySecondEngineVisibility()
{
    ensurePlayerInfoController();
    if (!m_playerInfoController) return;
    if (m_playMode) m_playerInfoController->setPlayMode(*m_playMode);
    m_playerInfoController->updateSecondEngineVisibility();
}

void PlayerInfoWiring::onSetPlayersNames(const QString& p1, const QString& p2)
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->onSetPlayersNames(p1, p2);
    }
}

void PlayerInfoWiring::onSetEngineNames(const QString& e1, const QString& e2)
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        if (m_playMode) {
            m_playerInfoController->setPlayMode(*m_playMode);
        }
        m_playerInfoController->onSetEngineNames(e1, e2);

        // 外部参照も更新
        const QString newEngine1 = m_playerInfoController->engineName1();
        const QString newEngine2 = m_playerInfoController->engineName2();
        if (m_engineName1) *m_engineName1 = newEngine1;
        if (m_engineName2) *m_engineName2 = newEngine2;

    }

    // 検討モード・詰み探索モードの場合、検討タブと思考タブにエンジン名を設定
    if (m_playMode &&
        (*m_playMode == PlayMode::ConsiderationMode || *m_playMode == PlayMode::TsumiSearchMode)) {
        if (m_analysisTab) {
            m_analysisTab->setConsiderationEngineName(e1);
            if (m_analysisTab->info1() && !e1.isEmpty()) {
                m_analysisTab->info1()->setDisplayNameFallback(e1);
            }
        }
    }
}

void PlayerInfoWiring::updateGameInfoPlayerNames(const QString& blackName, const QString& whiteName)
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->updateGameInfoPlayerNames(blackName, whiteName);
    }
}

void PlayerInfoWiring::setOriginalGameInfo(const QList<KifGameInfoItem>& items)
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->setOriginalGameInfo(items);
    }
}

void PlayerInfoWiring::updateGameInfoForCurrentMatch()
{
    ensurePlayerInfoController();
    if (m_playerInfoController) {
        if (m_playMode) {
            m_playerInfoController->setPlayMode(*m_playMode);
        }
        if (m_humanName1 && m_humanName2) {
            m_playerInfoController->setHumanNames(*m_humanName1, *m_humanName2);
        }
        if (m_engineName1 && m_engineName2) {
            m_playerInfoController->setEngineNames(*m_engineName1, *m_engineName2);
        }
        m_playerInfoController->updateGameInfoForCurrentMatch();
    }
}

void PlayerInfoWiring::onPlayerNamesResolved(const QString& human1, const QString& human2,
                                              const QString& engine1, const QString& engine2,
                                              int playMode)
{
    qCDebug(lcUi) << "onPlayerNamesResolved: playMode=" << playMode;

    // 外部参照を更新
    if (m_humanName1) *m_humanName1 = human1;
    if (m_humanName2) *m_humanName2 = human2;
    if (m_engineName1) *m_engineName1 = engine1;
    if (m_engineName2) *m_engineName2 = engine2;
    if (m_playMode) *m_playMode = static_cast<PlayMode>(playMode);

    ensurePlayerInfoController();
    if (m_playerInfoController) {
        m_playerInfoController->onPlayerNamesResolved(human1, human2, engine1, engine2, playMode);
    }

}

void PlayerInfoWiring::resolveNamesAndSetupGameInfo(const QString& human1, const QString& human2,
                                                     const QString& engine1, const QString& engine2,
                                                     int playMode,
                                                     const QString& startSfen,
                                                     const TimeControlInfo& timeInfo)
{
    // 読み込んだ棋譜の途中から続ける対局では、本譜の対局者などの対局情報をそのまま残す
    const bool keepHeader = continuesExistingRecord();
    QList<KifGameInfoItem> recordHeader;
    if (keepHeader) {
        ensureGameInfoController();
        if (m_gameInfoController) {
            m_gameInfoController->commitPendingEditor();
            recordHeader = m_gameInfoController->gameInfo();
        }
    }

    // まず対局者名の確定処理
    onPlayerNamesResolved(human1, human2, engine1, engine2, playMode);

    // プレイモードに応じた先手・後手名を決定
    const PlayMode mode = static_cast<PlayMode>(playMode);
    QString blackName, whiteName;
    switch (mode) {
    case PlayMode::HumanVsHuman:
        blackName = human1;
        whiteName = human2;
        break;
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        blackName = human1;
        whiteName = engine2;
        break;
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        blackName = engine1;
        whiteName = human2;
        break;
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
        blackName = engine1;
        whiteName = engine2;
        break;
    default:
        blackName.clear();
        whiteName.clear();
        break;
    }

    if (keepHeader) {
        if (m_gameInfoController) {
            m_gameInfoController->setGameInfo(recordHeader);
        }
        m_keepRecordHeader = true;
        if (m_setContinuationNote) {
            const QDateTime start = timeInfo.gameStartDateTime.isValid()
                ? timeInfo.gameStartDateTime : QDateTime::currentDateTime();
            const bool handicap =
                KifuExportMetadataBuilder::isHandicap(KifuExportMetadataBuilder::handicapLabel(startSfen));
            const QString timeText = timeInfo.hasTimeControl
                ? KifuExportMetadataBuilder::timeControlText(timeInfo.blackTime, timeInfo.whiteTime, handicap)
                : tr("無制限");
            m_setContinuationNote(tr("対局：▲%1 △%2（%3 開始、持ち時間 %4）")
                                      .arg(blackName, whiteName,
                                           start.toString(QStringLiteral("yyyy/MM/dd HH:mm")), timeText));
        }
        return;
    }

    // 対局情報を設定
    setGameInfoForMatchStart(
        timeInfo.gameStartDateTime,
        blackName,
        whiteName,
        KifuExportMetadataBuilder::handicapLabel(startSfen),
        timeInfo.hasTimeControl,
        timeInfo.blackTime,
        timeInfo.whiteTime
    );
}

void PlayerInfoWiring::resolveNamesWithTimeController(const QString& human1, const QString& human2,
                                                       const QString& engine1, const QString& engine2,
                                                       int playMode,
                                                       const QString& startSfen,
                                                       TimeControlController* timeController)
{
    if (timeController) {
        // 終了日時をクリア（新しい対局が始まるため）
        timeController->clearGameEndTime();

        // TimeControlController から TimeControlInfo を構築
        TimeControlInfo tcInfo;
        tcInfo.hasTimeControl = timeController->hasTimeControl();
        tcInfo.blackTime = timeController->settings().black;
        tcInfo.whiteTime = timeController->settings().white;
        tcInfo.gameStartDateTime = timeController->gameStartDateTime();

        resolveNamesAndSetupGameInfo(
            human1, human2, engine1, engine2, playMode,
            startSfen, tcInfo);
    } else {
        onPlayerNamesResolved(human1, human2, engine1, engine2, playMode);
    }
}

void PlayerInfoWiring::onMenuPlayerNamesResolved(const QString& human1, const QString& human2,
                                                  const QString& engine1, const QString& engine2,
                                                  int playMode)
{
    const QString startSfen = m_startSfenStr ? *m_startSfenStr : QString();
    TimeControlController* tc = m_timeControllerRef ? *m_timeControllerRef : nullptr;
    resolveNamesWithTimeController(human1, human2, engine1, engine2, playMode, startSfen, tc);
}

void PlayerInfoWiring::setGameInfoForMatchStart(const QDateTime& startDateTime,
                                                const QString& blackName,
                                                const QString& whiteName,
                                                const QString& handicap,
                                                bool hasTimeControl,
                                                const KifuTimeControlSide& blackTime,
                                                const KifuTimeControlSide& whiteTime)
{
    ensureGameInfoController();
    if (!m_gameInfoController) return;
    m_keepRecordHeader = false;

    QList<KifGameInfoItem> items;

    // 対局日
    items.append({GameInfoKeys::kGameDate, startDateTime.toString(QStringLiteral("yyyy/MM/dd"))});

    // 開始日時
    items.append({GameInfoKeys::kStartDateTime, startDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"))});

    // 先手・後手（駒落ちは柿木形式の KIF と同じく下手・上手）
    const QString handicapLabel = handicap.isEmpty() ? QStringLiteral("平手") : handicap;
    items.append({KifuExportMetadataBuilder::blackPlayerKey(handicapLabel), blackName});
    items.append({KifuExportMetadataBuilder::whitePlayerKey(handicapLabel), whiteName});

    // 手合割
    items.append({GameInfoKeys::kHandicap, handicapLabel});

    // 未開始の「未設定」と、時間制限のない対局を区別する。
    items.append({GameInfoKeys::kTimeControl,
                  hasTimeControl ? KifuExportMetadataBuilder::timeControlText(
                                       blackTime, whiteTime, KifuExportMetadataBuilder::isHandicap(handicapLabel))
                                 : QStringLiteral("無制限")});

    m_gameInfoController->setGameInfoForMatch(items);

    qCDebug(lcUi) << "setGameInfoForMatchStart: items=" << items.size()
                   << " hasTimeControl=" << hasTimeControl;
}

bool PlayerInfoWiring::continuesExistingRecord() const
{
    return m_continuesExistingRecord && m_continuesExistingRecord();
}

void PlayerInfoWiring::updateGameInfoWithEndTime(const QDateTime& endDateTime)
{
    if (!m_gameInfoController || m_keepRecordHeader) return;
    m_gameInfoController->updateGameInfoValue(
        GameInfoKeys::kEndDateTime, endDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss")));

    qCDebug(lcUi) << "updateGameInfoWithEndTime:"
                   << endDateTime.toString(Qt::ISODate);
}

void PlayerInfoWiring::updateGameInfoWithTimeControl(bool hasTimeControl,
                                                     const KifuTimeControlSide& blackTime,
                                                     const KifuTimeControlSide& whiteTime)
{
    if (!m_gameInfoController || continuesExistingRecord()) return;
    if (!hasTimeControl) {
        m_gameInfoController->updateGameInfoValue(GameInfoKeys::kTimeControl, QStringLiteral("無制限"));
        return;
    }

    const bool handicap = m_startSfenStr
        && KifuExportMetadataBuilder::isHandicap(KifuExportMetadataBuilder::handicapLabel(*m_startSfenStr));
    const QString timeStr = KifuExportMetadataBuilder::timeControlText(blackTime, whiteTime, handicap);
    m_gameInfoController->updateGameInfoValue(GameInfoKeys::kTimeControl, timeStr);

    qCDebug(lcUi) << "updateGameInfoWithTimeControl:"
                   << timeStr;
}
