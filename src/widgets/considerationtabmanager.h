#ifndef CONSIDERATIONTABMANAGER_H
#define CONSIDERATIONTABMANAGER_H

/// @file considerationtabmanager.h
/// @brief EngineAnalysisTab から分離した検討タブ管理クラスの定義

#include <QObject>
#include <QString>
#include <memory>

class QWidget;
class QTableView;
class QToolButton;
class QPushButton;
class QLabel;
class QComboBox;
class QSpinBox;
class QTimer;
class QRadioButton;
class QCheckBox;
class QModelIndex;

class EngineInfoWidget;
class LogViewFontManager;
class ShogiEngineThinkingModel;

/**
 * @brief 検討タブのUI構築・状態管理を担うマネージャ
 *
 * EngineAnalysisTab から検討タブ関連の責務を分離したクラス。
 * UI構築は buildConsiderationUi() で行い、各種設定の保存・復元も担当する。
 */
class ConsiderationTabManager : public QObject
{
    Q_OBJECT

public:
    explicit ConsiderationTabManager(QObject* parent = nullptr);
    ~ConsiderationTabManager() override;

    /// 検討タブのUIを構築し、ツールバー・情報・テーブルビューを parentWidget に配置する
    void buildConsiderationUi(QWidget* parentWidget);

    // --- アクセサ ---
    EngineInfoWidget* considerationInfo() const { return m_considerationInfo; }
    QTableView* considerationView() const { return m_considerationView; }

    // モデル設定
    void setConsiderationThinkingModel(ShogiEngineThinkingModel* m);
    ShogiEngineThinkingModel* considerationModel() const { return m_considerationModel; }

    // 候補手の数
    int considerationMultiPV() const;
    void setConsiderationMultiPV(int value);

    // 時間設定
    void setConsiderationTimeLimit(bool unlimited, int byoyomiSec);
    bool isUnlimitedTime() const;
    int byoyomiSec() const;

    // エンジン名設定
    void setConsiderationEngineName(const QString& name);

    // エンジン選択
    int selectedEngineIndex() const;
    QString selectedEngineName() const;

    // 矢印表示
    bool isShowArrowsChecked() const;

    // エンジンリスト読み込み
    void loadEngineList();

    // 設定の保存・復元
    void loadConsiderationTabSettings();
    void saveConsiderationTabSettings();

    // 経過時間タイマー制御
    void startElapsedTimer();
    void stopElapsedTimer();
    void resetElapsedTimer();

    // 検討実行状態の設定（ボタン表示切替用）
    void setConsiderationRunning(bool running);

    // フォントサイズ（外部からの取得用）
    int considerationFontSize() const { return m_considerationFontSize; }

signals:
    // 検討タブの候補手の数変更シグナル
    void considerationMultiPVChanged(int value);

    // 検討中止シグナル
    void stopConsiderationRequested();

    // 検討開始シグナル
    void startConsiderationRequested();

    // エンジン設定ボタンが押されたシグナル
    void engineSettingsRequested(int engineNumber, const QString& engineName);

    // 思考時間設定が変更されたシグナル
    void considerationTimeSettingsChanged(bool unlimited, int byoyomiSec);

    // 矢印表示設定が変更されたシグナル
    void showArrowsChanged(bool checked);

    // 検討中にエンジン選択が変更されたシグナル
    void considerationEngineChanged(int index, const QString& name);

    // 読み筋がクリックされた時のシグナル（engineIndex=2: 検討タブ）
    void pvRowClicked(int engineIndex, int row);

private slots:
    void onConsiderationViewClicked(const QModelIndex& index);
    void onConsiderationFontIncrease();
    void onConsiderationFontDecrease();
    void onMultiPVComboBoxChanged(int index);
    void onEngineComboBoxChanged(int index);
    void onEngineSettingsClicked();
    void onTimeSettingChanged();
    void onElapsedTimerTick();

private:
    void initFontManager();

    // 検討タブ用UI
    EngineInfoWidget* m_considerationInfo = nullptr;
    QTableView* m_considerationView = nullptr;
    ShogiEngineThinkingModel* m_considerationModel = nullptr;
    QWidget* m_considerationToolbar = nullptr;
    QToolButton* m_btnConsiderationFontDecrease = nullptr;
    QToolButton* m_btnConsiderationFontIncrease = nullptr;
    QComboBox* m_engineComboBox = nullptr;
    QPushButton* m_btnEngineSettings = nullptr;
    QRadioButton* m_unlimitedTimeRadioButton = nullptr;
    QRadioButton* m_considerationTimeRadioButton = nullptr;
    QSpinBox* m_byoyomiSecSpinBox = nullptr;
    QLabel* m_byoyomiSecUnitLabel = nullptr;
    QLabel* m_elapsedTimeLabel = nullptr;
    QTimer* m_elapsedTimer = nullptr;
    int m_elapsedSeconds = 0;
    int m_considerationTimeLimitSec = 0;
    QLabel* m_multiPVLabel = nullptr;
    QComboBox* m_multiPVComboBox = nullptr;
    QToolButton* m_btnStopConsideration = nullptr;
    QCheckBox* m_showArrowsCheckBox = nullptr;
    int m_considerationFontSize = 10;
    std::unique_ptr<LogViewFontManager> m_considerationFontManager;
    bool m_considerationRunning = false;
};

#endif // CONSIDERATIONTABMANAGER_H
