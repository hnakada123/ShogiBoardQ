/// @file josekiwindowcontroller.cpp
/// @brief 定跡ウィンドウの表示・更新と、定跡手の着手の実装

#include "josekiwindowcontroller.h"

#include "humanturnpolicy.h"
#include "logcategories.h"

#include "boardmovecontroller.h"
#include "branchtreeeditcontroller.h"
#include "josekibookcontroller.h"
#include "josekibooksession.h"
#include "josekiexpanddialog.h"
#include "josekiwindow.h"
#include "kifubranchnode.h"
#include "kifunavigationcontroller.h"
#include "josekisettings.h"
#include "kifupresentation.h"
#include "kifunavigationstate.h"

#include <QAction>
#include <QDockWidget>
#include <QMessageBox>
#include <functional>

JosekiWindowController::JosekiWindowController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_book(deps.book)
    , m_branchEdit(deps.branchEdit)
    , m_navState(deps.navState)
    , m_humanTurn(deps.humanTurn)
    , m_boardMoves(deps.boardMoves)
    , m_displayAction(new QAction(tr("定跡を表示"), this))
    , m_recordBoardMovesAction(new QAction(tr("盤の手を登録"), this))
{
    Q_ASSERT(deps.book && deps.boardArrows && m_navState && m_boardMoves && m_branchEdit && deps.navController);
    m_displayAction->setCheckable(true);
    m_displayAction->setChecked(JosekiSettings::josekiWindowDisplayEnabled());
    m_recordBoardMovesAction->setCheckable(true);
    deps.boardArrows->setChecked(JosekiSettings::boardArrowsVisible());
    connect(m_displayAction, &QAction::toggled, this, &JosekiWindowController::onDisplayToggled);
    connect(deps.boardArrows, &QAction::toggled, this, &JosekiWindowController::onBoardArrowsToggled);
    m_josekiWindow = new JosekiWindow(deps.book, {m_displayAction, m_recordBoardMovesAction, deps.boardArrows},
                                      m_parentWidget);
    // 定跡そのものの操作（ファイル・定跡手・マージ・ドロップ）は、表示中の手順を渡して任せる
    m_bookController = new JosekiBookController({deps.book, m_josekiWindow, m_navState}, this);

    // 定跡手選択シグナルを接続
    connect(m_josekiWindow, &JosekiWindow::josekiMoveSelected,
            this, &JosekiWindowController::onJosekiMoveSelected);

    // 定跡手を指せるかは、表示を作り直すたびに盤面クリックと同じ判定で問い合わせる
    m_josekiWindow->setHumanCanPlayProvider(std::bind(&JosekiWindowController::determineHumanCanPlay, this));

    connect(m_navState, &KifuNavigationState::changed, this, &JosekiWindowController::updateJosekiWindow);
    // 定跡が変わったら、表示中の手順が定跡を外れた手も合わせる
    connect(deps.book, &JosekiBookSession::contentChanged, this, &JosekiWindowController::updateJosekiWindow);
    connect(m_josekiWindow, &JosekiWindow::expandRequested, this, &JosekiWindowController::onExpandRequested);
    connect(m_josekiWindow, &JosekiWindow::deviationPlyRequested, deps.navController, &KifuNavigationController::selectPly);
}

void JosekiWindowController::attachDock(QDockWidget* dock)
{
    m_dockWidget = dock;
    m_dockShown = dock && dock->isVisible();
    if (dock) {
        connect(dock, &QDockWidget::visibilityChanged,
                this, &JosekiWindowController::onDockVisibilityChanged);
    }
    m_bookController->attachDock(dock);
}

bool JosekiWindowController::confirmClose()
{
    return m_bookController->confirmClose();
}

void JosekiWindowController::onDockVisibilityChanged(bool visible)
{
    m_dockShown = visible;
    // 裏に回っている間に進んだ局面は、前面に戻ったときに反映する
    if (visible) updateJosekiWindow();
}

void JosekiWindowController::updateJosekiWindow()
{
    // 定跡ウィンドウが存在し、表示されている場合のみ更新。タブの裏に回ったドックの中身は
    // isVisible() が真のままなので、ドックが前面にあるかも見る（1手ごとに呼ばれる）
    if (!m_josekiWindow->isVisible()) {
        return;
    }
    if (m_dockWidget && !m_dockShown) return;

    // 表示中の手の局面を定跡ウィンドウに設定する。人間が着手可能かどうかは、定跡ウィンドウが
    // 表示を作り直すたびに setHumanCanPlayProvider の判定で問い合わせる
    m_josekiWindow->setCurrentSfen(m_navState->currentSfen());
    updateBookDeviation();
}

void JosekiWindowController::onDisplayToggled(bool displayed)
{
    JosekiSettings::josekiWindowDisplayEnabled.set(displayed);
    updateBookDeviation();
}

void JosekiWindowController::onBoardArrowsToggled(bool visible)
{
    JosekiSettings::boardArrowsVisible.set(visible);
}

void JosekiWindowController::updateBookDeviation()
{
    const RecordLine line = m_navState->currentRecordLine();
    if (m_book->isEmpty() || line.moveCount() == 0 || !m_displayAction->isChecked()) {
        m_josekiWindow->setBookDeviation(-1, QString());
        return;
    }
    const QStringList sfens = line.sfens();
    const QStringList moves = line.usiMoves();
    const std::optional<int> index = JosekiLines::firstDeviation(m_book->snapshot(), sfens, moves);
    if (!index) {
        m_josekiWindow->setBookDeviation(0, QString());
        return;
    }
    const QString move = KifuPresentation::move(sfens.at(*index), moves.at(*index), KifuPresentation::options());
    m_josekiWindow->setBookDeviation(line.startPly + *index, move);
}

void JosekiWindowController::onExpandRequested()
{
    JosekiExpandDialog dialog(m_josekiWindow);
    // ダイアログを開いている間に読み書きが始まったら展開しない
    if (dialog.exec() != QDialog::Accepted || m_book->isBusy()) return;
    const JosekiLines::ExpandOptions options = dialog.options();
    if (!m_branchEdit->canEdit()) {
        QMessageBox::information(m_parentWidget, tr("定跡を棋譜に展開"), tr("対局中は棋譜に展開できません。"));
        return;
    }
    // 終局の行を表示していれば、その前の局面から展開する
    KifuBranchNode* node = m_navState->currentNode();
    if (node && node->isTerminal()) node = node->parent();
    if (!node) return;
    const std::optional<UsiMove> previous = node->isActualMove() ? std::optional<UsiMove>(node->move().move)
                                                                 : std::nullopt;
    const JosekiLines::ExpandResult result =
        JosekiLines::expand(m_book->snapshot(), node->sfen(), node->ply() + 1, previous, options);
    const int added = m_branchEdit->addVariationLines(node, result.lines);
    QString text = added > 0 ? tr("棋譜に %1 手を足しました。").arg(added)
                             : tr("足せる定跡手がありませんでした（棋譜にすでにある手は足しません）。");
    if (result.transpositions > 0)
        text += QLatin1Char('\n') + tr("別の手順で同じ局面に着いたため %1 か所で止めました。").arg(result.transpositions);
    if (result.truncated)
        text += QLatin1Char('\n') + tr("手の数が上限（%1 手）に達したため、途中で止めました。").arg(options.maxMoves);
    QMessageBox::information(m_parentWidget, tr("定跡を棋譜に展開"), text);
}

bool JosekiWindowController::determineHumanCanPlay() const
{
    // 盤面クリックと同じ判定を使う。対局中は人間の手番だけ、対局外（終局後を含む）は
    // 指した手を棋譜の変化として記録できる待機中だけ指せる
    return m_humanTurn && m_humanTurn->isHumanTurnNow();
}

void JosekiWindowController::onJosekiMoveSelected(const QString& usiMove)
{
    qCDebug(lcUi) << "onJosekiMoveSelected: usiMove=" << usiMove;

    if (usiMove.isEmpty()) {
        qCDebug(lcUi) << "onJosekiMoveSelected: empty move";
        return;
    }
    // 指せるかは定跡ウィンドウが押したときに determineHumanCanPlay で確かめている
    const auto move = UsiMove::parse(usiMove);
    if (!move) {
        qCWarning(lcUi) << "onJosekiMoveSelected: invalid move format";
        m_josekiWindow->showMoveFailed(usiMove);
        return;
    }
    // 定跡手は成り・不成が決まっているので、成りの選択を出さずにそのまま指す
    const bool moveSuccess = m_boardMoves->playUsiMove(*move);
    qCDebug(lcUi) << "Move result: success=" << moveSuccess;
    if (!moveSuccess) m_josekiWindow->showMoveFailed(usiMove);
}
