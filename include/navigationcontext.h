#ifndef NAVIGATIONCONTEXT_H
#define NAVIGATIONCONTEXT_H

#include <QtGlobal>

class INavigationContext {
public:
    virtual ~INavigationContext() = default;

    // 状態取得
    virtual bool hasResolvedRows() const = 0;
    virtual int  resolvedRowCount() const = 0;
    virtual int  activeResolvedRow() const = 0;
    virtual int  maxPlyAtRow(int row) const = 0; // その行の最終手数 (= r.disp.size())

    // 現在手数（m_activePly 優先、なければ棋譜ビューの選択など）
    virtual int  currentPly() const = 0;

    // 反映（盤面・棋譜欄・分岐・矢印まで一括同期）
    virtual void applySelect(int row, int ply) = 0;
};

#endif // NAVIGATIONCONTEXT_H
