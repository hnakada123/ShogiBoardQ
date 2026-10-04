/// @file tsumesearchflowcontroller.cpp
/// @brief 詰み探索フローコントローラクラスの実装

#include "tsumesearchflowcontroller.h"

#include "logcategories.h"
#include "analysisflowcontroller.h"
#include "tsumeshogisearchdialog.h"
#include "matchcoordinator.h"
#include "tsumepositionutil.h"

#include <QObject>
#include <QDialog>
#include <QTimer>
#include <QtGlobal>

TsumeSearchFlowController::TsumeSearchFlowController(QObject* parent)
    : QObject(parent)
{
}

bool TsumeSearchFlowController::runWithDialog(const Deps& d, QWidget* parent)
{
    if (!d.match) {
        if (d.onError) d.onError(QStringLiteral("内部エラー: MatchCoordinator が未初期化です。"));
        return false;
    }
    const QString pos = buildPositionForMate(d);
    if (pos.isEmpty()) {
        if (d.onError) d.onError(QStringLiteral("詰み探索用の局面（SFEN）が取得できません。棋譜を読み込むか局面を指定してください。"));
        return false;
    }

    // 選択結果を取り出してダイアログを閉じてから、非同期探索を開始する。
    QString enginePath;
    QString engineName;
    int byoyomiMs = 0;  // 0 は無制限

    {
        TsumeShogiSearchDialog dlg(parent);
        if (dlg.exec() != QDialog::Accepted) return false;

        const auto& engines = dlg.engineList();
        const int idx = dlg.engineNumber();

        if (engines.isEmpty() || idx < 0 || idx >= engines.size()) {
            if (d.onError) d.onError(QStringLiteral("詰み探索エンジンの選択が不正です。"));
            return false;
        }

        const auto& engine = engines.at(idx);
        enginePath = engine.path;
        engineName = engine.name;

        if (!dlg.unlimitedTimeFlag()) {
            byoyomiMs = dlg.byoyomiSec() * 1000;  // 秒 → ms
        }
    }
    // ここで dlg は完全に破棄されている。

    // ダイアログの終了処理を終えてから、次のイベントループで開始する。
    MatchCoordinator* match = d.match;
    QTimer::singleShot(0, match, [match, enginePath, engineName, pos, byoyomiMs]() {
        MatchCoordinator::AnalysisOptions opt;
        opt.enginePath  = enginePath;
        opt.engineName  = engineName;
        opt.positionStr = pos;
        opt.byoyomiMs   = byoyomiMs;
        opt.mode        = PlayMode::TsumiSearchMode;
        match->startAnalysis(opt);
    });
    return true;
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

