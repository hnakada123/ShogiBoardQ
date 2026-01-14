#include "pvclickcontroller.h"

#include <QDebug>

#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"
#include "shogiinforecord.h"
#include "shogimove.h"
#include "pvboarddialog.h"

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

// --------------------------------------------------------
// スロット
// --------------------------------------------------------

void PvClickController::onPvRowClicked(int engineIndex, int row)
{
    qDebug() << "[PvClick] onPvRowClicked: engineIndex=" << engineIndex << " row=" << row;

    // 対象のモデルを取得
    ShogiEngineThinkingModel* model = (engineIndex == 0) ? m_modelThinking1 : m_modelThinking2;
    if (!model) {
        qDebug() << "[PvClick] onPvRowClicked: model is null";
        return;
    }

    // モデルの行数をチェック
    if (row < 0 || row >= model->rowCount()) {
        qDebug() << "[PvClick] onPvRowClicked: row out of range";
        return;
    }

    // ShogiInfoRecord を取得
    const ShogiInfoRecord* record = model->recordAt(row);
    if (!record) {
        qDebug() << "[PvClick] onPvRowClicked: record is null";
        return;
    }

    // 読み筋（漢字表記）を取得
    QString kanjiPv = record->pv();
    qDebug() << "[PvClick] onPvRowClicked: kanjiPv=" << kanjiPv;

    if (kanjiPv.isEmpty()) {
        qDebug() << "[PvClick] onPvRowClicked: kanjiPv is empty";
        return;
    }

    // USI形式の読み筋を取得
    QString usiPvStr = record->usiPv();
    QStringList usiMoves;
    if (!usiPvStr.isEmpty()) {
        usiMoves = usiPvStr.split(' ', Qt::SkipEmptyParts);
        qDebug() << "[PvClick] onPvRowClicked: usiMoves from record:" << usiMoves;
    }

    // USI moves が空の場合、UsiCommLogModel から検索を試みる
    if (usiMoves.isEmpty()) {
        UsiCommLogModel* logModel = (engineIndex == 0) ? m_lineEditModel1 : m_lineEditModel2;
        usiMoves = searchUsiMovesFromLog(logModel);
    }

    // 現在の局面SFENを取得
    QString currentSfen = resolveCurrentSfen(record->baseSfen());
    qDebug() << "[PvClick] onPvRowClicked: currentSfen=" << currentSfen;
    qDebug() << "[PvClick] onPvRowClicked: record->baseSfen()=" << record->baseSfen();
    qDebug() << "[PvClick] onPvRowClicked: record->lastUsiMove()=" << record->lastUsiMove();

    // PvBoardDialog を表示
    PvBoardDialog* dlg = new PvBoardDialog(currentSfen, usiMoves, qobject_cast<QWidget*>(parent()));
    dlg->setKanjiPv(kanjiPv);

    // 対局者名を設定
    QString blackName, whiteName;
    resolvePlayerNames(blackName, whiteName);
    dlg->setPlayerNames(blackName, whiteName);

    // 起動時の局面に至った最後の手を設定
    // まずShogiInfoRecordから取得を試みる（棋譜解析モード用）
    QString lastUsiMove = record->lastUsiMove();
    qDebug() << "[PvClick] onPvRowClicked: lastUsiMove from record=" << lastUsiMove;
    if (lastUsiMove.isEmpty()) {
        // フォールバック: m_usiMovesまたはm_gameMovesから取得
        lastUsiMove = resolveLastUsiMove();
        qDebug() << "[PvClick] onPvRowClicked: lastUsiMove from fallback=" << lastUsiMove;
    }
    if (!lastUsiMove.isEmpty()) {
        qDebug() << "[PvClick] onPvRowClicked: calling setLastMove with:" << lastUsiMove;
        dlg->setLastMove(lastUsiMove);
    } else {
        qDebug() << "[PvClick] onPvRowClicked: lastUsiMove is EMPTY, no highlight will be set";
    }

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
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
                    qDebug() << "[PvClick] found usiMoves from log:" << usiMoves;
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
    qDebug() << "[PvClick] baseSfen from record:" << currentSfen;

    // baseSfenが空の場合のフォールバック
    if (currentSfen.isEmpty() && m_sfenRecord && m_sfenRecord->size() >= 2) {
        currentSfen = m_sfenRecord->at(m_sfenRecord->size() - 2).trimmed();
        qDebug() << "[PvClick] fallback to sfenRecord[size-2]:" << currentSfen;
    } else if (currentSfen.isEmpty() && m_sfenRecord && !m_sfenRecord->isEmpty()) {
        currentSfen = m_sfenRecord->first().trimmed();
        qDebug() << "[PvClick] fallback to sfenRecord first:" << currentSfen;
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
    qDebug() << "[PvClick] m_usiMoves size=" << (m_usiMoves ? m_usiMoves->size() : -1)
             << " m_gameMoves size=" << (m_gameMoves ? m_gameMoves->size() : -1);

    if (m_usiMoves && !m_usiMoves->isEmpty()) {
        lastUsiMove = m_usiMoves->last();
        qDebug() << "[PvClick] using m_usiMoves.last():" << lastUsiMove;
    } else if (m_gameMoves && !m_gameMoves->isEmpty()) {
        // m_gameMovesから最後の手をUSI形式に変換
        const ShogiMove& lastMove = m_gameMoves->last();
        int fromFile = lastMove.fromSquare.x() + 1;
        int fromRank = lastMove.fromSquare.y() + 1;
        int toFile = lastMove.toSquare.x() + 1;
        int toRank = lastMove.toSquare.y() + 1;

        qDebug() << "[PvClick] lastMove from m_gameMoves (after +1):"
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
        qDebug() << "[PvClick] generated USI move:" << lastUsiMove;
    } else {
        qDebug() << "[PvClick] both m_usiMoves and m_gameMoves are empty";
    }

    return lastUsiMove;
}
