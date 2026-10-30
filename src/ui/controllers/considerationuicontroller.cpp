/// @file considerationuicontroller.cpp
/// @brief 検討タブの操作（開始・中止・エンジン設定・条件の変更）の実装

#include "considerationuicontroller.h"

#include "analysissession.h"
#include "changeenginesettingsdialog.h"
#include "considerationpanel.h"
#include "registeredengines.h"
#include "errorbus.h"
#include "kifunavigationstate.h"
#include "logcategories.h"

ConsiderationUiController::ConsiderationUiController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_considerationPanel(deps.considerationPanel)
    , m_session(deps.session)
    , m_navState(deps.navState)
{
    if (m_considerationPanel) {
        connect(m_considerationPanel, &ConsiderationPanel::startConsiderationRequested,
                this, &ConsiderationUiController::startConsideration);
        connect(m_considerationPanel, &ConsiderationPanel::stopConsiderationRequested,
                this, &ConsiderationUiController::stopConsideration);
        connect(m_considerationPanel, &ConsiderationPanel::engineSettingsRequested,
                this, &ConsiderationUiController::onEngineSettingsRequested);
        connect(m_considerationPanel, &ConsiderationPanel::settingsChanged,
                this, &ConsiderationUiController::onSettingsChanged);
    }
}

void ConsiderationUiController::startConsideration()
{
    if (!m_session || !m_navState || !m_considerationPanel) return;
    // エンジン破棄中の場合は検討開始を拒否（ハングアップ防止）
    if (m_session->isShutdownInProgress()) {
        qCDebug(lcUi) << "startConsideration: engine shutdown in progress, ignoring";
        return;
    }
    const ConsiderationSettings settings = m_considerationPanel->settings();
    if (settings.engineName.isEmpty()) {
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Error, tr("将棋エンジンが選択されていません。"));
        return;
    }
    // エンジンは名前（検討タブで選んだもの）で引く。番号で引くと、タブの一覧を作った後に
    // 登録が変わったときに名前とパスが食い違う
    const std::optional<EngineEntry> engine = RegisteredEngines::load().findByName(settings.engineName);
    if (!engine || engine->path.isEmpty()) {
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, tr("検討エンジンの選択が不正です。"));
        return;
    }

    // 表示中の手順（分岐ツリー）から、検討する局面と直前の手を決める
    const std::optional<UsiPosition> position = m_navState->currentUsiPosition();
    if (!position) {
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, tr("検討する局面を読めません。"));
        return;
    }

    AnalysisOptions opt;
    opt.mode = AnalysisOptions::Mode::Consideration;
    opt.enginePath = engine->path;
    opt.engineName = engine->name;
    opt.position = *position;
    opt.byoyomiMs = settings.byoyomiMs();
    opt.multiPV = settings.multiPV;
    qCDebug(lcUi).noquote() << "startConsideration: engine=" << opt.engineName
                            << "position=" << opt.position.command().left(50);
    // 検討タブ・思考タブの表示は、検討が探索を始めたときに EngineDisplayPresenter が切り替える
    m_session->start(opt);
}

void ConsiderationUiController::onEngineSettingsRequested(const EngineEntry& engine)
{
    qCDebug(lcUi) << "onEngineSettingsRequested engine=" << engine.name;
    ChangeEngineSettingsDialog::showFor(engine, m_parentWidget);
}

void ConsiderationUiController::onSettingsChanged(const ConsiderationSettings& settings)
{
    // 検討中でなければ、次に始めるときにこの条件を使う
    if (!m_session || !m_session->isConsidering()) return;
    if (settings.engineName != m_session->engineName()) {
        qCDebug(lcUi) << "onSettingsChanged: engine changed to" << settings.engineName;
        // 今の検討を中止して新しいエンジンで始め直す
        m_session->stop();
        startConsideration();
        return;
    }
    if (settings.multiPV != m_session->considerationMultiPV()) {
        qCDebug(lcAnalysis) << "onSettingsChanged: multiPV=" << settings.multiPV;
        m_session->updateMultiPV(settings.multiPV);
    }
}

void ConsiderationUiController::stopConsideration()
{
    if (m_session) m_session->stop();
}
