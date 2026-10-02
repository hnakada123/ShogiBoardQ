/// @file analysisflowcontroller_dialog.cpp
/// @brief 解析条件ダイアログと解析用エンジンの準備

#include "analysisflowcontroller.h"
#include "analysisresultspresenter.h"
#include "kifuanalysisdialog.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "logcategories.h"

void AnalysisFlowController::applyDialogOptions(KifuAnalysisDialog* dlg)
{
    AnalysisCoordinator::Options opt;
    opt.movetimeMs = dlg->byoyomiSec() * 1000;

    const int sfenSize = static_cast<int>(m_sfenHistory->size());

    // 解析範囲の最大値を設定
    // 注: sfenRecordには終局指し手（投了、中断など）は含まれないため、
    //     sfenSize - 1 が最後の指し手の局面インデックスとなる
    int maxEndPly = sfenSize - 1;

    // ダイアログの設定に基づいて範囲を決定
    if (dlg->initPosition()) {
        // "開始局面から最終手まで"が選択された場合
        opt.startPly = 0;
        opt.endPly = qMax(0, maxEndPly);
    } else {
        // 範囲指定が選択された場合
        opt.startPly = qBound(0, dlg->startPly(), maxEndPly);
        opt.endPly = qBound(opt.startPly, dlg->endPly(), maxEndPly);
    }

    qCDebug(lcAnalysis).noquote() << "applyDialogOptions: startPly=" << opt.startPly
                                  << "endPly=" << opt.endPly << "maxEndPly=" << maxEndPly;

    opt.multiPV    = 1;    // ダイアログ未対応なら 1 固定
    opt.centerTree = true;

    m_coord->setOptions(opt);
    m_analysisStartPly = opt.startPly;
    if (m_presenter) {
        m_presenter->beginAnalysis(opt.endPly - opt.startPly + 1, dlg->engineName());
    }
}

bool AnalysisFlowController::runWithDialog(const Deps& d, QWidget* parent)
{
    qCDebug(lcAnalysis).noquote() << "runWithDialog START";
    qCDebug(lcAnalysis).noquote() << "d.gameController=" << d.gameController;
    qCDebug(lcAnalysis).noquote() << "d.usi=" << d.usi;

    // 依存の必須チェック（usi以外）
    if (!d.sfenRecord || d.sfenRecord->isEmpty()) {
        if (d.displayError) d.displayError(tr("内部エラー: sfenRecord が未準備です。棋譜読み込み後に実行してください。"));
        return false;
    }
    if (!d.analysisModel) {
        if (d.displayError) d.displayError(tr("内部エラー: 解析モデルが未準備です。"));
        return false;
    }

    // ダイアログを生成してユーザに選択してもらう
    KifuAnalysisDialog dlg(parent);

    // 最大手数を設定
    // 注: sfenRecordには終局指し手（投了、中断など）は含まれないため、
    //     sfenSize - 1 が最後の指し手の局面インデックスとなる
    int maxPly = static_cast<int>(d.sfenRecord->size()) - 1;
    dlg.setMaxPly(qMax(0, maxPly));

    const int result = dlg.exec();
    if (result != QDialog::Accepted) return false;

    // GameControllerを保持
    m_gameController = d.gameController;
    qCDebug(lcAnalysis).noquote() << "m_gameController=" << m_gameController;
    if (m_gameController) {
        qCDebug(lcAnalysis).noquote() << "m_gameController->board()=" << m_gameController->board();
    }

    // Usiが渡されていない場合は内部で生成
    Deps actualDeps = d;
    if (!actualDeps.usi) {
        qCDebug(lcAnalysis).noquote() << "Creating internal Usi instance...";

        // 既存の内部Usiを破棄（メモリリーク防止）
        if (m_ownsUsi && m_usi) {
            if (m_connUsiBestMove) {
                QObject::disconnect(m_connUsiBestMove);
                m_connUsiBestMove = {};
            }
            if (m_connUsiInfoLine) {
                QObject::disconnect(m_connUsiInfoLine);
                m_connUsiInfoLine = {};
            }
            if (m_connUsiThinkingInfo) {
                QObject::disconnect(m_connUsiThinkingInfo);
                m_connUsiThinkingInfo = {};
            }
            if (m_connUsiError) {
                QObject::disconnect(m_connUsiError);
                m_connUsiError = {};
            }
            m_usi->sendQuitCommand();
            m_usi->blockSignals(true);
            m_usi->deleteLater();
            m_usi = nullptr;
            m_ownsUsi = false;
        }

        // ログモデル: 渡されたものがあればそれを使用、なければ生成
        UsiCommLogModel* logModelToUse = d.logModel;
        if (!logModelToUse) {
            if (!m_ownedLogModel) {
                m_ownedLogModel = new UsiCommLogModel(this);
            } else {
                m_ownedLogModel->clear();
            }
            logModelToUse = m_ownedLogModel;
        }

        // ThinkingModel: 渡されたものがあればそれを使用、なければ生成
        ShogiEngineThinkingModel* thinkingModelToUse = d.thinkingModel;
        if (!thinkingModelToUse) {
            if (!m_ownedThinkingModel) {
                m_ownedThinkingModel = new ShogiEngineThinkingModel(this);
            } else {
                m_ownedThinkingModel->clearAllItems();
            }
            thinkingModelToUse = m_ownedThinkingModel;
        }

        // Usiインスタンスを生成（GameControllerを渡して盤面情報を取得可能にする）
        m_usi = new Usi(logModelToUse, thinkingModelToUse, m_gameController, this);
        m_ownsUsi = true;

        actualDeps.usi = m_usi;
        actualDeps.logModel = logModelToUse;
        actualDeps.thinkingModel = thinkingModelToUse;
        qCDebug(lcAnalysis).noquote() << "Internal Usi created:" << m_usi;
        qCDebug(lcAnalysis).noquote() << "Using logModel:" << logModelToUse;
        qCDebug(lcAnalysis).noquote() << "Using thinkingModel:" << thinkingModelToUse;
    }

    // 以降は既存の start(...) に委譲（Presenter への表示や接続も start 側で実施）
    start(actualDeps, &dlg);
    return m_lastStartSucceeded;
}
