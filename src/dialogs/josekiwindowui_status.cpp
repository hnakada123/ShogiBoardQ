/// @file josekiwindowui_status.cpp
/// @brief JosekiWindow の状態表示・更新メソッド群

#include "josekiwindow.h"

#include "josekirepository.h"
#include "kifupresentation.h"
#include "notationutils.h"

#include <QDockWidget>
#include <QFileInfo>
#include <QLocale>
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
    m_addMoveButton->setEnabled(!isIoBusy() && !m_currentSfen.isEmpty());
    m_filePathLabel->setToolTip(m_currentFilePath);
    m_noticeLabel->setVisible(m_modified);
    if (m_fileStatusLabel) {
        if (m_modified) {
            m_fileStatusLabel->setText(tr("未保存"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: #cc6600; font-weight: bold;"));
        } else if (hasData) {
            m_fileStatusLabel->setText(tr("✓読込済"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: green; font-weight: bold;"));
        } else {
            m_fileStatusLabel->setText(QString());
            m_fileStatusLabel->setStyleSheet(QString());
        }
    }
    if (m_stopButton)
        m_stopButton->setVisible(hasData || !m_displayEnabled);
    if (m_statusLabel) {
        QStringList statusParts;
        if (!m_currentFilePath.isEmpty())
            statusParts << tr("ファイル: %1").arg(QFileInfo(m_currentFilePath).fileName());
        else
            statusParts << tr("ファイル: 未選択");
        statusParts << tr("局面数: %1").arg(QLocale().toString(m_repository->positionCount()));
        if (!m_displayEnabled)
            statusParts << tr("【停止中】");
        else
            statusParts << tr("定跡: %1件").arg(m_currentMoves.size());
        m_statusLabel->setText(statusParts.join(QStringLiteral("  |  ")));
    }
    if (m_emptyGuideLabel && m_tableWidget) {
        bool showGuide = m_currentMoves.isEmpty();
        if (!m_displayEnabled)
            m_emptyGuideLabel->setText(tr("定跡表示を停止しています。「再開」で表示を再開します。"));
        else if (m_currentSfen.isEmpty())
            m_emptyGuideLabel->setText(tr("将棋盤で局面を表示すると、その局面の定跡を確認できます。"));
        else if (m_currentFilePath.isEmpty() && !hasData)
            m_emptyGuideLabel->setText(tr("「開く」で定跡ファイルを読み込むと、表示中の局面の定跡手が表示されます。\n"
                                        "「新規」で新しい定跡ファイルを作ることもできます。"));
        else
            m_emptyGuideLabel->setText(tr("この局面には定跡が登録されていません。\n"
                                        "「＋追加」で指し手を登録するか、「マージ」から棋譜を取り込めます。"));
        m_emptyGuideLabel->setVisible(showGuide);
        if (showGuide) {
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
    // 平手・駒落ちの開始局面は手合の名前、それ以外は手数で示す（任意の局面を「駒落ち」と呼ばない）
    const QString handicap = (plyNumber == 1) ? NotationUtils::handicapLabelForSfen(m_currentSfen) : QString();
    QString positionDesc;
    if (handicap == QStringLiteral("平手"))
        positionDesc = tr("初期配置");
    else if (!handicap.isEmpty())
        positionDesc = KifuPresentation::infoValue(QStringLiteral("手合割"), handicap);
    else
        positionDesc = tr("%1手目").arg(plyNumber);
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
    m_saveButton->setEnabled(!isIoBusy() && modified);
    updateWindowTitle();
    if (m_dockWidget) {
        QString dockTitle = tr("定跡");
        if (m_modified) dockTitle += QStringLiteral(" *");
        m_dockWidget->setWindowTitle(dockTitle);
    }
    updateStatusDisplay();
}

bool JosekiWindow::confirmClose()
{
    if (!confirmDiscardChanges()) return false;
    saveSettings();
    return true;
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
    msgBox.setText(tr("定跡データに未保存の変更があります。\n保存しますか？"));
    msgBox.setIcon(QMessageBox::Question);
    QPushButton *saveBtn = msgBox.addButton(tr("保存"), QMessageBox::AcceptRole);
    QPushButton *discardBtn = msgBox.addButton(tr("破棄"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("キャンセル"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(saveBtn);
    msgBox.exec();
    if (msgBox.clickedButton() == saveBtn) {
        const QString filePath = m_currentFilePath.isEmpty() ? selectSaveFilePath() : m_currentFilePath;
        if (filePath.isEmpty()) return false;
        // 同期保存（ダイアログ応答のフロー上、完了を待つ必要がある）
        if (saveToFile(filePath)) applySavedFilePath(filePath);
        return !m_modified;
    }
    if (msgBox.clickedButton() == discardBtn) return true;
    return false;
}
