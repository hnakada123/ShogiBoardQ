#ifndef CONSIDERATIONPANEL_H
#define CONSIDERATIONPANEL_H

/// @file considerationpanel.h
/// @brief 検討ドックの中身（ツールバー・エンジン情報・候補手の表）

#include <QWidget>
#include <QString>

#include "considerationsettings.h"
#include "enginelistsettings.h"
#include "fontsizehelper.h"
#include "shogienginethinkingmodel.h"

class QToolButton;
class QPushButton;
class QLabel;
class QComboBox;
class QSpinBox;
class QTimer;
class QRadioButton;
class QCheckBox;
class QModelIndex;
class QMenu;

class EngineInfoWidget;
class EngineSelector;
class ThinkingTableView;

/**
 * @brief 検討タブのUI構築・状態管理を担うマネージャ
 *
 * 検討ドックの中身（ツールバー・情報・表）。部品はコンストラクタで作る。
 * - 設定（エンジン・時間・候補手の数・矢印）は ConsiderationSettings を正本として持ち、利用者が変えると
 *   保存して settingsChanged で知らせる。ウィジェットはその値を出すだけ
 * - 実行の状態（RunState）は検討の表示の持ち主（EngineDisplayPresenter）が設定する。開始/中止のボタンは
 *   状態に応じて startConsiderationRequested か stopConsiderationRequested を出す
 */
class ConsiderationPanel : public QWidget
{
    Q_OBJECT

public:
    /// 検討の実行の状態（ボタンの表示・経過時間・時間設定の可否が決まる）
    enum class RunState {
        Idle,       ///< 検討していない（ボタンは「検討開始」、経過時間は 0）
        Running,    ///< 局面を探索している（経過時間を数える。時間設定は変えられない）
        Waiting,    ///< 時間切れで次の局面を待っている（経過時間を止め、ボタンは「検討中止」のまま）
    };
    Q_ENUM(RunState)

    /// ツールバー・情報・表を作り、保存した検討の条件を出す
    explicit ConsiderationPanel(QWidget* parent = nullptr);
    ~ConsiderationPanel() override;
    /// 検討の表が model を出していれば選択を外す（読み筋ダイアログを閉じたとき）
    void clearViewSelection(const ShogiEngineThinkingModel* model);

    // --- アクセサ ---
    EngineInfoWidget* considerationInfo() const { return m_considerationInfo; }
    ThinkingTableView* considerationView() const { return m_considerationView; }

    // モデル設定
    void setConsiderationThinkingModel(ShogiEngineThinkingModel* m);
    ShogiEngineThinkingModel* considerationModel() const { return m_considerationModel; }

    /// 検討の条件（正本）
    const ConsiderationSettings& settings() const { return m_settings; }

    /// 検討しているエンジンの名前（情報欄に出す）
    void setConsiderationEngineName(const QString& name);

    /// 実行の状態（Running にするたびに経過時間を数え直す）
    void setRunState(ConsiderationPanel::RunState state);
    RunState runState() const { return m_runState; }

    // 検討開始ボタンの有効/無効（検討中は中止操作を優先して無効化しない）
    void setStartConsiderationEnabled(bool enabled);

    // フォントサイズ（外部からの取得用）
    int considerationFontSize() const { return m_considerationFontSize.fontSize(); }

public slots:
    /// 登録エンジンの一覧を読み直す（エンジンの登録・削除の後。選んでいたエンジンが残っていれば選んだまま）
    void reloadEngines();

signals:
    /// 検討の条件が変わった（利用者の操作）
    void settingsChanged(const ConsiderationSettings& settings);
    /// 開始/中止のボタンを押した（検討していないとき）
    void startConsiderationRequested();
    /// 開始/中止のボタンを押した（検討しているとき）
    void stopConsiderationRequested();
    // エンジン設定ボタンが押されたシグナル
    void engineSettingsRequested(const EngineEntry& engine);

    // 読み筋の行の「盤面」ボタンが押された
    void pvRowClicked(ShogiEngineThinkingModel* model, int row);
    /// 読み筋の行を右クリックした（受け手が menu に項目を足す）
    void rowContextMenuAboutToShow(ShogiEngineThinkingModel* model, int row, QMenu* menu);

private slots:
    /// 画面の値から検討の条件を読み直す（利用者がどれかの設定を変えた）
    void onControlChanged();
    void onRunButtonClicked();
    void onElapsedTimerTick();

private:
    void buildToolbarControls();
    void layoutToolbar();
    void buildConsiderationView();
    /// エンジンの一覧を設定から読み、検討の条件を復元して、変更の通知をつなぐ
    void loadEnginesAndSettings();
    /// 検討の条件を画面に出す（通知は出さない）
    void showSettings();
    void saveSettings() const;
    void applyFontSize();
    void updateTimeControls();
    void updateRunButton();
    void resetElapsedTime();

    // 検討タブ用UI
    EngineInfoWidget* m_considerationInfo = nullptr;
    ThinkingTableView* m_considerationView = nullptr;
    ShogiEngineThinkingModel* m_considerationModel = nullptr;
    QWidget* m_considerationToolbar = nullptr;
    QToolButton* m_btnConsiderationFontDecrease = nullptr;
    QToolButton* m_btnConsiderationFontIncrease = nullptr;
    QComboBox* m_engineComboBox = nullptr;
    QPushButton* m_btnEngineSettings = nullptr;
    /// エンジンの選択（m_engineComboBox・m_btnEngineSettings に結ぶ。QObject parent 所有）
    EngineSelector* m_engineSelector = nullptr;
    QRadioButton* m_unlimitedTimeRadioButton = nullptr;
    QRadioButton* m_considerationTimeRadioButton = nullptr;
    QSpinBox* m_byoyomiSecSpinBox = nullptr;
    QLabel* m_byoyomiSecUnitLabel = nullptr;
    QLabel* m_elapsedTimeLabel = nullptr;
    QTimer* m_elapsedTimer = nullptr;
    int m_elapsedSeconds = 0;
    QLabel* m_multiPVLabel = nullptr;
    QComboBox* m_multiPVComboBox = nullptr;
    QToolButton* m_btnStopConsideration = nullptr;
    QCheckBox* m_showArrowsCheckBox = nullptr;
    FontSizeHelper m_considerationFontSize;   ///< 検討タブの文字の大きさ（変更のたびに保存する）
    ConsiderationSettings m_settings;
    RunState m_runState = RunState::Idle;
    bool m_startConsiderationEnabled = true;
};

#endif // CONSIDERATIONPANEL_H
