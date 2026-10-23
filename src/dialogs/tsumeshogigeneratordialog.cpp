/// @file tsumeshogigeneratordialog.cpp
/// @brief 詰将棋局面生成ダイアログの設定・生成制御

#include "tsumeshogigeneratordialog.h"
#include "dialogfontscale.h"
#include "fontsizehelper.h"
#include "tsumeshogisettings.h"
#include "dialogutils.h"
#include "engineselector.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include "changeenginesettingsdialog.h"
#include "pvboarddialog.h"
#include "pvtext.h"
#include "tsumecollectionwriter.h"
#include "tsumeshogiexportheaderbuilder.h"
#include "tablestyles.h"
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLayout>
#include <QProgressBar>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QWidget>
#include <algorithm>
#include <utility>
#include "buttonstyles.h"
#include "countspinbox.h"
#include "tablebuttondelegate.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QShortcut>
#include <QVBoxLayout>

TsumeshogiGeneratorDialog::TsumeshogiGeneratorDialog(QWidget* parent)
    : QDialog(parent)
    , m_generator(new TsumeshogiGenerator(this))
{
    setWindowTitle(tr("詰将棋局面生成"));
    setupUi();
    DialogFontScale::Options fontOptions;
    fontOptions.decrease = m_footerUi.fontDecrease;
    fontOptions.increase = m_footerUi.fontIncrease;
    fontOptions.size = &TsumeshogiSettings::tsumeshogiGeneratorDialogSize;
    auto* fontScale = DialogFontScale::install(this, TsumeshogiSettings::tsumeshogiGeneratorFontSize, fontOptions);
    connect(fontScale->fontSize(), &FontSizeHelper::fontSizeChanged, this, &TsumeshogiGeneratorDialog::fitToFont);
    fitToFont();
    // エンジンの一覧と前回選んだエンジン
    EngineSelector::Options engineOptions;
    engineOptions.keys = EngineListSettings::tsumeshogiGeneratorSelection();
    m_engineSelector = new EngineSelector({m_settingsUi.engine, m_settingsUi.engineSettings, nullptr}, engineOptions, this);
    connect(m_engineSelector, &EngineSelector::settingsRequested,
            this, &TsumeshogiGeneratorDialog::showEngineSettingsDialog);
    bindSettings();
    loadSettings();

    // 初期状態
    setRunState(RunState::NotStarted);
    setStatusText(!m_engineSelector->hasEngines()
        ? tr("設定メニューで詰将棋エンジンを登録してください。") : tr("待機中"));
}

TsumeshogiGeneratorDialog::~TsumeshogiGeneratorDialog()
{
    // ウィジェット破棄前に停止する。トリミング未完了の局面は出力しない。Idle なら何もしない
    m_generator->stop();
    saveSettings();
}

void TsumeshogiGeneratorDialog::done(int result)
{
    onStopClicked();
    saveSettings();
    QDialog::done(result);
}

void TsumeshogiGeneratorDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    // 説明を開いたまま閉じた場合も、次に開いたとき設定欄を隠さない（配置が決まってから調整する）
    if (m_settingsUi.help->isChecked()) QTimer::singleShot(0, this, &TsumeshogiGeneratorDialog::fitSettingsArea);
}

void TsumeshogiGeneratorDialog::connectDialogSignals()
{
    connect(m_progressUi.start, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStartClicked);
    connect(m_progressUi.stop, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStopClicked);
    connect(m_resultsUi.save, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onSaveToFile);
    connect(m_resultsUi.copySelected, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopySelected);
    connect(m_resultsUi.copyAll, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopyAll);
    connect(m_footerUi.close, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::close);
    connect(m_settingsUi.restoreDefaults, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onRestoreDefaults);
    connect(m_resultsUi.table, &QTableWidget::clicked, this, &TsumeshogiGeneratorDialog::onResultTableClicked);
    connect(m_resultsUi.table, &QTableWidget::activated, this, &TsumeshogiGeneratorDialog::onResultTableActivated);
    connect(m_resultsUi.table, &QTableWidget::itemSelectionChanged, this, &TsumeshogiGeneratorDialog::updateResultActions);
    connect(m_settingsUi.maxPositions, &QSpinBox::valueChanged, this, &TsumeshogiGeneratorDialog::updateProgressBar);
    connect(m_settingsUi.help, &QToolButton::toggled, this, &TsumeshogiGeneratorDialog::toggleHelp);

    connect(m_generator, &TsumeshogiGenerator::positionFound,
            this, &TsumeshogiGeneratorDialog::onPositionFound);
    connect(m_generator, &TsumeshogiGenerator::progressUpdated,
            this, &TsumeshogiGeneratorDialog::onProgressUpdated);
    connect(m_generator, &TsumeshogiGenerator::finished,
            this, &TsumeshogiGeneratorDialog::onGeneratorFinished);
    connect(m_generator, &TsumeshogiGenerator::errorOccurred,
            this, &TsumeshogiGeneratorDialog::onGeneratorError);
    connect(m_generator, &TsumeshogiGenerator::searchPhaseStarted,
            this, &TsumeshogiGeneratorDialog::onSearchPhaseStarted);
    connect(m_generator, &TsumeshogiGenerator::trimmingProgress,
            this, &TsumeshogiGeneratorDialog::onTrimmingProgress);
    connect(m_generator, &TsumeshogiGenerator::verificationProgress,
            this, &TsumeshogiGeneratorDialog::onVerificationProgress);
    connect(m_generator, &TsumeshogiGenerator::verificationStatsUpdated,
            this, &TsumeshogiGeneratorDialog::onVerificationStatsUpdated);
}

void TsumeshogiGeneratorDialog::bindSettings()
{
    // 生成の条件（「既定値に戻す」の対象）と、表示の設定
    m_conditionsForm.add(m_settingsUi.targetMoves, TsumeshogiSettings::tsumeshogiGeneratorTargetMoves);
    m_conditionsForm.add(m_settingsUi.maxAttack, TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces);
    m_conditionsForm.add(m_settingsUi.maxDefend, TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces);
    m_conditionsForm.add(m_settingsUi.attackRange, TsumeshogiSettings::tsumeshogiGeneratorAttackRange);
    m_conditionsForm.add(m_settingsUi.timeout, TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec);
    m_conditionsForm.add(m_settingsUi.maxPositions, TsumeshogiSettings::tsumeshogiGeneratorMaxPositions);
    m_conditionsForm.add(m_settingsUi.allowFinalAlternatives,
                         TsumeshogiSettings::tsumeshogiGeneratorAllowFinalMoveAlternatives);
    m_displayForm.add(m_resultsUi.includePv, TsumeshogiSettings::tsumeshogiGeneratorIncludePv);
    m_displayForm.add(m_settingsUi.help, TsumeshogiSettings::tsumeshogiGeneratorHelpExpanded);
}

void TsumeshogiGeneratorDialog::loadSettings()
{
    m_conditionsForm.load();
    m_displayForm.load();
    // 旧バージョンで偶数が保存されていた場合に備えて奇数に補正する
    const int targetMoves = m_settingsUi.targetMoves->value();
    if (targetMoves % 2 == 0) m_settingsUi.targetMoves->setValue(qMax(1, targetMoves - 1));
}

void TsumeshogiGeneratorDialog::saveSettings()
{
    m_engineSelector->saveSelection();
    m_conditionsForm.save();
    m_displayForm.save();
}

void TsumeshogiGeneratorDialog::onStartClicked()
{
    if (isRunning()) return;
    const std::optional<Engine> engine = m_engineSelector->selected();
    if (!engine) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    // 設定を構築
    TsumeshogiGenerator::Settings settings;
    settings.enginePath = engine->path;
    settings.engineName = engine->name;
    settings.targetMoves = m_settingsUi.targetMoves->value();
    settings.timeoutMs = m_settingsUi.timeout->value() * 1000;
    settings.maxPositionsToFind = m_settingsUi.maxPositions->value();
    settings.allowFinalMoveAlternatives = m_settingsUi.allowFinalAlternatives->isChecked();
    settings.posGenSettings.maxAttackPieces = m_settingsUi.maxAttack->value();
    settings.posGenSettings.maxDefendPieces = m_settingsUi.maxDefend->value();
    settings.posGenSettings.attackRange = m_settingsUi.attackRange->value();

    // 結果をクリア
    m_results.clear();
    m_resultsUi.table->setRowCount(0);

    // ファイル保存時のコメントヘッダ用に、この実行の設定と開始日時を控える
    m_lastRunSettings = settings;
    m_lastRunStartedAt = QDateTime::currentDateTime();

    setRunState(RunState::Running);
    onProgressUpdated(0, 0, 0);
    onVerificationStatsUpdated(0, 0);
    setStatusText(tr("エンジンを起動中"));
    m_generator->start(settings);
}

void TsumeshogiGeneratorDialog::onStopClicked()
{
    if (!isRunning()) return;
    m_state = RunState::Stopping;
    m_generator->stop();
}

// ============================================================
// 詰将棋局面生成ダイアログの結果操作・表示更新処理
// ============================================================

void TsumeshogiGeneratorDialog::onPositionFound(const QString& sfen, const QStringList& pv)
{
    const int row = static_cast<int>(m_results.size());
    m_results.append({sfen, pv});
    const auto* scroll = m_resultsUi.table->verticalScrollBar();
    const bool followResults = scroll->value() == scroll->maximum();
    m_resultsUi.table->insertRow(row);

    auto* itemNum = new QTableWidgetItem(QString::number(row + 1));
    itemNum->setTextAlignment(Qt::AlignCenter);
    m_resultsUi.table->setItem(row, 0, itemNum);

    auto* itemSfen = new QTableWidgetItem(sfen);
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
    // エラーのときは onGeneratorError が状態と表示を決めている
    if (m_state == RunState::Failed) return;
    setRunState(m_state == RunState::Stopping ? RunState::Stopped : RunState::Finished);
    setStatusText(m_state == RunState::Stopped ? tr("停止しました（%1 局面を採択）").arg(m_results.size())
                                              : tr("生成完了（%1 局面を採択）").arg(m_results.size()));
}

void TsumeshogiGeneratorDialog::onGeneratorError(const QString& message)
{
    setRunState(RunState::Failed);
    setStatusText(tr("エラー: %1").arg(message));
    QMessageBox::warning(this, tr("エラー"), message);
}

void TsumeshogiGeneratorDialog::onSaveToFile()
{
    if (m_results.isEmpty()) return;

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
    for (const Result& result : std::as_const(m_results)) lines.append(exportLine(result));
    QString errorMessage;
    if (!TsumeCollectionWriter::save(filePath, header, lines, &errorMessage)) {
        QMessageBox::warning(this, tr("エラー"), tr("ファイルを保存できませんでした: %1").arg(errorMessage));
    }
}

void TsumeshogiGeneratorDialog::onCopySelected()
{
    QList<int> rows;
    const QModelIndexList selected = m_resultsUi.table->selectionModel()->selectedRows();
    for (const QModelIndex& index : selected) {
        if (index.row() < m_results.size()) rows.append(index.row());
    }
    if (rows.isEmpty()) return;
    std::sort(rows.begin(), rows.end());

    QStringList lines;
    for (const int row : std::as_const(rows)) lines.append(exportLine(m_results.at(row)));
    QApplication::clipboard()->setText(lines.join('\n'));
}

void TsumeshogiGeneratorDialog::onCopyAll()
{
    if (m_results.isEmpty()) return;

    QStringList lines;
    for (const Result& result : std::as_const(m_results)) lines.append(exportLine(result));
    QApplication::clipboard()->setText(lines.join('\n'));
}

void TsumeshogiGeneratorDialog::onRestoreDefaults()
{
    if (isRunning()) return;
    // 既定値は設定の宣言の表のもの
    m_conditionsForm.reset();
}

void TsumeshogiGeneratorDialog::onResultTableClicked(const QModelIndex& index)
{
    if (!index.isValid() || index.column() != 2) return;
    onResultTableActivated(index);
}

void TsumeshogiGeneratorDialog::onResultTableActivated(const QModelIndex& index)
{
    if (!index.isValid() || index.row() >= m_results.size()) return;
    const Result& result = m_results.at(index.row());
    if (result.pv.isEmpty()) return;

    auto* dlg = new PvBoardDialog(PvText::play(result.sfen, result.pv), this);
    dlg->setPlayerNames(tr("攻方"), tr("玉方"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void TsumeshogiGeneratorDialog::updateResultActions()
{
    const bool hasResults = !m_results.isEmpty();
    m_resultsUi.save->setEnabled(hasResults);
    m_resultsUi.copyAll->setEnabled(hasResults);
    m_resultsUi.copySelected->setEnabled(m_resultsUi.table->selectionModel()->hasSelection());
    m_resultsUi.emptyText->setVisible(!hasResults);
    m_resultsUi.emptyText->setText(isRunning()
        ? tr("条件に合う局面を探索しています。\n採択した局面から順に表示します。")
        : tr("生成した局面がここに表示されます。\n設定を確認して「開始」を押してください。"));
}

void TsumeshogiGeneratorDialog::updateProgressBar()
{
    const int limit = m_state != RunState::NotStarted ? m_lastRunSettings.maxPositionsToFind
                                                      : m_settingsUi.maxPositions->value();
    const int found = static_cast<int>(m_results.size());
    m_progressUi.bar->setRange(0, limit > 0 ? limit : (isRunning() ? 0 : qMax(1, found)));
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

void TsumeshogiGeneratorDialog::showEngineSettingsDialog(const EngineEntry& engine)
{
    ChangeEngineSettingsDialog dialog(engine, this);
    dialog.exec();
}

void TsumeshogiGeneratorDialog::fitToFont()
{
    // 文字の大きさは DialogFontScale が変える。表は見た目をスタイルシートで決めるので文字の大きさを渡し直す
    applyTableHeaderStyle();
    m_resultsUi.table->verticalHeader()->setDefaultSectionSize(QFontMetrics(font()).height() + 10);
    DialogUtils::fitWrappedLabels(this);
}

void TsumeshogiGeneratorDialog::applyTableHeaderStyle()
{
    if (!m_resultsUi.table) return;

    m_resultsUi.table->setStyleSheet(TableStyles::thinking(font().pointSize()));
}

void TsumeshogiGeneratorDialog::setRunState(RunState state)
{
    m_state = state;
    const bool running = isRunning();
    m_progressUi.start->setEnabled(!running && m_engineSelector->hasEngines());
    m_progressUi.stop->setEnabled(running);
    m_engineSelector->setEnabled(!running);
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

QString TsumeshogiGeneratorDialog::exportLine(const Result& result) const
{
    return TsumeCollectionWriter::line(result.sfen, result.pv, m_resultsUi.includePv->isChecked());
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

// ============================================================
// 詰将棋局面生成ダイアログのレイアウト
// ============================================================

namespace {
/// 偶数の直接入力は確定させず、前の値に戻す。
class OddSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;

protected:
    QValidator::State validate(QString& input, int& pos) const override
    {
        const QValidator::State state = QSpinBox::validate(input, pos);
        if (state != QValidator::Acceptable) return state;
        return (valueFromText(input) % 2 != 0) ? QValidator::Acceptable : QValidator::Intermediate;
    }
};

/// 内容の高さを希望サイズにする。QScrollArea::sizeHint は内容の大きさを最初に一度だけ記録するため、
/// 説明の開閉で設定欄の高さが変わらず、スクロールしないと設定が見えなくなる。
/// 内容の部品の大きさが変わったとき（文字の大きさ・エンジンの一覧）も、ダイアログのレイアウトが
/// 覚えている希望サイズを捨てさせる
class FittingScrollArea : public QScrollArea
{
public:
    using QScrollArea::QScrollArea;

    QSize sizeHint() const override
    {
        if (!widget()) return QScrollArea::sizeHint();
        const int frame = 2 * frameWidth();
        return widget()->sizeHint() + QSize(frame, frame);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        // setWidget() が内容の部品にこのフィルタを付ける
        if (watched == widget() && event->type() == QEvent::LayoutRequest) updateGeometry();
        return QScrollArea::eventFilter(watched, event);
    }
};
} // namespace

void TsumeshogiGeneratorDialog::setupUi()
{
    setObjectName(QStringLiteral("tsumeshogiGeneratorDialog"));
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // 文字を拡大した場合や小さい画面でも、設定をスクロールして操作できる。
    m_settingsUi.scroll = new FittingScrollArea(this);
    m_settingsUi.scroll->setObjectName(QStringLiteral("generatorSettingsScroll"));
    m_settingsUi.scroll->setWidgetResizable(true);
    m_settingsUi.scroll->setFrameShape(QFrame::NoFrame);
    m_settingsUi.scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    auto* form = new QWidget(m_settingsUi.scroll);
    auto* formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    buildFormSection(formLayout);
    m_settingsUi.scroll->setWidget(form);
    m_settingsUi.scroll->setMinimumHeight(180);
    mainLayout->addWidget(m_settingsUi.scroll);
    buildProgressSection(mainLayout);
    buildResultsSection(mainLayout);
    connectDialogSignals();

    // 設定入力中の Enter で不用意に生成を開始しない。
    const auto buttons = findChildren<QPushButton*>();
    for (auto* button : buttons) button->setAutoDefault(false);
}

void TsumeshogiGeneratorDialog::buildFormSection(QVBoxLayout* mainLayout)
{
    auto* engineGroup = new QGroupBox(tr("エンジン設定"), this);
    auto* engineLayout = new QVBoxLayout(engineGroup);
    auto* engineRow = new QHBoxLayout;
    auto* engineLabel = new QLabel(tr("エンジン:"), this);
    engineRow->addWidget(engineLabel);
    m_settingsUi.engine = new QComboBox(this);
    m_settingsUi.engine->setObjectName(QStringLiteral("generatorEngine"));
    m_settingsUi.engine->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_settingsUi.engine->setMinimumContentsLength(20);
    engineLabel->setBuddy(m_settingsUi.engine);
    engineRow->addWidget(m_settingsUi.engine, 1);
    m_settingsUi.engineSettings = new QPushButton(tr("設定..."), this);
    engineRow->addWidget(m_settingsUi.engineSettings);
    engineLayout->addLayout(engineRow);
    auto* engineNote = new QLabel(tr("最短手順を返す詰将棋エンジンを使用してください。"), this);
    engineNote->setWordWrap(true);
    auto* hintRow = new QHBoxLayout;
    hintRow->addWidget(engineNote, 1);
    m_settingsUi.help = new QToolButton(this);
    m_settingsUi.help->setObjectName(QStringLiteral("generatorHelp"));
    m_settingsUi.help->setText(tr("エンジン設定・採択条件について"));
    m_settingsUi.help->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_settingsUi.help->setCheckable(true);
    m_settingsUi.help->setArrowType(Qt::RightArrow);
    hintRow->addWidget(m_settingsUi.help);
    engineLayout->addLayout(hintRow);
    m_settingsUi.helpText = new QLabel(
        tr("※ エンジンは最短手順を返す設定で使用してください（KomoringHeights の場合: PostSearchLevel = MinLength）")
        + QStringLiteral("\n")
        + tr("主手順の攻手が一意な局面だけを出力します（成・不成も区別）。玉方が早く詰む変化での別の詰め方は許容し、判定不能の局面は出力しません。"), this);
    m_settingsUi.helpText->setWordWrap(true);
    m_settingsUi.helpText->hide();
    engineLayout->addWidget(m_settingsUi.helpText);
    mainLayout->addWidget(engineGroup);

    auto* settingsGroup = new QGroupBox(tr("生成設定"), this);
    auto* settingsLayout = new QVBoxLayout(settingsGroup);
    auto* columns = new QHBoxLayout;
    columns->setSpacing(24);
    auto* leftForm = new QFormLayout;
    auto* rightForm = new QFormLayout;
    leftForm->setVerticalSpacing(6);
    rightForm->setVerticalSpacing(6);
    leftForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    rightForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    columns->addLayout(leftForm, 1);
    columns->addLayout(rightForm, 1);
    settingsLayout->addLayout(columns);

    m_settingsUi.targetMoves = new OddSpinBox(this);
    m_settingsUi.targetMoves->setObjectName(QStringLiteral("generatorTargetMoves"));
    m_settingsUi.targetMoves->setRange(1, 99);
    m_settingsUi.targetMoves->setSingleStep(2);
    m_settingsUi.targetMoves->setSuffix(tr(" 手詰"));
    leftForm->addRow(tr("目標手数:"), m_settingsUi.targetMoves);
    m_settingsUi.maxAttack = new CountSpinBox(tr(" 枚", "1 のとき"), tr(" 枚"), this);
    m_settingsUi.maxAttack->setRange(1, 10);
    m_settingsUi.maxAttack->setToolTip(tr("攻方の盤上の駒と持駒の合計枚数の上限です。"));
    leftForm->addRow(tr("攻め駒上限:"), m_settingsUi.maxAttack);
    m_settingsUi.maxDefend = new CountSpinBox(tr(" 枚", "1 のとき"), tr(" 枚"), this);
    m_settingsUi.maxDefend->setRange(0, 5);
    m_settingsUi.maxDefend->setToolTip(tr("玉を除く、玉方の盤上の駒の枚数の上限です。"));
    leftForm->addRow(tr("守り駒上限:"), m_settingsUi.maxDefend);
    m_settingsUi.maxPositions = new CountSpinBox(tr(" 局面", "1 のとき"), tr(" 局面"), this);
    m_settingsUi.maxPositions->setObjectName(QStringLiteral("generatorMaxPositions"));
    m_settingsUi.maxPositions->setRange(0, 10000);
    m_settingsUi.maxPositions->setSpecialValueText(tr("無制限"));
    m_settingsUi.maxPositions->setToolTip(tr("採択した局面がこの件数に達すると終了します。0 は停止するまで生成を続けます。"));
    rightForm->addRow(tr("生成上限:"), m_settingsUi.maxPositions);
    m_settingsUi.timeout = new QSpinBox(this);
    m_settingsUi.timeout->setRange(1, 300);
    m_settingsUi.timeout->setSuffix(tr(" 秒"));
    m_settingsUi.timeout->setToolTip(tr("候補・駒除去後の詰み探索と、それぞれの余詰検査全体に使う時間です。検査時間を超えた局面は採択しません。"));
    rightForm->addRow(tr("探索時間/局面:"), m_settingsUi.timeout);
    m_settingsUi.attackRange = new CountSpinBox(tr(" マス（玉中心）", "1 のとき"), tr(" マス（玉中心）"), this);
    m_settingsUi.attackRange->setRange(1, 8);
    rightForm->addRow(tr("配置範囲:"), m_settingsUi.attackRange);
    auto* options = new QHBoxLayout;
    m_settingsUi.allowFinalAlternatives = new QCheckBox(tr("最終手の複数解を許容する"), this);
    m_settingsUi.allowFinalAlternatives->setToolTip(
        tr("オンにすると、主手順の最終手に複数の詰手があっても採択します（1手詰の初手は除く）。オフにすると最終手も一意な局面だけを出力します。"));
    options->addWidget(m_settingsUi.allowFinalAlternatives);
    options->addStretch();
    m_settingsUi.restoreDefaults = new QPushButton(tr("既定値に戻す"), this);
    m_settingsUi.restoreDefaults->setObjectName(QStringLiteral("generatorRestoreDefaults"));
    m_settingsUi.restoreDefaults->setToolTip(tr("生成設定を既定値に戻します。"));
    options->addWidget(m_settingsUi.restoreDefaults);
    settingsLayout->addLayout(options);
    mainLayout->addWidget(settingsGroup);
}

void TsumeshogiGeneratorDialog::buildProgressSection(QVBoxLayout* mainLayout)
{
    auto* controlLayout = new QHBoxLayout;
    m_progressUi.start = new QPushButton(tr("開始"), this);
    m_progressUi.start->setObjectName(QStringLiteral("generatorStart"));
    m_progressUi.start->setToolTip(tr("現在の結果をクリアして、新しく生成を開始します。"));
    m_progressUi.start->setStyleSheet(ButtonStyles::primaryAction());
    m_progressUi.stop = new QPushButton(tr("停止"), this);
    m_progressUi.stop->setObjectName(QStringLiteral("generatorStop"));
    m_progressUi.stop->setToolTip(tr("生成を停止します。採択済みの局面は保存・コピーできます。"));
    m_progressUi.stop->setStyleSheet(ButtonStyles::dangerStop());
    controlLayout->addWidget(m_progressUi.start);
    controlLayout->addWidget(m_progressUi.stop);
    m_progressUi.status = new QLabel(this);
    m_progressUi.status->setObjectName(QStringLiteral("generatorStatus"));
    m_progressUi.status->setTextFormat(Qt::PlainText);
    m_progressUi.status->setWordWrap(true);
    controlLayout->addWidget(m_progressUi.status, 1);
    mainLayout->addLayout(controlLayout);

    m_progressUi.bar = new QProgressBar(this);
    m_progressUi.bar->setObjectName(QStringLiteral("generatorProgress"));
    m_progressUi.bar->setAccessibleName(tr("生成上限に対する採択局面数"));
    mainLayout->addWidget(m_progressUi.bar);
    auto* progressLayout = new QHBoxLayout;
    m_progressUi.counts = new QLabel(this);
    m_progressUi.elapsed = new QLabel(this);
    onProgressUpdated(0, 0, 0);
    progressLayout->addWidget(m_progressUi.counts, 1);
    progressLayout->addWidget(m_progressUi.elapsed);
    mainLayout->addLayout(progressLayout);
    m_progressUi.verification = new QLabel(this);
    m_progressUi.verification->setWordWrap(true);
    onVerificationStatsUpdated(0, 0);
    mainLayout->addWidget(m_progressUi.verification);
}

void TsumeshogiGeneratorDialog::buildResultsSection(QVBoxLayout* mainLayout)
{
    auto* resultLabel = new QLabel(tr("結果一覧"), this);
    resultLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    mainLayout->addWidget(resultLabel);

    m_resultsUi.table = new QTableWidget(0, 4, this);
    m_resultsUi.table->setObjectName(QStringLiteral("generatorResults"));
    m_resultsUi.table->setAccessibleName(tr("生成した詰将棋局面"));
    m_resultsUi.table->setHorizontalHeaderLabels({tr("#"), tr("SFEN"), tr("盤面"), tr("手数")});
    m_resultsUi.table->horizontalHeader()->setStretchLastSection(false);
    m_resultsUi.table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_resultsUi.table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_resultsUi.table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultsUi.table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultsUi.table->setItemDelegateForColumn(2, new TableButtonDelegate(m_resultsUi.table));
    m_resultsUi.table->verticalHeader()->setVisible(false);
    m_resultsUi.table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsUi.table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_resultsUi.table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsUi.table->setWordWrap(false);
    m_resultsUi.table->setShowGrid(false);
    m_resultsUi.table->setMinimumHeight(180);
    // 空でも表見出しを残し、生成の始め方を結果領域内に案内する。
    auto* emptyLayout = new QVBoxLayout(m_resultsUi.table->viewport());
    m_resultsUi.emptyText = new QLabel(tr("生成した局面がここに表示されます。\n設定を確認して「開始」を押してください。"), m_resultsUi.table->viewport());
    m_resultsUi.emptyText->setAlignment(Qt::AlignCenter);
    m_resultsUi.emptyText->setWordWrap(true);
    m_resultsUi.emptyText->setAttribute(Qt::WA_TransparentForMouseEvents);
    emptyLayout->addWidget(m_resultsUi.emptyText);
    mainLayout->addWidget(m_resultsUi.table, 1);

    auto* exportLayout = new QHBoxLayout;
    m_resultsUi.includePv = new QCheckBox(tr("手順も出力"), this);
    m_resultsUi.includePv->setObjectName(QStringLiteral("generatorIncludePv"));
    m_resultsUi.includePv->setToolTip(tr("ファイル保存・コピー時に、SFEN の後ろに USI 形式の詰み手順（moves ...）を付加します"));
    exportLayout->addWidget(m_resultsUi.includePv);
    exportLayout->addStretch();
    m_resultsUi.copySelected = new QPushButton(tr("選択コピー"), this);
    m_resultsUi.copySelected->setObjectName(QStringLiteral("generatorCopySelected"));
    m_resultsUi.copySelected->setToolTip(tr("選択した局面をコピー（Ctrl+C）。Ctrl・Shift キーで複数選択できます。"));
    exportLayout->addWidget(m_resultsUi.copySelected);
    m_resultsUi.copyAll = new QPushButton(tr("全コピー"), this);
    m_resultsUi.copyAll->setObjectName(QStringLiteral("generatorCopyAll"));
    exportLayout->addWidget(m_resultsUi.copyAll);
    m_resultsUi.save = new QPushButton(tr("ファイル保存"), this);
    m_resultsUi.save->setObjectName(QStringLiteral("generatorSave"));
    m_resultsUi.save->setStyleSheet(ButtonStyles::fileOperation());
    exportLayout->addWidget(m_resultsUi.save);
    mainLayout->addLayout(exportLayout);
    auto* copy = new QShortcut(QKeySequence::Copy, m_resultsUi.table);
    copy->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copy, &QShortcut::activated, this, &TsumeshogiGeneratorDialog::onCopySelected);

    auto* bottomLayout = new QHBoxLayout;
    m_footerUi.fontDecrease = new QToolButton(this);
    m_footerUi.fontDecrease->setObjectName(QStringLiteral("tsumeshogiGeneratorFontDecrease"));
    bottomLayout->addWidget(m_footerUi.fontDecrease);
    m_footerUi.fontIncrease = new QToolButton(this);
    m_footerUi.fontIncrease->setObjectName(QStringLiteral("tsumeshogiGeneratorFontIncrease"));
    bottomLayout->addWidget(m_footerUi.fontIncrease);
    bottomLayout->addStretch();
    m_footerUi.close = new QPushButton(tr("閉じる"), this);
    bottomLayout->addWidget(m_footerUi.close);
    mainLayout->addLayout(bottomLayout);
}
