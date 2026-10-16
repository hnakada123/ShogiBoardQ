#ifndef USILOGPANEL_H
#define USILOGPANEL_H

/// @file usilogpanel.h
/// @brief USI通信ログパネルクラスの定義

#include <QWidget>
#include <QColor>

#include "fontsizehelper.h"

class QAction;
class QPlainTextEdit;
class QToolButton;
class QToolBar;
class QPushButton;
class QLineEdit;
class QComboBox;
class ElideLabel;

class UsiCommLogModel;

/**
 * @brief USI通信ログの表示・操作を担うパネル
 *
 * USI通信ログドックの中身。部品はコンストラクタで作る。ツールバー（文字の大きさ・エンジン名表示）、
 * ログ表示エリア、コマンド入力バーを持つ。
 */
class UsiLogPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UsiLogPanel(QWidget* parent = nullptr);
    ~UsiLogPanel() override;

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
    void onCommandEntered();
    void onEngine1NameChanged();
    void onEngine2NameChanged();
    void onLog1Line(const QString& line);
    void onLog2Line(const QString& line);
    void onWrapToggled(bool checked);
    void scrollToLatest();
    void copyAll();
    void updateLogActions();
    void updateSendButton();
    void onTargetChanged(int index);

private:
    void buildToolbar();
    void buildCommandBar();
    void applyFontSize();

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
    FontSizeHelper m_fontSize;   ///< ログ・コマンド入力の文字の大きさ（変更のたびに保存する）

    QWidget* m_commandBar = nullptr;
    QComboBox* m_targetCombo = nullptr;
    QLineEdit* m_commandInput = nullptr;
    QPushButton* m_sendButton = nullptr;

    UsiCommLogModel* m_log1 = nullptr;
    UsiCommLogModel* m_log2 = nullptr;
};

#endif // USILOGPANEL_H
