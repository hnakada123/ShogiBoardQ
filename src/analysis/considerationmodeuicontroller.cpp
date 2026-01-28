#include "considerationmodeuicontroller.h"

#include "engineanalysistab.h"
#include "engineinfowidget.h"
#include "shogiview.h"
#include "matchcoordinator.h"
#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"
#include "shogimove.h"
#include "kifurecordlistmodel.h"
#include "shogiutils.h"

#include <QDebug>
#include <QColor>

ConsiderationModeUIController::ConsiderationModeUIController(QObject* parent)
    : QObject(parent)
{
}

void ConsiderationModeUIController::setAnalysisTab(EngineAnalysisTab* tab)
{
    m_analysisTab = tab;
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

void ConsiderationModeUIController::setCurrentSfenStr(const QString& sfen)
{
    m_currentSfenStr = sfen;
}

void ConsiderationModeUIController::setShowArrowsEnabled(bool enabled)
{
    m_showArrows = enabled;
    updateArrows();
}

void ConsiderationModeUIController::onModeStarted()
{
    qDebug().noquote() << "[ConsiderationModeUIController::onModeStarted] Initializing consideration mode";

    if (m_analysisTab && m_considerationModel) {
        // 検討タブに専用モデルを設定
        m_analysisTab->setConsiderationThinkingModel(m_considerationModel);

        // 検討タブのEngineInfoWidgetにもモデルを設定
        if (m_analysisTab->considerationInfo() && m_commLogModel) {
            m_analysisTab->considerationInfo()->setModel(m_commLogModel);
        }

        // 思考タブのEngineInfoWidgetにもモデルを設定
        if (m_analysisTab->info1() && m_commLogModel) {
            m_analysisTab->info1()->setModel(m_commLogModel);
        }

        // 矢印更新用のシグナル接続
        connectArrowUpdateSignals();

        // 矢印表示チェックボックスの状態変更時
        connect(m_analysisTab, &EngineAnalysisTab::showArrowsChanged,
                this, &ConsiderationModeUIController::onShowArrowsChanged,
                Qt::UniqueConnection);
    }
}

void ConsiderationModeUIController::onTimeSettingsReady(bool unlimited, int byoyomiSec)
{
    qDebug().noquote() << "[ConsiderationModeUIController::onTimeSettingsReady] unlimited=" << unlimited
                       << " byoyomiSec=" << byoyomiSec;

    if (m_analysisTab) {
        // 時間設定を検討タブに反映
        m_analysisTab->setConsiderationTimeLimit(unlimited, byoyomiSec);

        // 経過時間タイマーを開始
        m_analysisTab->startElapsedTimer();

        // ボタンを「検討中止」に切り替え
        m_analysisTab->setConsiderationRunning(true);

        // 検討中のMultiPV変更を接続
        connect(m_analysisTab, &EngineAnalysisTab::considerationMultiPVChanged,
                this, &ConsiderationModeUIController::onMultiPVChanged,
                Qt::UniqueConnection);

        // 検討中止ボタンを接続
        connect(m_analysisTab, &EngineAnalysisTab::stopConsiderationRequested,
                this, &ConsiderationModeUIController::stopRequested,
                Qt::UniqueConnection);

        // 検討開始ボタンを接続
        connect(m_analysisTab, &EngineAnalysisTab::startConsiderationRequested,
                this, &ConsiderationModeUIController::startRequested,
                Qt::UniqueConnection);
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
    qDebug().noquote() << "[ConsiderationModeUIController::onModeEnded] ENTER";

    if (m_analysisTab) {
        // 経過時間タイマーを停止
        qDebug().noquote() << "[ConsiderationModeUIController::onModeEnded] Stopping elapsed timer";
        m_analysisTab->stopElapsedTimer();

        // ボタンを「検討開始」に切り替え
        qDebug().noquote() << "[ConsiderationModeUIController::onModeEnded] Calling setConsiderationRunning(false)";
        m_analysisTab->setConsiderationRunning(false);
        qDebug().noquote() << "[ConsiderationModeUIController::onModeEnded] setConsiderationRunning(false) returned";
    }

    // 検討終了時に矢印をクリア
    if (m_shogiView) {
        m_shogiView->clearArrows();
    }

    qDebug().noquote() << "[ConsiderationModeUIController::onModeEnded] EXIT";
}

void ConsiderationModeUIController::onWaitingStarted()
{
    qDebug().noquote() << "[ConsiderationModeUIController::onWaitingStarted] ENTER";

    if (m_analysisTab) {
        // 経過時間タイマーを停止（ボタンは「検討中止」のまま）
        qDebug().noquote() << "[ConsiderationModeUIController::onWaitingStarted] Stopping elapsed timer";
        m_analysisTab->stopElapsedTimer();
        // setConsiderationRunning(false) は呼ばない（ボタンは「検討中止」のまま）
    }

    qDebug().noquote() << "[ConsiderationModeUIController::onWaitingStarted] EXIT";
}

void ConsiderationModeUIController::onMultiPVChanged(int value)
{
    qDebug().noquote() << "[ConsiderationModeUIController::onMultiPVChanged] value=" << value;

    // 検討中の場合のみMatchCoordinatorに転送
    emit multiPVChangeRequested(value);
}

void ConsiderationModeUIController::onDialogMultiPVReady(int multiPV)
{
    qDebug().noquote() << "[ConsiderationModeUIController::onDialogMultiPVReady] multiPV=" << multiPV;

    // 検討タブのMultiPVコンボボックスを更新
    if (m_analysisTab) {
        m_analysisTab->setConsiderationMultiPV(multiPV);
    }
}

void ConsiderationModeUIController::onShowArrowsChanged(bool checked)
{
    m_showArrows = checked;
    updateArrows();
}

void ConsiderationModeUIController::connectArrowUpdateSignals()
{
    if (m_arrowSignalsConnected || !m_considerationModel) return;

    connect(m_considerationModel, &ShogiEngineThinkingModel::rowsInserted,
            this, &ConsiderationModeUIController::updateArrows,
            Qt::UniqueConnection);
    connect(m_considerationModel, &ShogiEngineThinkingModel::dataChanged,
            this, &ConsiderationModeUIController::updateArrows,
            Qt::UniqueConnection);
    connect(m_considerationModel, &ShogiEngineThinkingModel::modelReset,
            this, &ConsiderationModeUIController::updateArrows,
            Qt::UniqueConnection);

    m_arrowSignalsConnected = true;
}

bool ConsiderationModeUIController::parseUsiMove(const QString& usiMove,
                                                  int& fromFile, int& fromRank,
                                                  int& toFile, int& toRank)
{
    if (usiMove.isEmpty()) return false;

    // 駒打ちの場合: "P*3c" のような形式
    if (usiMove.length() >= 4 && usiMove.at(1) == '*') {
        fromFile = 0;
        fromRank = 0;
        // 移動先の座標を取得
        QChar toFileChar = usiMove.at(2);
        QChar toRankChar = usiMove.at(3);
        if (toFileChar >= '1' && toFileChar <= '9' && toRankChar >= 'a' && toRankChar <= 'i') {
            toFile = toFileChar.digitValue();
            toRank = toRankChar.toLatin1() - 'a' + 1;
            return true;
        }
        return false;
    }

    // 通常の指し手: "7g7f" のような形式
    if (usiMove.length() >= 4) {
        QChar fromFileChar = usiMove.at(0);
        QChar fromRankChar = usiMove.at(1);
        QChar toFileChar = usiMove.at(2);
        QChar toRankChar = usiMove.at(3);

        if (fromFileChar >= '1' && fromFileChar <= '9' &&
            fromRankChar >= 'a' && fromRankChar <= 'i' &&
            toFileChar >= '1' && toFileChar <= '9' &&
            toRankChar >= 'a' && toRankChar <= 'i') {
            fromFile = fromFileChar.digitValue();
            fromRank = fromRankChar.toLatin1() - 'a' + 1;
            toFile = toFileChar.digitValue();
            toRank = toRankChar.toLatin1() - 'a' + 1;
            return true;
        }
    }

    return false;
}

void ConsiderationModeUIController::updateArrows()
{
    if (!m_shogiView) return;

    // 矢印表示がOFFの場合はクリアして終了
    if (!m_showArrows) {
        m_shogiView->clearArrows();
        return;
    }

    // 検討モデルがない場合はクリアして終了
    if (!m_considerationModel || m_considerationModel->rowCount() == 0) {
        m_shogiView->clearArrows();
        return;
    }

    // 「矢印表示」チェックボックスの状態を確認
    if (m_analysisTab && !m_analysisTab->isShowArrowsChecked()) {
        m_shogiView->clearArrows();
        return;
    }

    QVector<ShogiView::Arrow> arrows;

    // 検討モデルの各行から読み筋の最初の指し手を取得して矢印を作成
    const int rowCount = m_considerationModel->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QString usiPv = m_considerationModel->usiPvAt(row);
        if (usiPv.isEmpty()) continue;

        // 読み筋の最初の指し手を取得（スペース区切り）
        QString firstMove = usiPv.split(' ').first();
        if (firstMove.isEmpty()) continue;

        int fromFile = 0, fromRank = 0, toFile = 0, toRank = 0;
        if (!parseUsiMove(firstMove, fromFile, fromRank, toFile, toRank)) continue;

        ShogiView::Arrow arrow;
        arrow.fromFile = fromFile;
        arrow.fromRank = fromRank;
        arrow.toFile = toFile;
        arrow.toRank = toRank;
        arrow.priority = row + 1;  // 優先順位（1が最善手）

        // 駒打ちの場合は打つ駒を設定（USI形式: "P*3c" → 'P'）
        if (fromFile == 0 || fromRank == 0) {
            if (firstMove.length() >= 1) {
                // USI形式の駒打ちは "P*3c" のような形式
                // 最初の文字が駒種（大文字）
                QChar usiPiece = firstMove.at(0);

                // 現在の手番を確認（SFENの手番フィールドを使用）
                // SFENフォーマット: "position sfen ... b ..." (b=先手, w=後手)
                bool isBlackTurn = true;  // デフォルトは先手
                if (!m_currentSfenStr.isEmpty()) {
                    // SFENの手番フィールドを探す（スペース区切りの2番目）
                    QStringList sfenParts = m_currentSfenStr.split(' ');
                    if (sfenParts.size() >= 2) {
                        isBlackTurn = (sfenParts.at(1) == "b");
                    }
                }

                // USI形式では P=歩, L=香, N=桂, S=銀, G=金, B=角, R=飛
                // ShogiViewの駒文字は 先手:大文字, 後手:小文字
                if (isBlackTurn) {
                    arrow.dropPiece = usiPiece;  // 先手は大文字
                } else {
                    arrow.dropPiece = usiPiece.toLower();  // 後手は小文字
                }
            }
        }

        // 最善手（最初の行）は濃い赤、それ以外は薄い赤
        if (row == 0) {
            arrow.color = QColor(255, 0, 0, 200);  // 濃い赤（半透明）
        } else {
            arrow.color = QColor(255, 100, 100, 150);  // 薄い赤（より透明）
        }

        arrows.append(arrow);
    }

    m_shogiView->setArrows(arrows);
}

bool ConsiderationModeUIController::updatePositionIfInConsiderationMode(
    int row, const QString& newPosition,
    const QVector<ShogiMove>* gameMoves,
    KifuRecordListModel* kifuRecordModel)
{
    // 検討モード中でなければ何もしない
    if (!m_match) return false;

    qDebug().noquote() << "[ConsiderationUIController] updatePositionIfInConsiderationMode: row=" << row
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
        ShogiUtils::parseMoveCoordinateFromModel(kifuRecordModel, row, &previousFileTo, &previousRankTo);
    }

    const bool updated = m_match->updateConsiderationPosition(newPosition, previousFileTo, previousRankTo);
    qDebug().noquote() << "[ConsiderationUIController] updateConsiderationPosition returned:" << updated
                       << "previousFileTo=" << previousFileTo << "previousRankTo=" << previousRankTo;

    // ポジションが変更された場合、経過時間タイマーをリセットして再開
    if (updated && m_analysisTab) {
        m_analysisTab->startElapsedTimer();
    }

    return updated;
}
