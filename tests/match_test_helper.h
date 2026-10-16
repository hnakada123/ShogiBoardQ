#ifndef MATCH_TEST_HELPER_H
#define MATCH_TEST_HELPER_H

/// @file match_test_helper.h
/// @brief 対局の司令塔（MatchCoordinator）を本物の盤・時計・エンジンの枠で動かすテスト用の組み立て

#include <QCoreApplication>
#include <QList>
#include <QSignalSpy>
#include <memory>

#include "appactivitystate.h"
#include "enginelifecyclemanager.h"
#include "matchcoordinator.h"
#include "matchrecordsink.h"
#include "usimatchhandler.h"
#include "sfenreplay.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"

/// エンジンが指した手（指した側・考えた時間・評価値）
inline UsiMatchMove engineMove(const QString& usi, Turn side, qint64 elapsedMs = 1000, const UsiScore& score = {})
{
    UsiMatchMove move;
    move.side = side;
    move.move = *UsiMove::parse(usi);
    move.elapsedMs = elapsedMs;
    move.score = score;
    return move;
}

/// 司令塔が書いた棋譜の行を覚える受け手
class RecordingSink final : public MatchRecordSink {
public:
    bool canAppendRecord() const override { return acceptRecords; }
    void appendRecord(const RecordEntry& entry) override { records.append(entry); }
    bool canUndoRecords(int count) const override { return records.size() >= count; }
    void undoRecords(int count) override { records.resize(records.size() - count); }

    QList<RecordEntry> records;
    bool acceptRecords = true;
};

/// 対局の司令塔とその部品（盤・時計・動作状態・エンジンの枠・棋譜の受け手）
struct MatchHarness {
    ShogiGameController gc;
    ShogiClock clock;
    AppActivityState activity;
    EngineLifecycleManager engines{&clock};
    RecordingSink sink;
    std::unique_ptr<MatchCoordinator> mc;
    std::unique_ptr<QSignalSpy> messages;  ///< 終局・中断の知らせ（gameOverMessage）

    MatchHarness()
    {
        gc.newGame(SfenUtils::hirateSfen());
        MatchCoordinator::Deps deps;
        deps.gc = &gc;
        deps.clock = &clock;
        deps.activity = &activity;
        deps.engines = &engines;
        mc = std::make_unique<MatchCoordinator>(deps);
        mc->setRecordSink(&sink);
        messages = std::make_unique<QSignalSpy>(mc.get(), &MatchCoordinator::gameOverMessage);
    }

    ~MatchHarness()
    {
        messages.reset();
        mc.reset();
        engines.destroyEngines();
    }

    MatchHarness(const MatchHarness&) = delete;
    MatchHarness& operator=(const MatchHarness&) = delete;

    /// 対局を準備して始める（時計を動かし、初手がエンジンなら思考させる）
    void start(MatchCoordinator::StartOptions opt)
    {
        if (opt.sfenStart.isEmpty()) opt.sfenStart = SfenUtils::hirateSfen();
        mc->start(opt);
        mc->beginPlay();
    }

    void startHumanVsHuman(const QString& sfen = SfenUtils::hirateSfen())
    {
        MatchCoordinator::StartOptions opt;
        opt.sfenStart = sfen;
        start(opt);
    }

    /// 人間の手を盤に指して司令塔に渡す（指せなければ false）
    bool playHuman(const QString& usi)
    {
        const auto move = UsiMove::parse(usi);
        if (!move) return false;
        const std::optional<RecordEntry> played = gc.applyMove(*move, mc->lastMoveDestination());
        if (!played) return false;
        mc->onHumanMove(*played);
        return true;
    }

    int messageCount() const { return static_cast<int>(messages->count()); }
    QString lastMessageTitle() const { return messages->isEmpty() ? QString() : messages->last().at(0).toString(); }
    QString lastMessageText() const { return messages->isEmpty() ? QString() : messages->last().at(1).toString(); }

    static QString mockEnginePath()
    {
        QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
#ifdef Q_OS_WIN
        path += QStringLiteral(".exe");
#endif
        return path;
    }
};

#endif // MATCH_TEST_HELPER_H
