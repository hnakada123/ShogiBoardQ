/// @file josekiwindow.cpp
/// @brief 定跡ウィンドウクラスの実装（薄い View 層）
/// setupUi() / applyFontSize() は josekiwindowui.cpp に分離
/// ファイルI/O / 非同期処理は josekiwindowio.cpp に分離

#include "josekiwindow.h"
#include "josekirepository.h"
#include "josekipresenter.h"

#include <QCloseEvent>
#include <QShowEvent>
#include <QDockWidget>

#include "josekimovedialog.h"
#include "josekimergedialog.h"
#include "josekisettings.h"
#include "sfenpositiontracer.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QClipboard>
#include <QApplication>
#include <QTimer>

JosekiWindow::JosekiWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_repository(new JosekiRepository())
    , m_presenter(new JosekiPresenter(m_repository, this))
    , m_fontHelper({JosekiSettings::josekiWindowFontSize(), 6, 24, 1,
                    JosekiSettings::setJosekiWindowFontSize})
{
    setupUi();
    loadSettings();

    connect(&m_loadWatcher, &QFutureWatcher<JosekiLoadResult>::finished,
            this, &JosekiWindow::onAsyncLoadFinished);
    connect(&m_saveWatcher, &QFutureWatcher<JosekiSaveResult>::finished,
            this, &JosekiWindow::onAsyncSaveFinished);
}

int JosekiWindow::currentPlyNumber() const
{
    const QStringList sfenParts = m_currentSfen.split(QChar(' '));
    if (sfenParts.size() >= 4) {
        bool ok;
        int ply = sfenParts.at(3).toInt(&ok);
        if (ok) return ply;
    }
    return 1;
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
        if (!confirmDiscardChanges()) { event->ignore(); return true; }
        saveSettings();
    }
    return QWidget::eventFilter(obj, event);
}

void JosekiWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmDiscardChanges()) { event->ignore(); return; }
    saveSettings();
    QWidget::closeEvent(event);
}

void JosekiWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    applyFontSize();
}

void JosekiWindow::onFontSizeIncrease()
{
    if (m_fontHelper.increase()) applyFontSize();
}

void JosekiWindow::onFontSizeDecrease()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

// ============================================================
// ファイル操作スロット
// ============================================================

void JosekiWindow::onOpenButtonClicked()
{
    if (isIoBusy()) return;
    if (!confirmDiscardChanges()) return;

    QString startDir;
    if (!m_currentFilePath.isEmpty()) startDir = QFileInfo(m_currentFilePath).absolutePath();

    QString filePath = QFileDialog::getOpenFileName(
        this, tr("定跡ファイルを開く"), startDir,
        tr("定跡ファイル (*.db);;すべてのファイル (*)"));

    if (!filePath.isEmpty()) {
        addToRecentFiles(filePath);
        saveSettings();
        loadAndApplyFileAsync(filePath);
    }
}

void JosekiWindow::onNewButtonClicked()
{
    if (!confirmDiscardChanges()) return;
    m_repository->clear();
    m_currentFilePath.clear();
    m_filePathLabel->setText(tr("新規ファイル（未保存）"));
    m_filePathLabel->setStyleSheet(QStringLiteral("color: blue;"));
    setModified(false);
    updateStatusDisplay();
    updateJosekiDisplay();
}

void JosekiWindow::onSaveButtonClicked()
{
    if (isIoBusy()) return;
    if (m_currentFilePath.isEmpty()) { onSaveAsButtonClicked(); return; }
    saveToFileAsync(m_currentFilePath);
}

void JosekiWindow::onSaveAsButtonClicked()
{
    if (isIoBusy()) return;

    QString startDir;
    if (!m_currentFilePath.isEmpty()) startDir = QFileInfo(m_currentFilePath).absolutePath();

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("定跡ファイルを保存"), startDir,
        tr("定跡ファイル (*.db);;すべてのファイル (*)"));
    if (filePath.isEmpty()) return;

    if (!filePath.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive))
        filePath += QStringLiteral(".db");

    saveToFileAsync(filePath);
}

// ============================================================
// 最近使ったファイル
// ============================================================

void JosekiWindow::addToRecentFiles(const QString &filePath)
{
    m_recentFiles.removeAll(filePath);
    m_recentFiles.prepend(filePath);
    while (m_recentFiles.size() > 5) m_recentFiles.removeLast();
    updateRecentFilesMenu();
}

void JosekiWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear();
    if (m_recentFiles.isEmpty()) {
        QAction *emptyAction = m_recentFilesMenu->addAction(tr("（履歴なし）"));
        emptyAction->setEnabled(false);
        return;
    }
    for (const QString &filePath : std::as_const(m_recentFiles)) {
        QAction *action = m_recentFilesMenu->addAction(QFileInfo(filePath).fileName());
        action->setData(filePath);
        action->setToolTip(filePath);
        connect(action, &QAction::triggered, this, &JosekiWindow::onRecentFileClicked);
    }
    m_recentFilesMenu->addSeparator();
    connect(m_recentFilesMenu->addAction(tr("履歴をクリア")),
            &QAction::triggered, this, &JosekiWindow::onClearRecentFilesClicked);
}

void JosekiWindow::onClearRecentFilesClicked()
{
    m_recentFiles.clear();
    m_currentFilePath.clear();
    m_repository->clear();
    m_currentMoves.clear();
    m_filePathLabel->setText(tr("未選択"));
    clearTable();
    updateStatusDisplay();
    updateRecentFilesMenu();
    saveSettings();
}

void JosekiWindow::onRecentFileClicked()
{
    if (isIoBusy()) return;

    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) return;
    QString filePath = action->data().toString();
    if (filePath.isEmpty() || !confirmDiscardChanges()) return;

    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, tr("エラー"), tr("ファイルが見つかりません: %1").arg(filePath));
        m_recentFiles.removeAll(filePath);
        updateRecentFilesMenu();
        return;
    }
    addToRecentFiles(filePath);
    saveSettings();
    loadAndApplyFileAsync(filePath);
}

// ============================================================
// 局面設定
// ============================================================

void JosekiWindow::setCurrentSfen(const QString &sfen)
{
    m_currentSfen = sfen;
    m_currentSfen.remove(QLatin1Char('\r'));
    if (m_currentSfen == QStringLiteral("startpos"))
        m_currentSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    if (m_currentSfenLabel)
        m_currentSfenLabel->setText(m_currentSfen.isEmpty() ? tr("(未設定)") : m_currentSfen);

    updateJosekiDisplay();
}

void JosekiWindow::setHumanCanPlay(bool canPlay)
{
    if (m_humanCanPlay != canPlay) { m_humanCanPlay = canPlay; updateJosekiDisplay(); }
}

// ============================================================
// 定跡手操作（統合ヘルパー）
// ============================================================

void JosekiWindow::editMoveAt(int row)
{
    if (row < 0 || row >= m_currentMoves.size()) return;

    QString normalizedSfen = JosekiPresenter::normalizeSfen(m_currentSfen);
    if (!m_repository->containsPosition(normalizedSfen)) return;

    const JosekiMove &currentMove = m_currentMoves.at(row);
    SfenPositionTracer tracer;
    (void)tracer.setFromSfen(m_currentSfen);
    QString japaneseMoveStr = JosekiPresenter::usiMoveToJapanese(currentMove.move, currentPlyNumber(), tracer);

    JosekiMoveDialog dialog(this, true);
    dialog.setValue(currentMove.value);
    dialog.setDepth(currentMove.depth);
    dialog.setFrequency(currentMove.frequency);
    dialog.setComment(currentMove.comment);
    dialog.setEditMoveDisplay(japaneseMoveStr);
    if (dialog.exec() != QDialog::Accepted) return;

    m_presenter->editMove(normalizedSfen, currentMove.move,
                          dialog.value(), dialog.depth(), dialog.frequency(), dialog.comment());
    setModified(true);
    updateJosekiDisplay();
}

void JosekiWindow::deleteMoveAt(int row)
{
    if (row < 0 || row >= m_currentMoves.size()) return;

    QString normalizedSfen = JosekiPresenter::normalizeSfen(m_currentSfen);
    if (!m_repository->containsPosition(normalizedSfen)) return;

    const JosekiMove &currentMove = m_currentMoves.at(row);

    // 日本語表記で確認（ボタンからの場合）
    SfenPositionTracer tracer;
    (void)tracer.setFromSfen(m_currentSfen);
    QString japaneseMoveStr = JosekiPresenter::usiMoveToJapanese(currentMove.move, currentPlyNumber(), tracer);

    QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("削除確認"),
        tr("定跡手「%1」を削除しますか？").arg(japaneseMoveStr),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result != QMessageBox::Yes) return;

    const QVector<JosekiMove> &repoMoves = m_repository->movesForPosition(normalizedSfen);
    for (int i = 0; i < repoMoves.size(); ++i) {
        if (repoMoves[i].move == currentMove.move) {
            m_presenter->deleteMove(normalizedSfen, i);
            break;
        }
    }
    setModified(true);
    updateJosekiDisplay();
}

// ============================================================
// 定跡手操作スロット
// ============================================================

void JosekiWindow::onPlayButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    bool ok;
    int row = button->property("row").toInt(&ok);
    if (!ok || row < 0 || row >= m_currentMoves.size()) return;
    emit josekiMoveSelected(m_currentMoves[row].move);
}

void JosekiWindow::onAddMoveButtonClicked()
{
    if (m_currentSfen.isEmpty()) {
        QMessageBox::warning(this, tr("定跡手追加"),
            tr("局面が設定されていません。\n将棋盤で局面を表示してから定跡手を追加してください。"));
        return;
    }

    JosekiMoveDialog dialog(this, false);
    if (dialog.exec() != QDialog::Accepted) return;

    JosekiMove newMove;
    newMove.move = dialog.move();
    newMove.nextMove = dialog.nextMove();
    newMove.value = dialog.value();
    newMove.depth = dialog.depth();
    newMove.frequency = dialog.frequency();
    newMove.comment = dialog.comment();

    QString normalizedSfen = JosekiPresenter::normalizeSfen(m_currentSfen);

    if (m_presenter->hasDuplicateMove(normalizedSfen, newMove.move)) {
        QMessageBox::StandardButton result = QMessageBox::question(
            this, tr("確認"),
            tr("指し手「%1」は既に登録されています。\n上書きしますか？").arg(newMove.move),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (result == QMessageBox::No) return;
        m_repository->removeMoveByUsi(normalizedSfen, newMove.move);
    }

    m_presenter->addMove(normalizedSfen, m_currentSfen, newMove);
    setModified(true);
    updateJosekiDisplay();
}

void JosekiWindow::onEditButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    bool ok;
    int row = button->property("row").toInt(&ok);
    if (ok) editMoveAt(row);
}

void JosekiWindow::onDeleteButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    bool ok;
    int row = button->property("row").toInt(&ok);
    if (ok) deleteMoveAt(row);
}

void JosekiWindow::onAutoLoadCheckBoxChanged(Qt::CheckState state)
{
    m_autoLoadEnabled = (state == Qt::Checked);
    JosekiSettings::setJosekiWindowAutoLoadEnabled(m_autoLoadEnabled);
}

void JosekiWindow::onStopButtonClicked()
{
    m_displayEnabled = !m_stopButton->isChecked();
    if (m_displayEnabled) {
        m_stopButton->setText(tr("■ 停止"));
        m_stopButton->setStyleSheet(QString());
        updateJosekiDisplay();
    } else {
        m_stopButton->setText(tr("▶ 再開"));
        m_stopButton->setStyleSheet(QStringLiteral("color: #cc0000; font-weight: bold;"));
        clearTable();
    }
    JosekiSettings::setJosekiWindowDisplayEnabled(m_displayEnabled);
    updateStatusDisplay();
}

void JosekiWindow::onSfenDetailToggled(bool checked)
{
    m_sfenDetailWidget->setVisible(checked);
    JosekiSettings::setJosekiWindowSfenDetailVisible(checked);
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
// テーブルイベント
// ============================================================

void JosekiWindow::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (!m_humanCanPlay) {
        QMessageBox::information(this, tr("情報"), tr("現在はエンジンの手番のため着手できません。"));
        return;
    }
    if (row >= 0 && row < m_currentMoves.size())
        emit josekiMoveSelected(m_currentMoves[row].move);
}

void JosekiWindow::onTableContextMenu(const QPoint &pos)
{
    QModelIndex index = m_tableWidget->indexAt(pos);
    if (!index.isValid()) return;
    int row = index.row();
    if (row < 0 || row >= m_currentMoves.size()) return;
    m_actionPlay->setEnabled(m_humanCanPlay);
    m_tableWidget->selectRow(row);
    m_tableContextMenu->exec(m_tableWidget->viewport()->mapToGlobal(pos));
}

void JosekiWindow::onContextMenuPlay()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentMoves.size()) return;
    if (!m_humanCanPlay) {
        QMessageBox::information(this, tr("情報"), tr("現在はエンジンの手番のため着手できません。"));
        return;
    }
    emit josekiMoveSelected(m_currentMoves[row].move);
}

void JosekiWindow::onContextMenuEdit() { editMoveAt(m_tableWidget->currentRow()); }
void JosekiWindow::onContextMenuDelete() { deleteMoveAt(m_tableWidget->currentRow()); }

void JosekiWindow::onContextMenuCopyMove()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentMoves.size()) return;
    QApplication::clipboard()->setText(m_currentMoves[row].move);
    if (m_statusLabel) {
        m_statusLabel->setText(tr("「%1」をコピーしました").arg(m_currentMoves[row].move));
        QTimer::singleShot(2000, this, &JosekiWindow::onRestoreStatusDisplay);
    }
}

void JosekiWindow::onRestoreStatusDisplay() { updateStatusDisplay(); }

// ============================================================
// マージ
// ============================================================

void JosekiWindow::onMergeFromCurrentKifu()
{
    if (!ensureFilePath()) return;
    emit requestKifuDataForMerge();
}

void JosekiWindow::setKifuDataForMerge(const QStringList &sfenList, const QStringList &moveList,
                                        const QStringList &japaneseMoveList, int currentPly)
{
    if (moveList.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("棋譜に指し手がありません。"));
        return;
    }
    QVector<KifuMergeEntry> entries = m_presenter->buildMergeEntries(sfenList, moveList, japaneseMoveList, currentPly);
    if (entries.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("登録可能な指し手がありません。"));
        return;
    }

    JosekiMergeDialog *dialog = new JosekiMergeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setTargetJosekiFile(m_currentFilePath);
    dialog->setRegisteredMoves(m_repository->mergeRegisteredMoves());
    connect(dialog, &JosekiMergeDialog::registerMove, this, &JosekiWindow::onMergeRegisterMove);
    dialog->setKifuData(entries, currentPly);
    dialog->show();
}

void JosekiWindow::onMergeFromKifuFile()
{
    if (!ensureFilePath()) return;

    QString kifFilePath = QFileDialog::getOpenFileName(
        this, tr("棋譜ファイルを選択"), QDir::homePath(),
        tr("KIF形式 (*.kif *.kifu);;すべてのファイル (*)"));
    if (kifFilePath.isEmpty()) return;

    QVector<KifuMergeEntry> entries;
    QString errorMessage;
    if (!m_presenter->buildMergeEntriesFromKifFile(kifFilePath, entries, &errorMessage)) {
        if (errorMessage.isEmpty())
            QMessageBox::information(this, tr("情報"), tr("棋譜に指し手がありません。"));
        else
            QMessageBox::warning(this, tr("エラー"), tr("棋譜ファイルの読み込みに失敗しました。\n%1").arg(errorMessage));
        return;
    }
    if (entries.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("登録可能な指し手がありません。"));
        return;
    }

    JosekiMergeDialog *dialog = new JosekiMergeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setTargetJosekiFile(m_currentFilePath);
    dialog->setRegisteredMoves(m_repository->mergeRegisteredMoves());
    dialog->setWindowTitle(tr("棋譜から定跡にマージ - %1").arg(QFileInfo(kifFilePath).fileName()));
    connect(dialog, &JosekiMergeDialog::registerMove, this, &JosekiWindow::onMergeRegisterMove);
    dialog->setKifuData(entries, -1);
    dialog->show();
}

void JosekiWindow::onMergeRegisterMove(const QString &sfen, const QString &sfenWithPly, const QString &usiMove)
{
    m_presenter->registerMergeMove(sfen, sfenWithPly, usiMove, m_currentFilePath);
    updateJosekiDisplay();
}
