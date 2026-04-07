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

    // ダイアログの寿命を内側スコープへ閉じ込め、startAnalysis を呼ぶ前に
    // 確実に破棄させる。初期化シーケンス（waitForUsiOk/waitForReadyOk）は
    // 内部で QCoreApplication::processEvents() を呼ぶため、ダイアログがまだ
    // スタック上に残っていると、ダイアログ閉鎖に伴う mouseRelease や
    // deferred delete などが再入的に処理され、リリースビルドではタイミング
    // 上 GUI がハングアップしてしまう。
    QString enginePath;
    QString engineName;
    int byoyomiMs = 0;  // 0 は無制限

    {
        TsumeShogiSearchDialog dlg(parent);
        if (dlg.exec() != QDialog::Accepted) return false;

        const QList<ConsiderationDialog::Engine>& engines = dlg.engineList();
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

    // startAnalysis は内部で waitForUsiOk/waitForReadyOk を呼び出し、その間
    // QCoreApplication::processEvents() で入力イベントを含む全イベントを
    // 処理する（参照: usiprotocolhandler_wait.cpp）。
    // ダイアログ閉鎖に伴うイベントが残ったまま入ると再入が発生してハング
    // するため、QTimer::singleShot(0,...) で次のイベントループ周回まで
    // 開始を遅延し、ダイアログ閉鎖イベントを完全に流し切ってから開始する。
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

