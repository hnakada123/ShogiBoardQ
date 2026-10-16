#ifndef USIMODELBINDING_H
#define USIMODELBINDING_H

/// @file usimodelbinding.h
/// @brief エンジン1台の思考情報・通信ログを、表示のモデルへ反映する

#include <QObject>
#include <QPointer>

#include "enginesearchstatus.h"
#include "thinkinginforow.h"
#include "usimessages.h"

class ShogiEngineThinkingModel;
class Usi;
class UsiCommLogModel;

/**
 * @brief エンジン1台（Usi）の思考情報・情報欄の値・通信ログを、表示のモデルへ反映する
 *
 * エンジンは表示のモデルに触れない。このクラスがエンジンのシグナルを受けて、思考の表（先頭に足す）・
 * 検討の表（multipv の行を更新する）・情報欄と通信ログ（UsiCommLogModel）を更新する。
 * エンジンを親にして作り、エンジンと一緒に破棄される。
 */
class UsiModelBinding : public QObject
{
    Q_OBJECT

public:
    struct Models {
        UsiCommLogModel* commLog = nullptr;             ///< 情報欄と通信ログ
        ShogiEngineThinkingModel* thinking = nullptr;   ///< 思考の表
    };

    /// engine の子として作る。エンジン名を情報欄に出す
    UsiModelBinding(Usi* engine, const Models& models);

    /// 検討の表にも出す（model が nullptr なら出さない）。maxMultiPV は表の行数
    void setConsideration(ShogiEngineThinkingModel* model, int maxMultiPV);

private slots:
    void onThinkingRow(const ThinkingInfoRow& row);
    void onThinkingCleared();
    void onSearchStatus(const EngineSearchStatus& status);
    void onCommandSent(const QString& command);
    void onLineReceived(const QString& line);
    void onStderrReceived(const QString& line);
    void onBestMove(const UsiBestMove& result);

private:
    void appendLog(const QString& mark, const QString& text);

    QString m_engineTag;   ///< 通信ログの行に付ける "E1"・"E2"
    QPointer<UsiCommLogModel> m_commLog;
    QPointer<ShogiEngineThinkingModel> m_thinking;
    QPointer<ShogiEngineThinkingModel> m_consideration;
    int m_considerationMaxMultiPV = 1;
};

#endif // USIMODELBINDING_H
