#ifndef CONSIDERATIONTABMANAGER_H
#define CONSIDERATIONTABMANAGER_H

/// @file considerationtabmanager.h
/// @brief 検討ドックの中身（ツールバー・エンジン情報・候補手の表）

#include <QObject>
#include <QMetaObject>
#include <QString>

#include "fontsizehelper.h"
#include "shogienginethinkingmodel.h"

class QWidget;
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
class ThinkingTableView;

/**
 * @brief 検討タブのUI構築・状態管理を担うマネージャ
 *
 * ドックの中身は buildUi() で作る。候補手の数・時間・エンジンの選択などの設定の保存・復元も担当する。
 */
class ConsiderationTabManager : public QObject
{
    Q_OBJECT

public:
    explicit ConsiderationTabManager(QObject* parent = nullptr);
    ~ConsiderationTabManager() override;

    /// 検討ドックの中身（ツールバー・情報・表）を作る
    QWidget* buildUi(QWidget* parent);
    /// 検討タブのUIを構築し、ツールバー・情報・テーブルビューを parentWidget に配置する
    void buildConsiderationUi(QWidget* parentWidget);
    /// 検討の表が model を出していれば選択を外す（読み筋ダイアログを閉じたとき）
    void clearViewSelection(const ShogiEngineThinkingModel* model);

    // --- アクセサ ---
    EngineInfoWidget* considerationInfo() const { return m_considerationInfo; }
    ThinkingTableView* considerationView() const { return m_considerationView; }

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

    // 検討開始ボタンの有効/無効（検討中は中止操作を優先して無効化しない）
    void setStartConsiderationEnabled(bool enabled);

    // フォントサイズ（外部からの取得用）
    int considerationFontSize() const { return m_considerationFontSize.fontSize(); }

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

    // 読み筋の行の「盤面」ボタンが押された
    void pvRowClicked(ShogiEngineThinkingModel* model, int row);

private slots:
    void onMultiPVComboBoxChanged(int index);
    void onEngineComboBoxChanged(int index);
    void onEngineSettingsClicked();
    void onTimeSettingChanged();
    void onElapsedTimerTick();

private:
    void buildToolbarControls(QWidget* parentWidget);
    void layoutToolbar();
    void buildConsiderationView(QWidget* parentWidget);
    void applyFontSize();
    void updateTimeControls();

    // 検討タブ用UI
    EngineInfoWidget* m_considerationInfo = nullptr;
    ThinkingTableView* m_considerationView = nullptr;
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
    FontSizeHelper m_considerationFontSize;   ///< 検討タブの文字の大きさ（変更のたびに保存する）
    bool m_considerationRunning = false;
    bool m_startConsiderationEnabled = true;
    QMetaObject::Connection m_stopButtonConnection;
};

#endif // CONSIDERATIONTABMANAGER_H
