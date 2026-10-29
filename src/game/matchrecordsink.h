#ifndef MATCHRECORDSINK_H
#define MATCHRECORDSINK_H

/// @file matchrecordsink.h
/// @brief 対局の棋譜の受け手（司令塔が確定した行を書き込む先）

#include <QString>

#include "matchautosave.h"
#include "matchplayers.h"
#include "recordentry.h"
#include "timecontrol.h"

/// 司令塔が始める対局の新しい棋譜（通信対局はサーバーの知らせで始まる）
struct MatchRecordStart {
    QString startSfen;          ///< 開始局面（新しい棋譜の根）
    MatchPlayers players;       ///< 対局者
    TimeControl timeControl;    ///< 持ち時間
    MatchAutoSave autoSave;     ///< 終局した棋譜の自動保存
};

/**
 * @brief 対局の棋譜の受け手
 *
 * 司令塔（ローカル対局の MatchCoordinator・通信対局の CsaGameCoordinator）は一手・終局を確定するたびに、
 * 棋譜の行をここへ書き込み、終局の行の後で棋譜を確定する。受け手は分岐ツリー（ライブ対局のセッション）に
 * 追記する。ローカル対局の棋譜は受け手（GameSession）が対局を始めるときに作り、通信対局の棋譜は
 * 司令塔が beginRecord で作らせる。
 */
class MatchRecordSink {
public:
    /// 新しい棋譜を作り、対局者・持ち時間・開始日時を記録する（司令塔が対局を始めるとき）
    virtual void beginRecord(const MatchRecordStart& start) = 0;
    /// 行を追記できるか（対局中のセッションが終わっていれば false。司令塔はその手を指さない）
    virtual bool canAppendRecord() const = 0;
    virtual void appendRecord(const RecordEntry& entry) = 0;
    /// 末尾の count 行を取り消せるか
    virtual bool canUndoRecords(int count) const = 0;
    virtual void undoRecords(int count) = 0;
    /// 終局の行を書いた後で棋譜を確定し、終了日時を記録する（autoSave なら自動保存の指定に従って保存する）
    virtual void finishRecord(bool autoSave) = 0;

protected:
    ~MatchRecordSink() = default;
};

#endif // MATCHRECORDSINK_H
