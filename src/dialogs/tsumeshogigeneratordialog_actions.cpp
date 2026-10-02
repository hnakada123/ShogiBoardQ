/// @file tsumeshogigeneratordialog_actions.cpp
/// @brief 詰将棋局面生成ダイアログの結果操作・表示更新処理

#include "tsumeshogigeneratordialog.h"

#include "changeenginesettingsdialog.h"
#include "pvboarddialog.h"
#include "tsumeshogiexportheaderbuilder.h"
#include "tsumeshogikanjibuilder.h"
#include "tsumeshogisettings.h"
#include "tablestyles.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QSaveFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextStream>
#include <QToolButton>
#include <QWidget>
#include <algorithm>
#include <utility>

void TsumeshogiGeneratorDialog::onPositionFound(const QString& sfen, const QStringList& pv)
{
    const int row = m_tableResults->rowCount();
    const auto* scroll = m_tableResults->verticalScrollBar();
    const bool followResults = scroll->value() == scroll->maximum();
    m_tableResults->insertRow(row);

    auto* itemNum = new QTableWidgetItem(QString::number(row + 1));
    itemNum->setTextAlignment(Qt::AlignCenter);
    m_tableResults->setItem(row, 0, itemNum);

    auto* itemSfen = new QTableWidgetItem(sfen);
    itemSfen->setData(Qt::UserRole, QVariant(pv));
    itemSfen->setToolTip(sfen);
    m_tableResults->setItem(row, 1, itemSfen);

    auto* boardItem = new QTableWidgetItem(tr("表示"));
    boardItem->setToolTip(tr("盤面と詰み手順を表示します。行のダブルクリック、または Enter キーでも開けます。"));
    m_tableResults->setItem(row, 2, boardItem);

    auto* itemMoves = new QTableWidgetItem(QString::number(pv.size()));
    itemMoves->setTextAlignment(Qt::AlignCenter);
    m_tableResults->setItem(row, 3, itemMoves);

    if (followResults) m_tableResults->scrollToBottom();
    updateResultActions();
    updateProgressBar();
}

void TsumeshogiGeneratorDialog::onProgressUpdated(int tried, int found, qint64 elapsedMs)
{
    m_labelProgress->setText(tr("探索済み: %1 局面 / 採択: %2 局面").arg(tried).arg(found));
    m_labelElapsed->setText(tr("経過時間: %1").arg(formatElapsedTime(elapsedMs)));
}

void TsumeshogiGeneratorDialog::onGeneratorFinished()
{
    setRunningState(false);
    if (m_runFailed) return;
    setStatusText(m_stopRequested ? tr("停止しました（%1 局面を採択）").arg(m_tableResults->rowCount())
                                 : tr("生成完了（%1 局面を採択）").arg(m_tableResults->rowCount()));
}

void TsumeshogiGeneratorDialog::onGeneratorError(const QString& message)
{
    m_runFailed = true;
    setRunningState(false);
    setStatusText(tr("エラー: %1").arg(message));
    QMessageBox::warning(this, tr("エラー"), message);
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

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("エラー"),
                             tr("ファイルを保存できませんでした: %1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);

    // 先頭に ShogiBoardQ のバージョン・生成日時・生成設定をコメント行（'#' 始まり）で記録する。
    // TsumeCollection::parse と SfenCollectionDialog の読み込みはコメント行を読み飛ばす
    const QStringList header = TsumeshogiExportHeaderBuilder::build(
        QStringLiteral(APP_VERSION), m_lastRunStartedAt, m_lastRunSettings);
    for (const QString& line : header) {
        out << line << '\n';
    }

    for (int row = 0; row < m_tableResults->rowCount(); ++row) {
        const QString line = exportLine(row);
        if (!line.isEmpty()) {
            out << line << '\n';
        }
    }
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        QMessageBox::warning(this, tr("エラー"),
                             tr("ファイルを保存できませんでした: %1").arg(file.errorString()));
    }
}

void TsumeshogiGeneratorDialog::onCopySelected()
{
    auto selectedRows = m_tableResults->selectionModel()->selectedRows(1);
    if (selectedRows.isEmpty()) return;
    std::sort(selectedRows.begin(), selectedRows.end(),
              [](const QModelIndex& left, const QModelIndex& right) { return left.row() < right.row(); });

    QStringList lines;
    for (const auto& index : std::as_const(selectedRows)) {
        lines.append(exportLine(index.row()));
    }
    QApplication::clipboard()->setText(lines.join('\n'));
}

void TsumeshogiGeneratorDialog::onCopyAll()
{
    if (m_tableResults->rowCount() == 0) return;

    QStringList lines;
    for (int row = 0; row < m_tableResults->rowCount(); ++row) {
        const QString line = exportLine(row);
        if (!line.isEmpty()) {
            lines.append(line);
        }
    }
    QApplication::clipboard()->setText(lines.join('\n'));
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
    if (m_running) return;
    m_spinTargetMoves->setValue(3);
    m_spinMaxAttack->setValue(4);
    m_spinMaxDefend->setValue(1);
    m_spinAttackRange->setValue(3);
    m_spinTimeout->setValue(5);
    m_spinMaxPositions->setValue(10);
    m_checkAllowFinalAlternatives->setChecked(true);
}

void TsumeshogiGeneratorDialog::onResultTableClicked(const QModelIndex& index)
{
    if (!index.isValid() || index.column() != 2) return;
    onResultTableActivated(index);
}

void TsumeshogiGeneratorDialog::onResultTableActivated(const QModelIndex& index)
{
    if (!index.isValid()) return;

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

void TsumeshogiGeneratorDialog::updateResultActions()
{
    const bool hasResults = m_tableResults->rowCount() > 0;
    m_btnSaveToFile->setEnabled(hasResults);
    m_btnCopyAll->setEnabled(hasResults);
    m_btnCopySelected->setEnabled(m_tableResults->selectionModel()->hasSelection());
    m_labelEmptyResults->setVisible(!hasResults);
    m_labelEmptyResults->setText(m_running
        ? tr("条件に合う局面を探索しています。\n採択した局面から順に表示します。")
        : tr("生成した局面がここに表示されます。\n設定を確認して「開始」を押してください。"));
}

void TsumeshogiGeneratorDialog::updateProgressBar()
{
    const int limit = m_lastRunStartedAt.isValid() ? m_lastRunSettings.maxPositionsToFind
                                                : m_spinMaxPositions->value();
    const int found = m_tableResults->rowCount();
    m_progressBar->setRange(0, limit > 0 ? limit : (m_running ? 0 : qMax(1, found)));
    m_progressBar->setValue(found);
    m_progressBar->setFormat(limit > 0 ? tr("%v / %m 局面") : tr("%1 局面（無制限）").arg(found));
}

void TsumeshogiGeneratorDialog::toggleHelp(bool visible)
{
    m_labelHelp->setVisible(visible);
    m_btnHelp->setArrowType(visible ? Qt::DownArrow : Qt::RightArrow);
}

void TsumeshogiGeneratorDialog::showEngineSettingsDialog()
{
    const int engineIndex = m_comboEngine->currentIndex();
    if (engineIndex < 0 || engineIndex >= m_engineList.size()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    ChangeEngineSettingsDialog dialog(this);
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
    m_tableResults->verticalHeader()->setDefaultSectionSize(QFontMetrics(f).height() + 10);
    m_btnFontDecrease->setEnabled(size > 8);
    m_btnFontIncrease->setEnabled(size < 24);
}

void TsumeshogiGeneratorDialog::applyTableHeaderStyle()
{
    if (!m_tableResults) return;

    m_tableResults->setStyleSheet(TableStyles::thinking(m_fontHelper.fontSize()));
}

void TsumeshogiGeneratorDialog::setRunningState(bool running)
{
    m_running = running;
    m_btnStart->setEnabled(!running && !m_engineList.isEmpty());
    m_btnStop->setEnabled(running);
    m_comboEngine->setEnabled(!running);
    m_btnEngineSetting->setEnabled(!running && !m_engineList.isEmpty());
    m_spinTargetMoves->setEnabled(!running);
    m_spinMaxAttack->setEnabled(!running);
    m_spinMaxDefend->setEnabled(!running);
    m_spinAttackRange->setEnabled(!running);
    m_spinTimeout->setEnabled(!running);
    m_spinMaxPositions->setEnabled(!running);
    m_checkAllowFinalAlternatives->setEnabled(!running);
    m_btnRestoreDefaults->setEnabled(!running);
    updateResultActions();
    updateProgressBar();
}

void TsumeshogiGeneratorDialog::onSearchPhaseStarted()
{
    setStatusText(tr("探索中"));
}

void TsumeshogiGeneratorDialog::onTrimmingProgress(int candidate, int total)
{
    setStatusText(tr("トリミング中（候補 %1/%2）").arg(candidate).arg(total));
}

void TsumeshogiGeneratorDialog::onVerificationProgress(int queries)
{
    setStatusText(tr("余詰検査中（問い合わせ %1 回）").arg(queries));
}

void TsumeshogiGeneratorDialog::onVerificationStatsUpdated(int rejected, int inconclusive)
{
    m_labelVerification->setText(tr("検査で除外: %1 局面（うち判定不能: %2 局面）")
                                    .arg(rejected).arg(inconclusive));
}

void TsumeshogiGeneratorDialog::setStatusText(const QString& status)
{
    m_labelStatus->setText(tr("状態: %1").arg(status));
}

QString TsumeshogiGeneratorDialog::exportLine(int row) const
{
    const auto* item = m_tableResults->item(row, 1);
    if (!item) return QString();

    QString line = item->text();
    if (m_checkIncludePv->isChecked()) {
        // USI の position 構文に合わせて "moves" に続けて手順を付加する
        const QStringList pv = item->data(Qt::UserRole).toStringList();
        if (!pv.isEmpty()) {
            line += QStringLiteral(" moves ") + pv.join(QLatin1Char(' '));
        }
    }
    return line;
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
