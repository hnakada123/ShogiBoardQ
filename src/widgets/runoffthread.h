#ifndef RUNOFFTHREAD_H
#define RUNOFFTHREAD_H

/// @file runoffthread.h
/// @brief 重い処理を別スレッドで行い、終わるまで待つ runOffThread

#include <QEventLoop>
#include <QFutureWatcher>
#include <QtConcurrentRun>

#include <type_traits>
#include <utility>

/**
 * @brief 関数を別スレッドで実行し、終わるまで待って結果を返す（画面の描き直しは止めない）
 *
 * 待つ間はイベントループを回す。利用者の入力は待たせるので、操作の再入は起きない。
 * function は別スレッドで動くので、UI と共有する状態に触らない。
 */
template <typename Function>
auto runOffThread(Function function)
{
    using Result = std::invoke_result_t<Function>;
    QFutureWatcher<Result> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<Result>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(QtConcurrent::run(std::move(function)));
    if (!watcher.isFinished()) loop.exec(QEventLoop::ExcludeUserInputEvents);
    return watcher.result();
}

#endif // RUNOFFTHREAD_H
