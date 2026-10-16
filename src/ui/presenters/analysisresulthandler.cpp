/// @file analysisresulthandler.cpp
/// @brief 解析結果ハンドラクラスの実装

#include "analysisresulthandler.h"
#include "evaluationseries.h"

#include "analysiscoordinator.h"
#include "kifuanalysislistmodel.h"
#include "kifuanalysisresultsdisplay.h"
#include "sfenutils.h"

#include <QString>
#include <QStringList>

#include "logcategories.h"
#include "analysisresultspanel.h"
#include "pvboarddialog.h"

namespace {
QString sanitizeUsiPv(const QString& rawPv, bool isBook)
{
    if (isBook) {
        return QString();
    }

    QString usiPv = rawPv;
    const qsizetype parenPos = usiPv.indexOf(QLatin1Char('('));
    if (parenPos > 0) {
        usiPv = usiPv.left(parenPos).trimmed();
    }
    // 詰んだ局面で返る resign / win は指し手ではないため、候補手や読み筋の盤面表示に使わない
    QStringList moves;
    const QStringList tokens = usiPv.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        if (token == QLatin1String("resign") || token == QLatin1String("win")) break;
        moves.append(token);
    }
    return moves.join(QLatin1Char(' '));
}

QString resolveMoveLabel(const AnalysisResultHandler::Refs& refs, int ply)
{
    if (ply == 0) return QObject::tr("=== 開始局面 ===");
    if (refs.line && ply > 0 && ply <= refs.line->moveCount()) {
        const QString label = refs.line->displayText(ply);
        if (!label.isEmpty()) return label;
    }
    return QStringLiteral("ply %1").arg(ply);
}

QString resolveLastUsiMove(const AnalysisResultHandler::Refs& refs, int ply)
{
    return refs.line ? refs.line->usiMove(ply).toString() : QString();
}

} // namespace

AnalysisResultHandler::AnalysisResultHandler(const Refs& refs)
    : m_refs(refs)
{
}

void AnalysisResultHandler::updatePending(int ply, const UsiInfo& info)
{
    // bestmove で確定するまで、最新の info で上書きし続ける。
    // 漢字の読み筋（updatePendingPvKanji）は思考情報の整形から届くので、ここでは消さない
    if (m_pendingPly != ply) {
        m_pendingScore = {};
        m_pendingPv.clear();
    }
    m_pendingPly = ply;
    // エンジン設定で MultiPV を2以上にしていても、結果は最善の読み筋で確定する
    if (info.multipv > 1) return;
    if (info.hasScore()) m_pendingScore = info.score;
    if (info.hasPv()) m_pendingPv = info.pv.join(QLatin1Char(' '));
}

void AnalysisResultHandler::updatePendingPvKanji(const QString& pvKanjiStr, int multipv)
{
    if (multipv > 1) return;
    // ThinkingInfoPresenterからの漢字PVを保存（思考タブと同じ内容を棋譜解析結果に使用）
    m_pendingPvKanji = pvKanjiStr;
    qCDebug(lcAnalysis).noquote() << "saved pvKanjiStr=" << pvKanjiStr.left(50);
}

void AnalysisResultHandler::commitPendingResult()
{
    if (!m_refs.analysisModel) {
        qCDebug(lcAnalysis).noquote() << "commitPendingResult: analysisModel is null";
        return;
    }

    // m_pendingPlyが-1の場合は定跡（infoなしでbestmoveが来た）
    // m_coordから現在のplyを取得
    int ply = m_pendingPly;
    bool isBook = false;
    if (ply < 0 && m_refs.coord) {
        ply = m_refs.coord->currentPly();
        isBook = true;
        qCDebug(lcAnalysis).noquote() << "book move detected, ply=" << ply;
    }

    if (ply < 0) {
        qCDebug(lcAnalysis).noquote() << "commitPendingResult skipped: ply=" << ply;
        return;
    }

    const UsiScore score = isBook ? UsiScore{} : m_pendingScore;
    const QString usiPv = sanitizeUsiPv(m_pendingPv, isBook);

    // 漢字PVがあればそれを使用、なければUSI形式PV、定跡なら「定跡」
    QString pv;
    if (isBook) {
        pv = tr("（定跡）");
    } else if (!m_pendingPvKanji.isEmpty()) {
        pv = m_pendingPvKanji;
    } else {
        pv = m_pendingPv;
    }

    // 一時保存をリセット
    m_pendingPly = -1;
    m_pendingScore = {};
    m_pendingPv.clear();
    m_pendingPvKanji.clear();

    const QString moveLabel = resolveMoveLabel(m_refs, ply);

    const QString position = m_refs.line && ply <= m_refs.line->moveCount()
        ? SfenUtils::normalizePositionLikeSfen(m_refs.line->sfenAt(ply)) : QString();
    const bool isGoteTurn = position.isEmpty() ? ply % 2 == 1
        : position.section(QLatin1Char(' '), 1, 1) == QStringLiteral("w");
    // エンジンは手番側から見た評価値を出す（先手から見た値への直しは評価値グラフの点と同じ規則）
    const UsiScore blackScore = EvaluationSeries::blackView(isGoteTurn ? Turn::White : Turn::Black, score);
    std::optional<int> difference;
    if (blackScore.isCp()) {
        if (m_previousCp) difference = blackScore.value - *m_previousCp;
        m_previousCp = blackScore.value;
    } else {
        m_previousCp.reset();
    }

    qCDebug(lcAnalysis).noquote() << "commitPendingResult: ply=" << ply << "moveLabel=" << moveLabel << "pv=" << pv.left(30);

    auto* resultItem = new KifuAnalysisResultsDisplay(moveLabel, blackScore, difference, pv);

    resultItem->setUsiPv(usiPv);
    if (m_refs.line && ply > 0 && ply <= m_refs.line->moveCount() + 1)
        resultItem->beforeSfen = SfenUtils::normalizePositionLikeSfen(m_refs.line->sfenAt(ply - 1));

    // 局面SFENを設定
    if (m_refs.line && ply >= 0 && ply <= m_refs.line->moveCount()) {
        const QString sfen = SfenUtils::normalizePositionLikeSfen(m_refs.line->sfenAt(ply));
        resultItem->setSfen(sfen);
    }

    const QString lastMove = resolveLastUsiMove(m_refs, ply);
    if (!lastMove.isEmpty()) {
        resultItem->setLastUsiMove(lastMove);
        qCDebug(lcAnalysis).noquote() << "setLastUsiMove: ply=" << ply << "lastMove=" << lastMove;
    }

    // 候補手を設定（前の行の読み筋の最初の指し手）
    int prevRow = m_refs.analysisModel->rowCount() - 1;  // 今追加しようとしている行の1つ前
    if (prevRow >= 0) {
        KifuAnalysisResultsDisplay* prevItem = m_refs.analysisModel->item(prevRow);
        if (prevItem) {
            if (!resultItem->beforeSfen.isEmpty() && resultItem->beforeSfen == prevItem->sfen()) {
                resultItem->candidateUsi = prevItem->usiPv().simplified().section(QLatin1Char(' '), 0, 0);
                resultItem->candidateSfen = prevItem->sfen();
            }
            if (!resultItem->candidateUsi.isEmpty()) {
                QString candidate = KifuPresentation::move(resultItem->candidateSfen, resultItem->candidateUsi,
                    {KifuPresentation::Notation::Japanese, false});
                const QString previous = prevItem->lastUsiMove();
                if (previous.size() >= 4 && previous.mid(2, 2) == resultItem->candidateUsi.mid(2, 2))
                    candidate.replace(1, 2, QStringLiteral("同　"));
                resultItem->setCandidateMove(candidate);
            }
        }
    }

    m_refs.analysisModel->appendItem(resultItem);

    // グラフには次の局面の準備のときに足す
    m_lastCommittedPly = ply;
    m_lastCommittedScore = blackScore;
}

// ============================================================
// AnalysisResultHandler のPVダイアログ・棋譜変換処理
// ============================================================

void AnalysisResultHandler::showPvBoardDialog(int row)
{
    qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: row=" << row;

    if (!m_refs.analysisModel) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: analysisModel is null";
        return;
    }

    if (row < 0 || row >= m_refs.analysisModel->rowCount()) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: row out of range";
        return;
    }

    KifuAnalysisResultsDisplay* item = m_refs.analysisModel->item(row);
    if (!item) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: item is null";
        return;
    }

    const QString kanjiPv = item->principalVariation();
    if (kanjiPv.isEmpty()) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: kanjiPv is empty";
        return;
    }

    QString baseSfen = item->sfen();
    // 各結果は対象局面を保持している。表示行番号を手数として再解釈しない。
    if (baseSfen.isEmpty()) return;

    QStringList usiMoves;
    const QString usiPvStr = item->usiPv();
    if (!usiPvStr.isEmpty()) {
        usiMoves = usiPvStr.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    }

    QWidget* parentWidget = nullptr;
    if (m_refs.presenter && m_refs.presenter->container()) {
        parentWidget = m_refs.presenter->container();
    }

    auto* dlg = new PvBoardDialog(baseSfen, usiMoves, parentWidget);
    dlg->setKanjiPv(kanjiPv);

    const QString blackName = m_refs.blackPlayerName.isEmpty() ? tr("先手") : m_refs.blackPlayerName;
    const QString whiteName = m_refs.whitePlayerName.isEmpty() ? tr("後手") : m_refs.whitePlayerName;
    dlg->setPlayerNames(blackName, whiteName);
    dlg->setFlipMode(m_refs.boardFlipped);

    QString lastMove = item->lastUsiMove();
    if (!lastMove.isEmpty()) {
        dlg->setLastMove(lastMove);
    }

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}
