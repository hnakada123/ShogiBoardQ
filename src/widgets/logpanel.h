#ifndef LOGPANEL_H
#define LOGPANEL_H

/// @file logpanel.h
/// @brief 通信ログのパネルの共通部分（USI・CSA）

#include <QColor>
#include <QString>
#include <QWidget>

#include "fontsizehelper.h"
#include "setting.h"

class QAction;
class QHBoxLayout;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolBar;
class QToolButton;

/**
 * @brief 通信ログのパネル（USI 通信ログ・CSA 通信ログで共通）
 *
 * 等幅のログ（5000行まで）・文字の大きさ（A-/A+）・折り返し・最新へ・全てコピー・消去の操作列と、
 * コマンドの入力欄を持つ。追記は閲覧位置と選択範囲を保ち、末尾を見ているときだけ新着に追従する。
 * 派生クラスは操作列に部品を足し（addToolbarWidget）、コマンドの欄の前に部品を置き（commandLayout）、
 * 送ってよいか（canSend）と入力したコマンドの行き先（commandEntered）を決める。
 */
class LogPanel : public QWidget
{
    Q_OBJECT

public:
    /// 部品の objectName（自動化 API・テストが探す名前。パネルごとに決める）
    struct Names {
        QString view;
        QString toolbar;
        QString wrap;
        QString latest;
        QString copy;
        QString clear;
        QString commandInput;
        QString sendButton;
    };

    LogPanel(const Setting<int>& fontSize, const Setting<bool>& wrapLines, const Names& names, QWidget* parent);
    ~LogPanel() override;

    /// 1行足す（色を指定しなければ文字の既定の色）
    void appendLine(const QString& line, const QColor& color = QColor());
    /// ログを消す
    void clear();

signals:
    /// コマンドの欄で送った（Enter・送信ボタン。前後の空白は除く）
    void commandEntered(const QString& command);

protected:
    QPlainTextEdit* logView() const { return m_logView; }
    QLineEdit* commandInput() const { return m_commandInput; }
    QPushButton* sendButton() const { return m_sendButton; }
    /// 操作列の文字の大きさの後ろ（右寄せの操作の前）に部品を足す
    void addToolbarWidget(QWidget* widget);
    /// コマンドの欄の並び（入力欄の前に部品を差し込める）
    QHBoxLayout* commandLayout() const { return m_commandLayout; }
    /// コマンドを送ってよいか（CSA は接続中だけ）
    virtual bool canSend() const { return true; }

protected slots:
    void updateSendButton();

private slots:
    void onCommandEntered();
    void onWrapToggled(bool checked);
    void scrollToLatest();
    void copyAll();
    void updateLogActions();
    void applyFontSize();

private:
    void buildToolbar(const Names& names);
    void buildCommandBar(const Names& names);

    const Setting<bool>& m_wrapLines;
    FontSizeHelper m_fontSize;   ///< ログ・コマンドの欄の文字の大きさ（変更のたびに保存する）
    QToolBar* m_toolbar = nullptr;
    QAction* m_spacerAction = nullptr;   ///< 派生クラスの部品はこの前に置く
    QPlainTextEdit* m_logView = nullptr;
    QAction* m_wrapAction = nullptr;
    QAction* m_latestAction = nullptr;
    QAction* m_copyAction = nullptr;
    QAction* m_clearAction = nullptr;
    QWidget* m_commandBar = nullptr;
    QHBoxLayout* m_commandLayout = nullptr;
    QLineEdit* m_commandInput = nullptr;
    QPushButton* m_sendButton = nullptr;
};

#endif // LOGPANEL_H
