/// @file evaluationgraphcontroller.cpp
/// @brief 評価値グラフコントローラクラスの実装

#include "evaluationgraphcontroller.h"

#include "loggingcategory.h"
#include <QTimer>

#include "evalgraphpresenter.h"
#include "evaluationchartwidget.h"
#include "matchcoordinator.h"

EvaluationGraphController::EvaluationGraphController(QObject* parent)
    : QObject(parent)
{
}

EvaluationGraphController::~EvaluationGraphController() = default;

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void EvaluationGraphController::setEvalChart(EvaluationChartWidget* chart)
{
    m_evalChart = chart;
}

void EvaluationGraphController::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void EvaluationGraphController::setSfenRecord(QStringList* sfenRecord)
{
    m_sfenRecord = sfenRecord;
}

// --------------------------------------------------------
// エンジン名の設定
// --------------------------------------------------------

void EvaluationGraphController::setEngine1Name(const QString& name)
{
    qCDebug(lcUi) << "setEngine1Name called: name=" << name;
    m_engine1Name = name;

    // EvaluationChartWidgetにも即座に設定
    if (m_evalChart) {
        qCDebug(lcUi) << "setEngine1Name: forwarding to EvaluationChartWidget";
        m_evalChart->setEngine1Name(name);
    } else {
        qCDebug(lcUi) << "setEngine1Name: m_evalChart is NULL!";
    }
}

void EvaluationGraphController::setEngine2Name(const QString& name)
{
    qCDebug(lcUi) << "setEngine2Name called: name=" << name;
    m_engine2Name = name;

    // EvaluationChartWidgetにも即座に設定
    if (m_evalChart) {
        qCDebug(lcUi) << "setEngine2Name: forwarding to EvaluationChartWidget";
        m_evalChart->setEngine2Name(name);
    } else {
        qCDebug(lcUi) << "setEngine2Name: m_evalChart is NULL!";
    }
}

// --------------------------------------------------------
// 評価値スコアの管理
// --------------------------------------------------------

void EvaluationGraphController::clearScores()
{
    m_scoreCp.clear();
}

void EvaluationGraphController::trimToPly(int maxPly)
{
    // m_scoreCp は 2手ごとに1要素（先手の手で追加）
    // インデックス i は手数 (i+1)*2 に対応（例: i=0 は2手目、i=1 は4手目）
    // maxPly までのデータを残すには、(maxPly / 2) 個の要素を残す
    const int keepCount = maxPly / 2;
    if (keepCount < m_scoreCp.size()) {
        m_scoreCp = m_scoreCp.mid(0, keepCount);
    }

    // チャートウィジェットもトリム
    if (m_evalChart) {
        m_evalChart->trimToPly(maxPly);
    }
}

const QList<int>& EvaluationGraphController::scores() const
{
    return m_scoreCp;
}

QList<int>& EvaluationGraphController::scoresRef()
{
    return m_scoreCp;
}

// --------------------------------------------------------
// グラフ更新スロット
// --------------------------------------------------------

void EvaluationGraphController::redrawEngine1Graph(int ply)
{
    qCDebug(lcUi) << "redrawEngine1Graph() called with ply=" << ply;

    // EvEモードでplyが渡された場合は保存しておく
    m_pendingPlyForEngine1 = ply;

    // bestmoveと同時に受信したinfo行の処理が完了するのを待つため、遅延させる
    QTimer::singleShot(50, this, &EvaluationGraphController::doRedrawEngine1Graph);
}

void EvaluationGraphController::redrawEngine2Graph(int ply)
{
    qCDebug(lcUi) << "redrawEngine2Graph() called with ply=" << ply;

    // EvEモードでplyが渡された場合は保存しておく
    m_pendingPlyForEngine2 = ply;

    // bestmoveと同時に受信したinfo行の処理が完了するのを待つため、遅延させる
    QTimer::singleShot(50, this, &EvaluationGraphController::doRedrawEngine2Graph);
}

// --------------------------------------------------------
// 遅延実行スロット
// --------------------------------------------------------

void EvaluationGraphController::doRedrawEngine1Graph()
{
    qCDebug(lcUi) << "doRedrawEngine1Graph() delayed execution";

    // 手数を取得：EvEモードでplyが渡された場合はそれを使用、それ以外はsfenRecordから計算
    int ply;
    if (m_pendingPlyForEngine1 >= 0) {
        // EvEモードで渡されたplyを使用
        ply = m_pendingPlyForEngine1;
        m_pendingPlyForEngine1 = -1;  // 使用後リセット
        qCDebug(lcUi) << "P1: using pending ply =" << ply;
    } else {
        // sfenRecordから計算（従来の動作）
        ply = m_sfenRecord ? qMax(0, static_cast<int>(m_sfenRecord->size() - 1)) : 0;
        qCDebug(lcUi) << "P1: actual ply (from sfenRecord) =" << ply
                      << ", sfenRecord size =" << (m_sfenRecord ? m_sfenRecord->size() : -1);
    }

    EvalGraphPresenter::appendPrimaryScore(m_scoreCp, m_match);

    const int cpAfter = m_scoreCp.isEmpty() ? 0 : m_scoreCp.last();
    qCDebug(lcUi) << "P1: after appendPrimaryScore, m_scoreCp.size() =" << m_scoreCp.size()
                  << ", last cp =" << cpAfter;

    // 実際のチャートウィジェットにも描画
    if (!m_evalChart) {
        qCDebug(lcUi) << "P1: m_evalChart is NULL!";
        return;
    }

    // エンジン名を設定
    m_evalChart->setEngine1Name(m_engine1Name);

    qCDebug(lcUi) << "P1: calling m_evalChart->appendScoreP1(" << ply << "," << cpAfter << ", false)";
    m_evalChart->appendScoreP1(ply, cpAfter, false);
    qCDebug(lcUi) << "P1: appendScoreP1 done, chart countP1 =" << m_evalChart->countP1();
}

void EvaluationGraphController::doRedrawEngine2Graph()
{
    qCDebug(lcUi) << "doRedrawEngine2Graph() delayed execution";

    // 手数を取得：EvEモードでplyが渡された場合はそれを使用、それ以外はsfenRecordから計算
    int ply;
    if (m_pendingPlyForEngine2 >= 0) {
        // EvEモードで渡されたplyを使用
        ply = m_pendingPlyForEngine2;
        m_pendingPlyForEngine2 = -1;  // 使用後リセット
        qCDebug(lcUi) << "P2: using pending ply =" << ply;
    } else {
        // sfenRecordから計算（従来の動作）
        ply = m_sfenRecord ? qMax(0, static_cast<int>(m_sfenRecord->size() - 1)) : 0;
        qCDebug(lcUi) << "P2: actual ply (from sfenRecord) =" << ply
                      << ", sfenRecord size =" << (m_sfenRecord ? m_sfenRecord->size() : -1);
    }

    EvalGraphPresenter::appendSecondaryScore(m_scoreCp, m_match);

    const int cpAfter = m_scoreCp.isEmpty() ? 0 : m_scoreCp.last();
    qCDebug(lcUi) << "P2: after appendSecondaryScore, m_scoreCp.size() =" << m_scoreCp.size()
                  << ", last cp =" << cpAfter;

    // 実際のチャートウィジェットにも描画
    if (!m_evalChart) {
        qCDebug(lcUi) << "P2: m_evalChart is NULL!";
        return;
    }

    // エンジン名を設定
    m_evalChart->setEngine2Name(m_engine2Name.isEmpty() ? m_engine1Name : m_engine2Name);

    // 後手/上手のエンジン評価値は符号を反転させてプロット
    // USIエンジンは手番側から見た評価値を出力するため、後手の評価値を先手視点に変換
    qCDebug(lcUi) << "P2: calling m_evalChart->appendScoreP2(" << ply << "," << cpAfter << ", true)";
    m_evalChart->appendScoreP2(ply, cpAfter, true);
    qCDebug(lcUi) << "P2: appendScoreP2 done, chart countP2 =" << m_evalChart->countP2();
}

// --------------------------------------------------------
// 評価値プロットの削除（待った機能用）
// --------------------------------------------------------

void EvaluationGraphController::removeLastP1Score()
{
    qCDebug(lcUi) << "removeLastP1Score() called";

    // 内部スコアリストから削除
    if (!m_scoreCp.isEmpty()) {
        m_scoreCp.removeLast();
        qCDebug(lcUi) << "m_scoreCp removed last, new size =" << m_scoreCp.size();
    }

    // チャートウィジェットから削除
    if (!m_evalChart) {
        qCDebug(lcUi) << "removeLastP1Score: m_evalChart is NULL!";
        return;
    }

    m_evalChart->removeLastP1();
    qCDebug(lcUi) << "removeLastP1Score done, chart countP1 =" << m_evalChart->countP1();
}

void EvaluationGraphController::removeLastP2Score()
{
    qCDebug(lcUi) << "removeLastP2Score() called";

    // 内部スコアリストから削除
    if (!m_scoreCp.isEmpty()) {
        m_scoreCp.removeLast();
        qCDebug(lcUi) << "m_scoreCp removed last, new size =" << m_scoreCp.size();
    }

    // チャートウィジェットから削除
    if (!m_evalChart) {
        qCDebug(lcUi) << "removeLastP2Score: m_evalChart is NULL!";
        return;
    }

    m_evalChart->removeLastP2();
    qCDebug(lcUi) << "removeLastP2Score done, chart countP2 =" << m_evalChart->countP2();
}

// --------------------------------------------------------
// 現在手数の設定（縦線表示用）
// --------------------------------------------------------

void EvaluationGraphController::setCurrentPly(int ply)
{
    qCDebug(lcUi) << "setCurrentPly() called with ply =" << ply;

    if (!m_evalChart) {
        qCDebug(lcUi) << "setCurrentPly: m_evalChart is NULL!";
        return;
    }

    m_evalChart->setCurrentPly(ply);
}
