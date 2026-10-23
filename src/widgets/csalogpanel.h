#ifndef CSALOGPANEL_H
#define CSALOGPANEL_H

/// @file csalogpanel.h
/// @brief CSA通信ログパネルクラスの定義

#include "logpanel.h"

class QLabel;

/**
 * @brief CSA通信ログの表示・操作を担うパネル
 *
 * CSA通信ログドック（と通信対局の待機ダイアログのログ）の中身（共通の部分は LogPanel）。
 * 操作列に接続状態を出し、コマンドはサーバーに接続している間だけ送れる。
 */
class CsaLogPanel : public LogPanel
{
    Q_OBJECT

public:
    explicit CsaLogPanel(QWidget* parent = nullptr);
    ~CsaLogPanel() override;

    /// ログ行を追記
    void append(const QString& line) { appendLine(line); }

    void setConnected(bool connected);

signals:
    /// CSAコマンド送信シグナル
    void csaRawCommandRequested(const QString& command);

protected:
    bool canSend() const override { return m_connected; }

private:
    QLabel* m_connectionStatus = nullptr;
    bool m_connected = false;
};

#endif // CSALOGPANEL_H
