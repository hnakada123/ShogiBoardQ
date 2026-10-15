/// @file pvclickcontroller.cpp
/// @brief 読み筋クリックコントローラクラスの実装

#include "pvclickcontroller.h"

#include "appstate.h"
#include "kifunavigationstate.h"

#include "logcategories.h"

#include "shogienginethinkingmodel.h"
#include "shogiview.h"
#include "shogiinforecord.h"
#include "pvboarddialog.h"
#include "sfenutils.h"

namespace {
static QString normalizedSfen(QString sfen)
{
    return SfenUtils::normalizeSfenKey(sfen);
}

static bool isStartposSfen(const QString& sfen)
{
    return SfenUtils::isHirateStart(sfen);
}
}  // namespace

PvClickController::PvClickController(QObject* parent)
    : QObject(parent)
{
}

PvClickController::~PvClickController() = default;

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void PvClickController::setThinkingModels(ShogiEngineThinkingModel* model1, ShogiEngineThinkingModel* model2)
{
    m_modelThinking1 = model1;
    m_modelThinking2 = model2;
}

void PvClickController::setConsiderationModel(ShogiEngineThinkingModel* model)
{
    m_considerationModel = model;
}

void PvClickController::setStateRefs(const StateRefs& refs)
{
    m_stateRefs = refs;
}

void PvClickController::setShogiView(ShogiView* view)
{
    m_shogiView = view;
}

void PvClickController::syncFromRefs()
{
    if (const AppState* state = m_stateRefs.state) {
        m_players = state->players;
        m_currentSfenStr = state->currentSfenStr;
        m_startSfenStr = state->startSfenStr;
    }
    m_line = m_stateRefs.navState ? m_stateRefs.navState->currentRecordLine() : RecordLine{};
    m_boardFlipped = m_shogiView ? m_shogiView->flipMode() : false;
}

// --------------------------------------------------------
// スロット
// --------------------------------------------------------

void PvClickController::onPvRowClicked(int engineIndex, int row)
{
    syncFromRefs();

    qCDebug(lcUi) << "onPvRowClicked: engineIndex=" << engineIndex << " row=" << row;

    // 対象のモデルを取得（engineIndex: 0=エンジン1, 1=エンジン2, 2=検討タブ）
    ShogiEngineThinkingModel* model = modelForEngineIndex(engineIndex);
    if (!model) {
        qCDebug(lcUi) << "onPvRowClicked: model is null";
        return;
    }

    // モデルの行数をチェック
    if (row < 0 || row >= model->rowCount()) {
        qCDebug(lcUi) << "onPvRowClicked: row out of range";
        return;
    }

    // ShogiInfoRecord を取得
    const ShogiInfoRecord* record = model->recordAt(row);
    if (!record) {
        qCDebug(lcUi) << "onPvRowClicked: record is null";
        return;
    }

    // 読み筋（漢字表記）を取得
    QString kanjiPv = record->pv();
    qCDebug(lcUi) << "onPvRowClicked: kanjiPv=" << kanjiPv;

    if (kanjiPv.isEmpty()) {
        qCDebug(lcUi) << "onPvRowClicked: kanjiPv is empty";
        return;
    }

    // USI形式の読み筋を取得（局面に合った手まで）。局面と合わない手があって残っていなければ、
    // その行の info でエンジンが送った読み筋をそのまま使う
    QStringList usiMoves = record->usiPv().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (usiMoves.isEmpty()) {
        usiMoves = record->enginePv();
    }
    qCDebug(lcUi) << "onPvRowClicked: usiMoves from record:" << usiMoves;

    //重要: 読み筋（PV）が生成された時点の局面を使用する
    // record->baseSfen() は読み筋生成時の局面SFENを保持している
    QString currentSfen = resolveCurrentSfen(record->baseSfen());
    qCDebug(lcUi) << "onPvRowClicked: currentSfen=" << currentSfen;
    qCDebug(lcUi) << "onPvRowClicked: record->baseSfen()=" << record->baseSfen();
    qCDebug(lcUi) << "onPvRowClicked: record->lastUsiMove()=" << record->lastUsiMove();
    qCDebug(lcUi) << "onPvRowClicked: line positions=" << m_line.moveCount() + 1;

    // 起動時の局面に至った最後の手を設定（読み筋生成時に保存された情報）
    QString lastUsiMove = record->lastUsiMove();
    QString prevSfenForHighlight;
    qCDebug(lcUi) << "onPvRowClicked: lastUsiMove from record=" << lastUsiMove;
    resolveHighlightContext(currentSfen, lastUsiMove, prevSfenForHighlight);
    qCDebug(lcUi) << "onPvRowClicked: prevSfenForHighlight="
                  << (prevSfenForHighlight.isEmpty() ? QStringLiteral("<empty>") : prevSfenForHighlight.left(60));

    // PvBoardDialog を表示
    PvBoardDialog* dlg = new PvBoardDialog(currentSfen, usiMoves, qobject_cast<QWidget*>(parent()));
    dlg->setKanjiPv(kanjiPv);
    dlg->setProperty("pv_engine_index", engineIndex);
    connect(dlg, &QDialog::finished, this, &PvClickController::onPvDialogFinished);

    // 対局者名を設定
    QString blackName, whiteName;
    resolvePlayerNames(blackName, whiteName);
    dlg->setPlayerNames(blackName, whiteName);

    // GUI本体の盤面反転状態を反映
    dlg->setFlipMode(m_boardFlipped);

    // lastUsiMoveがまだ空の場合の追加フォールバック
    if (lastUsiMove.isEmpty()) {
        const QString baseSfenNormalized = normalizedSfen(currentSfen);

        // 開始局面の場合はlastUsiMoveは不要
        if (isStartposSfen(baseSfenNormalized)) {
            qCDebug(lcUi) << "onPvRowClicked: start position detected, no lastUsiMove needed";
        } else {
            qCDebug(lcUi) << "onPvRowClicked: lastUsiMove is empty, will use diffSfenForHighlight";
        }
    }
    if (!lastUsiMove.isEmpty()) {
        qCDebug(lcUi) << "onPvRowClicked: calling setLastMove with:" << lastUsiMove;
        dlg->setLastMove(lastUsiMove);
    } else {
        qCDebug(lcUi) << "onPvRowClicked: lastUsiMove is EMPTY, no highlight will be set";
    }
    if (!prevSfenForHighlight.isEmpty()) {
        qCDebug(lcUi) << "onPvRowClicked: calling setPrevSfenForHighlight";
        dlg->setPrevSfenForHighlight(prevSfenForHighlight);
    }

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void PvClickController::onPvDialogFinished(int result)
{
    Q_UNUSED(result)

    QObject* dlg = sender();
    if (!dlg) return;

    bool ok = false;
    const int engineIndex = dlg->property("pv_engine_index").toInt(&ok);
    if (!ok) return;

    emit pvDialogClosed(engineIndex);
}

// --------------------------------------------------------
// Private メソッド
// --------------------------------------------------------

ShogiEngineThinkingModel* PvClickController::modelForEngineIndex(int engineIndex) const
{
    switch (engineIndex) {
    case 0:
        return m_modelThinking1;
    case 1:
        return m_modelThinking2;
    case 2:
        return m_considerationModel;
    default:
        return nullptr;
    }
}

void PvClickController::resolveHighlightContext(const QString& currentSfen,
                                                QString& lastUsiMove,
                                                QString& prevSfenForHighlight) const
{
    // 読み筋の局面が表示中の手順の何手目かを探し、その直前の局面と、その局面に至った手を使う
    if (currentSfen.isEmpty()) return;
    const QString curNorm = normalizedSfen(currentSfen);
    int ply = -1;
    for (int i = 0; i <= m_line.moveCount(); ++i) {
        if (normalizedSfen(m_line.sfenAt(i)) == curNorm) {
            ply = i;
            break;
        }
    }
    qCDebug(lcUi) << "onPvRowClicked: ply of base position in current line=" << ply;
    if (ply <= 0) return;

    prevSfenForHighlight = m_line.sfenAt(ply - 1);
    if (lastUsiMove.isEmpty()) {
        lastUsiMove = m_line.usiMove(ply).toString();
    }
}

QString PvClickController::resolveCurrentSfen(const QString& baseSfen) const
{
    QString currentSfen = baseSfen;
    qCDebug(lcUi) << "baseSfen from record:" << currentSfen;

    // 読み筋に局面が付いていなければ、表示中の局面とみなす
    const QString displayedSfen = !m_currentSfenStr.isEmpty()
        ? m_currentSfenStr
        : m_line.sfenAt(m_line.currentPly).trimmed();
    if (currentSfen.isEmpty()) {
        currentSfen = displayedSfen;
    }

    // baseSfenがstartposでも、現在局面がstartposでないなら現在局面を優先
    if (!currentSfen.isEmpty() && isStartposSfen(currentSfen)
        && !displayedSfen.isEmpty() && !isStartposSfen(displayedSfen)) {
        qCDebug(lcUi) << "override startpos baseSfen with currentSfen=" << displayedSfen;
        currentSfen = displayedSfen;
    }

    if (currentSfen.isEmpty()) {
        currentSfen = m_startSfenStr;
    }
    if (currentSfen.isEmpty()) {
        currentSfen = SfenUtils::hirateSfen();
    }
    return currentSfen;
}

void PvClickController::resolvePlayerNames(QString& blackName, QString& whiteName) const
{
    blackName = m_players.name(Turn::Black);
    whiteName = m_players.name(Turn::White);
    if (blackName.isEmpty()) blackName = tr("先手");
    if (whiteName.isEmpty()) whiteName = tr("後手");
}
