#ifndef MATCHSESSIONSTATE_H
#define MATCHSESSIONSTATE_H

#include <optional>

#include "matchtypes.h"
#include "startoptions.h"

/// 対局の条件と終局の所有者。変更は対局上の操作として行い、内部フィールドを公開しない。
class MatchSessionState
{
public:
    /// 対局を始める。開始の手順（options.recordLine）は対局の手順として MatchCoordinator が持つので、ここには残さない
    void begin(const MatchStartOptions& options)
    {
        m_options = options;
        m_options.recordLine = RecordLine{};
        m_started = true;
        m_end.reset();
    }
    /// 一度でも対局を始めたか
    bool started() const { return m_started; }
    /// 対局の条件（対局者・エンジン・開始局面・最大手数・自動保存・定跡）
    const MatchStartOptions& options() const { return m_options; }
    const MatchPlayers& players() const { return m_options.players; }
    int maxMoves() const { return m_options.maxMoves; }
    /// 終局の情報（終局していなければ nullopt）
    const std::optional<MatchGameEndInfo>& end() const { return m_end; }
    void clearEnd() { m_end.reset(); }
    /// 終局する（終局済みなら何もせず false）
    bool finish(const MatchGameEndInfo& info)
    {
        if (m_end) return false;
        m_end = info;
        return true;
    }

private:
    MatchStartOptions m_options;
    bool m_started = false;
    std::optional<MatchGameEndInfo> m_end;
};

#endif
