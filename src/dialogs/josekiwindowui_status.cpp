/// @file josekiwindowui_status.cpp
/// @brief JosekiWindow の状態表示・更新メソッド群

#include "josekiwindow.h"

#include "josekirepository.h"
#include "sfenutils.h"

#include <QDockWidget>
#include <QFileInfo>
#include <QMessageBox>

void JosekiWindow::clearTable()
{
    m_tableWidget->setRowCount(0);
    m_currentMoves.clear();
    updateStatusDisplay();
}

void JosekiWindow::updateStatusDisplay()
{
    bool hasData = !m_repository->isEmpty();
    if (m_fileStatusLabel) {
        if (hasData) {
            m_fileStatusLabel->setText(tr("✓読込済"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: green; font-weight: bold;"));
        } else {
            m_fileStatusLabel->setText(QString());
            m_fileStatusLabel->setStyleSheet(QString());
        }
    }
    if (m_stopButton)
        m_stopButton->setVisible(hasData);
    if (m_statusLabel) {
        QStringList statusParts;
        if (!m_currentFilePath.isEmpty())
            statusParts << tr("ファイル: %1").arg(QFileInfo(m_currentFilePath).fileName());
        else
            statusParts << tr("ファイル: 未選択");
        statusParts << tr("局面数: %1").arg(m_repository->positionCount());
        if (!m_displayEnabled)
            statusParts << tr("【停止中】");
        else if (!m_currentMoves.isEmpty())
            statusParts << tr("定跡: %1件").arg(m_currentMoves.size());
        m_statusLabel->setText(statusParts.join(QStringLiteral("  |  ")));
    }
    if (m_emptyGuideLabel && m_tableWidget) {
        bool showGuide = m_displayEnabled && m_currentMoves.isEmpty() && !m_currentSfen.isEmpty();
        m_emptyGuideLabel->setVisible(showGuide);
        if (showGuide) {
            m_emptyGuideLabel->setGeometry(m_tableWidget->geometry());
            m_emptyGuideLabel->raise();
        }
    }
}

void JosekiWindow::updatePositionSummary()
{
    if (!m_positionSummaryLabel) return;
    if (m_currentSfen.isEmpty()) {
        m_positionSummaryLabel->setText(tr("(未設定)"));
        m_currentSfenLabel->setText(QString());
        return;
    }
    const QStringList parts = m_currentSfen.split(QChar(' '));
    int plyNumber = 1;
    QString turn = tr("先手");
    if (parts.size() >= 2)
        turn = (parts[1] == QStringLiteral("b")) ? tr("先手") : tr("後手");
    if (parts.size() >= 4) {
        bool ok;
        plyNumber = parts[3].toInt(&ok);
        if (!ok) plyNumber = 1;
    }
    QString positionDesc;
    if (plyNumber == 1 && !parts.isEmpty()) {
        if (SfenUtils::isHirateBoardSfen(parts[0]))
            positionDesc = tr("初期配置");
        else
            positionDesc = tr("駒落ち");
    } else {
        positionDesc = tr("%1手目").arg(plyNumber);
    }
    m_positionSummaryLabel->setText(tr("%1 (%2番)").arg(positionDesc, turn));
    m_positionSummaryLabel->setToolTip(m_currentSfen);
    m_currentSfenLabel->setText(tr("局面SFEN: %1").arg(m_currentSfen));
}

void JosekiWindow::updateWindowTitle()
{
    QString title = tr("定跡ウィンドウ");
    if (!m_currentFilePath.isEmpty())
        title = QFileInfo(m_currentFilePath).fileName() + QStringLiteral(" - ") + title;
    if (m_modified)
        title = QStringLiteral("* ") + title;
    setWindowTitle(title);
}

void JosekiWindow::setModified(bool modified)
{
    m_modified = modified;
    m_saveButton->setEnabled(!m_currentFilePath.isEmpty() && modified);
    updateWindowTitle();
    if (m_dockWidget) {
        QString dockTitle = tr("定跡");
        if (m_modified) dockTitle += QStringLiteral(" *");
        m_dockWidget->setWindowTitle(dockTitle);
    }
    if (m_fileStatusLabel) {
        if (m_modified) {
            m_fileStatusLabel->setText(tr("未保存"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: #cc6600; font-weight: bold;"));
        } else if (!m_repository->isEmpty()) {
            m_fileStatusLabel->setText(tr("✓読込済"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: green; font-weight: bold;"));
        } else {
            m_fileStatusLabel->setText(QString());
            m_fileStatusLabel->setStyleSheet(QString());
        }
    }
}

bool JosekiWindow::confirmDiscardChanges()
{
    if (isIoBusy()) {
        QMessageBox::information(this, tr("処理中"), tr("ファイルの読み込み/保存が完了するまでお待ちください。"));
        return false;
    }
    if (!m_modified) return true;
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("確認"));
    msgBox.setText(tr("定跡データに未保存の変更があります。\n変更を破棄しますか？"));
    msgBox.setIcon(QMessageBox::Question);
    QPushButton *saveBtn = msgBox.addButton(tr("保存"), QMessageBox::AcceptRole);
    QPushButton *discardBtn = msgBox.addButton(tr("破棄"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("キャンセル"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(saveBtn);
    msgBox.exec();
    if (msgBox.clickedButton() == saveBtn) {
        if (m_currentFilePath.isEmpty()) {
            onSaveAsButtonClicked();
            return !m_modified;
        }
        // 同期保存（ダイアログ応答のフロー上、完了を待つ必要がある）
        if (saveToFile(m_currentFilePath)) setModified(false);
        return !m_modified;
    }
    if (msgBox.clickedButton() == discardBtn) return true;
    return false;
}
