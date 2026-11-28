#ifndef TSUMESHOGIGENERATORSETTINGSPANEL_H
#define TSUMESHOGIGENERATORSETTINGSPANEL_H

/// @file tsumeshogigeneratorsettingspanel.h
/// @brief 詰将棋局面生成ダイアログの、エンジンと生成設定の欄

#include "engineselectionsettings.h"
#include "settingsformbinding.h"
#include "tsumeretrogradegenerator.h"
#include "tsumeshogigenerator.h"

#include <QWidget>

#include <optional>

class EngineSelector;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QToolButton;
class QVBoxLayout;

/**
 * @brief 詰将棋局面生成ダイアログの上段（エンジンの選択と生成設定）
 *
 * 生成方式（ランダム配置／詰み局面から逆算・編集）ごとの欄の出し分け、欄の値と設定の保存・復元、
 * 逆算・編集のファイル（種局面・除外・途中保存）の欄を受け持ち、欄の値を生成器の設定にして返す。
 * 生成の開始・停止と進み具合・結果はダイアログ（TsumeshogiGeneratorDialog）が受け持つ。
 */
class TsumeshogiGeneratorSettingsPanel : public QWidget
{
    Q_OBJECT

public:
    /// 生成方式（コンボの位置と同じ）
    enum class Mode {
        Random = 0,
        Retrograde = 1
    };

    explicit TsumeshogiGeneratorSettingsPanel(QWidget* parent = nullptr);

    /// 保存した設定を欄に入れる
    void loadSettings();
    /// 欄の値と選んだエンジンを保存する
    void saveSettings();

    Mode mode() const;
    bool hasEngines() const;
    std::optional<EngineEntry> selectedEngine() const;
    /// 生成上限（0 は無制限）
    int maxPositions() const;
    /// 説明か逆算・編集の設定を開いていて、欄が高くなっているか
    bool isExpanded() const;

    /// ランダム配置の生成の設定
    TsumeshogiGenerator::Settings randomSettings(const EngineEntry& engine) const;
    /// 逆算・編集の生成の設定（種局面・除外のファイルも読む）。作れなければ nullopt（理由を errorMessage に入れる）
    std::optional<TsumeRetrogradeGenerator::Settings> retrogradeSettings(const EngineEntry& engine,
                                                                         QString* errorMessage) const;

    /// 生成中は欄を変えられないようにする
    void setRunning(bool running);
    /// 説明や逆算・編集の設定の開閉で変わった高さを、欄の中のレイアウトに計算し直させる
    void refreshLayout();

signals:
    void maxPositionsChanged();
    /// 欄の高さが変わった（説明の開閉・生成方式の切り替え・既定値に戻す）
    void contentHeightChanged();

private slots:
    void onModeChanged(int index);
    void onBrowseSeedFile();
    void onClearSeedFile();
    void onBrowseExcludeFile();
    void onClearExcludeFile();
    void onBrowseCheckpointFile();
    void onClearCheckpointFile();
    void onResumeCheckpointToggled(bool checked);
    void onRestoreDefaults();
    void toggleHelp(bool visible);
    void showEngineSettingsDialog(const EngineEntry& engine);

private:
    void buildEngineGroup(QVBoxLayout* mainLayout);
    void buildConditionsGroup(QVBoxLayout* mainLayout);
    /// 目標手数・駒数の上限など、2列に並べる数値の欄
    void buildConditionRows();
    /// 逆算・編集だけの条件とファイルの欄
    QWidget* buildRetrogradeExtras();
    void connectSignals();
    /// 入力欄と設定の対応を作る
    void bindSettings();
    void applyModeUi();

    struct Ui {
        QComboBox* engine = nullptr;
        QPushButton* engineSettings = nullptr;
        QComboBox* mode = nullptr;
        QFormLayout* leftForm = nullptr;
        QFormLayout* rightForm = nullptr;
        QSpinBox* targetMoves = nullptr;
        QSpinBox* maxAttack = nullptr;
        QSpinBox* maxDefend = nullptr;
        QSpinBox* attackRange = nullptr;
        QSpinBox* retroMaxAttack = nullptr;
        QSpinBox* retroMaxDefend = nullptr;
        QSpinBox* beamWidth = nullptr;
        QSpinBox* branchFactor = nullptr;
        QSpinBox* verifyBudget = nullptr;
        QLabel* timeoutLabel = nullptr;
        QSpinBox* timeout = nullptr;
        QSpinBox* maxPositions = nullptr;
        QCheckBox* allowFinalAlternatives = nullptr; ///< 主手順の最終手の複数解を許容するか
        QCheckBox* requireNoSurplus = nullptr;       ///< 逆算モード：駒余りを禁止するか
        QCheckBox* excludeSimilar = nullptr;         ///< 逆算モード：類似作を除外するか
        QWidget* retroFilesWidget = nullptr;         ///< 逆算モード：種局面・途中保存のファイル設定
        QLineEdit* seedFile = nullptr;
        QPushButton* browseSeed = nullptr;
        QPushButton* clearSeed = nullptr;
        QLineEdit* excludeFile = nullptr;
        QPushButton* browseExclude = nullptr;
        QPushButton* clearExclude = nullptr;
        QLineEdit* checkpointFile = nullptr;
        QPushButton* browseCheckpoint = nullptr;
        QPushButton* clearCheckpoint = nullptr;
        QCheckBox* resumeCheckpoint = nullptr;
        QToolButton* help = nullptr;
        QLabel* helpText = nullptr;
        QPushButton* restoreDefaults = nullptr;
    };

    Ui m_ui;
    /// エンジンの選択（コンボ・設定ボタン。QObject parent 所有）
    EngineSelector* m_engineSelector = nullptr;
    SettingsFormBinding m_conditionsForm;  ///< 生成の条件の欄と設定（「既定値に戻す」の対象）
    SettingsFormBinding m_displayForm;     ///< 説明を開いているかの設定
    bool m_running = false;
};

#endif // TSUMESHOGIGENERATORSETTINGSPANEL_H
