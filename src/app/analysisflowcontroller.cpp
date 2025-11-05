#include "analysisflowcontroller.h"

#include "analysiscoordinator.h"
#include "analysisresultspresenter.h"
#include "kifuanalysisdialog.h"
#include "kifuanalysislistmodel.h"
#include "engineanalysistab.h"
#include "kifudisplay.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "kifuanalysisresultsdisplay.h"

#include <limits>
#include <QString>
#include <QObject>
#include <QtGlobal>

AnalysisFlowController::AnalysisFlowController(QObject* parent)
    : QObject(parent)
{
}

void AnalysisFlowController::start(const Deps& d, KifuAnalysisDialog* dlg)
{
    if (!d.sfenRecord || d.sfenRecord->isEmpty()) {
        if (d.displayError) d.displayError(QStringLiteral("内部エラー: sfenRecord が未準備です。棋譜読み込み後に実行してください。"));
        return;
    }
    if (!d.analysisModel) {
        if (d.displayError) d.displayError(QStringLiteral("内部エラー: 解析モデルが未準備です。"));
        return;
    }
    if (!d.usi) {
        if (d.displayError) d.displayError(QStringLiteral("内部エラー: Usi インスタンスが未初期化です。"));
        return;
    }
    if (!dlg) return;

    // Cache deps
    m_sfenRecord    = d.sfenRecord;
    m_moveRecords   = d.moveRecords;
    m_analysisModel = d.analysisModel;
    m_analysisTab   = d.analysisTab;
    m_usi           = d.usi;
    m_logModel      = d.logModel;
    m_activePly     = d.activePly;
    m_err           = d.displayError;
    m_prevEvalCp    = 0; // 差分用の前回値をリセット

    // Coordinator（初回のみ）
    if (!m_coord) {
        AnalysisCoordinator::Deps cd;
        cd.sfenRecord = m_sfenRecord;       // ★ 解析対象のSFEN列
        m_coord = new AnalysisCoordinator(cd, this);

        // (A) AC → エンジンへ USI 文字列を橋渡し
        QObject::connect(
            m_coord, &AnalysisCoordinator::requestSendUsiCommand,
            m_usi,   &Usi::sendRaw,
            Qt::UniqueConnection
            );

        // (B) エンジン標準出力（info/bestmove）→ AC へ橋渡し（既存のログモデル経由）
        if (m_logModel) {
            QObject::connect(
                m_logModel, &UsiCommLogModel::usiCommLogChanged,
                this,       &AnalysisFlowController::onUsiCommLogChanged_,
                Qt::UniqueConnection
                );
        }

        // (C) 進捗を結果モデルへ投入（差分は直前値から算出）
        QObject::connect(
            m_coord, &AnalysisCoordinator::analysisProgress,
            this,    &AnalysisFlowController::onAnalysisProgress_,
            Qt::UniqueConnection
            );
    }

    // 結果ビュー（Presenter）— 既存APIは showWithModel(...)
    if (!m_presenter) {
        m_presenter = new AnalysisResultsPresenter(this);
    }
    m_presenter->showWithModel(m_analysisModel);

    // ダイアログ設定を AC オプションへ反映
    applyDialogOptions_(dlg);

    // エンジン起動
    const int  engineIdx = dlg->engineNumber();
    const auto engines   = dlg->engineList();
    if (engineIdx < 0 || engineIdx >= engines.size()) {
        if (m_err) m_err(QStringLiteral("エンジン選択が不正です。"));
        return;
    }
    const QString enginePath = engines.at(engineIdx).path;
    const QString engineName = dlg->engineName();
    m_usi->startAndInitializeEngine(enginePath, engineName); // usi→usiok/setoption/isready→readyok

    // 解析開始
    m_coord->startAnalyzeRange();
}

void AnalysisFlowController::applyDialogOptions_(KifuAnalysisDialog* dlg)
{
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = dlg->byoyomiSec() * 1000;

    const int startRaw = dlg->initPosition() ? 0 : qMax(0, m_activePly);
    opt.startPly = qBound(0, startRaw, m_sfenRecord->size() - 1);
    opt.endPly   = m_sfenRecord->size() - 1;

    opt.multiPV    = 1;    // ダイアログ未対応なら 1 固定
    opt.centerTree = true;

    m_coord->setOptions(opt);
}

// ======================
//  スロット実装（非ラムダ）
// ======================

void AnalysisFlowController::onUsiCommLogChanged_()
{
    if (!m_coord || !m_logModel) return;

    const QString line = m_logModel->usiCommLog().trimmed();
    if (line.startsWith(QStringLiteral("info "))) {
        m_coord->onEngineInfoLine(line);
    } else if (line.startsWith(QStringLiteral("bestmove "))) {
        m_coord->onEngineBestmoveReceived(line);
    }
}

void AnalysisFlowController::onAnalysisProgress_(int ply, int /*depth*/, int /*seldepth*/,
                                                 int scoreCp, int mate,
                                                 const QString& pv, const QString& /*raw*/)
{
    if (!m_analysisModel) return;

    // 指し手ラベル
    QString moveLabel;
    if (m_moveRecords && ply >= 0 && m_moveRecords->size() > ply && m_moveRecords->at(ply)) {
        moveLabel = m_moveRecords->at(ply)->currentMove();
    } else {
        moveLabel = QStringLiteral("ply %1").arg(ply);
    }

    // 評価値 / 差分
    QString evalStr;
    int curVal = 0;
    if (mate != 0) {
        evalStr = QStringLiteral("mate %1").arg(mate);
        curVal  = m_prevEvalCp; // 詰みは差分0扱い（前回値維持）
    } else if (scoreCp != std::numeric_limits<int>::min()) {
        evalStr = QString::number(scoreCp);
        curVal  = scoreCp;
    } else {
        evalStr = QStringLiteral("0");
        curVal  = 0;
    }
    const QString diff = QString::number(curVal - m_prevEvalCp);
    m_prevEvalCp = curVal;

    // ★ KifuAnalysisResultsDisplay は (Move, Eval, Diff, PV) の4引数
    m_analysisModel->appendItem(new KifuAnalysisResultsDisplay(
        moveLabel,
        evalStr,
        diff,
        pv
        ));
}
