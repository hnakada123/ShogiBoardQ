#ifndef THREADTYPES_H
#define THREADTYPES_H

#include <QString>
#include <QList>
#include <atomic>

// スレッド間受け渡し用の共通型

// 非同期ジョブの世代番号（stale結果破棄用）
using JobGeneration = quint64;

// 非同期ジョブのキャンセルフラグ
// ワーカースレッドで定期的にチェックし、trueならジョブを中断する
using CancelFlag = std::shared_ptr<std::atomic_bool>;

inline CancelFlag makeCancelFlag()
{
    return std::make_shared<std::atomic_bool>(false);
}

#endif // THREADTYPES_H
