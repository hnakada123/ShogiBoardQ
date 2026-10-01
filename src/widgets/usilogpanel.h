#ifndef USILOGPANEL_H
#define USILOGPANEL_H

/// @file usilogpanel.h
/// @brief USI通信ログパネルクラスの定義

#include <QObject>
#include <QColor>
#include <memory>

class QWidget;
class QAction;
class QPlainTextEdit;
class QToolButton;
class QToolBar;
class QPushButton;
class QLineEdit;
class QComboBox;
class ElideLabel;

class LogViewFontManager;
class UsiCommLogModel;

/**
 * @brief USI通信ログの表示・操作を担うパネル
 *
 * EngineAnalysisTab から USI通信ログ関連の責務を分離したクラス。
 * ツールバー（フォントサイズ・エンジン名表示）、コマンド入力バー、
 * ログ表示エリアを持つ。
 */
class UsiLogPanel : public QObject
{
    Q_OBJECT

public:
    explicit UsiLogPanel(QObject* parent = nullptr);
    ~UsiLogPanel() override;

    /// USI通信ログページのUIを構築し返す
    QWidget* buildUi(QWidget* parent);

    /// USI通信ログモデルを設定（エンジン名・ログ表示を接続）
    void setModels(UsiCommLogModel* log1, UsiCommLogModel* log2);

    /// 色付きログ行を追記
    void appendColoredLog(const QString& logLine, const QColor& lineColor);

    /// ステータスメッセージを追記（グレー表示）
    void appendStatus(const QString& message);

    /// ログをクリア
    void clear();

signals:
    /// USIコマンド送信シグナル（target: 0=E1, 1=E2, 2=両方）
    void usiCommandRequested(int target, const QString& command);

private slots:
    void onFontIncrease();
    void onFontDecrease();
    void onCommandEntered();
    void onEngine1NameChanged();
    void onEngine2NameChanged();
    void onLog1Changed();
    void onLog2Changed();
    void onWrapToggled(bool checked);
    void scrollToLatest();
    void copyAll();
    void updateLogActions();
    void updateSendButton();
    void onTargetChanged(int index);

private:
    void buildToolbar();
    void buildCommandBar();
    void initFontManager();

    QWidget* m_container = nullptr;
    QToolBar* m_toolbar = nullptr;
    QPlainTextEdit* m_logView = nullptr;
    ElideLabel* m_engine1Label = nullptr;
    ElideLabel* m_engine2Label = nullptr;
    QAction* m_wrapAction = nullptr;
    QAction* m_latestAction = nullptr;
    QAction* m_copyAction = nullptr;
    QAction* m_clearAction = nullptr;
    QToolButton* m_btnFontIncrease = nullptr;
    QToolButton* m_btnFontDecrease = nullptr;
    int m_fontSize = 10;
    std::unique_ptr<LogViewFontManager> m_fontManager;

    QWidget* m_commandBar = nullptr;
    QComboBox* m_targetCombo = nullptr;
    QLineEdit* m_commandInput = nullptr;
    QPushButton* m_sendButton = nullptr;

    UsiCommLogModel* m_log1 = nullptr;
    UsiCommLogModel* m_log2 = nullptr;
};

#endif // USILOGPANEL_H
