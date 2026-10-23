#ifndef THREADTYPES_H
#define THREADTYPES_H

#include <atomic>
#include <memory>

// スレッド間受け渡し用の共通型

// 非同期ジョブのキャンセルフラグ
// ワーカースレッドで定期的にチェックし、trueならジョブを中断する
using CancelFlag = std::shared_ptr<std::atomic_bool>;

inline CancelFlag makeCancelFlag()
{
    return std::make_shared<std::atomic_bool>(false);
}

#endif // THREADTYPES_H
