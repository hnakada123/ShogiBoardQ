#ifndef AUTOMATIONINTERACTIONS_H
#define AUTOMATIONINTERACTIONS_H

/// @file automationinteractions.h
/// @brief 自動化 API の、応答の後に行う UI 操作（予約・実行時の再検査・結果の記録）

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <functional>

/**
 * @brief 応答を返した後にメインスレッドで実行する遅延呼び出し
 *
 * モーダルダイアログを開く動作や終了処理を、JSON-RPC の応答送信後に実行するために使う。
 * `run()` の後に自身を削除する。UI 操作は AutomationInteractions::queue で予約し、
 * これを直接使うのは app.quit だけ。
 */
class AutomationDeferredCall : public QObject
{
    Q_OBJECT
public:
    explicit AutomationDeferredCall(std::function<void()> callback, QObject* parent = nullptr);
    /// 次のイベントループ反復で callback を実行するよう予約する
    static void schedule(std::function<void()> callback, QObject* parent);

public slots:
    void run();

private:
    std::function<void()> m_callback;
};

/**
 * @brief 応答の後に行う UI 操作1つ
 *
 * モーダルダイアログを開く操作でも応答を返せるよう、操作は応答の後のイベントループで行う。
 * check は予約の時点（失敗すればその場でエラーの応答）と、実行の直前（応答の後に対象が隠れた・
 * 別のダイアログに遮られた、など）の2回走る。メソッドは同じ検査を予約の前に書かない。
 */
struct QueuedInteraction {
    QPointer<QObject> target;     ///< 操作の対象。実行前に消えたら取り消す
    std::function<void()> check;  ///< 予約の時点と実行の直前の検査（AutomationError を投げて止める。省略可）
    std::function<void()> perform; ///< 操作。AutomationError を投げれば失敗として記録する
};

/**
 * @brief 予約した UI 操作と、その結果の記録
 *
 * 結果は `ui.interaction` で引ける: state は pending（未実行）・running（実行中。開いたモーダルダイアログが
 * 閉じるまで）・done・failed（error に分類・内容）・cancelled（実行前に対象が消えた）。
 * 実行時の失敗は握りつぶさず、ここに記録する。記録は新しいものから kHistoryLimit 件まで残す。
 */
namespace AutomationInteractions {

inline constexpr int kHistoryLimit = 256;

/// interaction の check を走らせてから予約し、response に "queued": true と "interaction"（番号）を足して返す。
/// check が失敗すれば予約せずに AutomationError を投げる
QJsonObject queue(QueuedInteraction interaction, QJsonObject response);

/// 番号の操作の状態 {interaction, state, error?}。知らない番号・古くて消えた番号は AutomationError（NotFound）
QJsonObject status(int id);

} // namespace AutomationInteractions

#endif // AUTOMATIONINTERACTIONS_H
