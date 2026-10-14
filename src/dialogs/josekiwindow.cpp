/// @file josekiwindow.cpp
/// @brief 定跡ウィンドウクラスの実装（薄い View 層: 利用者の操作）
/// 画面の構築と表示は josekiwindowui.cpp、ファイル操作は josekiwindowio.cpp、
/// 棋譜からのマージは josekiwindowmerge.cpp に分離

#include "josekiwindow.h"
#include "dialogutils.h"
#include "josekibooksession.h"
#include "josekimovedialog.h"
#include "josekipresenter.h"
#include "josekirepository.h"
#include "josekisettings.h"
#include "sfenpositiontracer.h"
#include "sfenutils.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDockWidget>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>

JosekiWindow::JosekiWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_session(new JosekiBookSession(this))
    , m_presenter(new JosekiPresenter(m_session->repository(), this))
    , m_fontHelper(JosekiSettings::josekiWindowFontSize, 6, 24, 1)
{
    setupUi();
    connect(m_presenter, &JosekiPresenter::modifiedChanged, m_session, &JosekiBookSession::setModified);
    connect(m_session, &JosekiBookSession::modifiedChanged, this, &JosekiWindow::onModifiedChanged);
    connect(m_session, &JosekiBookSession::busyChanged, this, &JosekiWindow::onBusyChanged);
    connect(m_session, &JosekiBookSession::recentFilesChanged, this, &JosekiWindow::updateRecentFilesMenu);
    connect(m_session, &JosekiBookSession::loaded, this, &JosekiWindow::onBookLoaded);
    connect(m_session, &JosekiBookSession::loadFailed, this, &JosekiWindow::onBookLoadFailed);
    connect(m_session, &JosekiBookSession::saved, this, &JosekiWindow::onBookSaved);
    connect(m_session, &JosekiBookSession::saveFailed, this, &JosekiWindow::onBookSaveFailed);
    loadSettings();
}

JosekiWindow::~JosekiWindow()
{
    // 読み書きの途中で閉じたときは待機カーソルを戻す（ワーカーは値の写しで動くので待たない）
    if (m_session->isBusy()) QApplication::restoreOverrideCursor();
}

// ============================================================
// ウィンドウライフサイクル
// ============================================================

void JosekiWindow::setDockWidget(QDockWidget *dock)
{
    m_dockWidget = dock;
    if (m_dockWidget) m_dockWidget->installEventFilter(this);
}

bool JosekiWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_dockWidget && event->type() == QEvent::Close) {
        if (!confirmClose()) { event->ignore(); return true; }
    }
    return QWidget::eventFilter(obj, event);
}

void JosekiWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmClose()) { event->ignore(); return; }
    QWidget::closeEvent(event);
}

void JosekiWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateJosekiDisplay();
}

bool JosekiWindow::confirmClose()
{
    if (!confirmDiscardChanges()) return false;
    saveSettings();
    return true;
}

// ============================================================
// 局面と着手の可否
// ============================================================

void JosekiWindow::setCurrentSfen(const QString &sfen)
{
    QString next = sfen;
    next.remove(QLatin1Char('\r'));
    if (next == QStringLiteral("startpos"))
        next = SfenUtils::hirateSfen();
    const bool sameSfen = (next == m_currentSfen);
    m_currentSfen = next;

    if (m_currentSfenLabel)
        m_currentSfenLabel->setText(m_currentSfen.isEmpty() ? tr("(未設定)") : m_currentSfen);

    // 1手ごとに複数の経路から同じ局面が届く。表に出している局面と同じで、人間が指せるかも
    // 変わらなければ作り直さない（定跡データの変更は、変更した処理が自分で表示を更新する）
    if (sameSfen && m_pendingAutoLoadPath.isEmpty() && m_displayedSfen == m_currentSfen) {
        const bool couldPlay = m_humanCanPlay;
        refreshHumanCanPlay();
        if (m_humanCanPlay == couldPlay) return;
    }

    updateJosekiDisplay();
}

void JosekiWindow::setHumanCanPlayProvider(std::function<bool()> provider)
{
    m_humanCanPlayProvider = std::move(provider);
}

void JosekiWindow::refreshHumanCanPlay()
{
    if (m_humanCanPlayProvider) m_humanCanPlay = m_humanCanPlayProvider();
}

void JosekiWindow::setHumanCanPlay(bool canPlay)
{
    if (m_humanCanPlay != canPlay) { m_humanCanPlay = canPlay; updateJosekiDisplay(); }
}

void JosekiWindow::onMoveResult(bool success, const QString &usiMove)
{
    if (!success) {
        QMessageBox::warning(this, tr("着手エラー"),
            tr("定跡手「%1」を指すことができませんでした。\n\n"
               "この定跡手は現在の局面では合法手ではない可能性があります。\n"
               "定跡ファイルのデータに誤りがある可能性があります。").arg(usiMove));
    }
}

// ============================================================
// 定跡手の操作
// ============================================================

void JosekiWindow::playMoveAt(int row)
{
    if (row < 0 || row >= m_currentMoves.size()) return;
    refreshHumanCanPlay();
    if (!m_humanCanPlay) {
        QMessageBox::information(this, tr("情報"), tr("定跡手は、対局中の自分の手番か、対局していないときに指せます。"));
        return;
    }
    emit josekiMoveSelected(m_currentMoves[row].move);
}

void JosekiWindow::editMoveAt(int row)
{
    if (isIoBusy()) return;
    if (row < 0 || row >= m_currentMoves.size()) return;

    const QString normalizedSfen = JosekiPresenter::normalizeSfen(m_currentSfen);
    if (!m_session->repository()->containsPosition(normalizedSfen)) return;

    const JosekiMove currentMove = m_currentMoves.at(row);
    SfenPositionTracer tracer;
    (void)tracer.setFromSfen(m_currentSfen);

    JosekiMoveDialog dialog(this, true);
    dialog.setMove(currentMove.move);
    dialog.setNextMove(currentMove.nextMove);
    dialog.setCurrentSfen(m_currentSfen);
    dialog.setValue(currentMove.value);
    dialog.setDepth(currentMove.depth);
    dialog.setFrequency(currentMove.frequency);
    dialog.setComment(currentMove.comment);
    dialog.setEditMoveDisplay(JosekiPresenter::usiMoveToJapanese(currentMove.move, tracer));
    if (dialog.exec() != QDialog::Accepted) return;

    m_presenter->editMove(normalizedSfen, currentMove.move,
                          dialog.value(), dialog.depth(), dialog.frequency(), dialog.comment(), dialog.nextMove());
    updateJosekiDisplay();
}

void JosekiWindow::deleteMoveAt(int row)
{
    if (isIoBusy()) return;
    if (row < 0 || row >= m_currentMoves.size()) return;

    const QString normalizedSfen = JosekiPresenter::normalizeSfen(m_currentSfen);
    if (!m_session->repository()->containsPosition(normalizedSfen)) return;

    const JosekiMove currentMove = m_currentMoves.at(row);
    SfenPositionTracer tracer;
    (void)tracer.setFromSfen(m_currentSfen);
    const QString japaneseMove = JosekiPresenter::usiMoveToJapanese(currentMove.move, tracer);
    if (!DialogUtils::confirmAction(this, tr("削除確認"),
                                    tr("定跡手「%1」を削除しますか？").arg(japaneseMove), tr("削除する"))) {
        return;
    }

    const QList<JosekiMove> &repoMoves = m_session->repository()->movesForPosition(normalizedSfen);
    for (int i = 0; i < repoMoves.size(); ++i) {
        if (repoMoves[i].move == currentMove.move) {
            m_presenter->deleteMove(normalizedSfen, i);
            break;
        }
    }
    updateJosekiDisplay();
}

void JosekiWindow::onAddMoveButtonClicked()
{
    if (isIoBusy()) return;
    if (m_currentSfen.isEmpty()) {
        QMessageBox::warning(this, tr("定跡手追加"),
            tr("局面が設定されていません。\n将棋盤で局面を表示してから定跡手を追加してください。"));
        return;
    }

    // ダイアログを開いている間に局面が進んでも、開いたときの局面に追加する
    const QString targetSfen = m_currentSfen;
    JosekiMoveDialog dialog(this, false);
    dialog.setCurrentSfen(targetSfen);
    if (dialog.exec() != QDialog::Accepted) return;

    const JosekiMove newMove{dialog.move(), dialog.nextMove(), dialog.value(),
                             dialog.depth(), dialog.frequency(), dialog.comment()};
    const QString normalizedSfen = JosekiPresenter::normalizeSfen(targetSfen);

    if (m_presenter->hasDuplicateMove(normalizedSfen, newMove.move)) {
        SfenPositionTracer tracer;
        (void)tracer.setFromSfen(targetSfen);
        if (!DialogUtils::confirmAction(this, tr("確認"),
                tr("指し手「%1」は既に登録されています。\n上書きしますか？")
                    .arg(JosekiPresenter::usiMoveToJapanese(newMove.move, tracer)),
                tr("上書きする"))) {
            return;
        }
        m_session->repository()->removeMoveByUsi(normalizedSfen, newMove.move);
    }

    m_presenter->addMove(normalizedSfen, targetSfen, newMove);
    updateJosekiDisplay();
}

void JosekiWindow::onPlayButtonClicked()
{
    if (auto *button = qobject_cast<QPushButton*>(sender())) playMoveAt(button->property("row").toInt());
}

void JosekiWindow::onEditButtonClicked()
{
    if (auto *button = qobject_cast<QPushButton*>(sender())) editMoveAt(button->property("row").toInt());
}

void JosekiWindow::onDeleteButtonClicked()
{
    if (auto *button = qobject_cast<QPushButton*>(sender())) deleteMoveAt(button->property("row").toInt());
}

void JosekiWindow::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    playMoveAt(row);
}

void JosekiWindow::onTableContextMenu(const QPoint &pos)
{
    if (isIoBusy()) return;
    const QModelIndex index = m_tableWidget->indexAt(pos);
    if (!index.isValid() || index.row() >= m_currentMoves.size()) return;
    refreshHumanCanPlay();
    m_actionPlay->setEnabled(m_humanCanPlay);
    m_tableWidget->selectRow(index.row());
    m_tableContextMenu->exec(m_tableWidget->viewport()->mapToGlobal(pos));
}

void JosekiWindow::onContextMenuPlay() { playMoveAt(m_tableWidget->currentRow()); }
void JosekiWindow::onContextMenuEdit() { editMoveAt(m_tableWidget->currentRow()); }
void JosekiWindow::onContextMenuDelete() { deleteMoveAt(m_tableWidget->currentRow()); }

void JosekiWindow::onContextMenuCopyMove()
{
    const int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentMoves.size()) return;
    QApplication::clipboard()->setText(m_currentMoves[row].move);
    showTransientStatus(tr("「%1」をコピーしました").arg(m_currentMoves[row].move));
}

// ============================================================
// 表示の設定
// ============================================================

void JosekiWindow::onFontSizeIncrease()
{
    if (m_fontHelper.increase()) applyFontSize();
}

void JosekiWindow::onFontSizeDecrease()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

void JosekiWindow::onAutoLoadCheckBoxChanged(Qt::CheckState state)
{
    m_autoLoadEnabled = (state == Qt::Checked);
    JosekiSettings::josekiWindowAutoLoadEnabled.set(m_autoLoadEnabled);
}

void JosekiWindow::onStopButtonClicked()
{
    m_displayEnabled = !m_stopButton->isChecked();
    m_stopButton->setText(m_displayEnabled ? tr("■ 停止") : tr("▶ 再開"));
    if (m_displayEnabled) updateJosekiDisplay();
    else clearTable();
    JosekiSettings::josekiWindowDisplayEnabled.set(m_displayEnabled);
    updateStatusDisplay();
}

void JosekiWindow::onSfenDetailToggled(bool checked)
{
    m_sfenDetailWidget->setVisible(checked);
    JosekiSettings::josekiWindowSfenDetailVisible.set(checked);
}

void JosekiWindow::onRestoreStatusDisplay() { updateStatusDisplay(); }
