/// @file tsumesearchflowcontroller.cpp
/// @brief 詰み探索フローコントローラクラスの実装

#include "tsumesearchflowcontroller.h"

#include "logcategories.h"
#include "analysisflowcontroller.h"
#include "tsumeshogisearchdialog.h"
#include "analysissession.h"
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
    if (!d.session) {
        if (d.onError) d.onError(QStringLiteral("内部エラー: 詰み探索のセッションが未初期化です。"));
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
    AnalysisSession* session = d.session;
    QTimer::singleShot(0, session, [session, enginePath, engineName, pos, byoyomiMs]() {
        AnalysisOptions opt;
        opt.mode        = AnalysisOptions::Mode::Tsume;
        opt.enginePath  = enginePath;
        opt.engineName  = engineName;
        opt.positionStr = pos;
        opt.byoyomiMs   = byoyomiMs;
        session->start(opt);
    });
    return true;
}

QString TsumeSearchFlowController::buildPositionForMate(const Deps& d) const
{
    const int ply = d.line.currentPly;
    // 途中局面は開始局面からの指し手で送る（エンジンに千日手の判断材料を渡すため）
    if (!d.line.isEmpty() && ply > 0) {
        return d.line.positionCommand(ply);
    }
    // 開始局面（詰将棋の問題など）は、玉の有無から攻方を決めて手番を合わせる
    return TsumePositionUtil::buildPositionForMate(d.line.isEmpty() ? d.fallbackSfen : d.line.startSfen);
}

