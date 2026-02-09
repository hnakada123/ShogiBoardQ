/// @file tsumesearchflowcontroller.cpp
/// @brief 詰み探索フローコントローラクラスの実装

#include "tsumesearchflowcontroller.h"

#include "analysisflowcontroller.h"
#include "tsumeshogisearchdialog.h"
#include "matchcoordinator.h"
#include "tsumepositionutil.h"

#include <QObject>
#include <QDialog>
#include <QtGlobal>

TsumeSearchFlowController::TsumeSearchFlowController(QObject* parent)
    : QObject(parent)
{
}

void TsumeSearchFlowController::runWithDialog(const Deps& d, QWidget* parent)
{
    if (!d.match) {
        if (d.onError) d.onError(QStringLiteral("内部エラー: MatchCoordinator が未初期化です。"));
        return;
    }
    const QString pos = buildPositionForMate(d);
    if (pos.isEmpty()) {
        if (d.onError) d.onError(QStringLiteral("詰み探索用の局面（SFEN）が取得できません。棋譜を読み込むか局面を指定してください。"));
        return;
    }

    TsumeShogiSearchDialog dlg(parent);
    if (dlg.exec() != QDialog::Accepted) return;

    const QList<ConsiderationDialog::Engine>& engines = dlg.getEngineList();
    const int idx = dlg.getEngineNumber();

    if (engines.isEmpty() || idx < 0 || idx >= engines.size()) {
        if (d.onError) d.onError(QStringLiteral("詰み探索エンジンの選択が不正です。"));
        return;
    }

    const auto& engine = engines.at(idx);

    int byoyomiMs = 0;  // 0 は無制限
    if (!dlg.unlimitedTimeFlag()) {
        byoyomiMs = dlg.getByoyomiSec() * 1000;  // 秒 → ms
    }

    startAnalysis(d.match, engine.path, engine.name, pos, byoyomiMs);
}

QString TsumeSearchFlowController::buildPositionForMate(const Deps& d) const
{
    qCDebug(lcAnalysis).noquote() << "buildPositionForMate:"
                                  << "usiMoves=" << (d.usiMoves ? QString::number(d.usiMoves->size()) : "null")
                                  << "startPositionCmd=" << d.startPositionCmd
                                  << "currentMoveIndex=" << d.currentMoveIndex;

    // USI形式の指し手リストが利用可能な場合はmoves形式を使用
    if (d.usiMoves && !d.usiMoves->isEmpty() && !d.startPositionCmd.isEmpty()) {
        const QString result = TsumePositionUtil::buildPositionWithMoves(
            d.usiMoves, d.startPositionCmd, qMax(0, d.currentMoveIndex));
        qCDebug(lcAnalysis).noquote() << "using moves format:" << result;
        return result;
    }
    // フォールバック: SFEN形式
    qCDebug(lcAnalysis).noquote() << "fallback to SFEN format";
    return TsumePositionUtil::buildPositionForMate(
        d.sfenRecord, d.startSfenStr, d.positionStrList, qMax(0, d.currentMoveIndex));
}

void TsumeSearchFlowController::startAnalysis(MatchCoordinator* match,
                                               const QString& enginePath,
                                               const QString& engineName,
                                               const QString& positionStr,
                                               int byoyomiMs)
{
    MatchCoordinator::AnalysisOptions opt;
    opt.enginePath  = enginePath;
    opt.engineName  = engineName;
    opt.positionStr = positionStr;
    opt.byoyomiMs   = byoyomiMs;
    opt.mode        = PlayMode::TsumiSearchMode;

    match->startAnalysis(opt);
}
