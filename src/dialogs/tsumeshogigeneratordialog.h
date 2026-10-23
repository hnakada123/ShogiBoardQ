#ifndef TSUMESHOGIGENERATORDIALOG_H
#define TSUMESHOGIGENERATORDIALOG_H

/// @file tsumeshogigeneratordialog.h
/// @brief 詰将棋局面生成ダイアログクラスの定義

#include "enginelistsettings.h"
#include "settingsformbinding.h"
#include <QDateTime>
#include <QDialog>
#include <QList>

#include "tsumeshogigenerator.h"

class EngineSelector;
class QCheckBox;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTableWidget;
class QToolButton;
class QVBoxLayout;

/**
 * @brief 詰将棋局面生成ダイアログ
 *
 * エンジン選択、生成パラメータ設定、プログレス表示、結果テーブルを
 * 1つのダイアログにまとめる。
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
    void onSaveToFile();
    void onCopySelected();
    void onCopyAll();
    void onRestoreDefaults();
    void onResultTableClicked(const QModelIndex& index);
    void onResultTableActivated(const QModelIndex& index);
    void updateResultActions();
    void updateProgressBar();
    void toggleHelp(bool visible);
    void fitSettingsArea();
    void showEngineSettingsDialog(const EngineListSettings::EngineEntry& engine);

private:
    /// 登録済みエンジン（名前・実行ファイル・作者）
    using Engine = EngineListSettings::EngineEntry;
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
    void buildFormSection(QVBoxLayout* mainLayout);
    void buildProgressSection(QVBoxLayout* mainLayout);
    void buildResultsSection(QVBoxLayout* mainLayout);
    void connectDialogSignals();
    /// 入力欄と設定の対応を作る
    void bindSettings();
    void loadSettings();
    void saveSettings();
    /// 表の行の高さ・見た目を今の文字に合わせる
    void fitToFont();
    void applyTableHeaderStyle();
    void setRunState(RunState state);
    bool isRunning() const { return m_state == RunState::Running || m_state == RunState::Stopping; }
    void setStatusText(const QString& status);
    QString formatElapsedTime(qint64 ms) const;
    QString exportLine(const Result& result) const;

    /// エンジンと生成設定（スクロールできる上段）
    struct SettingsPanel {
        QComboBox* engine = nullptr;
        QPushButton* engineSettings = nullptr;
        QScrollArea* scroll = nullptr;
        QSpinBox* targetMoves = nullptr;
        QSpinBox* maxAttack = nullptr;
        QSpinBox* maxDefend = nullptr;
        QSpinBox* attackRange = nullptr;
        QSpinBox* timeout = nullptr;
        QSpinBox* maxPositions = nullptr;
        QCheckBox* allowFinalAlternatives = nullptr; ///< 主手順の最終手の複数解を許容するか
        QToolButton* help = nullptr;
        QLabel* helpText = nullptr;
        QPushButton* restoreDefaults = nullptr;
    };
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

    SettingsPanel m_settingsUi;
    ProgressPanel m_progressUi;
    ResultsPanel m_resultsUi;
    FooterPanel m_footerUi;

    // エンジンリスト
    /// エンジンの選択（コンボ・設定ボタン。QObject parent 所有）
    EngineSelector* m_engineSelector = nullptr;
    SettingsFormBinding m_conditionsForm;  ///< 生成の条件の欄と設定（「既定値に戻す」の対象）
    SettingsFormBinding m_displayForm;     ///< 表示の欄（読み筋を含める・説明を開く）と設定

    // ジェネレータ（parent ownership。ダイアログと同寿命で、開始のたびに再利用する）
    TsumeshogiGenerator* m_generator = nullptr;

    // 直近の生成で使った設定と開始日時（ファイル保存時のコメントヘッダに記録する）
    TsumeshogiGenerator::Settings m_lastRunSettings;
    QDateTime m_lastRunStartedAt;
    RunState m_state = RunState::NotStarted;
    QList<Result> m_results;   ///< 採択した局面（表は表示だけ）
};

#endif // TSUMESHOGIGENERATORDIALOG_H
