#ifndef TSUMESHOGIGENERATORDIALOG_H
#define TSUMESHOGIGENERATORDIALOG_H

/// @file tsumeshogigeneratordialog.h
/// @brief 詰将棋局面生成ダイアログクラスの定義

#include <QDialog>
#include <QList>

#include "fontsizehelper.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QToolButton;
class QVBoxLayout;
class TsumeshogiGenerator;

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
    void onFontIncrease();
    void onFontDecrease();
    void onRestoreDefaults();
    void onResultTableClicked(const QModelIndex& index);
    void showEngineSettingsDialog();

private:
    /// エンジン情報
    struct Engine {
        QString name;
        QString path;
        QString author;
    };

    void setupUi();
    void buildFormSection(QVBoxLayout* mainLayout);
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

    // エンジン設定
    QComboBox* m_comboEngine = nullptr;
    QPushButton* m_btnEngineSetting = nullptr;

    // 生成設定
    QSpinBox* m_spinTargetMoves = nullptr;
    QSpinBox* m_spinMaxAttack = nullptr;
    QSpinBox* m_spinMaxDefend = nullptr;
    QSpinBox* m_spinAttackRange = nullptr;
    QSpinBox* m_spinTimeout = nullptr;
    QSpinBox* m_spinMaxPositions = nullptr;
    QCheckBox* m_checkAllowFinalAlternatives = nullptr; ///< 主手順の最終手の複数解を許容するか

    // 制御ボタン
    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnStop = nullptr;

    // プログレス表示
    QLabel* m_labelProgress = nullptr;
    QLabel* m_labelElapsed = nullptr;
    QLabel* m_labelStatus = nullptr;
    QLabel* m_labelVerification = nullptr;

    // 結果テーブル
    QTableWidget* m_tableResults = nullptr;

    // フォントボタン
    QToolButton* m_btnFontDecrease = nullptr;
    QToolButton* m_btnFontIncrease = nullptr;

    // 既定値に戻すボタン
    QPushButton* m_btnRestoreDefaults = nullptr;

    // 出力オプション（SFEN に詰み手順を付加するか）
    QCheckBox* m_checkIncludePv = nullptr;

    // ファイル保存ボタン
    QPushButton* m_btnSaveToFile = nullptr;

    // コピーボタン
    QPushButton* m_btnCopySelected = nullptr;
    QPushButton* m_btnCopyAll = nullptr;

    // 閉じるボタン
    QPushButton* m_btnClose = nullptr;

    // エンジンリスト
    QList<Engine> m_engineList;

    // ジェネレータ（parent ownership。ダイアログと同寿命で、開始のたびに再利用する）
    TsumeshogiGenerator* m_generator = nullptr;

    // フォントサイズヘルパー
    FontSizeHelper m_fontHelper;
};

#endif // TSUMESHOGIGENERATORDIALOG_H
