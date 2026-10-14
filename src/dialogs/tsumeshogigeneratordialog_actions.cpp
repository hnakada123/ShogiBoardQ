/// @file tsumeshogigeneratordialog_actions.cpp
/// @brief 詰将棋局面生成ダイアログの結果操作・表示更新処理

#include "dialogutils.h"
#include "tsumeshogigeneratordialog.h"

#include "changeenginesettingsdialog.h"
#include "pvboarddialog.h"
#include "tsumecollectionwriter.h"
#include "tsumeshogiexportheaderbuilder.h"
#include "tsumeshogikanjibuilder.h"
#include "tsumeshogisettings.h"
#include "tablestyles.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QHeaderView>
#include <QLayout>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>
#include <QWidget>
#include <algorithm>
#include <utility>

void TsumeshogiGeneratorDialog::onPositionFound(const QString& sfen, const QStringList& pv)
{
    const int row = m_resultsUi.table->rowCount();
    const auto* scroll = m_resultsUi.table->verticalScrollBar();
    const bool followResults = scroll->value() == scroll->maximum();
    m_resultsUi.table->insertRow(row);

    auto* itemNum = new QTableWidgetItem(QString::number(row + 1));
    itemNum->setTextAlignment(Qt::AlignCenter);
    m_resultsUi.table->setItem(row, 0, itemNum);

    auto* itemSfen = new QTableWidgetItem(sfen);
    itemSfen->setData(Qt::UserRole, QVariant(pv));
    itemSfen->setToolTip(sfen);
    m_resultsUi.table->setItem(row, 1, itemSfen);

    auto* boardItem = new QTableWidgetItem(tr("表示"));
    boardItem->setToolTip(tr("盤面と詰み手順を表示します。行のダブルクリック、または Enter キーでも開けます。"));
    m_resultsUi.table->setItem(row, 2, boardItem);

    auto* itemMoves = new QTableWidgetItem(QString::number(pv.size()));
    itemMoves->setTextAlignment(Qt::AlignCenter);
    m_resultsUi.table->setItem(row, 3, itemMoves);

    if (followResults) m_resultsUi.table->scrollToBottom();
    updateResultActions();
    updateProgressBar();
}

void TsumeshogiGeneratorDialog::onProgressUpdated(int tried, int found, qint64 elapsedMs)
{
    m_progressUi.counts->setText(tr("探索済み: %1 局面 / 採択: %2 局面").arg(tried).arg(found));
    m_progressUi.elapsed->setText(tr("経過時間: %1").arg(formatElapsedTime(elapsedMs)));
}

void TsumeshogiGeneratorDialog::onGeneratorFinished()
{
    setRunningState(false);
    if (m_runFailed) return;
    setStatusText(m_stopRequested ? tr("停止しました（%1 局面を採択）").arg(m_resultsUi.table->rowCount())
                                 : tr("生成完了（%1 局面を採択）").arg(m_resultsUi.table->rowCount()));
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
    if (m_resultsUi.table->rowCount() == 0) return;

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("SFENをファイルに保存"),
        TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory(),
        tr("テキストファイル (*.txt);;すべてのファイル (*)"));
    if (filePath.isEmpty()) return;

    TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory.set(QFileInfo(filePath).absolutePath());

    // 先頭に ShogiBoardQ のバージョン・生成日時・生成設定をコメント行（'#' 始まり）で記録する
    const QStringList header = TsumeshogiExportHeaderBuilder::build(
        QStringLiteral(APP_VERSION), m_lastRunStartedAt, m_lastRunSettings);
    QStringList lines;
    for (int row = 0; row < m_resultsUi.table->rowCount(); ++row) lines.append(exportLine(row));
    QString errorMessage;
    if (!TsumeCollectionWriter::save(filePath, header, lines, &errorMessage)) {
        QMessageBox::warning(this, tr("エラー"), tr("ファイルを保存できませんでした: %1").arg(errorMessage));
    }
}

void TsumeshogiGeneratorDialog::onCopySelected()
{
    auto selectedRows = m_resultsUi.table->selectionModel()->selectedRows(1);
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
    if (m_resultsUi.table->rowCount() == 0) return;

    QStringList lines;
    for (int row = 0; row < m_resultsUi.table->rowCount(); ++row) {
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
    m_settingsUi.targetMoves->setValue(3);
    m_settingsUi.maxAttack->setValue(4);
    m_settingsUi.maxDefend->setValue(1);
    m_settingsUi.attackRange->setValue(3);
    m_settingsUi.timeout->setValue(5);
    m_settingsUi.maxPositions->setValue(10);
    m_settingsUi.allowFinalAlternatives->setChecked(true);
}

void TsumeshogiGeneratorDialog::onResultTableClicked(const QModelIndex& index)
{
    if (!index.isValid() || index.column() != 2) return;
    onResultTableActivated(index);
}

void TsumeshogiGeneratorDialog::onResultTableActivated(const QModelIndex& index)
{
    if (!index.isValid()) return;

    const auto* sfenItem = m_resultsUi.table->item(index.row(), 1);
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
    const bool hasResults = m_resultsUi.table->rowCount() > 0;
    m_resultsUi.save->setEnabled(hasResults);
    m_resultsUi.copyAll->setEnabled(hasResults);
    m_resultsUi.copySelected->setEnabled(m_resultsUi.table->selectionModel()->hasSelection());
    m_resultsUi.emptyText->setVisible(!hasResults);
    m_resultsUi.emptyText->setText(m_running
        ? tr("条件に合う局面を探索しています。\n採択した局面から順に表示します。")
        : tr("生成した局面がここに表示されます。\n設定を確認して「開始」を押してください。"));
}

void TsumeshogiGeneratorDialog::updateProgressBar()
{
    const int limit = m_lastRunStartedAt.isValid() ? m_lastRunSettings.maxPositionsToFind
                                                : m_settingsUi.maxPositions->value();
    const int found = m_resultsUi.table->rowCount();
    m_progressUi.bar->setRange(0, limit > 0 ? limit : (m_running ? 0 : qMax(1, found)));
    m_progressUi.bar->setValue(found);
    m_progressUi.bar->setFormat(limit > 0 ? tr("%v / %m 局面") : tr("%1 局面（無制限）").arg(found));
}

void TsumeshogiGeneratorDialog::toggleHelp(bool visible)
{
    m_settingsUi.helpText->setVisible(visible);
    m_settingsUi.help->setArrowType(visible ? Qt::DownArrow : Qt::RightArrow);
    if (isVisible()) fitSettingsArea();
}

void TsumeshogiGeneratorDialog::fitSettingsArea()
{
    // 説明を開くと設定欄が高くなる。結果一覧の高さを譲ってもらい、それでも足りなければ
    // 画面に収まる範囲でダイアログを高くして、設定をスクロールせずに見られるようにする。
    // 閉じたときは設定欄を縮めて結果一覧へ高さを戻す。説明文から設定欄までの各部品が記録している
    // 希望サイズを先に破棄し、開閉後の高さで計算させる
    for (QWidget* w = m_settingsUi.helpText->parentWidget(); w && w != m_settingsUi.scroll; w = w->parentWidget()) {
        if (QLayout* l = w->layout()) l->invalidate();
        w->updateGeometry();
    }
    m_settingsUi.scroll->updateGeometry();
    layout()->activate();
    const int missing = m_settingsUi.scroll->widget()->sizeHint().height() - m_settingsUi.scroll->viewport()->height();
    if (missing <= 0) return;
    int newHeight = height() + missing;
    if (const QScreen* s = screen()) {
        const int frame = frameGeometry().height() - height();
        newHeight = qMin(newHeight, qMax(height(), s->availableGeometry().height() - frame));
    }
    resize(width(), newHeight);
}

void TsumeshogiGeneratorDialog::showEngineSettingsDialog()
{
    const int engineIndex = m_settingsUi.engine->currentIndex();
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
    const QFont font = m_fontHelper.applyTo(this);
    applyTableHeaderStyle();
    m_resultsUi.table->verticalHeader()->setDefaultSectionSize(QFontMetrics(font).height() + 10);
    DialogUtils::fitWrappedLabels(this);
}

void TsumeshogiGeneratorDialog::applyTableHeaderStyle()
{
    if (!m_resultsUi.table) return;

    m_resultsUi.table->setStyleSheet(TableStyles::thinking(m_fontHelper.fontSize()));
}

void TsumeshogiGeneratorDialog::setRunningState(bool running)
{
    m_running = running;
    m_progressUi.start->setEnabled(!running && !m_engineList.isEmpty());
    m_progressUi.stop->setEnabled(running);
    m_settingsUi.engine->setEnabled(!running);
    m_settingsUi.engineSettings->setEnabled(!running && !m_engineList.isEmpty());
    m_settingsUi.targetMoves->setEnabled(!running);
    m_settingsUi.maxAttack->setEnabled(!running);
    m_settingsUi.maxDefend->setEnabled(!running);
    m_settingsUi.attackRange->setEnabled(!running);
    m_settingsUi.timeout->setEnabled(!running);
    m_settingsUi.maxPositions->setEnabled(!running);
    m_settingsUi.allowFinalAlternatives->setEnabled(!running);
    m_settingsUi.restoreDefaults->setEnabled(!running);
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
    m_progressUi.verification->setText(tr("検査で除外: %1 局面（うち判定不能: %2 局面）")
                                    .arg(rejected).arg(inconclusive));
}

void TsumeshogiGeneratorDialog::setStatusText(const QString& status)
{
    m_progressUi.status->setText(tr("状態: %1").arg(status));
}

QString TsumeshogiGeneratorDialog::exportLine(int row) const
{
    const auto* item = m_resultsUi.table->item(row, 1);
    if (!item) return QString();
    return TsumeCollectionWriter::line(item->text(), item->data(Qt::UserRole).toStringList(),
                                       m_resultsUi.includePv->isChecked());
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
