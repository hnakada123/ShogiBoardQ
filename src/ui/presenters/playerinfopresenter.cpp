/// @file playerinfopresenter.cpp
/// @brief 対局者名・エンジン名の表示の実装

#include "playerinfopresenter.h"

#include "appactivitystate.h"
#include "considerationtabmanager.h"
#include "engineinfowidget.h"
#include "evaluationgraphpresenter.h"
#include "gameinfokeys.h"
#include "kifudocument.h"
#include "logcategories.h"
#include "shogiview.h"
#include "thinkingpanel.h"
#include "usicommlogmodel.h"

PlayerInfoPresenter::PlayerInfoPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.document && m_deps.activity);
    if (m_deps.thinking) m_deps.thinking->setSecondEngineVisible(m_secondEngineVisible);
    syncBoardNamesWithGameInfo();
}

PlayerInfoPresenter::~PlayerInfoPresenter() = default;

// --------------------------------------------------------
// 対局者
// --------------------------------------------------------

void PlayerInfoPresenter::showMatchPlayers()
{
    const bool duringGame = m_deps.activity->is(AppActivityState::Activity::DuringGame);
    showPlayers(duringGame ? &m_deps.document->players() : nullptr);
}

void PlayerInfoPresenter::showPlayers(const MatchPlayers& players)
{
    showPlayers(&players);
}

void PlayerInfoPresenter::showPlayers(const MatchPlayers* players)
{
    qCDebug(lcUi).noquote() << "showPlayers: black=" << (players ? players->name(Turn::Black) : QString())
                            << " white=" << (players ? players->name(Turn::White) : QString());

    // 将棋盤の対局者名。対局していなければ対局情報の名前（初期値は「先手」「後手」）を出す
    if (players) setBoardNames(players->name(Turn::Black), players->name(Turn::White));
    else syncBoardNamesWithGameInfo();

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
    if (m_deps.logModel1) m_deps.logModel1->setEngineName(log1);
    if (m_deps.logModel2) m_deps.logModel2->setEngineName(log2);

    // エンジン同士の対局だけ2つ目のエンジン情報を表示する
    setSecondEngineVisible(players && players->engineVsEngine());

    // 評価値グラフは先手のエンジンを P1、後手のエンジンを P2 として描く
    if (m_deps.evalGraph) {
        m_deps.evalGraph->setEngine1Name(players ? players->blackEngineName : QString());
        m_deps.evalGraph->setEngine2Name(players ? players->whiteEngineName : QString());
    }
}

void PlayerInfoPresenter::setSecondEngineVisible(bool visible)
{
    m_secondEngineVisible = visible;
    if (m_deps.thinking) m_deps.thinking->setSecondEngineVisible(m_secondEngineVisible);
}

void PlayerInfoPresenter::setBoardNames(const QString& black, const QString& white)
{
    if (!m_deps.shogiView) return;
    m_deps.shogiView->chrome()->setBlackPlayerName(black.isEmpty() ? tr("先手") : black);
    m_deps.shogiView->chrome()->setWhitePlayerName(white.isEmpty() ? tr("後手") : white);
}

void PlayerInfoPresenter::syncBoardNamesWithGameInfo()
{
    // 先手・後手が無ければ駒落ちの下手・上手、それも無ければ省略名
    setBoardNames(m_deps.document->headerPlayerName(Turn::Black), m_deps.document->headerPlayerName(Turn::White));
}

void PlayerInfoPresenter::onGameInfoUpdated(const QList<KifGameInfoItem>& /*items*/)
{
    // 反映した対局情報は文書に入っている
    syncBoardNamesWithGameInfo();
}

void PlayerInfoPresenter::onAnalysisEngineNamesChanged(const QString& engine1, const QString& /*engine2*/)
{
    // 検討・詰み探索のエンジンは1つ（2つ目のエンジン欄を隠し、評価値グラフは E1 に出す）
    setSecondEngineVisible(false);
    if (m_deps.evalGraph) {
        m_deps.evalGraph->setEngine1Name(engine1);
        m_deps.evalGraph->setEngine2Name(QString());
    }

    // 検討タブと思考タブにエンジン名を出す
    if (m_deps.consideration) m_deps.consideration->setConsiderationEngineName(engine1);
    if (m_deps.thinking && !engine1.isEmpty()) m_deps.thinking->info1()->setDisplayNameFallback(engine1);
}
