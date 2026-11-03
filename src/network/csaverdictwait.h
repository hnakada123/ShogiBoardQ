#ifndef CSAVERDICTWAIT_H
#define CSAVERDICTWAIT_H

/// @file csaverdictwait.h
/// @brief 解釈できない指し手行の後、サーバーの裁定を待つ間の状態

#include <QObject>
#include <QString>

class QTimer;

/**
 * @brief 解釈できない指し手行の後の、サーバーの裁定（#ILLEGAL_MOVE と勝敗）の待ち（仕様 3.4）
 *
 * 不正な着手・不正な書式の指し手も指し手行として届き、続けて #ILLEGAL_MOVE と勝敗が送られる。
 * そこで終局させずに待ち、届かなければ待ち始めたときの理由で終局させる。待ちのまま次の指し手行が来たときも
 * 同じ理由で終局させる（take で受け取る）。ログの文と終局の手順は司令塔（CsaGameCoordinator）が持つ。
 */
class CsaVerdictWait : public QObject
{
    Q_OBJECT

public:
    /// timeoutMs 待っても裁定が届かなければ timedOut を出す
    explicit CsaVerdictWait(int timeoutMs, QObject* parent = nullptr);

    /// 裁定を待ち始める（届かなければ failureText で終局させる）
    void begin(const QString& failureText);
    /// 裁定を待っているか
    bool isPending() const { return !m_failureText.isEmpty(); }
    /// 待ちをやめ、待っていた終局の理由を返す（待っていなければ空）
    QString take();
    /// 待ちを取り消す（終局・切断）
    void clear();

signals:
    /// 裁定が届かないまま待ちの時間が過ぎた（failureText で終局させる。待ちは取り消してある）
    void timedOut(const QString& failureText);

private slots:
    void onTimeout();

private:
    QTimer* m_timer;
    QString m_failureText;   ///< 裁定が届かなかったときの終局の理由（空なら待ちなし）
};

#endif // CSAVERDICTWAIT_H
