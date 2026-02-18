/// @file boardsetupcontroller.cpp
/// @brief 盤面初期設定コントローラクラスの実装

#include "boardsetupcontroller.h"

#include "loggingcategory.h"
#include <QTimer>
#include <QWheelEvent>
#include <QEvent>

#include "shogiview.h"
#include "boardinteractioncontroller.h"
#include "matchcoordinator.h"
#include "timecontrolcontroller.h"
#include "positioneditcontroller.h"
#include "shogiclock.h"
#include "shogimove.h"

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

void BoardSetupController::setTimeController(TimeControlController* tc)
{
    m_timeController = tc;
}

void BoardSetupController::setPositionEditController(PositionEditController* posEdit)
{
    m_posEdit = posEdit;
}

// --------------------------------------------------------
// 状態設定
// --------------------------------------------------------

void BoardSetupController::setPlayMode(PlayMode mode)
{
    m_playMode = mode;
}

void BoardSetupController::setSfenRecord(QStringList* sfenRecord)
{
    m_sfenHistory = sfenRecord;
}

void BoardSetupController::setGameMoves(QVector<ShogiMove>* gameMoves)
{
    m_gameMoves = gameMoves;
}

void BoardSetupController::setCurrentMoveIndex(int* currentMoveIndex)
{
    m_currentMoveIndex = currentMoveIndex;
}

// --------------------------------------------------------
// コールバック設定
// --------------------------------------------------------

void BoardSetupController::setEnsurePositionEditCallback(EnsurePositionEditCallback cb)
{
    m_ensurePositionEdit = std::move(cb);
}

void BoardSetupController::setEnsureTimeControllerCallback(EnsureTimeControllerCallback cb)
{
    m_ensureTimeController = std::move(cb);
}

void BoardSetupController::setUpdateGameRecordCallback(UpdateGameRecordCallback cb)
{
    m_updateGameRecord = std::move(cb);
}

void BoardSetupController::setRedrawEngine1GraphCallback(RedrawEngine1GraphCallback cb)
{
    m_redrawEngine1Graph = std::move(cb);
}

void BoardSetupController::setRedrawEngine2GraphCallback(RedrawEngine2GraphCallback cb)
{
    m_redrawEngine2Graph = std::move(cb);
}

void BoardSetupController::setRefreshBranchTreeCallback(RefreshBranchTreeCallback cb)
{
    m_refreshBranchTree = std::move(cb);
}

// --------------------------------------------------------
// セットアップ・配線
// --------------------------------------------------------

void BoardSetupController::setupBoardInteractionController()
{
    // 既存があれば入れ替え
    if (m_boardController) {
        m_boardController->deleteLater();
        m_boardController = nullptr;
    }

    // コントローラ生成
    m_boardController = new BoardInteractionController(m_shogiView, m_gameController, this);

    // 盤クリックの配線
    connectBoardClicks();

    // NOTE: connectMoveRequested()はMainWindow側で呼び出す
    // （MainWindowが最新のm_playModeとm_matchを設定してから委譲するため）

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

void BoardSetupController::connectMoveRequested()
{
    if (!m_boardController) {
        qCWarning(lcUi) << "connectMoveRequested: missing boardController";
        return;
    }

    QObject::connect(
        m_boardController, &BoardInteractionController::moveRequested,
        this,              &BoardSetupController::onMoveRequested,
        Qt::UniqueConnection);
}

// --------------------------------------------------------
// スロット
// --------------------------------------------------------

void BoardSetupController::onMoveRequested(const QPoint& from, const QPoint& to)
{
    qCDebug(lcUi) << "onMoveRequested from=" << from << " to=" << to
                 << " m_playMode=" << int(m_playMode);

    // --- 編集モードは Controller へ丸投げ ---
    if (m_boardController && m_boardController->mode() == BoardInteractionController::Mode::Edit) {
        if (m_ensurePositionEdit) {
            m_ensurePositionEdit();
        }
        if (!m_posEdit || !m_shogiView || !m_gameController) return;

        const bool ok = m_posEdit->applyEditMove(from, to, m_shogiView, m_gameController, m_boardController);
        if (!ok) qCDebug(lcUi) << "editPosition failed (edit-mode move rejected)";
        return;
    }

    // ▼▼▼ 通常対局 ▼▼▼
    if (!m_gameController) {
        qCWarning(lcUi) << "m_gameController is null";
        return;
    }

    const PlayMode matchMode = (m_match ? m_match->playMode() : PlayMode::NotStarted);
    PlayMode modeNow = (matchMode != PlayMode::NotStarted) ? matchMode : m_playMode;

    qCDebug(lcUi) << "effective modeNow=" << int(modeNow)
                 << "(ui m_playMode=" << int(m_playMode) << ", matchMode=" << int(matchMode) << ")";

    // 着手前の手番
    const auto moverBefore = m_gameController->currentPlayer();

    // validateAndMove は参照引数なのでローカルに退避
    QPoint hFrom = from, hTo = to;

    // 次の着手番号は「記録サイズ」を信頼する
    const int recSizeBefore = (m_sfenHistory ? static_cast<int>(m_sfenHistory->size()) : 0);
    const int nextIdx       = qMax(1, recSizeBefore);

    // 合法判定＆盤面反映
    const bool ok = m_gameController->validateAndMove(
        hFrom, hTo, m_lastMove, modeNow, const_cast<int&>(nextIdx), m_sfenHistory, *m_gameMoves);

    if (m_boardController) m_boardController->onMoveApplied(hFrom, hTo, ok);
    if (!ok) {
        qCDebug(lcUi) << "validateAndMove failed (human move rejected)";
        return;
    }

    // UI 側の現在カーソルを同期
    if (m_sfenHistory && m_currentMoveIndex) {
        *m_currentMoveIndex = static_cast<int>(m_sfenHistory->size() - 1);
    }

    Q_EMIT moveApplied(hFrom, hTo, ok);

    // --- 対局モードごとの後処理 ---
    switch (modeNow) {
    case PlayMode::HumanVsHuman: {
        qCDebug(lcUi) << "HvH: delegate post-human-move to MatchCoordinator";
        if (m_match) {
            m_match->onHumanMove(hFrom, hTo, QString());
        }

        // HvH でも「指し手＋考慮時間」を棋譜欄に追記する
        QString elapsed;
        ShogiClock* clk = m_timeController ? m_timeController->clock() : nullptr;
        if (clk) {
            elapsed = (moverBefore == ShogiGameController::Player1)
                ? clk->getPlayer1ConsiderationAndTotalTime()
                : clk->getPlayer2ConsiderationAndTotalTime();
        } else {
            if (m_ensureTimeController) {
                m_ensureTimeController();
            }
            elapsed = QStringLiteral("00:00/00:00:00");
        }

        if (m_updateGameRecord) {
            m_updateGameRecord(m_lastMove, elapsed);
        }

        // 最大手数チェック
        if (m_match && m_sfenHistory) {
            const int maxMoves = m_match->maxMoves();
            const int currentMoveIdx = static_cast<int>(m_sfenHistory->size() - 1);
            if (maxMoves > 0 && currentMoveIdx >= maxMoves) {
                m_match->handleMaxMovesJishogi();
            }
        }
        break;
    }

    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        if (m_match) {
            qCDebug(lcUi) << "HvE: forwarding to MatchCoordinator::onHumanMove";
            m_match->onHumanMove(hFrom, hTo, m_lastMove);
        }
        break;

    case PlayMode::CsaNetworkMode: {
        qCDebug(lcUi) << "CsaNetworkMode: emitting csaMoveRequested";
        qCDebug(lcUi) << "BoardSetup: hFrom=" << hFrom << "hTo=" << hTo;
        // 成り判定（移動先が敵陣であれば成りダイアログを表示すべきだが、
        // ここでは簡易的にfalseを送信。実際には成り確認ダイアログが必要）
        bool promote = m_gameController ? m_gameController->promote() : false;
        qCDebug(lcUi) << "BoardSetup: promote=" << promote;
        Q_EMIT csaMoveRequested(hFrom, hTo, promote);
        qCDebug(lcUi) << "BoardSetup: csaMoveRequested emitted";
        break;
    }

    default:
        qCDebug(lcUi) << "not a live play mode; skip post-handling";
        break;
    }
}

void BoardSetupController::onMoveCommitted(ShogiGameController::Player mover, int ply)
{
    // 分岐ツリー更新を遅延呼び出し
    if (m_refreshBranchTree) {
        QTimer::singleShot(0, this, [this]() {
            if (m_refreshBranchTree) {
                m_refreshBranchTree();
            }
        });
    }

    // EvE の評価グラフ更新
    const bool isEvE =
        (m_playMode == PlayMode::EvenEngineVsEngine) ||
        (m_playMode == PlayMode::HandicapEngineVsEngine);

    if (isEvE) {
        if (mover == ShogiGameController::Player1) {
            if (m_redrawEngine1Graph) {
                m_redrawEngine1Graph(ply);
            }
        } else if (mover == ShogiGameController::Player2) {
            if (m_redrawEngine2Graph) {
                m_redrawEngine2Graph(ply);
            }
        }
    }
}
