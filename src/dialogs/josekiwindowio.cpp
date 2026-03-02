/// @file josekiwindowio.cpp
/// @brief JosekiWindow のファイルI/O・非同期処理メソッド群

#include "josekiwindow.h"
#include "josekirepository.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QtConcurrent>

// ============================================================
// ファイル操作ヘルパー（同期）
// ============================================================

bool JosekiWindow::loadAndApplyFile(const QString &filePath)
{
    QString errorMessage;
    if (!m_repository->loadFromFile(filePath, &errorMessage)) {
        if (!errorMessage.isEmpty()) QMessageBox::warning(this, tr("エラー"), errorMessage);
        return false;
    }
    m_currentFilePath = filePath;
    m_filePathLabel->setText(filePath);
    m_filePathLabel->setStyleSheet(QString());
    setModified(false);
    updateStatusDisplay();
    return true;
}

bool JosekiWindow::saveToFile(const QString &filePath)
{
    QString errorMessage;
    if (!m_repository->saveToFile(filePath, &errorMessage)) {
        QMessageBox::warning(this, tr("エラー"), errorMessage);
        return false;
    }
    return true;
}

bool JosekiWindow::ensureFilePath()
{
    if (!m_currentFilePath.isEmpty()) return true;

    QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("保存先の指定"),
        tr("定跡ファイルの保存先が設定されていません。\nOKを選択すると保存先が指定できます。"),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
    if (result != QMessageBox::Ok) return false;

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("定跡ファイルの保存先を指定"), QDir::homePath(),
        tr("定跡ファイル (*.db);;すべてのファイル (*)"));
    if (filePath.isEmpty()) return false;

    if (!filePath.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive))
        filePath += QStringLiteral(".db");

    m_currentFilePath = filePath;
    m_filePathLabel->setText(filePath);
    m_filePathLabel->setStyleSheet(QString());

    if (!saveToFile(m_currentFilePath)) {
        m_currentFilePath.clear();
        m_filePathLabel->setText(tr("新規ファイル（未保存）"));
        m_filePathLabel->setStyleSheet(QStringLiteral("color: blue;"));
        return false;
    }
    addToRecentFiles(m_currentFilePath);
    updateStatusDisplay();
    return true;
}

// ============================================================
// 非同期I/O
// ============================================================

bool JosekiWindow::isIoBusy() const
{
    return m_ioBusy;
}

void JosekiWindow::setIoBusy(bool busy)
{
    m_ioBusy = busy;

    m_openButton->setEnabled(!busy);
    m_newButton->setEnabled(!busy);
    m_saveButton->setEnabled(!busy && !m_currentFilePath.isEmpty() && m_modified);
    m_saveAsButton->setEnabled(!busy);
    m_recentButton->setEnabled(!busy);
    m_addMoveButton->setEnabled(!busy);
    m_mergeButton->setEnabled(!busy);

    if (busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
    } else {
        QApplication::restoreOverrideCursor();
    }
}

void JosekiWindow::loadAndApplyFileAsync(const QString &filePath)
{
    if (isIoBusy()) return;

    setIoBusy(true);
    m_currentFilePath = filePath;
    if (m_statusLabel) {
        m_statusLabel->setText(tr("読み込み中..."));
    }

    m_loadWatcher.setFuture(QtConcurrent::run(&JosekiRepository::parseFromFile, filePath));
}

void JosekiWindow::onAsyncLoadFinished()
{
    JosekiLoadResult result = m_loadWatcher.result();
    setIoBusy(false);

    if (!result.success) {
        if (!result.errorMessage.isEmpty()) {
            QMessageBox::warning(this, tr("エラー"), result.errorMessage);
        }
        m_currentFilePath.clear();
        m_filePathLabel->setText(tr("未選択"));
        m_filePathLabel->setStyleSheet(QStringLiteral("color: gray;"));
        updateStatusDisplay();
        return;
    }

    m_repository->applyLoadResult(std::move(result));
    m_filePathLabel->setText(m_currentFilePath);
    m_filePathLabel->setStyleSheet(QString());
    setModified(false);
    updateStatusDisplay();
    updateJosekiDisplay();
}

void JosekiWindow::saveToFileAsync(const QString &filePath)
{
    if (isIoBusy()) return;

    setIoBusy(true);
    m_pendingSaveFilePath = filePath;
    if (m_statusLabel) {
        m_statusLabel->setText(tr("保存中..."));
    }

    // スナップショットをコピーしてワーカーに渡す
    const QMap<QString, QList<JosekiMove>> dataCopy = m_repository->josekiData();
    const QMap<QString, QString> sfenCopy = m_repository->sfenWithPlyMap();

    m_saveWatcher.setFuture(
        QtConcurrent::run(&JosekiRepository::serializeToFile, filePath, dataCopy, sfenCopy));
}

void JosekiWindow::onAsyncSaveFinished()
{
    JosekiSaveResult result = m_saveWatcher.result();
    setIoBusy(false);

    if (!result.success) {
        QMessageBox::warning(this, tr("エラー"), result.errorMessage);
        updateStatusDisplay();
        return;
    }

    setModified(false);

    // 名前を付けて保存の場合にファイルパスを更新
    if (!m_pendingSaveFilePath.isEmpty() && m_pendingSaveFilePath != m_currentFilePath) {
        m_currentFilePath = m_pendingSaveFilePath;
        m_filePathLabel->setText(m_currentFilePath);
        m_filePathLabel->setStyleSheet(QString());
        addToRecentFiles(m_currentFilePath);
        saveSettings();
    }
    m_pendingSaveFilePath.clear();

    updateStatusDisplay();
}
