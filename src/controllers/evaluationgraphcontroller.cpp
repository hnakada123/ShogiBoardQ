#include "evaluationgraphcontroller.h"

#include <QDebug>
#include <QTimer>

#include "evalgraphpresenter.h"
#include "evaluationchartwidget.h"
#include "recordpane.h"
#include "matchcoordinator.h"

EvaluationGraphController::EvaluationGraphController(QObject* parent)
    : QObject(parent)
{
}

EvaluationGraphController::~EvaluationGraphController() = default;

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void EvaluationGraphController::setRecordPane(RecordPane* pane)
{
    m_recordPane = pane;
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
    m_engine1Name = name;
}

void EvaluationGraphController::setEngine2Name(const QString& name)
{
    m_engine2Name = name;
}

// --------------------------------------------------------
// 評価値スコアの管理
// --------------------------------------------------------

void EvaluationGraphController::clearScores()
{
    m_scoreCp.clear();
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
    qDebug() << "[EVAL_GRAPH] redrawEngine1Graph() called with ply=" << ply;

    // EvEモードでplyが渡された場合は保存しておく
    m_pendingPlyForEngine1 = ply;

    // bestmoveと同時に受信したinfo行の処理が完了するのを待つため、遅延させる
    QTimer::singleShot(50, this, &EvaluationGraphController::doRedrawEngine1Graph);
}

void EvaluationGraphController::redrawEngine2Graph(int ply)
{
    qDebug() << "[EVAL_GRAPH] redrawEngine2Graph() called with ply=" << ply;

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
    qDebug() << "[EVAL_GRAPH] doRedrawEngine1Graph() delayed execution";

    // 手数を取得：EvEモードでplyが渡された場合はそれを使用、それ以外はsfenRecordから計算
    int ply;
    if (m_pendingPlyForEngine1 >= 0) {
        // EvEモードで渡されたplyを使用
        ply = m_pendingPlyForEngine1;
        m_pendingPlyForEngine1 = -1;  // 使用後リセット
        qDebug() << "[EVAL_GRAPH] P1: using pending ply =" << ply;
    } else {
        // sfenRecordから計算（従来の動作）
        ply = m_sfenRecord ? qMax(0, static_cast<int>(m_sfenRecord->size() - 1)) : 0;
        qDebug() << "[EVAL_GRAPH] P1: actual ply (from sfenRecord) =" << ply
                 << ", sfenRecord size =" << (m_sfenRecord ? m_sfenRecord->size() : -1);
    }

    EvalGraphPresenter::appendPrimaryScore(m_scoreCp, m_match);

    const int cpAfter = m_scoreCp.isEmpty() ? 0 : m_scoreCp.last();
    qDebug() << "[EVAL_GRAPH] P1: after appendPrimaryScore, m_scoreCp.size() =" << m_scoreCp.size()
             << ", last cp =" << cpAfter;

    // 実際のチャートウィジェットにも描画
    if (!m_recordPane) {
        qDebug() << "[EVAL_GRAPH] P1: m_recordPane is NULL!";
        return;
    }

    EvaluationChartWidget* ec = m_recordPane->evalChart();
    if (!ec) {
        qDebug() << "[EVAL_GRAPH] P1: evalChart() returned NULL!";
        return;
    }

    // エンジン名を設定
    ec->setEngine1Name(m_engine1Name);

    qDebug() << "[EVAL_GRAPH] P1: calling ec->appendScoreP1(" << ply << "," << cpAfter << ", false)";
    ec->appendScoreP1(ply, cpAfter, false);
    qDebug() << "[EVAL_GRAPH] P1: appendScoreP1 done, chart countP1 =" << ec->countP1();
}

void EvaluationGraphController::doRedrawEngine2Graph()
{
    qDebug() << "[EVAL_GRAPH] doRedrawEngine2Graph() delayed execution";

    // 手数を取得：EvEモードでplyが渡された場合はそれを使用、それ以外はsfenRecordから計算
    int ply;
    if (m_pendingPlyForEngine2 >= 0) {
        // EvEモードで渡されたplyを使用
        ply = m_pendingPlyForEngine2;
        m_pendingPlyForEngine2 = -1;  // 使用後リセット
        qDebug() << "[EVAL_GRAPH] P2: using pending ply =" << ply;
    } else {
        // sfenRecordから計算（従来の動作）
        ply = m_sfenRecord ? qMax(0, static_cast<int>(m_sfenRecord->size() - 1)) : 0;
        qDebug() << "[EVAL_GRAPH] P2: actual ply (from sfenRecord) =" << ply
                 << ", sfenRecord size =" << (m_sfenRecord ? m_sfenRecord->size() : -1);
    }

    EvalGraphPresenter::appendSecondaryScore(m_scoreCp, m_match);

    const int cpAfter = m_scoreCp.isEmpty() ? 0 : m_scoreCp.last();
    qDebug() << "[EVAL_GRAPH] P2: after appendSecondaryScore, m_scoreCp.size() =" << m_scoreCp.size()
             << ", last cp =" << cpAfter;

    // 実際のチャートウィジェットにも描画
    if (!m_recordPane) {
        qDebug() << "[EVAL_GRAPH] P2: m_recordPane is NULL!";
        return;
    }

    EvaluationChartWidget* ec = m_recordPane->evalChart();
    if (!ec) {
        qDebug() << "[EVAL_GRAPH] P2: evalChart() returned NULL!";
        return;
    }

    // エンジン名を設定
    ec->setEngine2Name(m_engine2Name.isEmpty() ? m_engine1Name : m_engine2Name);

    qDebug() << "[EVAL_GRAPH] P2: calling ec->appendScoreP2(" << ply << "," << cpAfter << ", false)";
    ec->appendScoreP2(ply, cpAfter, false);
    qDebug() << "[EVAL_GRAPH] P2: appendScoreP2 done, chart countP2 =" << ec->countP2();
}

// --------------------------------------------------------
// 評価値プロットの削除（待った機能用）
// --------------------------------------------------------

void EvaluationGraphController::removeLastP1Score()
{
    qDebug() << "[EVAL_GRAPH] removeLastP1Score() called";

    // 内部スコアリストから削除
    if (!m_scoreCp.isEmpty()) {
        m_scoreCp.removeLast();
        qDebug() << "[EVAL_GRAPH] m_scoreCp removed last, new size =" << m_scoreCp.size();
    }

    // チャートウィジェットから削除
    if (!m_recordPane) {
        qDebug() << "[EVAL_GRAPH] removeLastP1Score: m_recordPane is NULL!";
        return;
    }

    EvaluationChartWidget* ec = m_recordPane->evalChart();
    if (!ec) {
        qDebug() << "[EVAL_GRAPH] removeLastP1Score: evalChart() returned NULL!";
        return;
    }

    ec->removeLastP1();
    qDebug() << "[EVAL_GRAPH] removeLastP1Score done, chart countP1 =" << ec->countP1();
}

void EvaluationGraphController::removeLastP2Score()
{
    qDebug() << "[EVAL_GRAPH] removeLastP2Score() called";

    // 内部スコアリストから削除
    if (!m_scoreCp.isEmpty()) {
        m_scoreCp.removeLast();
        qDebug() << "[EVAL_GRAPH] m_scoreCp removed last, new size =" << m_scoreCp.size();
    }

    // チャートウィジェットから削除
    if (!m_recordPane) {
        qDebug() << "[EVAL_GRAPH] removeLastP2Score: m_recordPane is NULL!";
        return;
    }

    EvaluationChartWidget* ec = m_recordPane->evalChart();
    if (!ec) {
        qDebug() << "[EVAL_GRAPH] removeLastP2Score: evalChart() returned NULL!";
        return;
    }

    ec->removeLastP2();
    qDebug() << "[EVAL_GRAPH] removeLastP2Score done, chart countP2 =" << ec->countP2();
}
