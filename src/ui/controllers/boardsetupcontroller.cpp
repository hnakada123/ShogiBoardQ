/// @file boardsetupcontroller.cpp
/// @brief 盤面初期設定コントローラクラスの実装

#include "boardsetupcontroller.h"

#include "logcategories.h"
#include <QWheelEvent>
#include <QEvent>

#include "appactivitystate.h"
#include "boardinteractioncontroller.h"
#include "matchcoordinator.h"
#include "positioneditcontroller.h"
#include "shogigamecontroller.h"
#include "shogiview.h"

namespace {

class BoardWheelZoomFilter : public QObject
{
public:
    explicit BoardWheelZoomFilter(ShogiView* view, QObject* parent = nullptr)
        : QObject(parent), m_view(view) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_view && event->type() == QEvent::Wheel) {
            auto* wheelEvent = static_cast<QWheelEvent*>(event);
            if (wheelEvent->modifiers() & Qt::ControlModifier) {
                const int delta = wheelEvent->angleDelta().y();
                if (delta > 0) {
                    m_view->enlargeBoard(true);
                } else if (delta < 0) {
                    m_view->reduceBoard(true);
                }
                return true; // イベント消費
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    ShogiView* m_view = nullptr;
};

} // namespace

BoardSetupController::BoardSetupController(QObject* parent)
    : QObject(parent)
{
}

BoardSetupController::~BoardSetupController() = default;

BoardInteractionController* BoardSetupController::boardController() const
{
    return m_boardController.data();
}

void BoardSetupController::disconnectBoardInteractionController(BoardInteractionController* controller)
{
    if (!controller) {
        return;
    }

    if (m_shogiView) {
        QObject::disconnect(m_shogiView, nullptr, controller, nullptr);
    }
    QObject::disconnect(controller, nullptr, nullptr, nullptr);
}

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void BoardSetupController::setShogiView(ShogiView* view)
{
    m_shogiView = view;
}

void BoardSetupController::setGameController(ShogiGameController* gc)
{
    m_gameController = gc;
}

void BoardSetupController::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void BoardSetupController::setActivityState(AppActivityState* activity)
{
    m_activity = activity;
}

void BoardSetupController::setPositionEditController(PositionEditController* posEdit)
{
    m_posEdit = posEdit;
}

// --------------------------------------------------------
// コールバック設定
// --------------------------------------------------------

void BoardSetupController::setVariationInputCallbacks(IsVariationInputCallback isVariationInput,
                                                       RecordVariationMoveCallback recordVariationMove)
{
    m_isVariationInput = std::move(isVariationInput);
    m_recordVariationMove = std::move(recordVariationMove);
}

// --------------------------------------------------------
// セットアップ・配線
// --------------------------------------------------------

void BoardSetupController::setupBoardInteractionController()
{
    // 既存があれば入れ替え
    if (m_boardController) {
        m_boardController->clearAllHighlights();
        disconnectBoardInteractionController(m_boardController);
        m_boardController->deleteLater();
        m_boardController = nullptr;
    }

    // コントローラ生成
    m_boardController = new BoardInteractionController(m_shogiView, m_gameController, this);

    // 盤クリックの配線
    connectBoardClicks();

    // 着手要求（moveRequested）はここではつながない。MainWindowComposer が
    // MainWindowActions::handleMoveRequested へつなぎ、そこから onMoveRequested() を呼ぶ

    // 既定モード
    m_boardController->setMode(BoardInteractionController::Mode::HumanVsHuman);

    // Ctrl+ホイールで盤サイズ変更するフィルタを装着
    if (m_shogiView && !m_wheelFilter) {
        m_wheelFilter = new BoardWheelZoomFilter(m_shogiView, this);
        m_shogiView->installEventFilter(m_wheelFilter);
    }
}

void BoardSetupController::connectBoardClicks()
{
    if (!m_shogiView || !m_boardController) {
        qCWarning(lcUi) << "connectBoardClicks: missing shogiView or boardController";
        return;
    }

    QObject::connect(m_shogiView, &ShogiView::clicked,
                     m_boardController, &BoardInteractionController::onLeftClick,
                     Qt::UniqueConnection);

    QObject::connect(m_shogiView, &ShogiView::rightClicked,
                     m_boardController, &BoardInteractionController::onRightClick,
                     Qt::UniqueConnection);
}

// --------------------------------------------------------
// スロット
// --------------------------------------------------------

void BoardSetupController::onMoveRequested(const QPoint& from, const QPoint& to)
{
    qCDebug(lcUi) << "onMoveRequested from=" << from << " to=" << to;

    // --- 編集モードは Controller へ丸投げ ---
    if (m_boardController && m_boardController->mode() == BoardInteractionController::Mode::Edit) {
        if (!m_posEdit || !m_shogiView || !m_gameController) return;

        const bool ok = m_posEdit->applyEditMove(from, to, m_shogiView, m_gameController, m_boardController);
        if (!ok) qCDebug(lcUi) << "editPosition failed (edit-mode move rejected)";
        return;
    }

    // --- 対局していないとき：指した手を棋譜の変化として記録する ---
    if (m_isVariationInput && m_recordVariationMove && m_isVariationInput()) {
        QPoint hFrom = from, hTo = to;
        const bool ok = m_recordVariationMove(hFrom, hTo);
        if (m_boardController) m_boardController->onMoveApplied(hFrom, hTo, ok);
        if (!ok) qCDebug(lcUi) << "variation move rejected";
        return;
    }

    const AppActivityState::Activity activity =
        m_activity ? m_activity->activity() : AppActivityState::Activity::Idle;

    if (activity == AppActivityState::Activity::DuringCsaGame) {
        // 通信対局は確認を受けてから確定する。入力側は盤面や履歴を変更しない。
        Q_EMIT csaMoveRequested(from, to);
        if (m_boardController) m_boardController->onMoveApplied(from, to, false);
        return;
    }

    // ▼▼▼ 対局（人間の手番） ▼▼▼
    if (!m_gameController || !m_match || activity != AppActivityState::Activity::DuringGame
        || !m_match->canAcceptMove() || !m_match->isHumanSide(m_gameController->currentPlayer())) {
        if (m_boardController) m_boardController->onMoveApplied(from, to, false);
        return;
    }
    const auto update = m_gameController->positionUpdate();

    // validateAndMove は成りの選択などで座標を書き換えるのでローカルに退避
    QPoint hFrom = from, hTo = to;
    const std::optional<RecordEntry> played = m_gameController->validateAndMove(hFrom, hTo);

    if (m_boardController) m_boardController->onMoveApplied(hFrom, hTo, played.has_value());
    if (!played) {
        qCDebug(lcUi) << "validateAndMove failed (human move rejected)";
        return;
    }

    // 考慮時間の確定・棋譜への追記・千日手と最大手数の判定・エンジンへの手番渡しは対局の司令塔が行う
    m_match->onHumanMove(*played);
}
