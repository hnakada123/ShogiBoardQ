/// @file playerinfopresenter.cpp
/// @brief 対局者名・エンジン名と対局情報の表示の実装

#include "playerinfopresenter.h"

#include <QTabWidget>

#include "appsettings.h"
#include "appstate.h"
#include "considerationtabmanager.h"
#include "engineanalysistab.h"
#include "engineinfowidget.h"
#include "evaluationgraphcontroller.h"
#include "gameinfokeys.h"
#include "gameinfopanecontroller.h"
#include "gamerecordmodel.h"
#include "kifuexportmetadata.h"
#include "livegamesession.h"
#include "logcategories.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "usicommlogmodel.h"

PlayerInfoPresenter::PlayerInfoPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_tabWidget(deps.tabWidget)
    , m_shogiView(deps.shogiView)
    , m_state(deps.state)
    , m_timeController(deps.timeController)
    , m_gameRecord(deps.gameRecord)
    , m_liveGameSession(deps.liveGameSession)
    , m_gameInfoController(new GameInfoPaneController(deps.parentWidget))
{
    connect(m_gameInfoController, &GameInfoPaneController::gameInfoUpdated,
            this, &PlayerInfoPresenter::onGameInfoUpdated);
}

PlayerInfoPresenter::~PlayerInfoPresenter() = default;

// --------------------------------------------------------
// 後から作られる表示先
// --------------------------------------------------------

void PlayerInfoPresenter::setAnalysisTab(EngineAnalysisTab* analysisTab)
{
    m_analysisTab = analysisTab;
    if (m_analysisTab) m_analysisTab->setSecondEngineVisible(m_secondEngineVisible);
}

void PlayerInfoPresenter::setEvalGraphController(EvaluationGraphController* controller)
{
    m_evalGraphController = controller;
}

void PlayerInfoPresenter::setLogModels(UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    m_logModel1 = log1;
    m_logModel2 = log2;
}

void PlayerInfoPresenter::addGameInfoTabAtStartup()
{
    // 対局情報はドックとして表示するため、起動時は初期データだけ用意する
    m_gameInfoController->resetGameInfo();
    syncBoardNamesWithGameInfo();

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
            this, &PlayerInfoPresenter::tabCurrentChanged,
            Qt::UniqueConnection);

    qCDebug(lcUi) << "addGameInfoTabAtStartup: current tab restored without game-info tab insertion";
}

// --------------------------------------------------------
// 対局者
// --------------------------------------------------------

void PlayerInfoPresenter::showMatchPlayers()
{
    showPlayers(m_state ? m_state->currentMatchPlayers() : nullptr);
}

void PlayerInfoPresenter::showPlayers(const MatchPlayers* players)
{
    const QString blackName = players ? players->name(Turn::Black) : QString();
    const QString whiteName = players ? players->name(Turn::White) : QString();
    qCDebug(lcUi).noquote() << "showPlayers: black=" << blackName << " white=" << whiteName;

    // 将棋盤の対局者名・対局情報の先手・後手。対局していなければ対局情報の名前（初期値は「先手」「後手」）を出す
    if (players) {
        if (m_shogiView) {
            m_shogiView->setBlackPlayerName(blackName);
            m_shogiView->setWhitePlayerName(whiteName);
        }
        updateGameInfoPlayerNames(blackName, whiteName);
    } else {
        syncBoardNamesWithGameInfo();
    }

    // 通信ログの見出し。エンジンの枠は人間対エンジンでは E1 だけ、エンジン同士では E1=先手・E2=後手
    QString log1;
    QString log2;
    if (players && players->engineVsEngine()) {
        log1 = players->blackEngineName;
        log2 = players->whiteEngineName;
    } else if (players && players->blackIsEngine) {
        log1 = players->blackEngineName;
    } else if (players && players->whiteIsEngine) {
        log1 = players->whiteEngineName;
    }
    if (m_logModel1) m_logModel1->setEngineName(log1);
    if (m_logModel2) m_logModel2->setEngineName(log2);

    // エンジン同士の対局だけ2つ目のエンジン情報を表示する
    setSecondEngineVisible(players && players->engineVsEngine());

    // 評価値グラフは先手のエンジンを P1、後手のエンジンを P2 として描く
    if (m_evalGraphController) {
        m_evalGraphController->setEngine1Name(players ? players->blackEngineName : QString());
        m_evalGraphController->setEngine2Name(players ? players->whiteEngineName : QString());
    }
}

void PlayerInfoPresenter::updateGameInfoPlayerNames(const QString& blackName, const QString& whiteName)
{
    if (m_gameInfoController) m_gameInfoController->updatePlayerNames(blackName, whiteName);
}

void PlayerInfoPresenter::setSecondEngineVisible(bool visible)
{
    m_secondEngineVisible = visible;
    if (m_analysisTab) m_analysisTab->setSecondEngineVisible(m_secondEngineVisible);
}

void PlayerInfoPresenter::syncBoardNamesWithGameInfo()
{
    applyBoardNames(m_gameInfoController->gameInfo());
}

void PlayerInfoPresenter::applyBoardNames(const QList<KifGameInfoItem>& items)
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
}

void PlayerInfoPresenter::onGameInfoUpdated(const QList<KifGameInfoItem>& items)
{
    applyBoardNames(items);
    if (m_gameRecord) m_gameRecord->markDirty();
}

void PlayerInfoPresenter::onAnalysisEngineNamesChanged(const QString& engine1, const QString& /*engine2*/)
{
    // 検討・詰み探索のエンジンは1つ（2つ目のエンジン欄を隠し、評価値グラフは E1 に出す）
    setSecondEngineVisible(false);
    if (m_evalGraphController) {
        m_evalGraphController->setEngine1Name(engine1);
        m_evalGraphController->setEngine2Name(QString());
    }

    // 検討タブと思考タブにエンジン名を出す
    if (m_analysisTab) {
        m_analysisTab->considerationTabManager()->setConsiderationEngineName(engine1);
        if (m_analysisTab->info1() && !engine1.isEmpty()) {
            m_analysisTab->info1()->setDisplayNameFallback(engine1);
        }
    }
}

void PlayerInfoPresenter::onMatchPlayersResolved(const MatchPlayers& players)
{
    // 読み込んだ棋譜の途中から続ける対局では、本譜の対局者などの対局情報をそのまま残す
    // （対局者の表示で書き換える前に控える）
    const bool keepHeader = continuesExistingRecord();
    QList<KifGameInfoItem> recordHeader;
    if (keepHeader) {
        m_gameInfoController->commitPendingEditor();
        recordHeader = m_gameInfoController->gameInfo();
    }

    m_state->players = players;
    showPlayers(&players);

    // 新しい対局が始まるので終了日時を消し、時計に設定した持ち時間と開始日時を対局情報に書く
    TimeControl timeControl;
    QDateTime start;
    if (m_timeController) {
        m_timeController->clearGameEndTime();
        timeControl = m_timeController->timeControl();
        start = m_timeController->gameStartDateTime();
    }
    const QString blackName = players.name(Turn::Black);
    const QString whiteName = players.name(Turn::White);
    const QString handicap = KifuExportMetadataBuilder::handicapLabel(m_state->startSfenStr);

    if (keepHeader) {
        m_gameInfoController->setGameInfo(recordHeader);
        m_keepRecordHeader = true;
        // 続けた対局の対局者・日時は分岐の最初の手のメモにする
        const QString timeText = !timeControl.isUnlimited()
            ? KifuExportMetadataBuilder::timeControlText(timeControl, KifuExportMetadataBuilder::isHandicap(handicap))
            : tr("無制限");
        const QDateTime noteStart = start.isValid() ? start : QDateTime::currentDateTime();
        m_liveGameSession->setFirstMoveNote(tr("対局：▲%1 △%2（%3 開始、持ち時間 %4）")
                                                .arg(blackName, whiteName,
                                                     noteStart.toString(QStringLiteral("yyyy/MM/dd HH:mm")), timeText));
        return;
    }

    setGameInfoForMatchStart(start, blackName, whiteName, handicap, timeControl);
}

// --------------------------------------------------------
// 対局情報
// --------------------------------------------------------

void PlayerInfoPresenter::setOriginalGameInfo(const QList<KifGameInfoItem>& items)
{
    if (m_gameInfoController) m_gameInfoController->setOriginalGameInfo(items);
}

void PlayerInfoPresenter::setGameInfoForMatchStart(const QDateTime& startDateTime,
                                                const QString& blackName,
                                                const QString& whiteName,
                                                const QString& handicap,
                                                const TimeControl& timeControl)
{
    const bool hasTimeControl = !timeControl.isUnlimited();
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
                                       timeControl, KifuExportMetadataBuilder::isHandicap(handicapLabel))
                                 : QStringLiteral("無制限")});

    m_gameInfoController->setGameInfoForMatch(items);

    qCDebug(lcUi) << "setGameInfoForMatchStart: items=" << items.size()
                   << " hasTimeControl=" << hasTimeControl;
}

bool PlayerInfoPresenter::continuesExistingRecord() const
{
    return m_liveGameSession != nullptr && m_liveGameSession->isActive() && m_liveGameSession->anchorPly() > 0;
}

void PlayerInfoPresenter::updateGameInfoWithEndTime(const QDateTime& endDateTime)
{
    if (!m_gameInfoController || m_keepRecordHeader) return;
    m_gameInfoController->updateGameInfoValue(
        GameInfoKeys::kEndDateTime, endDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss")));

    qCDebug(lcUi) << "updateGameInfoWithEndTime:"
                   << endDateTime.toString(Qt::ISODate);
}

void PlayerInfoPresenter::updateGameInfoWithTimeControl(const TimeControl& timeControl)
{
    if (!m_gameInfoController || continuesExistingRecord()) return;
    if (timeControl.isUnlimited()) {
        m_gameInfoController->updateGameInfoValue(GameInfoKeys::kTimeControl, QStringLiteral("無制限"));
        return;
    }

    const bool handicap =
        KifuExportMetadataBuilder::isHandicap(KifuExportMetadataBuilder::handicapLabel(m_state->startSfenStr));
    const QString timeStr = KifuExportMetadataBuilder::timeControlText(timeControl, handicap);
    m_gameInfoController->updateGameInfoValue(GameInfoKeys::kTimeControl, timeStr);

    qCDebug(lcUi) << "updateGameInfoWithTimeControl:"
                   << timeStr;
}
