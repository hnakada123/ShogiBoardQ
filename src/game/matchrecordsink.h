#ifndef MATCHRECORDSINK_H
#define MATCHRECORDSINK_H

/// @file matchrecordsink.h
/// @brief 対局の棋譜の受け手（司令塔が確定した行を書き込む先）

#include "recordentry.h"

/**
 * @brief 対局の棋譜の受け手
 *
 * 司令塔（MatchCoordinator）は一手・終局を確定するたびに、棋譜の行をここへ書き込む。
 * 受け手は分岐ツリー（ライブ対局のセッション）に追記する。
 */
class MatchRecordSink {
public:
    /// 行を追記できるか（対局中のセッションが終わっていれば false。司令塔はその手を指さない）
    virtual bool canAppendRecord() const = 0;
    virtual void appendRecord(const RecordEntry& entry) = 0;
    /// 末尾の count 行を取り消せるか
    virtual bool canUndoRecords(int count) const = 0;
    virtual void undoRecords(int count) = 0;

protected:
    ~MatchRecordSink() = default;
};

#endif // MATCHRECORDSINK_H
