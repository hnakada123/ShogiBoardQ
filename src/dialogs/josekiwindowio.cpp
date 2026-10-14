/// @file josekiwindowio.cpp
/// @brief JosekiWindow のファイル操作（開く・新規・保存・最近使ったファイル）と、その確認ダイアログ
///
/// 読み書きそのものと、ファイルパス・変更状態・履歴の記録は JosekiBookSession が行う。

#include "josekiwindow.h"
#include "josekibooksession.h"
#include "josekimergedialog.h"
#include "josekirepository.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>

namespace {
QString bookFileFilter()
{
    return QCoreApplication::translate("JosekiWindow", "定跡ファイル (*.db);;すべてのファイル (*)");
}

QString withBookSuffix(QString filePath)
{
    if (!filePath.isEmpty() && !filePath.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive))
        filePath += QStringLiteral(".db");
    return filePath;
}
} // namespace

// ============================================================
// ファイル操作スロット
// ============================================================

void JosekiWindow::onOpenButtonClicked()
{
    if (isIoBusy() || !confirmDiscardChanges()) return;
    const QString startDir = m_session->filePath().isEmpty() ? QString() : QFileInfo(m_session->filePath()).absolutePath();
    const QString filePath = QFileDialog::getOpenFileName(this, tr("定跡ファイルを開く"), startDir, bookFileFilter());
    if (!filePath.isEmpty()) loadAndApplyFileAsync(filePath);
}

void JosekiWindow::onNewButtonClicked()
{
    if (!confirmDiscardChanges()) return;
    closeMergeDialogs();
    m_session->newBook();
    m_filePathLabel->setText(tr("新規ファイル（未保存）"));
    m_filePathLabel->setStyleSheet(QStringLiteral("color: blue;"));
    updateStatusDisplay();
    updateJosekiDisplay();
}

void JosekiWindow::onSaveButtonClicked()
{
    if (isIoBusy()) return;
    if (m_session->filePath().isEmpty()) { onSaveAsButtonClicked(); return; }
    saveToFileAsync(m_session->filePath());
}

void JosekiWindow::onSaveAsButtonClicked()
{
    if (isIoBusy()) return;
    const QString filePath = selectSaveFilePath();
    if (!filePath.isEmpty()) saveToFileAsync(filePath);
}

// ============================================================
// 最近使ったファイル
// ============================================================

void JosekiWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear();
    const QStringList &files = m_session->recentFiles();
    if (files.isEmpty()) {
        m_recentFilesMenu->addAction(tr("（履歴なし）"))->setEnabled(false);
        return;
    }
    for (const QString &filePath : files) {
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
    if (isIoBusy()) return;
    // 「履歴をクリア」は最近使ったファイルのみを対象とし、現在の編集中データ（未保存変更を含む）は保持する。
    m_session->clearRecentFiles();
    saveSettings();  // 履歴にないファイルは前回のファイルとして記録しない（次回は自動読込しない）
    showTransientStatus(tr("最近使ったファイル履歴をクリアしました"));
}

void JosekiWindow::onRecentFileClicked()
{
    if (isIoBusy()) return;
    auto *action = qobject_cast<QAction*>(sender());
    if (!action) return;
    const QString filePath = action->data().toString();
    if (filePath.isEmpty() || !confirmDiscardChanges()) return;

    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, tr("エラー"), tr("ファイルが見つかりません: %1").arg(filePath));
        m_session->removeRecentFile(filePath);
        return;
    }
    loadAndApplyFileAsync(filePath);
}

// ============================================================
// 読み込み・保存
// ============================================================

bool JosekiWindow::isIoBusy() const
{
    return m_session->isBusy();
}

void JosekiWindow::loadAndApplyFileAsync(const QString &filePath)
{
    if (isIoBusy()) return;
    closeMergeDialogs();
    m_statusLabel->setText(tr("読み込み中..."));
    m_session->startLoad(filePath);
}

void JosekiWindow::saveToFileAsync(const QString &filePath)
{
    if (isIoBusy()) return;
    if (filePath != m_session->filePath()) closeMergeDialogs();
    m_statusLabel->setText(tr("保存中..."));
    m_session->startSave(filePath);
}

bool JosekiWindow::saveToFile(const QString &filePath)
{
    QString errorMessage;
    if (!m_session->repository()->saveToFile(filePath, &errorMessage)) {
        QMessageBox::warning(this, tr("エラー"), errorMessage);
        return false;
    }
    return true;
}

void JosekiWindow::onBookLoaded(const QString &filePath)
{
    Q_UNUSED(filePath);
    showFilePath();
    saveSettings();
    updateStatusDisplay();
    updateJosekiDisplay();
}

void JosekiWindow::onBookLoadFailed(const QString &errorMessage)
{
    if (!errorMessage.isEmpty()) QMessageBox::warning(this, tr("エラー"), errorMessage);
    updateStatusDisplay();
}

void JosekiWindow::onBookSaved(const QString &filePath)
{
    applySavedFilePath(filePath);
}

void JosekiWindow::onBookSaveFailed(const QString &errorMessage)
{
    QMessageBox::warning(this, tr("エラー"), errorMessage);
    updateStatusDisplay();
}

void JosekiWindow::applySavedFilePath(const QString &filePath)
{
    if (filePath != m_session->filePath()) closeMergeDialogs();
    m_session->markSaved(filePath);
    showFilePath();
    saveSettings();
    updateStatusDisplay();
}

void JosekiWindow::onBusyChanged(bool busy)
{
    const bool modified = m_session->isModified();
    m_openButton->setEnabled(!busy);
    m_newButton->setEnabled(!busy);
    m_saveButton->setEnabled(!busy && modified);
    m_saveAsButton->setEnabled(!busy);
    m_recentButton->setEnabled(!busy);
    m_addMoveButton->setEnabled(!busy && !m_currentSfen.isEmpty());
    m_mergeButton->setEnabled(!busy);
    m_tableWidget->setEnabled(!busy);
    m_actionEdit->setEnabled(!busy);
    m_actionDelete->setEnabled(!busy);
    const auto dialogs = findChildren<JosekiMergeDialog*>();
    for (JosekiMergeDialog *dialog : dialogs) dialog->setEnabled(!busy);

    if (busy) QApplication::setOverrideCursor(Qt::WaitCursor);
    else QApplication::restoreOverrideCursor();
}

// ============================================================
// 変更状態
// ============================================================

void JosekiWindow::setModified(bool modified)
{
    m_session->setModified(modified);
}

void JosekiWindow::onModifiedChanged(bool modified)
{
    m_saveButton->setEnabled(!isIoBusy() && modified);
    updateWindowTitle();
    if (m_dockWidget) m_dockWidget->setWindowTitle(modified ? tr("定跡") + QStringLiteral(" *") : tr("定跡"));
    updateStatusDisplay();
}

bool JosekiWindow::confirmDiscardChanges()
{
    if (isIoBusy()) {
        QMessageBox::information(this, tr("処理中"), tr("ファイルの読み込み/保存が完了するまでお待ちください。"));
        return false;
    }
    if (!m_session->isModified()) return true;
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("確認"));
    msgBox.setText(tr("定跡データに未保存の変更があります。\n保存しますか？"));
    msgBox.setIcon(QMessageBox::Question);
    QPushButton *saveBtn = msgBox.addButton(tr("保存"), QMessageBox::AcceptRole);
    QPushButton *discardBtn = msgBox.addButton(tr("破棄"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("キャンセル"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(saveBtn);
    msgBox.exec();
    if (msgBox.clickedButton() == saveBtn) {
        const QString filePath = m_session->filePath().isEmpty() ? selectSaveFilePath() : m_session->filePath();
        if (filePath.isEmpty()) return false;
        // 同期保存（ダイアログ応答のフロー上、完了を待つ必要がある）
        if (saveToFile(filePath)) applySavedFilePath(filePath);
        return !m_session->isModified();
    }
    return msgBox.clickedButton() == discardBtn;
}

// ============================================================
// 保存先の選択
// ============================================================

QString JosekiWindow::selectSaveFilePath()
{
    const QString startDir = m_session->filePath().isEmpty() ? QString() : QFileInfo(m_session->filePath()).absolutePath();
    return withBookSuffix(QFileDialog::getSaveFileName(this, tr("定跡ファイルを保存"), startDir, bookFileFilter()));
}

bool JosekiWindow::ensureFilePath()
{
    if (!m_session->filePath().isEmpty()) return true;

    const QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("保存先の指定"),
        tr("定跡ファイルの保存先が設定されていません。\nOKを選択すると保存先が指定できます。"),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
    if (result != QMessageBox::Ok) return false;

    const QString filePath = withBookSuffix(QFileDialog::getSaveFileName(
        this, tr("定跡ファイルの保存先を指定"), QDir::homePath(), bookFileFilter()));
    if (filePath.isEmpty() || !saveToFile(filePath)) return false;
    applySavedFilePath(filePath);
    return true;
}

void JosekiWindow::closeMergeDialogs()
{
    const auto dialogs = findChildren<JosekiMergeDialog*>();
    for (JosekiMergeDialog *dialog : dialogs) {
        disconnect(dialog, &JosekiMergeDialog::registerMove, this, &JosekiWindow::onMergeRegisterMove);
        disconnect(dialog, &JosekiMergeDialog::registerAllMoves, this, &JosekiWindow::onMergeRegisterAllMoves);
        disconnect(this, &JosekiWindow::mergeRegistrationFinished, dialog, &JosekiMergeDialog::onRegistrationFinished);
        dialog->setEnabled(false);
        dialog->reject();
    }
}
