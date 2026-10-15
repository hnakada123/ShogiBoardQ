#ifndef MATCHSESSIONSTATE_H
#define MATCHSESSIONSTATE_H

#include "matchtypes.h"
#include "startoptions.h"
#include <array>

/// 対局の設定と終了状態の所有者。変更は対局上の操作として行い、内部フィールドを公開しない。
class MatchSessionState
{
public:
    void begin(const MatchStartOptions& options)
    {
        m_options = options;
        m_mode = options.mode;
        m_over = {};
        clearPonder();
    }
    const MatchStartOptions& options() const { return m_options; }
    PlayMode playMode() const { return m_mode; }
    void setPlayMode(PlayMode mode) { m_mode = mode; }
    int maxMoves() const { return m_options.maxMoves; }
    const MatchGameOverState& gameOver() const { return m_over; }
    void clearGameOver() { m_over = {}; }
    bool finish(const MatchGameEndInfo& info)
    {
        if (m_over.isOver) return false;
        m_over.isOver = true;
        m_over.hasLast = true;
        m_over.lastInfo = info;
        m_over.when = QDateTime::currentDateTime();
        return true;
    }
    bool markTerminalRecorded()
    {
        if (!m_over.isOver || m_over.moveAppended) return false;
        m_over.moveAppended = true;
        return true;
    }
    const QString& ponder(int slot) const { return m_ponder.at(slot == 2 ? 1 : 0); }
    void setPonder(int slot, const QString& position) { m_ponder.at(slot == 2 ? 1 : 0) = position; }
    void clearPonder() { m_ponder = {}; }

private:
    MatchStartOptions m_options;
    PlayMode m_mode = PlayMode::NotStarted;
    MatchGameOverState m_over;
    std::array<QString, 2> m_ponder;
};

#endif
