#ifndef USICOMMLOGMODEL_H
#define USICOMMLOGMODEL_H

/// @file usicommlogmodel.h
/// @brief エンジン1台の情報欄（エンジン名・探索状況）と通信ログの行

#include <QObject>
#include <QString>

#include "enginesearchstatus.h"

/**
 * @brief エンジン1台の情報欄の値と、通信ログの行の知らせ
 *
 * - 情報欄: エンジン名（engineNameChanged）と、探索状況・予想手（statusChanged）。探索状況は数値で持ち、
 *   書式は表示する側（EngineInfoWidget）が決める
 * - 通信ログ: 行を溜めずに logLineAppended で知らせる（UsiLogPanel が表示する）
 */
class UsiCommLogModel : public QObject
{
    Q_OBJECT

public:
    explicit UsiCommLogModel(QObject* parent = nullptr);

    QString engineName() const { return m_engineName; }
    /// 探索状況（info に含まれた項目を重ねたもの）
    const EngineSearchStatus& status() const { return m_status; }
    /// 予想手（相手の手の予想。表記済み）
    QString predictedMove() const { return m_predictedMove; }

    void setEngineName(const QString& engineName);
    /// info 1行の探索状況を重ねる（含まれなかった項目は前の値を残す）
    void updateStatus(const EngineSearchStatus& status);
    /// 探索状況を置き換える
    void setStatus(const EngineSearchStatus& status);
    void setPredictedMove(const QString& move);
    /// 通信ログの行を知らせる
    void appendLogLine(const QString& line);
    /// エンジン名・探索状況・予想手を消す
    void clear();

signals:
    void engineNameChanged();
    /// 探索状況・予想手が変わった
    void statusChanged();
    /// 通信ログの行が届いた（→ UsiLogPanel）
    void logLineAppended(const QString& line);

private:
    QString m_engineName;
    EngineSearchStatus m_status;
    QString m_predictedMove;
};

#endif // USICOMMLOGMODEL_H
