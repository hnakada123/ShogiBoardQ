/// @file pvclickcontroller.cpp
/// @brief 読み筋クリックコントローラクラスの実装

#include "pvclickcontroller.h"

#include "loggingcategory.h"

#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"
#include "shogiinforecord.h"
#include "shogimove.h"
#include "pvboarddialog.h"

namespace {
static QString normalizedSfen(QString sfen)
{
    sfen = sfen.trimmed();
    if (sfen.startsWith(QStringLiteral("position sfen "))) {
        sfen = sfen.mid(14).trimmed();
    } else if (sfen.startsWith(QStringLiteral("position "))) {
        sfen = sfen.mid(9).trimmed();
    }

    if (sfen == QLatin1String("startpos")) {
        sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }

    const QStringList parts = sfen.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 3) {
        return parts.mid(0, 3).join(' ');
    }

    return sfen;
}

static bool isStartposSfen(const QString& sfen)
{
    static const QString startposSfen =
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    return normalizedSfen(sfen).startsWith(startposSfen.left(60));
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

void PvClickController::setLogModels(UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    m_lineEditModel1 = log1;
    m_lineEditModel2 = log2;
}

void PvClickController::setConsiderationModel(ShogiEngineThinkingModel* model)
{
    m_considerationModel = model;
}

void PvClickController::setSfenRecord(QStringList* sfenRecord)
{
    m_sfenRecord = sfenRecord;
}

void PvClickController::setGameMoves(const QVector<ShogiMove>* gameMoves)
{
    m_gameMoves = gameMoves;
}

void PvClickController::setUsiMoves(const QStringList* usiMoves)
{
    m_usiMoves = usiMoves;
}

// --------------------------------------------------------
// 状態設定
// --------------------------------------------------------

void PvClickController::setPlayMode(PlayMode mode)
{
    m_playMode = mode;
}

void PvClickController::setPlayerNames(const QString& human1, const QString& human2,
                                        const QString& engine1, const QString& engine2)
{
    m_humanName1 = human1;
    m_humanName2 = human2;
    m_engineName1 = engine1;
    m_engineName2 = engine2;
}

void PvClickController::setCurrentSfen(const QString& sfen)
{
    m_currentSfenStr = sfen;
}

void PvClickController::setStartSfen(const QString& sfen)
{
    m_startSfenStr = sfen;
}

void PvClickController::setCurrentRecordIndex(int index)
{
    // Note: This value is no longer used for PV display.
    // The PV dialog uses record->baseSfen() which contains the position
    // from when the PV was generated, not the currently selected position in kifu.
    m_currentRecordIndex = index;
    qCDebug(lcUi) << "setCurrentRecordIndex:" << index << "(not used for PV display)";
}

// --------------------------------------------------------
// スロット
// --------------------------------------------------------

void PvClickController::onPvRowClicked(int engineIndex, int row)
{
    qCDebug(lcUi) << "onPvRowClicked: engineIndex=" << engineIndex << " row=" << row;

    // 対象のモデルを取得
    // engineIndex: 0=エンジン1, 1=エンジン2, 2=検討タブ
    ShogiEngineThinkingModel* model = nullptr;
    if (engineIndex == 0) {
        model = m_modelThinking1;
    } else if (engineIndex == 1) {
        model = m_modelThinking2;
    } else if (engineIndex == 2) {
        model = m_considerationModel;
    }
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

    // USI形式の読み筋を取得
    QString usiPvStr = record->usiPv();
    QStringList usiMoves;
    if (!usiPvStr.isEmpty()) {
        usiMoves = usiPvStr.split(' ', Qt::SkipEmptyParts);
        qCDebug(lcUi) << "onPvRowClicked: usiMoves from record:" << usiMoves;
    }

    // USI moves が空の場合、UsiCommLogModel から検索を試みる
    if (usiMoves.isEmpty()) {
        // engineIndex: 0=エンジン1, 1=エンジン2, 2=検討タブ（エンジン1のログを使用）
        UsiCommLogModel* logModel = (engineIndex == 1) ? m_lineEditModel2 : m_lineEditModel1;
        usiMoves = searchUsiMovesFromLog(logModel);
    }

    //重要: 読み筋（PV）が生成された時点の局面を使用する
    // record->baseSfen() は読み筋生成時の局面SFENを保持している
    // m_currentRecordIndex は棋譜欄で選択中の位置なので、PV表示には使用しない
    QString currentSfen = resolveCurrentSfen(record->baseSfen());
    qCDebug(lcUi) << "onPvRowClicked: currentSfen=" << currentSfen;
    qCDebug(lcUi) << "onPvRowClicked: record->baseSfen()=" << record->baseSfen();
    qCDebug(lcUi) << "onPvRowClicked: record->lastUsiMove()=" << record->lastUsiMove();
    qCDebug(lcUi) << "onPvRowClicked: sfenRecord size=" << (m_sfenRecord ? m_sfenRecord->size() : -1);

    // 起動時の局面に至った最後の手を設定
    // ShogiInfoRecordから取得（読み筋生成時に保存された情報）
    QString lastUsiMove = record->lastUsiMove();
    QString prevSfenForHighlight;
    qCDebug(lcUi) << "onPvRowClicked: lastUsiMove from record=" << lastUsiMove;

    //重要: 読み筋生成時の局面に基づいてprevSfenを検索する
    // m_currentRecordIndex（棋譜欄で選択中の位置）ではなく、
    // currentSfen（読み筋生成時の局面）に一致する位置を検索
    int matchedIndexInRecord = -1;
    if (m_sfenRecord && !currentSfen.isEmpty()) {
        const QString curNorm = normalizedSfen(currentSfen);
        for (int i = 0; i < m_sfenRecord->size(); ++i) {
            if (normalizedSfen(m_sfenRecord->at(i)) == curNorm) {
                matchedIndexInRecord = i;
                break;
            }
        }
        qCDebug(lcUi) << "onPvRowClicked: found matchedIndexInRecord by baseSfen comparison=" << matchedIndexInRecord;
    }

    // lastUsiMoveが空の場合、m_sfenRecordから前の局面を取得してハイライト計算用に使用
    if (matchedIndexInRecord > 0 && m_sfenRecord
        && matchedIndexInRecord - 1 < m_sfenRecord->size()) {
        prevSfenForHighlight = m_sfenRecord->at(matchedIndexInRecord - 1);
        qCDebug(lcUi) << "onPvRowClicked: prevSfenForHighlight from m_sfenRecord["
                      << (matchedIndexInRecord - 1) << "]";

        // lastUsiMoveが空の場合、m_usiMovesから取得を試みる
        if (lastUsiMove.isEmpty() && m_usiMoves
            && !m_usiMoves->isEmpty() && matchedIndexInRecord - 1 < m_usiMoves->size()) {
            lastUsiMove = m_usiMoves->at(matchedIndexInRecord - 1);
            qCDebug(lcUi) << "onPvRowClicked: lastUsiMove from m_usiMoves["
                         << (matchedIndexInRecord - 1) << "]=" << lastUsiMove;
        }
    }
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

QStringList PvClickController::searchUsiMovesFromLog(UsiCommLogModel* logModel) const
{
    QStringList usiMoves;
    if (!logModel) return usiMoves;

    QString fullLog = logModel->usiCommLog();
    QStringList lines = fullLog.split('\n');

    // 後ろから検索して、該当する深さの info pv を探す
    for (qsizetype i = lines.size() - 1; i >= 0; --i) {
        const QString& line = lines.at(i);
        if (line.contains(QStringLiteral("info ")) && line.contains(QStringLiteral(" pv "))) {
            qsizetype pvPos = line.indexOf(QStringLiteral(" pv "));
            if (pvPos >= 0) {
                QString pvPart = line.mid(pvPos + 4).trimmed();
                usiMoves = pvPart.split(' ', Qt::SkipEmptyParts);
                if (!usiMoves.isEmpty()) {
                    qCDebug(lcUi) << "found usiMoves from log:" << usiMoves;
                    break;
                }
            }
        }
    }
    return usiMoves;
}

QString PvClickController::resolveCurrentSfen(const QString& baseSfen) const
{
    QString currentSfen = baseSfen;
    qCDebug(lcUi) << "baseSfen from record:" << currentSfen;

    // baseSfenが空の場合のフォールバック
    if (currentSfen.isEmpty() && m_sfenRecord && m_sfenRecord->size() >= 2) {
        currentSfen = m_sfenRecord->at(m_sfenRecord->size() - 2).trimmed();
        qCDebug(lcUi) << "fallback to sfenRecord[size-2]:" << currentSfen;
    } else if (currentSfen.isEmpty() && m_sfenRecord && !m_sfenRecord->isEmpty()) {
        currentSfen = m_sfenRecord->first().trimmed();
        qCDebug(lcUi) << "fallback to sfenRecord first:" << currentSfen;
    }

    // baseSfenがstartposでも、現在局面がstartposでないなら現在局面を優先
    if (!currentSfen.isEmpty() && isStartposSfen(currentSfen)) {
        QString fallbackCurrent = m_currentSfenStr;
        if (fallbackCurrent.isEmpty() && m_sfenRecord && !m_sfenRecord->isEmpty()) {
            fallbackCurrent = m_sfenRecord->last().trimmed();
        }
        if (!fallbackCurrent.isEmpty() && !isStartposSfen(fallbackCurrent)) {
            qCDebug(lcUi) << "override startpos baseSfen with currentSfen=" << fallbackCurrent;
            currentSfen = fallbackCurrent;
        }
    }

    if (currentSfen.isEmpty()) {
        currentSfen = m_currentSfenStr;
    }
    if (currentSfen.isEmpty()) {
        currentSfen = m_startSfenStr;
    }
    if (currentSfen.isEmpty()) {
        currentSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }
    return currentSfen;
}

void PvClickController::resolvePlayerNames(QString& blackName, QString& whiteName) const
{
    switch (m_playMode) {
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        blackName = m_humanName1.isEmpty() ? tr("先手") : m_humanName1;
        whiteName = m_engineName2.isEmpty() ? tr("後手") : m_engineName2;
        break;
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        blackName = m_engineName1.isEmpty() ? tr("先手") : m_engineName1;
        whiteName = m_humanName2.isEmpty() ? tr("後手") : m_humanName2;
        break;
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
        blackName = m_engineName1.isEmpty() ? tr("先手") : m_engineName1;
        whiteName = m_engineName2.isEmpty() ? tr("後手") : m_engineName2;
        break;
    case PlayMode::HumanVsHuman:
        blackName = m_humanName1.isEmpty() ? tr("先手") : m_humanName1;
        whiteName = m_humanName2.isEmpty() ? tr("後手") : m_humanName2;
        break;
    default:
        blackName = m_humanName1.isEmpty() ? m_engineName1 : m_humanName1;
        whiteName = m_humanName2.isEmpty() ? m_engineName2 : m_humanName2;
        if (blackName.isEmpty()) blackName = tr("先手");
        if (whiteName.isEmpty()) whiteName = tr("後手");
        break;
    }
}

QString PvClickController::resolveLastUsiMove() const
{
    QString lastUsiMove;
    qCDebug(lcUi) << "m_usiMoves size=" << (m_usiMoves ? m_usiMoves->size() : -1)
                  << " m_gameMoves size=" << (m_gameMoves ? m_gameMoves->size() : -1);

    if (m_usiMoves && !m_usiMoves->isEmpty()) {
        lastUsiMove = m_usiMoves->last();
        qCDebug(lcUi) << "using m_usiMoves.last():" << lastUsiMove;
    } else if (m_gameMoves && !m_gameMoves->isEmpty()) {
        // m_gameMovesから最後の手をUSI形式に変換
        const ShogiMove& lastMove = m_gameMoves->last();
        int fromFile = lastMove.fromSquare.x() + 1;
        int fromRank = lastMove.fromSquare.y() + 1;
        int toFile = lastMove.toSquare.x() + 1;
        int toRank = lastMove.toSquare.y() + 1;

        qCDebug(lcUi) << "lastMove from m_gameMoves (after +1):"
                      << " fromFile=" << fromFile << " fromRank=" << fromRank
                      << " toFile=" << toFile << " toRank=" << toRank
                      << " isPromotion=" << lastMove.isPromotion;

        if (lastMove.fromSquare.x() == 0 && lastMove.fromSquare.y() == 0) {
            // 駒打ちの場合: P*5e 形式
            QChar pieceChar = lastMove.movingPiece.toUpper();
            QChar rankChar = QChar('a' + toRank - 1);
            lastUsiMove = QString("%1*%2%3").arg(pieceChar).arg(toFile).arg(rankChar);
        } else {
            // 通常の移動: 7g7f 形式
            QChar fromRankChar = QChar('a' + fromRank - 1);
            QChar toRankChar = QChar('a' + toRank - 1);
            lastUsiMove = QString("%1%2%3%4").arg(fromFile).arg(fromRankChar).arg(toFile).arg(toRankChar);
            if (lastMove.isPromotion) {
                lastUsiMove += '+';
            }
        }
        qCDebug(lcUi) << "generated USI move:" << lastUsiMove;
    } else {
        qCDebug(lcUi) << "both m_usiMoves and m_gameMoves are empty";
    }

    return lastUsiMove;
}
