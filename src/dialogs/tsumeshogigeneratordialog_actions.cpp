/// @file tsumeshogigeneratordialog_actions.cpp
/// @brief 詰将棋局面生成ダイアログの結果操作・表示更新処理

#include "tsumeshogigeneratordialog.h"

#include "changeenginesettingsdialog.h"
#include "pvboarddialog.h"
#include "tsumeshogikanjibuilder.h"
#include "tsumeshogisettings.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextStream>
#include <QWidget>
#include <utility>

void TsumeshogiGeneratorDialog::onPositionFound(const QString& sfen, const QStringList& pv)
{
    const int row = m_tableResults->rowCount();
    m_tableResults->insertRow(row);

    auto* itemNum = new QTableWidgetItem(QString::number(row + 1));
    itemNum->setTextAlignment(Qt::AlignCenter);
    m_tableResults->setItem(row, 0, itemNum);

    auto* itemSfen = new QTableWidgetItem(sfen);
    itemSfen->setData(Qt::UserRole, QVariant(pv));
    m_tableResults->setItem(row, 1, itemSfen);

    m_tableResults->setItem(row, 2, new QTableWidgetItem());

    auto* itemMoves = new QTableWidgetItem(QString::number(pv.size()));
    itemMoves->setTextAlignment(Qt::AlignCenter);
    m_tableResults->setItem(row, 3, itemMoves);

    m_tableResults->scrollToBottom();
}

void TsumeshogiGeneratorDialog::onProgressUpdated(int tried, int found, qint64 elapsedMs)
{
    m_labelProgress->setText(tr("探索済み: %1 局面 / 発見: %2 局面").arg(tried).arg(found));
    m_labelElapsed->setText(tr("経過時間: %1").arg(formatElapsedTime(elapsedMs)));
}

void TsumeshogiGeneratorDialog::onGeneratorFinished()
{
    setRunningState(false);
}

void TsumeshogiGeneratorDialog::onGeneratorError(const QString& message)
{
    QMessageBox::warning(this, tr("エラー"), message);
    setRunningState(false);
}

void TsumeshogiGeneratorDialog::onSaveToFile()
{
    if (m_tableResults->rowCount() == 0) return;

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("SFENをファイルに保存"),
        TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory(),
        tr("テキストファイル (*.txt);;すべてのファイル (*)"));
    if (filePath.isEmpty()) return;

    TsumeshogiSettings::setTsumeshogiGeneratorLastSaveDirectory(QFileInfo(filePath).absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("エラー"),
                             tr("ファイルを保存できませんでした: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    for (int row = 0; row < m_tableResults->rowCount(); ++row) {
        const auto* item = m_tableResults->item(row, 1);
        if (item) {
            out << item->text() << '\n';
        }
    }
}

void TsumeshogiGeneratorDialog::onCopySelected()
{
    const auto selectedRows = m_tableResults->selectionModel()->selectedRows(1);
    if (selectedRows.isEmpty()) return;

    QStringList sfenList;
    for (const auto& index : std::as_const(selectedRows)) {
        sfenList.append(index.data().toString());
    }
    QApplication::clipboard()->setText(sfenList.join('\n'));
}

void TsumeshogiGeneratorDialog::onCopyAll()
{
    if (m_tableResults->rowCount() == 0) return;

    QStringList sfenList;
    for (int row = 0; row < m_tableResults->rowCount(); ++row) {
        const auto* item = m_tableResults->item(row, 1);
        if (item) {
            sfenList.append(item->text());
        }
    }
    QApplication::clipboard()->setText(sfenList.join('\n'));
}

void TsumeshogiGeneratorDialog::onFontIncrease()
{
    if (m_fontHelper.increase()) applyFontSize();
}

void TsumeshogiGeneratorDialog::onFontDecrease()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

void TsumeshogiGeneratorDialog::onRestoreDefaults()
{
    m_spinTargetMoves->setValue(3);
    m_spinMaxAttack->setValue(4);
    m_spinMaxDefend->setValue(1);
    m_spinAttackRange->setValue(3);
    m_spinTimeout->setValue(5);
    m_spinMaxPositions->setValue(10);
}

void TsumeshogiGeneratorDialog::onResultTableClicked(const QModelIndex& index)
{
    if (!index.isValid() || index.column() != 2) return;

    const auto* sfenItem = m_tableResults->item(index.row(), 1);
    if (!sfenItem) return;

    const QString sfen = sfenItem->text();
    const QStringList pv = sfenItem->data(Qt::UserRole).toStringList();
    if (pv.isEmpty()) return;

    auto* dlg = new PvBoardDialog(sfen, pv, this);
    dlg->setPlayerNames(tr("攻方"), tr("玉方"));
    dlg->setKanjiPv(TsumeshogiKanjiBuilder::buildKanjiPv(sfen, pv));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void TsumeshogiGeneratorDialog::showEngineSettingsDialog()
{
    const int engineIndex = m_comboEngine->currentIndex();
    if (engineIndex < 0 || engineIndex >= m_engineList.size()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    ChangeEngineSettingsDialog dialog(this);
    dialog.setEngineNumber(engineIndex);
    dialog.setEngineName(m_engineList.at(engineIndex).name);
    dialog.setEngineAuthor(m_engineList.at(engineIndex).author);
    dialog.setupEngineOptionsDialog();
    dialog.exec();
}

void TsumeshogiGeneratorDialog::applyFontSize()
{
    const int size = m_fontHelper.fontSize();
    QFont f = font();
    f.setPointSize(size);
    setFont(f);

    const QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : std::as_const(widgets)) {
        if (widget) {
            widget->setFont(f);
        }
    }

    const QList<QComboBox*> comboBoxes = findChildren<QComboBox*>();
    for (QComboBox* comboBox : std::as_const(comboBoxes)) {
        if (comboBox && comboBox->view()) {
            comboBox->view()->setFont(f);
        }
    }

    applyTableHeaderStyle();
}

void TsumeshogiGeneratorDialog::applyTableHeaderStyle()
{
    if (!m_tableResults) return;

    m_tableResults->setStyleSheet(
        QStringLiteral(
            "QHeaderView::section {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #40acff, stop:1 #209cee);"
            "  color: white;"
            "  font-weight: normal;"
            "  font-size: %1pt;"
            "  padding: 2px 6px;"
            "  border: none;"
            "  border-bottom: 1px solid #209cee;"
            "}").arg(m_fontHelper.fontSize()));
}

void TsumeshogiGeneratorDialog::setRunningState(bool running)
{
    m_btnStart->setEnabled(!running);
    m_btnStop->setEnabled(running);
    m_comboEngine->setEnabled(!running);
    m_btnEngineSetting->setEnabled(!running);
    m_spinTargetMoves->setEnabled(!running);
    m_spinMaxAttack->setEnabled(!running);
    m_spinMaxDefend->setEnabled(!running);
    m_spinAttackRange->setEnabled(!running);
    m_spinTimeout->setEnabled(!running);
    m_spinMaxPositions->setEnabled(!running);
}

QString TsumeshogiGeneratorDialog::formatElapsedTime(qint64 ms) const
{
    const int totalSec = static_cast<int>(ms / 1000);
    const int hours = totalSec / 3600;
    const int minutes = (totalSec % 3600) / 60;
    const int seconds = totalSec % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}
