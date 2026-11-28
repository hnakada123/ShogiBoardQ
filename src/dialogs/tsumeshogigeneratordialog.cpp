/// @file tsumeshogigeneratordialog.cpp
/// @brief 詰将棋局面生成ダイアログ（生成の開始と停止・進み具合・結果）

#include "tsumeshogigeneratordialog.h"
#include "appsettings.h"
#include "dialogfontscale.h"
#include "fontsizehelper.h"
#include "tsumeshogisettings.h"
#include "dialogutils.h"
#include "tsumeshogigeneratorsettingspanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
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
    , m_retroGenerator(new TsumeRetrogradeGenerator(this))
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
    m_displayForm.add(m_resultsUi.includePv, TsumeshogiSettings::tsumeshogiGeneratorIncludePv);
    loadSettings();

    // 初期状態
    setRunState(RunState::NotStarted);
    setStatusText(!m_settingsPanel->hasEngines()
        ? tr("設定メニューで詰将棋エンジンを登録してください。") : tr("待機中"));
}

TsumeshogiGeneratorDialog::~TsumeshogiGeneratorDialog()
{
    // ウィジェット破棄前に停止する。トリミング未完了の局面は出力しない。Idle なら何もしない
    m_generator->stop();
    m_retroGenerator->stop();
}

void TsumeshogiGeneratorDialog::done(int result)
{
    // 閉じたら生成を止め、設定を保存する（保存はここの1回だけ）
    onStopClicked();
    saveSettings();
    QDialog::done(result);
}

void TsumeshogiGeneratorDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    // 説明や逆算設定を開いたまま閉じた場合も、次に開いたとき設定欄を隠さない（配置が決まってから調整する）
    if (m_settingsPanel->isExpanded()) {
        QTimer::singleShot(0, this, &TsumeshogiGeneratorDialog::fitSettingsArea);
    }
}

void TsumeshogiGeneratorDialog::connectDialogSignals()
{
    connect(m_progressUi.start, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStartClicked);
    connect(m_progressUi.stop, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStopClicked);
    connect(m_resultsUi.save, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onSaveToFile);
    connect(m_resultsUi.copySelected, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopySelected);
    connect(m_resultsUi.copyAll, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopyAll);
    connect(m_footerUi.close, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::close);
    connect(m_resultsUi.table, &QTableWidget::clicked, this, &TsumeshogiGeneratorDialog::onResultTableClicked);
    connect(m_resultsUi.table, &QTableWidget::activated, this, &TsumeshogiGeneratorDialog::onResultTableActivated);
    connect(m_resultsUi.table, &QTableWidget::itemSelectionChanged, this, &TsumeshogiGeneratorDialog::updateResultActions);
    connect(m_settingsPanel, &TsumeshogiGeneratorSettingsPanel::maxPositionsChanged,
            this, &TsumeshogiGeneratorDialog::updateProgressBar);
    connect(m_settingsPanel, &TsumeshogiGeneratorSettingsPanel::contentHeightChanged,
            this, &TsumeshogiGeneratorDialog::onSettingsHeightChanged);

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

    connect(m_retroGenerator, &TsumeRetrogradeGenerator::positionFound,
            this, &TsumeshogiGeneratorDialog::onRetroPositionFound);
    connect(m_retroGenerator, &TsumeRetrogradeGenerator::progressUpdated,
            this, &TsumeshogiGeneratorDialog::onRetroProgressUpdated);
    connect(m_retroGenerator, &TsumeRetrogradeGenerator::checkpointSaved,
            this, &TsumeshogiGeneratorDialog::onRetroCheckpointSaved);
    connect(m_retroGenerator, &TsumeRetrogradeGenerator::finished,
            this, &TsumeshogiGeneratorDialog::onRetroFinished);
    connect(m_retroGenerator, &TsumeRetrogradeGenerator::errorOccurred,
            this, &TsumeshogiGeneratorDialog::onGeneratorError);
}

void TsumeshogiGeneratorDialog::loadSettings()
{
    m_settingsPanel->loadSettings();
    m_displayForm.load();
}

void TsumeshogiGeneratorDialog::saveSettings()
{
    m_settingsPanel->saveSettings();
    m_displayForm.save();
}

void TsumeshogiGeneratorDialog::onStartClicked()
{
    if (isRunning()) return;
    const std::optional<EngineEntry> engine = m_settingsPanel->selectedEngine();
    if (!engine) {
        DialogUtils::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }
    if (m_settingsPanel->mode() == TsumeshogiGeneratorSettingsPanel::Mode::Retrograde) {
        startRetrograde(*engine);
    } else {
        startRandom(*engine);
    }
}

void TsumeshogiGeneratorDialog::beginRun(GeneratorMode mode)
{
    // 結果をクリア
    m_results.clear();
    m_resultsUi.table->setRowCount(0);

    // ファイル保存時のコメントヘッダ用に、この実行の方式と開始日時を控える（設定は呼び出し側が控える）
    m_activeMode = mode;
    m_lastRunStartedAt = QDateTime::currentDateTime();

    setRunState(RunState::Running);
    onProgressUpdated(0, 0, 0);
    onVerificationStatsUpdated(0, 0);
    setStatusText(tr("エンジンを起動中"));
}

void TsumeshogiGeneratorDialog::startRandom(const EngineEntry& engine)
{
    const TsumeshogiGenerator::Settings settings = m_settingsPanel->randomSettings(engine);
    m_lastRunSettings = settings;
    beginRun(GeneratorMode::Random);
    m_generator->start(settings);
}

void TsumeshogiGeneratorDialog::startRetrograde(const EngineEntry& engine)
{
    QString settingsError;
    const std::optional<TsumeRetrogradeGenerator::Settings> settings =
        m_settingsPanel->retrogradeSettings(engine, &settingsError);
    if (!settings) {
        DialogUtils::warning(this, tr("エラー"), settingsError);
        return;
    }

    m_lastRunRetroSettings = *settings;
    beginRun(GeneratorMode::Retrograde);
    if (!m_retroGenerator->start(*settings)) {
        if (isRunning()) setRunState(RunState::Failed);
        return;
    }
    // チェックポイントから再開した場合、既に採択済みの局面を結果一覧と進捗表示へ復元する
    const auto& restored = m_retroGenerator->foundPositions();
    if (!restored.isEmpty()) {
        for (const auto& fp : restored) {
            onPositionFound(fp.sfen, fp.pv);
        }
        onRetroProgressUpdated(m_retroGenerator->progress());
        setStatusText(tr("エンジンを起動中"));
    }
}

void TsumeshogiGeneratorDialog::onStopClicked()
{
    if (!isRunning()) return;
    m_state = RunState::Stopping;
    if (m_activeMode == GeneratorMode::Retrograde) {
        m_retroGenerator->stop();
    } else {
        m_generator->stop();
    }
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
    DialogUtils::warning(this, tr("エラー"), message);
}

void TsumeshogiGeneratorDialog::onRetroPositionFound(const QString& sfen, const QStringList& pv,
                                                     int removedPieces, int verifyQueries)
{
    Q_UNUSED(removedPieces)
    Q_UNUSED(verifyQueries)
    onPositionFound(sfen, pv);
}

void TsumeshogiGeneratorDialog::onRetroProgressUpdated(const TsumeRetrogradeGenerator::Progress& progress)
{
    m_progressUi.counts->setText(
        tr("候補生成: %1 局面 / 最大到達: %2 手 / 採択: %3 局面")
            .arg(progress.generatedCandidates)
            .arg(progress.maxPliesReached)
            .arg(progress.found));
    m_progressUi.elapsed->setText(tr("経過時間: %1").arg(formatElapsedTime(progress.elapsedMs)));
    m_progressUi.verification->setText(
        tr("検査で除外: %1 局面（うち判定不能: %2 / フロンティア: %3 / 修復成功: %4）")
            .arg(progress.certifiedRejected)
            .arg(progress.certifiedInconclusive)
            .arg(progress.frontierNodes)
            .arg(progress.repairSucceeded));
    if (isRunning()) {
        setStatusText(formatRetroPhase(progress));
    }
}

void TsumeshogiGeneratorDialog::onRetroCheckpointSaved(const QString& path)
{
    m_progressUi.status->setToolTip(tr("最新の途中保存: %1").arg(path));
}

void TsumeshogiGeneratorDialog::onRetroFinished(bool stopped)
{
    if (m_state == RunState::Failed) return;
    if (stopped) m_state = RunState::Stopping;
    onGeneratorFinished();
}

QString TsumeshogiGeneratorDialog::formatRetroPhase(const TsumeRetrogradeGenerator::Progress& progress) const
{
    if (progress.phase == QLatin1String("certifying")) {
        if (progress.trimTotal > 0) {
            return tr("厳密証明・不要駒検査中（候補 %1/%2、問い合わせ %3 回）")
                .arg(progress.trimIndex)
                .arg(progress.trimTotal)
                .arg(progress.certifierQueries);
        }
        return tr("厳密証明中（問い合わせ %1 回）").arg(progress.certifierQueries);
    }
    if (progress.phase == QLatin1String("repairing")) {
        return tr("余詰修復中（キュー %1 件）").arg(progress.repairQueueSize);
    }
    if (progress.phase == QLatin1String("checking_first")) {
        return tr("初手余詰検査中（最大到達 %1 手）").arg(progress.maxPliesReached);
    }
    return tr("逆算・成長探索中（最大到達 %1 手 / フロンティア %2）")
        .arg(progress.maxPliesReached)
        .arg(progress.frontierNodes);
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
    const QStringList header = (m_activeMode == GeneratorMode::Retrograde)
        ? TsumeshogiExportHeaderBuilder::build(QStringLiteral(APP_VERSION), m_lastRunStartedAt, m_lastRunRetroSettings)
        : TsumeshogiExportHeaderBuilder::build(QStringLiteral(APP_VERSION), m_lastRunStartedAt, m_lastRunSettings);
    QStringList lines;
    for (const Result& result : std::as_const(m_results)) lines.append(exportLine(result));
    QString errorMessage;
    if (!TsumeCollectionWriter::save(filePath, header, lines, &errorMessage)) {
        DialogUtils::warning(this, tr("エラー"), tr("ファイルを保存できませんでした: %1").arg(errorMessage));
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
    dlg->setSquareSize(AppSettings::squareSize());   // 本体の盤と同じ大きさ（変わるたびに保存している）
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    DialogUtils::showWindow(dlg);
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
    const int lastLimit = (m_activeMode == GeneratorMode::Retrograde)
        ? m_lastRunRetroSettings.maxPositionsToFind
        : m_lastRunSettings.maxPositionsToFind;
    const int limit = m_state != RunState::NotStarted ? lastLimit
                                                      : m_settingsPanel->maxPositions();
    const int found = static_cast<int>(m_results.size());
    m_progressUi.bar->setRange(0, limit > 0 ? limit : (isRunning() ? 0 : qMax(1, found)));
    m_progressUi.bar->setValue(found);
    m_progressUi.bar->setFormat(limit > 0 ? tr("%v / %m 局面") : tr("%1 局面（無制限）").arg(found));
}

void TsumeshogiGeneratorDialog::onSettingsHeightChanged()
{
    if (isVisible()) fitSettingsArea();
}

void TsumeshogiGeneratorDialog::fitSettingsArea()
{
    // 説明や逆算設定を開くと設定欄が高くなる。結果一覧の高さを譲ってもらい、それでも足りなければ
    // 画面に収まる範囲でダイアログを高くして、設定をスクロールせずに見られるようにする。
    // 閉じたときは設定欄を縮めて結果一覧へ高さを戻す。
    m_settingsPanel->refreshLayout();
    m_settingsScroll->viewport()->updateGeometry();
    m_settingsScroll->updateGeometry();
    layout()->activate();
    const int missing = m_settingsPanel->sizeHint().height() - m_settingsScroll->viewport()->height();
    if (missing <= 0) return;
    int newHeight = height() + missing;
    if (const QScreen* s = screen()) {
        const int frame = frameGeometry().height() - height();
        newHeight = qMin(newHeight, qMax(height(), s->availableGeometry().height() - frame));
    }
    resize(width(), newHeight);
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
    m_progressUi.start->setEnabled(!running && m_settingsPanel->hasEngines());
    m_progressUi.stop->setEnabled(running);
    m_settingsPanel->setRunning(running);
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
    m_settingsScroll = new FittingScrollArea(this);
    m_settingsScroll->setObjectName(QStringLiteral("generatorSettingsScroll"));
    m_settingsScroll->setWidgetResizable(true);
    m_settingsScroll->setFrameShape(QFrame::NoFrame);
    m_settingsScroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_settingsPanel = new TsumeshogiGeneratorSettingsPanel(m_settingsScroll);
    m_settingsScroll->setWidget(m_settingsPanel);
    m_settingsScroll->setMinimumHeight(180);
    mainLayout->addWidget(m_settingsScroll);
    buildProgressSection(mainLayout);
    buildResultsSection(mainLayout);
    connectDialogSignals();

    // 設定入力中の Enter で不用意に生成を開始しない。
    const auto buttons = findChildren<QPushButton*>();
    for (auto* button : buttons) button->setAutoDefault(false);
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
