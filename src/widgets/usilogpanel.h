#ifndef USILOGPANEL_H
#define USILOGPANEL_H

/// @file usilogpanel.h
/// @brief USI通信ログパネルクラスの定義

#include <QColor>

#include "logpanel.h"

class QComboBox;
class ElideLabel;
class UsiCommLogModel;

/**
 * @brief USI通信ログの表示・操作を担うパネル
 *
 * USI通信ログドックの中身（共通の部分は LogPanel）。操作列にエンジン名を出し、
 * コマンドの欄の前に送信先（E1・E2・両方）を置く。ログの行はエンジンごとの色で出す。
 */
class UsiLogPanel : public LogPanel
{
    Q_OBJECT

public:
    explicit UsiLogPanel(QWidget* parent = nullptr);
    ~UsiLogPanel() override;

    /// USI通信ログモデルを設定（エンジン名・ログ表示を接続）
    void setModels(UsiCommLogModel* log1, UsiCommLogModel* log2);

    /// 色付きログ行を追記
    void appendColoredLog(const QString& logLine, const QColor& lineColor) { appendLine(logLine, lineColor); }

    /// ステータスメッセージを追記（グレー表示）
    void appendStatus(const QString& message);

signals:
    /// USIコマンド送信シグナル（target: 0=E1, 1=E2, 2=両方）
    void usiCommandRequested(int target, const QString& command);

private slots:
    void onEngine1NameChanged();
    void onEngine2NameChanged();
    void onLog1Line(const QString& line);
    void onLog2Line(const QString& line);
    void onTargetChanged(int index);
    void sendToTarget(const QString& command);

private:
    ElideLabel* m_engine1Label = nullptr;
    ElideLabel* m_engine2Label = nullptr;
    QComboBox* m_targetCombo = nullptr;
    UsiCommLogModel* m_log1 = nullptr;
    UsiCommLogModel* m_log2 = nullptr;
};

#endif // USILOGPANEL_H
