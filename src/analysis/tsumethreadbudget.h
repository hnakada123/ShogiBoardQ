#ifndef TSUMETHREADBUDGET_H
#define TSUMETHREADBUDGET_H

#include <QThread>
#include <algorithm>
#include <atomic>

/// 内蔵探索の追加スレッドを共有する。呼出元の QtConcurrent ワーカーも1本に数える。
/// 複数のダイアログで同時探索しても、それぞれが全コアを使用しないようにする。
class TsumeThreadBudget
{
public:
    explicit TsumeThreadBudget(int maximum = 4)
    {
        const int limit = std::clamp(QThread::idealThreadCount() / 2, 1, 4) - 1;
        int used = s_extraThreads.load();
        do {
            m_extra = std::clamp(limit - used, 0, std::max(0, maximum - 1));
        } while (!s_extraThreads.compare_exchange_weak(used, used + m_extra));
    }

    ~TsumeThreadBudget() { s_extraThreads.fetch_sub(m_extra); }
    TsumeThreadBudget(const TsumeThreadBudget&) = delete;
    TsumeThreadBudget& operator=(const TsumeThreadBudget&) = delete;

    int threads() const { return 1 + m_extra; }

private:
    inline static std::atomic_int s_extraThreads{0};
    int m_extra = 0;
};

#endif // TSUMETHREADBUDGET_H
