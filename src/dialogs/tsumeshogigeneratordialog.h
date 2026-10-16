#ifndef TSUMESHOGIGENERATORDIALOG_H
#define TSUMESHOGIGENERATORDIALOG_H

/// @file tsumeshogigeneratordialog.h
/// @brief 詰将棋局面生成ダイアログクラスの定義

#include "enginelistsettings.h"
#include <QDateTime>
#include <QDialog>
#include <QList>

#include "fontsizehelper.h"
#include "tsumeshogigenerator.h"

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
    void showEngineSettingsDialog();

private:
    /// エンジン情報
    /// 登録済みエンジン（名前・実行ファイル・作者）
    using Engine = EngineListSettings::EngineEntry;

    void setupUi();
    void buildFormSection(QVBoxLayout* mainLayout);
    void buildProgressSection(QVBoxLayout* mainLayout);
    void buildResultsSection(QVBoxLayout* mainLayout);
    void connectDialogSignals();
    void readEngineNameAndDir();
    void loadSettings();
    void saveSettings();
    void applyFontSize();
    void applyTableHeaderStyle();
    void setRunningState(bool running);
    void setStatusText(const QString& status);
    QString formatElapsedTime(qint64 ms) const;
    QString exportLine(int row) const;

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
    QList<Engine> m_engineList;

    // ジェネレータ（parent ownership。ダイアログと同寿命で、開始のたびに再利用する）
    TsumeshogiGenerator* m_generator = nullptr;

    // 直近の生成で使った設定と開始日時（ファイル保存時のコメントヘッダに記録する）
    TsumeshogiGenerator::Settings m_lastRunSettings;
    QDateTime m_lastRunStartedAt;
    bool m_running = false;
    bool m_stopRequested = false;
    bool m_runFailed = false;

    // フォントサイズヘルパー
    FontSizeHelper m_fontHelper;
};

#endif // TSUMESHOGIGENERATORDIALOG_H
