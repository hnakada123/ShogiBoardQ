/// @file playerinfopresenter.cpp
/// @brief 対局者名・エンジン名と対局情報の表示の実装

#include "playerinfopresenter.h"

#include "appstate.h"
#include "considerationtabmanager.h"
#include "thinkingpanel.h"
#include "engineinfowidget.h"
#include "evaluationgraphpresenter.h"
#include "gameinfokeys.h"
#include "gameinfopane.h"
#include "gamerecordmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifuexportmetadata.h"
#include "livegamesession.h"
#include "logcategories.h"
#include "matchtimerecord.h"
#include "shogiview.h"
#include "usicommlogmodel.h"

PlayerInfoPresenter::PlayerInfoPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_shogiView(deps.shogiView)
    , m_state(deps.state)
    , m_timeRecord(deps.timeRecord)
    , m_tree(deps.tree)
    , m_gameRecord(deps.gameRecord)
    , m_liveGameSession(deps.liveGameSession)
    , m_thinking(deps.thinking)
    , m_consideration(deps.consideration)
    , m_evalGraphController(deps.evalGraph)
    , m_logModel1(deps.logModel1)
    , m_logModel2(deps.logModel2)
    , m_gameInfoPane(deps.gameInfo)
{
    Q_ASSERT(m_gameInfoPane);
    connect(m_gameInfoPane, &GameInfoPane::gameInfoUpdated,
            this, &PlayerInfoPresenter::onGameInfoUpdated);
    if (m_thinking) m_thinking->setSecondEngineVisible(m_secondEngineVisible);
    // 起動時の対局情報（対局者は「先手」「後手」）と盤の対局者名
    m_gameInfoPane->resetGameInfo();
    syncBoardNamesWithGameInfo();
}

PlayerInfoPresenter::~PlayerInfoPresenter() = default;

// --------------------------------------------------------
// 後から作られる表示先
// --------------------------------------------------------

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
            m_shogiView->chrome()->setBlackPlayerName(blackName);
            m_shogiView->chrome()->setWhitePlayerName(whiteName);
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
    if (m_gameInfoPane) m_gameInfoPane->updatePlayerNames(blackName, whiteName);
}

void PlayerInfoPresenter::setSecondEngineVisible(bool visible)
{
    m_secondEngineVisible = visible;
    if (m_thinking) m_thinking->setSecondEngineVisible(m_secondEngineVisible);
}

void PlayerInfoPresenter::syncBoardNamesWithGameInfo()
{
    applyBoardNames(m_gameInfoPane->gameInfo());
}

void PlayerInfoPresenter::applyBoardNames(const QList<KifGameInfoItem>& items)
{
    // 先手・後手が無ければ駒落ちの下手・上手、それも無ければ省略名
    QString black, white, shitate, uwate, blackShort, whiteShort;
    for (const auto& item : items) {
        const QString value = item.value.trimmed();
        if (item.key == GameInfoKeys::kBlackPlayer) black = value;
        else if (item.key == GameInfoKeys::kWhitePlayer) white = value;
        else if (item.key == GameInfoKeys::kShitatePlayer) shitate = value;
        else if (item.key == GameInfoKeys::kUwatePlayer) uwate = value;
        else if (item.key == GameInfoKeys::kBlackShortName) blackShort = value;
        else if (item.key == GameInfoKeys::kWhiteShortName) whiteShort = value;
    }
    if (m_shogiView) {
        if (black.isEmpty()) black = !shitate.isEmpty() ? shitate : !blackShort.isEmpty() ? blackShort : tr("先手");
        if (white.isEmpty()) white = !uwate.isEmpty() ? uwate : !whiteShort.isEmpty() ? whiteShort : tr("後手");

        m_shogiView->chrome()->setBlackPlayerName(black);
        m_shogiView->chrome()->setWhitePlayerName(white);
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
    if (m_consideration) m_consideration->setConsiderationEngineName(engine1);
    if (m_thinking && !engine1.isEmpty()) m_thinking->info1()->setDisplayNameFallback(engine1);
}

void PlayerInfoPresenter::onMatchPlayersResolved(const MatchPlayers& players)
{
    // 読み込んだ棋譜の途中から続ける対局では、本譜の対局者などの対局情報をそのまま残す
    // （対局者の表示で書き換える前に控える）
    const bool keepHeader = continuesExistingRecord();
    QList<KifGameInfoItem> recordHeader;
    if (keepHeader) {
        m_gameInfoPane->commitPendingEditor();
        recordHeader = m_gameInfoPane->gameInfo();
    }

    m_state->players = players;
    showPlayers(&players);

    // 新しい対局が始まるので終了日時を消し、時計に設定した持ち時間と開始日時を対局情報に書く
    const TimeControl timeControl = m_timeRecord ? m_timeRecord->timeControl() : TimeControl{};
    const QDateTime start = m_timeRecord ? m_timeRecord->startDateTime() : QDateTime();
    const QString blackName = players.name(Turn::Black);
    const QString whiteName = players.name(Turn::White);
    const QString handicap = KifuExportMetadataBuilder::handicapLabel(startSfen());

    if (keepHeader) {
        m_gameInfoPane->setGameInfo(recordHeader);
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

void PlayerInfoPresenter::onGameInfoLoaded(const QList<KifGameInfoItem>& items)
{
    if (m_gameInfoPane) m_gameInfoPane->setGameInfo(items);
    applyBoardNames(items);
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

    m_gameInfoPane->setGameInfoForMatch(items);

    qCDebug(lcUi) << "setGameInfoForMatchStart: items=" << items.size()
                   << " hasTimeControl=" << hasTimeControl;
}

QString PlayerInfoPresenter::startSfen() const
{
    return m_tree && m_tree->root() ? m_tree->root()->sfen() : QString();
}

bool PlayerInfoPresenter::continuesExistingRecord() const
{

    return m_liveGameSession != nullptr && m_liveGameSession->isActive() && m_liveGameSession->anchorPly() > 0;
}

void PlayerInfoPresenter::updateGameInfoWithEndTime(const QDateTime& endDateTime)
{
    if (!m_gameInfoPane || m_keepRecordHeader) return;
    m_gameInfoPane->updateGameInfoValue(
        GameInfoKeys::kEndDateTime, endDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss")));

    qCDebug(lcUi) << "updateGameInfoWithEndTime:"
                   << endDateTime.toString(Qt::ISODate);
}

void PlayerInfoPresenter::updateGameInfoWithTimeControl(const TimeControl& timeControl)
{
    if (!m_gameInfoPane || continuesExistingRecord()) return;
    if (timeControl.isUnlimited()) {
        m_gameInfoPane->updateGameInfoValue(GameInfoKeys::kTimeControl, QStringLiteral("無制限"));
        return;
    }

    const bool handicap =
        KifuExportMetadataBuilder::isHandicap(KifuExportMetadataBuilder::handicapLabel(startSfen()));
    const QString timeStr = KifuExportMetadataBuilder::timeControlText(timeControl, handicap);
    m_gameInfoPane->updateGameInfoValue(GameInfoKeys::kTimeControl, timeStr);

    qCDebug(lcUi) << "updateGameInfoWithTimeControl:"
                   << timeStr;
}
