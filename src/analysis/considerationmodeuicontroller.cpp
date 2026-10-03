/// @file considerationmodeuicontroller.cpp
/// @brief 検討モードUIコントローラクラスの実装

#include "considerationmodeuicontroller.h"

#include "logcategories.h"
#include "analysisflowcontroller.h"
#include "considerationtabmanager.h"
#include "engineinfowidget.h"
#include "shogiview.h"
#include "matchcoordinator.h"
#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"
#include "shogimove.h"
#include "kifurecordlistmodel.h"
#include "shogiutils.h"
#include "candidatearrowcontroller.h"

ConsiderationModeUIController::ConsiderationModeUIController(QObject* parent)
    : QObject(parent)
{
}

void ConsiderationModeUIController::setConsiderationTabManager(ConsiderationTabManager* manager)
{
    m_considerationTabManager = manager;
}

void ConsiderationModeUIController::setThinkingEngineInfo(EngineInfoWidget* info)
{
    m_thinkingInfo1 = info;
}

void ConsiderationModeUIController::setShogiView(ShogiView* view)
{
    m_shogiView = view;
}

void ConsiderationModeUIController::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void ConsiderationModeUIController::setConsiderationModel(ShogiEngineThinkingModel* model)
{
    m_considerationModel = model;
}

void ConsiderationModeUIController::setCommLogModel(UsiCommLogModel* model)
{
    m_commLogModel = model;
}

void ConsiderationModeUIController::onModeStarted()
{
    m_considerationActive = true;
    qCDebug(lcAnalysis).noquote() << "Initializing consideration mode";

    if (m_considerationTabManager && m_considerationModel) {
        // 検討タブに専用モデルを設定
        m_considerationTabManager->setConsiderationThinkingModel(m_considerationModel);

        // 検討タブのEngineInfoWidgetにもモデルを設定
        if (m_considerationTabManager->considerationInfo() && m_commLogModel) {
            m_considerationTabManager->considerationInfo()->setModel(m_commLogModel);
        }

        // 思考タブのEngineInfoWidgetにもモデルを設定
        if (m_thinkingInfo1 && m_commLogModel) {
            m_thinkingInfo1->setModel(m_commLogModel);
        }

        // 検討と対局で共有する矢印コントローラへ表示元を設定
        updateArrows();

        // 矢印表示チェックボックスの状態変更時
        connect(m_considerationTabManager, &ConsiderationTabManager::showArrowsChanged,
                this, &ConsiderationModeUIController::onShowArrowsChanged,
                Qt::UniqueConnection);
    }
}

void ConsiderationModeUIController::onTimeSettingsReady(bool unlimited, int byoyomiSec)
{
    qCDebug(lcAnalysis).noquote() << "onTimeSettingsReady: unlimited=" << unlimited
                                  << "byoyomiSec=" << byoyomiSec;

    if (m_considerationTabManager) {
        // 時間設定を検討タブに反映
        m_considerationTabManager->setConsiderationTimeLimit(unlimited, byoyomiSec);

        // 経過時間タイマーを開始
        m_considerationTabManager->startElapsedTimer();

        // ボタンを「検討中止」に切り替え
        m_considerationTabManager->setConsiderationRunning(true);

        // 検討中のMultiPV変更を接続
        connect(m_considerationTabManager, &ConsiderationTabManager::considerationMultiPVChanged,
                this, &ConsiderationModeUIController::onMultiPVChanged,
                Qt::UniqueConnection);

        // 検討中止ボタンを接続
        connect(m_considerationTabManager, &ConsiderationTabManager::stopConsiderationRequested,
                this, &ConsiderationModeUIController::stopRequested,
                Qt::UniqueConnection);

        // 開始要求はAnalysisTabWiringから接続済み。ここで再接続すると、
        // 2回目以降の開始操作でエンジンが二重に起動する。
    }

    // 検討終了時にタイマーを停止するための接続
    if (m_match) {
        connect(m_match, &MatchCoordinator::considerationModeEnded,
                this, &ConsiderationModeUIController::onModeEnded,
                Qt::UniqueConnection);

        // 検討待機開始時にタイマーを停止（ボタンは「検討中止」のまま）
        connect(m_match, &MatchCoordinator::considerationWaitingStarted,
                this, &ConsiderationModeUIController::onWaitingStarted,
                Qt::UniqueConnection);
    }
}

void ConsiderationModeUIController::onModeEnded()
{
    m_considerationActive = false;
    qCDebug(lcAnalysis).noquote() << "consideration mode ended";

    if (m_considerationTabManager) {
        // 経過時間タイマーを停止
        qCDebug(lcAnalysis).noquote() << "Stopping elapsed timer";
        m_considerationTabManager->stopElapsedTimer();

        // ボタンを「検討開始」に切り替え
        qCDebug(lcAnalysis).noquote() << "Calling setConsiderationRunning(false)";
        m_considerationTabManager->setConsiderationRunning(false);
        qCDebug(lcAnalysis).noquote() << "setConsiderationRunning(false) returned";
    }

    // 検討終了時に矢印をクリア
    if (m_shogiView) {
        updateArrows();
    }
}

void ConsiderationModeUIController::onWaitingStarted()
{
    qCDebug(lcAnalysis).noquote() << "onWaitingStarted";

    if (m_considerationTabManager) {
        // 経過時間タイマーを停止（ボタンは「検討中止」のまま）
        qCDebug(lcAnalysis).noquote() << "Stopping elapsed timer";
        m_considerationTabManager->stopElapsedTimer();
        // setConsiderationRunning(false) は呼ばない（ボタンは「検討中止」のまま）
    }
}

void ConsiderationModeUIController::onMultiPVChanged(int value)
{
    qCDebug(lcAnalysis).noquote() << "onMultiPVChanged: value=" << value;

    // 検討中の場合のみMatchCoordinatorに転送
    emit multiPVChangeRequested(value);
    if (m_considerationActive && m_considerationTabManager) {
        m_considerationTabManager->startElapsedTimer();
    }
}

void ConsiderationModeUIController::onDialogMultiPVReady(int multiPV)
{
    qCDebug(lcAnalysis).noquote() << "onDialogMultiPVReady: multiPV=" << multiPV;

    // 検討タブのMultiPVコンボボックスを更新
    if (m_considerationTabManager) {
        m_considerationTabManager->setConsiderationMultiPV(multiPV);
    }
}

void ConsiderationModeUIController::onShowArrowsChanged(bool checked)
{
    m_showArrows = checked;
    updateArrows();
}

void ConsiderationModeUIController::updateArrows()
{
    if (!m_shogiView) return;
    const bool show = m_showArrows && (!m_considerationTabManager
        || m_considerationTabManager->isShowArrowsChecked());
    CandidateArrowController::forView(m_shogiView)->setConsiderationState(
        m_considerationActive, show, m_considerationModel);
}

bool ConsiderationModeUIController::updatePositionIfInConsiderationMode(
    int row, const QString& newPosition,
    const QList<ShogiMove>* gameMoves,
    KifuRecordListModel* kifuRecordModel)
{
    // 検討モード中でなければ何もしない
    if (!m_match) return false;

    qCDebug(lcAnalysis).noquote() << "updatePositionIfInConsiderationMode: row=" << row
                                  << "newPosition.isEmpty=" << newPosition.isEmpty()
                                  << "newPosition=" << newPosition.left(80);

    if (newPosition.isEmpty()) return false;

    // 選択した手の移動先を取得（「同」表記のため）
    int previousFileTo = 0;
    int previousRankTo = 0;

    if (gameMoves && row >= 0 && row < gameMoves->size()) {
        const QPoint& toSquare = gameMoves->at(row).toSquare;
        previousFileTo = toSquare.x();
        previousRankTo = toSquare.y();
    } else if (kifuRecordModel && row > 0 && row < kifuRecordModel->rowCount()) {
        // 棋譜読み込み時: ShogiUtilsを使用して座標を解析（「同」の場合は自動的に遡る）
        if (auto coord = ShogiUtils::parseMoveCoordinateFromModel(kifuRecordModel, row)) {
            auto [file, rank] = *coord;
            previousFileTo = file;
            previousRankTo = rank;
        }
    }

    const bool updated = m_match->updateConsiderationPosition(newPosition, previousFileTo, previousRankTo);
    qCDebug(lcAnalysis).noquote() << "updateConsiderationPosition returned:" << updated
                                  << "previousFileTo=" << previousFileTo << "previousRankTo=" << previousRankTo;

    // ポジションが変更された場合、経過時間タイマーをリセットして再開
    if (updated && m_considerationTabManager) {
        m_considerationTabManager->startElapsedTimer();
    }

    return updated;
}
