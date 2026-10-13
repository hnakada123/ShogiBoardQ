/// @file josekiwindowwiring.cpp
/// @brief 定跡ウィンドウ配線クラスの実装

#include "josekiwindowwiring.h"

#include "logcategories.h"

#include "josekiwindow.h"
#include "shogigamecontroller.h"
#include "boardcoordinates.h"

#include <QDockWidget>

JosekiWindowWiring::JosekiWindowWiring(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_gameController(deps.gameController)
    , m_currentSfenStr(deps.currentSfenStr)
    , m_currentLine(deps.currentLine)
    , m_isHumanTurnNow(deps.isHumanTurnNow)
{
}

void JosekiWindowWiring::ensureJosekiWindow()
{
    if (m_josekiWindow) return;

    m_josekiWindow = new JosekiWindow(m_parentWidget);

    // 定跡手選択シグナルを接続
    connect(m_josekiWindow, &JosekiWindow::josekiMoveSelected,
            this, &JosekiWindowWiring::onJosekiMoveSelected);

    // 定跡手を指せるかは、表示を作り直すたびに盤面クリックと同じ判定で問い合わせる
    m_josekiWindow->setHumanCanPlayProvider(std::bind(&JosekiWindowWiring::determineHumanCanPlay, this));

    // 棋譜データ要求シグナルを接続
    connect(m_josekiWindow, &JosekiWindow::requestKifuDataForMerge,
            this, &JosekiWindowWiring::onRequestKifuDataForMerge);

    qCDebug(lcUi) << "JosekiWindow created and connected";
}

void JosekiWindowWiring::attachDock(QDockWidget* dock)
{
    m_dockWidget = dock;
    m_dockShown = dock && dock->isVisible();
    if (dock) {
        connect(dock, &QDockWidget::visibilityChanged,
                this, &JosekiWindowWiring::onDockVisibilityChanged);
    }
}

void JosekiWindowWiring::onDockVisibilityChanged(bool visible)
{
    m_dockShown = visible;
    // 裏に回っている間に進んだ局面は、前面に戻ったときに反映する
    if (visible) updateJosekiWindow();
}

void JosekiWindowWiring::updateJosekiWindow()
{
    // 定跡ウィンドウが存在し、表示されている場合のみ更新。タブの裏に回ったドックの中身は
    // isVisible() が真のままなので、ドックが前面にあるかも見る（1手ごとに呼ばれる）
    if (!m_josekiWindow || !m_josekiWindow->isVisible()) {
        return;
    }
    if (m_dockWidget && !m_dockShown) return;

    if (!m_currentSfenStr) return;

    // 現在の局面のSFENを定跡ウィンドウに設定する。人間が着手可能かどうかは、定跡ウィンドウが
    // 表示を作り直すたびに setHumanCanPlayProvider の判定で問い合わせる（ここで setHumanCanPlay を
    // 呼ぶと、手番が替わるたびに表を2回作り直していた）
    m_josekiWindow->setCurrentSfen(*m_currentSfenStr);
}

bool JosekiWindowWiring::determineHumanCanPlay() const
{
    // 盤面クリックと同じ判定を使う。対局中は人間の手番だけ、対局外（終局後を含む）は
    // 指した手を棋譜の変化として記録できる待機中だけ指せる
    return m_isHumanTurnNow && m_isHumanTurnNow();
}

void JosekiWindowWiring::onJosekiMoveSelected(const QString& usiMove)
{
    qCDebug(lcUi) << "onJosekiMoveSelected: usiMove=" << usiMove;

    if (usiMove.isEmpty()) {
        qCDebug(lcUi) << "onJosekiMoveSelected: empty move";
        return;
    }
    // 最後に表示を更新した後で終局・手番交代していれば、着手ボタンを消して何もしない
    if (!determineHumanCanPlay()) {
        if (m_josekiWindow) m_josekiWindow->setHumanCanPlay(false);
        return;
    }

    const auto move = UsiMove::parse(usiMove);
    if (!move) {
        qCWarning(lcUi) << "onJosekiMoveSelected: invalid move format";
        if (m_josekiWindow) {
            m_josekiWindow->onMoveResult(false, usiMove);
        }
        return;
    }
    const Turn mover = m_gameController ? m_gameController->currentPlayer() : Turn::Black;
    const QPoint from = BoardCoordinates::fromSquare(*move, mover);
    const QPoint to = BoardCoordinates::toSquare(*move);

    // 定跡手からの着手の場合、成り/不成が決まっているので強制成りモードを設定
    Q_EMIT forcedPromotionRequested(true, move->promote);

    // 着手できたかは局面が変わったかで判定する
    const QString sfenBefore = m_currentSfenStr ? *m_currentSfenStr : QString();

    // 指し手実行を要求
    Q_EMIT moveRequested(from, to);
    // 上位の入力ガードで着手処理に到達しなかった場合も指定を解除する。
    Q_EMIT forcedPromotionRequested(false, false);

    const bool moveSuccess = m_currentSfenStr && *m_currentSfenStr != sfenBefore;
    qCDebug(lcUi) << "Move result: success=" << moveSuccess;

    // 定跡ウィンドウに結果を通知（失敗時のみ）
    if (m_josekiWindow && !moveSuccess) {
        m_josekiWindow->onMoveResult(false, usiMove);
    }
}

void JosekiWindowWiring::onRequestKifuDataForMerge()
{
    if (!m_josekiWindow) {
        qCWarning(lcUi) << "onRequestKifuDataForMerge: m_josekiWindow is null";
        return;
    }

    // 表示中の手順（分岐を選んでいればその分岐）の局面・指し手・表記をそろえて渡す
    const RecordLine line = m_currentLine ? m_currentLine() : RecordLine{};
    qCDebug(lcUi) << "onRequestKifuDataForMerge: moves=" << line.moveCount()
                  << "currentPly=" << line.currentPly;
    m_josekiWindow->setKifuDataForMerge(line.sfens, line.usiMoves(), line.displayTexts, line.currentPly);
}
