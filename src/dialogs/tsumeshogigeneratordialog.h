#ifndef TSUMESHOGIGENERATORDIALOG_H
#define TSUMESHOGIGENERATORDIALOG_H

/// @file tsumeshogigeneratordialog.h
/// @brief 詰将棋局面生成ダイアログクラスの定義

#include "engineselectionsettings.h"
#include "settingsformbinding.h"
#include <QDateTime>
#include <QDialog>
#include <QList>

#include "tsumeretrogradegenerator.h"
#include "tsumeshogigenerator.h"

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QTableWidget;
class QToolButton;
class QVBoxLayout;
class TsumeshogiGeneratorSettingsPanel;

/**
 * @brief 詰将棋局面生成ダイアログ
 *
 * 生成の開始と停止、進み具合の表示、結果の表と書き出しを受け持つ。エンジンの選択と生成設定の欄
 * （生成方式ごとの欄・設定の保存・生成器の設定への変換）は TsumeshogiGeneratorSettingsPanel。
 */
class TsumeshogiGeneratorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TsumeshogiGeneratorDialog(QWidget* parent = nullptr);
    ~TsumeshogiGeneratorDialog() override;

protected:
    void done(int result) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onStartClicked();
    void onStopClicked();
    void onPositionFound(const QString& sfen, const QStringList& pv);
    void onProgressUpdated(int tried, int found, qint64 elapsedMs);
    void onGeneratorFinished();
    void onGeneratorError(const QString& message);
    void onSearchPhaseStarted();
    void onTrimmingProgress(int candidate, int total);
    void onVerificationProgress(int queries);
    void onVerificationStatsUpdated(int rejected, int inconclusive);
    void onRetroPositionFound(const QString& sfen, const QStringList& pv, int removedPieces, int verifyQueries);
    void onRetroProgressUpdated(const TsumeRetrogradeGenerator::Progress& progress);
    void onRetroCheckpointSaved(const QString& path);
    void onRetroFinished(bool stopped);
    void onSettingsHeightChanged();
    void onSaveToFile();
    void onCopySelected();
    void onCopyAll();
    void onResultTableClicked(const QModelIndex& index);
    void onResultTableActivated(const QModelIndex& index);
    void updateResultActions();
    void updateProgressBar();
    void fitSettingsArea();

private:
    enum class GeneratorMode {
        Random = 0,
        Retrograde = 1
    };

    /// 生成の進み
    enum class RunState {
        NotStarted,  ///< まだ開始していない
        Running,
        Stopping,    ///< 停止を頼み、終わるのを待っている
        Finished,    ///< 目標の数を採択した・探索を終えた
        Stopped,     ///< 利用者が止めた
        Failed       ///< エラーで終わった
    };
    /// 採択した局面（結果の表の行と同じ順。保存・コピー・盤の表示はここから読む）
    struct Result {
        QString sfen;
        QStringList pv;
    };

    void setupUi();
    void buildProgressSection(QVBoxLayout* mainLayout);
    void buildResultsSection(QVBoxLayout* mainLayout);
    void connectDialogSignals();
    void loadSettings();
    void saveSettings();
    /// 表の行の高さ・見た目を今の文字に合わせる
    void fitToFont();
    void applyTableHeaderStyle();
    /// 結果を空にして、生成中の表示にする（生成器を始める直前に呼ぶ）
    void beginRun(GeneratorMode mode);
    void startRandom(const EngineEntry& engine);
    void startRetrograde(const EngineEntry& engine);
    void setRunState(RunState state);
    bool isRunning() const { return m_state == RunState::Running || m_state == RunState::Stopping; }
    void setStatusText(const QString& status);
    QString formatElapsedTime(qint64 ms) const;
    QString exportLine(const Result& result) const;
    QString formatRetroPhase(const TsumeRetrogradeGenerator::Progress& progress) const;

    /// 開始・停止と進み具合
    struct ProgressPanel {
        QPushButton* start = nullptr;
        QPushButton* stop = nullptr;
        QLabel* counts = nullptr;
        QLabel* elapsed = nullptr;
        QLabel* status = nullptr;
        QLabel* verification = nullptr;
        QProgressBar* bar = nullptr;
    };
    /// 結果一覧と書き出し（SFEN に詰み手順を付加するか・保存・コピー）
    struct ResultsPanel {
        QTableWidget* table = nullptr;
        QLabel* emptyText = nullptr;
        QCheckBox* includePv = nullptr;
        QPushButton* save = nullptr;
        QPushButton* copySelected = nullptr;
        QPushButton* copyAll = nullptr;
    };
    /// 最下段: 文字サイズと閉じる
    struct FooterPanel {
        QToolButton* fontDecrease = nullptr;
        QToolButton* fontIncrease = nullptr;
        QPushButton* close = nullptr;
    };

    /// エンジンと生成設定の欄（スクロールできる上段。QObject parent 所有）
    QScrollArea* m_settingsScroll = nullptr;
    TsumeshogiGeneratorSettingsPanel* m_settingsPanel = nullptr;
    ProgressPanel m_progressUi;
    ResultsPanel m_resultsUi;
    FooterPanel m_footerUi;

    SettingsFormBinding m_displayForm;     ///< 表示の欄（読み筋を含める）と設定

    // ジェネレータ（parent ownership。ダイアログと同寿命で、開始のたびに再利用する）
    TsumeshogiGenerator* m_generator = nullptr;
    TsumeRetrogradeGenerator* m_retroGenerator = nullptr;

    // 直近の生成で使った設定と開始日時（ファイル保存時のコメントヘッダに記録する）
    GeneratorMode m_activeMode = GeneratorMode::Random;
    TsumeshogiGenerator::Settings m_lastRunSettings;
    TsumeRetrogradeGenerator::Settings m_lastRunRetroSettings;
    QDateTime m_lastRunStartedAt;
    RunState m_state = RunState::NotStarted;
    QList<Result> m_results;   ///< 採択した局面（表は表示だけ）
};

#endif // TSUMESHOGIGENERATORDIALOG_H
