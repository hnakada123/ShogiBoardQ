#ifndef AUTOMATIONCOMMANDS_H
#define AUTOMATIONCOMMANDS_H

/// @file automationcommands.h
/// @brief 自動化 API の JSON-RPC メソッド群の登録

#include <QObject>
#include <functional>

#include "automationcontext.h"

class AutomationDispatcher;

/**
 * @brief 応答を返した後にメインスレッドで実行する遅延呼び出し
 *
 * モーダルダイアログを開く動作や終了処理を、JSON-RPC の応答送信後に実行するために使う。
 * `run()` の後に自身を削除する。
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

/// メソッド群の登録関数。実装は automationcommands_*.cpp に分かれる
namespace AutomationCommands {

void registerAll(AutomationDispatcher& dispatcher, const AutomationContext& context);

void registerAppCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);      ///< app.*
void registerPositionCommands(AutomationDispatcher& dispatcher, const AutomationContext& context); ///< position.*
void registerKifuCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);     ///< kifu.*
void registerUiCommands(AutomationDispatcher& dispatcher, const AutomationContext& context);       ///< action.* / screenshot.* / dialog.* / widget.*

/// 現在表示中の局面の SFEN（SFEN 記録があればその値、無ければ盤から組み立てる）
QString currentSfen(const AutomationContext& context);

} // namespace AutomationCommands

#endif // AUTOMATIONCOMMANDS_H
