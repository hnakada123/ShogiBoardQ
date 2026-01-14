#include "kifuexportcontroller.h"

#include <QWidget>
#include <QClipboard>
#include <QApplication>
#include <QStatusBar>
#include <QTableWidget>
#include <QDebug>
#include <QMessageBox>

#include "gamerecordmodel.h"
#include "kifurecordlistmodel.h"
#include "gameinfopanecontroller.h"
#include "timecontrolcontroller.h"
#include "kifuloadcoordinator.h"
#include "recordpresenter.h"  // GameRecordPresenter
#include "matchcoordinator.h"
#include "replaycontroller.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "kifusavecoordinator.h"
#include "kifucontentbuilder.h"
#include "kifuclipboardservice.h"
#include "shogimove.h"

KifuExportController::KifuExportController(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
{
}

KifuExportController::~KifuExportController() = default;

void KifuExportController::setDependencies(const Dependencies& deps)
{
    m_deps = deps;
}

void KifuExportController::updateState(const QString& startSfen, PlayMode mode,
                                        const QString& human1, const QString& human2,
                                        const QString& engine1, const QString& engine2,
                                        int activeRow, int moveIndex, int aPly, int selPly)
{
    m_deps.startSfenStr = startSfen;
    m_deps.playMode = mode;
    m_deps.humanName1 = human1;
    m_deps.humanName2 = human2;
    m_deps.engineName1 = engine1;
    m_deps.engineName2 = engine2;
    m_deps.activeResolvedRow = activeRow;
    m_deps.currentMoveIndex = moveIndex;
    m_deps.activePly = aPly;
    m_deps.currentSelectedPly = selPly;
}

// --------------------------------------------------------
// ユーティリティ
// --------------------------------------------------------

QStringList KifuExportController::resolveUsiMoves() const
{
    // 1. m_usiMovesを優先
    if (m_deps.usiMoves && !m_deps.usiMoves->isEmpty()) {
        return *m_deps.usiMoves;
    }
    
    // 2. KifuLoadCoordinatorから取得
    if (m_deps.kifuLoadCoordinator) {
        QStringList moves = m_deps.kifuLoadCoordinator->kifuUsiMoves();
        if (!moves.isEmpty()) {
            qDebug().noquote() << "[KifuExport] kifuUsiMoves from KifuLoadCoordinator, size =" << moves.size();
            return moves;
        }
    }
    
    // 3. SFENレコードから生成
    if (m_deps.sfenRecord && m_deps.sfenRecord->size() > 1) {
        QStringList moves = sfenRecordToUsiMoves();
        qDebug().noquote() << "[KifuExport] usiMoves from sfenRecord, size =" << moves.size();
        return moves;
    }
    
    return QStringList();
}

GameRecordModel::ExportContext KifuExportController::buildExportContext() const
{
    GameRecordModel::ExportContext ctx;
    ctx.gameInfoTable = m_deps.gameInfoController ? m_deps.gameInfoController->tableWidget() : nullptr;
    ctx.recordModel = m_deps.kifuRecordModel;
    ctx.startSfen = m_deps.startSfenStr;
    ctx.playMode = m_deps.playMode;
    ctx.human1 = m_deps.humanName1;
    ctx.human2 = m_deps.humanName2;
    ctx.engine1 = m_deps.engineName1;
    ctx.engine2 = m_deps.engineName2;
    
    // 時間制御情報
    if (m_deps.timeController) {
        ctx.hasTimeControl = m_deps.timeController->hasTimeControl();
        ctx.initialTimeMs = static_cast<int>(m_deps.timeController->baseTimeMs());
        ctx.byoyomiMs = static_cast<int>(m_deps.timeController->byoyomiMs());
        ctx.fischerIncrementMs = static_cast<int>(m_deps.timeController->incrementMs());
        ctx.gameStartDateTime = m_deps.timeController->gameStartDateTime();
    }
    
    return ctx;
}

bool KifuExportController::isCurrentlyPlaying() const
{
    const bool isPlayingMode = (m_deps.playMode == PlayMode::EvenHumanVsEngine ||
                                m_deps.playMode == PlayMode::EvenEngineVsHuman ||
                                m_deps.playMode == PlayMode::EvenEngineVsEngine ||
                                m_deps.playMode == PlayMode::HandicapEngineVsHuman ||
                                m_deps.playMode == PlayMode::HandicapHumanVsEngine ||
                                m_deps.playMode == PlayMode::HandicapEngineVsEngine ||
                                m_deps.playMode == PlayMode::HumanVsHuman);
    
    const bool isGameOver = (m_deps.match && m_deps.match->gameOverState().isOver);
    
    return isPlayingMode && !isGameOver;
}

int KifuExportController::currentPly() const
{
    // ライブ追記モード中はUI側のトラッキング値を優先
    const bool liveAppend = m_deps.replayController ? m_deps.replayController->isLiveAppendMode() : false;
    if (liveAppend) {
        if (m_deps.currentSelectedPly >= 0) return m_deps.currentSelectedPly;
    }
    
    // 通常時はactivePlyを優先
    if (m_deps.activePly >= 0) return m_deps.activePly;
    
    return 0;
}

// --------------------------------------------------------
// ファイル保存
// --------------------------------------------------------

QString KifuExportController::saveToFile()
{
    if (!m_deps.gameRecord) {
        Q_EMIT statusMessage(tr("棋譜データがありません"), 3000);
        return QString();
    }
    
    QStringList usiMovesForExport = resolveUsiMoves();
    GameRecordModel::ExportContext ctx = buildExportContext();
    
    // 各形式の行リストを生成
    QStringList kifLines = m_deps.gameRecord->toKifLines(ctx);
    QStringList ki2Lines = m_deps.gameRecord->toKi2Lines(ctx);
    QStringList csaLines = m_deps.gameRecord->toCsaLines(ctx, usiMovesForExport);
    QStringList jkfLines = m_deps.gameRecord->toJkfLines(ctx);
    QStringList usenLines = m_deps.gameRecord->toUsenLines(ctx, usiMovesForExport);
    QStringList usiLines = m_deps.gameRecord->toUsiLines(ctx, usiMovesForExport);
    
    qDebug().noquote() << "[KifuExport] saveToFile: generated"
                       << kifLines.size() << "KIF,"
                       << ki2Lines.size() << "KI2,"
                       << csaLines.size() << "CSA,"
                       << jkfLines.size() << "JKF,"
                       << usenLines.size() << "USEN,"
                       << usiLines.size() << "USI lines";
    
    m_kifuDataList = kifLines;
    
    const QString path = KifuSaveCoordinator::saveViaDialogWithUsi(
        m_parentWidget,
        kifLines, ki2Lines, csaLines,
        jkfLines, usenLines, usiLines,
        m_deps.playMode,
        m_deps.humanName1, m_deps.humanName2,
        m_deps.engineName1, m_deps.engineName2);
    
    if (!path.isEmpty()) {
        if (m_deps.gameRecord) {
            m_deps.gameRecord->clearDirty();
        }
        Q_EMIT statusMessage(tr("棋譜を保存しました: %1").arg(path), 5000);
        Q_EMIT fileSaved(path);
    }
    
    return path;
}

bool KifuExportController::overwriteFile(const QString& filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }
    
    QStringList kifLines;
    
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        kifLines = m_deps.gameRecord->toKifLines(ctx);
        qDebug().noquote() << "[KifuExport] overwriteFile: generated" << kifLines.size() << "lines";
    } else {
        // フォールバック
        KifuExportContext ctx;
        ctx.gameInfoTable = m_deps.gameInfoController ? m_deps.gameInfoController->tableWidget() : nullptr;
        ctx.recordModel = m_deps.kifuRecordModel;
        ctx.resolvedRows = m_deps.resolvedRows;
        if (m_deps.recordPresenter) {
            ctx.liveDisp = &m_deps.recordPresenter->liveDisp();
        }
        ctx.commentsByRow = m_deps.commentsByRow;
        ctx.activeResolvedRow = m_deps.activeResolvedRow;
        ctx.startSfen = m_deps.startSfenStr;
        ctx.playMode = m_deps.playMode;
        ctx.human1 = m_deps.humanName1;
        ctx.human2 = m_deps.humanName2;
        ctx.engine1 = m_deps.engineName1;
        ctx.engine2 = m_deps.engineName2;
        
        kifLines = KifuContentBuilder::buildKifuDataList(ctx);
    }
    
    m_kifuDataList = kifLines;
    
    QString error;
    const bool ok = KifuSaveCoordinator::overwriteExisting(filePath, kifLines, &error);
    
    if (ok) {
        if (m_deps.gameRecord) {
            m_deps.gameRecord->clearDirty();
        }
        Q_EMIT statusMessage(tr("棋譜を上書き保存しました: %1").arg(filePath), 5000);
        Q_EMIT fileSaved(filePath);
    } else {
        QMessageBox::warning(m_parentWidget, tr("KIF Save Error"), error);
    }
    
    return ok;
}

// --------------------------------------------------------
// クリップボードコピー
// --------------------------------------------------------

bool KifuExportController::copyKifToClipboard()
{
    if (!m_deps.gameRecord) {
        Q_EMIT statusMessage(tr("KIF形式の棋譜データがありません"), 3000);
        return false;
    }
    
    KifuClipboardService::ExportContext ctx;
    ctx.gameInfoTable = m_deps.gameInfoController ? m_deps.gameInfoController->tableWidget() : nullptr;
    ctx.recordModel = m_deps.kifuRecordModel;
    ctx.gameRecord = m_deps.gameRecord;
    ctx.startSfen = m_deps.startSfenStr;
    ctx.playMode = m_deps.playMode;
    ctx.human1 = m_deps.humanName1;
    ctx.human2 = m_deps.humanName2;
    ctx.engine1 = m_deps.engineName1;
    ctx.engine2 = m_deps.engineName2;
    
    if (KifuClipboardService::copyKif(ctx)) {
        Q_EMIT statusMessage(tr("KIF形式の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    } else {
        Q_EMIT statusMessage(tr("KIF形式の棋譜データがありません"), 3000);
        return false;
    }
}

bool KifuExportController::copyKi2ToClipboard()
{
    if (!m_deps.gameRecord) {
        Q_EMIT statusMessage(tr("KI2形式の棋譜データがありません"), 3000);
        return false;
    }
    
    KifuClipboardService::ExportContext ctx;
    ctx.gameInfoTable = m_deps.gameInfoController ? m_deps.gameInfoController->tableWidget() : nullptr;
    ctx.recordModel = m_deps.kifuRecordModel;
    ctx.gameRecord = m_deps.gameRecord;
    ctx.startSfen = m_deps.startSfenStr;
    ctx.playMode = m_deps.playMode;
    ctx.human1 = m_deps.humanName1;
    ctx.human2 = m_deps.humanName2;
    ctx.engine1 = m_deps.engineName1;
    ctx.engine2 = m_deps.engineName2;
    
    if (KifuClipboardService::copyKi2(ctx)) {
        Q_EMIT statusMessage(tr("KI2形式の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    } else {
        Q_EMIT statusMessage(tr("KI2形式の棋譜データがありません"), 3000);
        return false;
    }
}

bool KifuExportController::copyCsaToClipboard()
{
    QStringList usiMovesForCsa = resolveUsiMoves();
    
    QStringList csaLines;
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        csaLines = m_deps.gameRecord->toCsaLines(ctx, usiMovesForCsa);
        qDebug().noquote() << "[KifuExport] copyCsaToClipboard: generated" << csaLines.size() << "lines";
    }
    
    if (csaLines.isEmpty()) {
        Q_EMIT statusMessage(tr("CSA形式の棋譜データがありません"), 3000);
        return false;
    }
    
    const QString csaText = csaLines.join(QStringLiteral("\n"));
    
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(csaText);
        Q_EMIT statusMessage(tr("CSA形式の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    }
    
    Q_EMIT statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

bool KifuExportController::copyUsiToClipboard()
{
    QStringList usiMovesForOutput = resolveUsiMoves();
    
    QStringList usiLines;
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        usiLines = m_deps.gameRecord->toUsiLines(ctx, usiMovesForOutput);
        qDebug().noquote() << "[KifuExport] copyUsiToClipboard: generated" << usiLines.size() << "lines";
    }
    
    if (usiLines.isEmpty()) {
        Q_EMIT statusMessage(tr("USI形式の棋譜データがありません"), 3000);
        return false;
    }
    
    const QString usiText = usiLines.join(QStringLiteral("\n"));
    
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(usiText);
        Q_EMIT statusMessage(tr("USI形式の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    }
    
    Q_EMIT statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

bool KifuExportController::copyUsiCurrentToClipboard()
{
    QStringList usiMovesForOutput = resolveUsiMoves();
    
    // 現在の手数まで制限
    const int limit = m_deps.currentMoveIndex;
    if (limit >= 0 && limit < usiMovesForOutput.size()) {
        usiMovesForOutput = usiMovesForOutput.mid(0, limit);
        qDebug().noquote() << "[KifuExport] copyUsiCurrentToClipboard: limited to" << limit << "moves";
    }
    
    QStringList usiLines;
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        usiLines = m_deps.gameRecord->toUsiLines(ctx, usiMovesForOutput);
    }
    
    if (usiLines.isEmpty()) {
        Q_EMIT statusMessage(tr("USI形式の棋譜データがありません"), 3000);
        return false;
    }
    
    const QString usiText = usiLines.join(QStringLiteral("\n"));
    
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(usiText);
        Q_EMIT statusMessage(tr("USI形式（現在の指し手まで）の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    }
    
    Q_EMIT statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

bool KifuExportController::copyJkfToClipboard()
{
    QStringList jkfLines;
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        jkfLines = m_deps.gameRecord->toJkfLines(ctx);
        qDebug().noquote() << "[KifuExport] copyJkfToClipboard: generated" << jkfLines.size() << "lines";
    }
    
    if (jkfLines.isEmpty()) {
        Q_EMIT statusMessage(tr("JKF形式の棋譜データがありません"), 3000);
        return false;
    }
    
    const QString jkfText = jkfLines.join(QStringLiteral("\n"));
    
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(jkfText);
        Q_EMIT statusMessage(tr("JKF形式の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    }
    
    Q_EMIT statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

bool KifuExportController::copyUsenToClipboard()
{
    QStringList usiMovesForOutput = resolveUsiMoves();
    
    QStringList usenLines;
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        usenLines = m_deps.gameRecord->toUsenLines(ctx, usiMovesForOutput);
        qDebug().noquote() << "[KifuExport] copyUsenToClipboard: generated" << usenLines.size() << "lines";
    }
    
    if (usenLines.isEmpty()) {
        Q_EMIT statusMessage(tr("USEN形式の棋譜データがありません"), 3000);
        return false;
    }
    
    const QString usenText = usenLines.join(QStringLiteral("\n"));
    
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(usenText);
        Q_EMIT statusMessage(tr("USEN形式の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    }
    
    Q_EMIT statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

KifuExportController::PositionData KifuExportController::getCurrentPositionData() const
{
    PositionData data;
    
    if (m_deps.sfenRecord && !m_deps.sfenRecord->isEmpty()) {
        int idx = -1;
        
        if (isCurrentlyPlaying()) {
            idx = static_cast<int>(m_deps.sfenRecord->size() - 1);
        } else {
            idx = currentPly();
        }
        
        if (idx < 0) idx = 0;
        else if (idx >= m_deps.sfenRecord->size()) {
            idx = static_cast<int>(m_deps.sfenRecord->size() - 1);
        }
        
        data.sfenStr = m_deps.sfenRecord->at(idx);
        data.moveIndex = idx;
        
        // 最終手の情報を取得
        if (idx > 0 && m_deps.kifuRecordModel && idx < m_deps.kifuRecordModel->rowCount()) {
            QModelIndex modelIdx = m_deps.kifuRecordModel->index(idx, 0);
            data.lastMoveStr = m_deps.kifuRecordModel->data(modelIdx, Qt::DisplayRole).toString();
        }
    } else if (m_deps.gameController && m_deps.gameController->board()) {
        ShogiBoard* board = m_deps.gameController->board();
        const QString boardSfen = board->convertBoardToSfen();
        QString standSfen = board->convertStandToSfen();
        if (standSfen.isEmpty()) standSfen = QStringLiteral("-");
        const QString turn = board->currentPlayer();
        const int moveNum = qMax(1, currentPly() + 1);
        data.sfenStr = QStringLiteral("%1 %2 %3 %4").arg(boardSfen, turn, standSfen, QString::number(moveNum));
        data.moveIndex = currentPly();
    }
    
    return data;
}

bool KifuExportController::copySfenToClipboard()
{
    PositionData pos = getCurrentPositionData();
    
    if (pos.sfenStr.isEmpty()) {
        Q_EMIT statusMessage(tr("SFEN形式の局面データがありません"), 3000);
        return false;
    }
    
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(pos.sfenStr);
        Q_EMIT statusMessage(tr("SFEN形式の局面をクリップボードにコピーしました"), 3000);
        qDebug().noquote() << "[KifuExport] copySfenToClipboard: copied" << pos.sfenStr.size() << "chars";
        return true;
    }
    
    Q_EMIT statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

QString KifuExportController::generateBodText(const PositionData& pos) const
{
    if (pos.sfenStr.isEmpty()) return QString();
    
    const QStringList parts = pos.sfenStr.split(QLatin1Char(' '));
    if (parts.size() < 4) return QString();
    
    const QString boardSfen = parts[0];
    const QString turnSfen = parts[1];
    const QString handSfen = parts[2];
    const int sfenMoveNum = parts[3].toInt();
    
    // 持ち駒を解析
    QMap<QChar, int> senteHand;
    QMap<QChar, int> goteHand;
    
    if (handSfen != QStringLiteral("-")) {
        int count = 0;
        for (qsizetype i = 0; i < handSfen.size(); ++i) {
            const QChar c = handSfen.at(i);
            if (c.isDigit()) {
                count = count * 10 + c.digitValue();
            } else {
                if (count == 0) count = 1;
                if (c.isUpper()) {
                    senteHand[c] += count;
                } else {
                    goteHand[c.toUpper()] += count;
                }
                count = 0;
            }
        }
    }
    
    // 持ち駒を日本語文字列に変換
    auto handToString = [](const QMap<QChar, int>& hand) -> QString {
        if (hand.isEmpty()) return QStringLiteral("なし");
        
        const QString order = QStringLiteral("RBGSNLP");
        const QMap<QChar, QString> pieceNames = {
            {QLatin1Char('R'), QStringLiteral("飛")},
            {QLatin1Char('B'), QStringLiteral("角")},
            {QLatin1Char('G'), QStringLiteral("金")},
            {QLatin1Char('S'), QStringLiteral("銀")},
            {QLatin1Char('N'), QStringLiteral("桂")},
            {QLatin1Char('L'), QStringLiteral("香")},
            {QLatin1Char('P'), QStringLiteral("歩")}
        };
        const QMap<int, QString> kanjiNumbers = {
            {2, QStringLiteral("二")}, {3, QStringLiteral("三")}, {4, QStringLiteral("四")},
            {5, QStringLiteral("五")}, {6, QStringLiteral("六")}, {7, QStringLiteral("七")},
            {8, QStringLiteral("八")}, {9, QStringLiteral("九")}, {10, QStringLiteral("十")},
            {11, QStringLiteral("十一")}, {12, QStringLiteral("十二")}, {13, QStringLiteral("十三")},
            {14, QStringLiteral("十四")}, {15, QStringLiteral("十五")}, {16, QStringLiteral("十六")},
            {17, QStringLiteral("十七")}, {18, QStringLiteral("十八")}
        };
        
        QString result;
        for (qsizetype i = 0; i < order.size(); ++i) {
            const QChar piece = order.at(i);
            if (hand.contains(piece) && hand[piece] > 0) {
                result += pieceNames[piece];
                if (hand[piece] > 1) {
                    result += kanjiNumbers.value(hand[piece], QString::number(hand[piece]));
                }
                result += QStringLiteral("　");
            }
        }
        
        return result.isEmpty() ? QStringLiteral("なし") : result.trimmed();
    };
    
    // 盤面を9x9配列に展開
    QVector<QVector<QString>> board(9, QVector<QString>(9));
    const QStringList ranks = boardSfen.split(QLatin1Char('/'));
    for (qsizetype rank = 0; rank < qMin(ranks.size(), qsizetype(9)); ++rank) {
        const QString& rankStr = ranks[rank];
        int file = 0;
        bool promoted = false;
        for (qsizetype k = 0; k < rankStr.size() && file < 9; ++k) {
            const QChar c = rankStr.at(k);
            if (c == QLatin1Char('+')) {
                promoted = true;
            } else if (c.isDigit()) {
                const int skip = c.toLatin1() - '0';
                for (int s = 0; s < skip && file < 9; ++s) {
                    board[rank][file++] = QStringLiteral(" ・");
                }
                promoted = false;
            } else {
                QString piece = c.isUpper() ? QStringLiteral(" ") : QStringLiteral("v");
                const QChar upperC = c.toUpper();
                const QMap<QChar, QString> pieceChars = {
                    {QLatin1Char('P'), promoted ? QStringLiteral("と") : QStringLiteral("歩")},
                    {QLatin1Char('L'), promoted ? QStringLiteral("杏") : QStringLiteral("香")},
                    {QLatin1Char('N'), promoted ? QStringLiteral("圭") : QStringLiteral("桂")},
                    {QLatin1Char('S'), promoted ? QStringLiteral("全") : QStringLiteral("銀")},
                    {QLatin1Char('G'), QStringLiteral("金")},
                    {QLatin1Char('B'), promoted ? QStringLiteral("馬") : QStringLiteral("角")},
                    {QLatin1Char('R'), promoted ? QStringLiteral("龍") : QStringLiteral("飛")},
                    {QLatin1Char('K'), QStringLiteral("玉")}
                };
                piece += pieceChars.value(upperC, QStringLiteral("？"));
                board[rank][file++] = piece;
                promoted = false;
            }
        }
        while (file < 9) {
            board[rank][file++] = QStringLiteral(" ・");
        }
    }
    
    // BOD形式の文字列を生成
    QStringList bodLines;
    bodLines << QStringLiteral("後手の持駒：%1").arg(handToString(goteHand));
    bodLines << QStringLiteral("  ９ ８ ７ ６ ５ ４ ３ ２ １");
    bodLines << QStringLiteral("+---------------------------+");
    
    const QStringList rankNames = {
        QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
        QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
        QStringLiteral("七"), QStringLiteral("八"), QStringLiteral("九")
    };
    
    for (int rank = 0; rank < 9; ++rank) {
        QString line = QStringLiteral("|");
        for (int file = 0; file < 9; ++file) {
            line += board[rank][file];
        }
        line += QStringLiteral("|") + rankNames[rank];
        bodLines << line;
    }
    
    bodLines << QStringLiteral("+---------------------------+");
    bodLines << QStringLiteral("先手の持駒：%1").arg(handToString(senteHand));
    bodLines << (turnSfen == QStringLiteral("b") ? QStringLiteral("先手番") : QStringLiteral("後手番"));
    
    if (pos.moveIndex > 0) {
        const int displayMoveNum = sfenMoveNum - 1;
        if (!pos.lastMoveStr.isEmpty()) {
            bodLines << QStringLiteral("手数＝%1  %2  まで").arg(displayMoveNum).arg(pos.lastMoveStr);
        } else {
            bodLines << QStringLiteral("手数＝%1  まで").arg(displayMoveNum);
        }
    }
    
    return bodLines.join(QStringLiteral("\n"));
}

bool KifuExportController::copyBodToClipboard()
{
    PositionData pos = getCurrentPositionData();
    
    if (pos.sfenStr.isEmpty()) {
        Q_EMIT statusMessage(tr("BOD形式の局面データがありません"), 3000);
        return false;
    }
    
    QString bodText = generateBodText(pos);
    if (bodText.isEmpty()) {
        Q_EMIT statusMessage(tr("SFEN形式の解析に失敗しました"), 3000);
        return false;
    }
    
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(bodText);
        Q_EMIT statusMessage(tr("BOD形式の局面をクリップボードにコピーしました"), 3000);
        qDebug().noquote() << "[KifuExport] copyBodToClipboard: copied" << bodText.size() << "chars";
        return true;
    }
    
    Q_EMIT statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

// --------------------------------------------------------
// USI指し手変換
// --------------------------------------------------------

QStringList KifuExportController::gameMovesToUsiMoves(const QVector<ShogiMove>& moves)
{
    QStringList usiMoves;
    usiMoves.reserve(moves.size());
    
    for (const ShogiMove& mv : moves) {
        QString usiMove;
        
        if (mv.fromSquare.x() >= 9) {
            // 駒打ち
            QChar pieceChar = mv.movingPiece.toUpper();
            int toFile = mv.toSquare.x() + 1;
            int toRank = mv.toSquare.y() + 1;
            QChar toRankChar = QChar('a' + toRank - 1);
            usiMove = QStringLiteral("%1*%2%3").arg(pieceChar).arg(toFile).arg(toRankChar);
        } else {
            // 通常移動
            int fromFile = mv.fromSquare.x() + 1;
            int fromRank = mv.fromSquare.y() + 1;
            int toFile = mv.toSquare.x() + 1;
            int toRank = mv.toSquare.y() + 1;
            QChar fromRankChar = QChar('a' + fromRank - 1);
            QChar toRankChar = QChar('a' + toRank - 1);
            usiMove = QStringLiteral("%1%2%3%4").arg(fromFile).arg(fromRankChar).arg(toFile).arg(toRankChar);
            if (mv.isPromotion) {
                usiMove += QLatin1Char('+');
            }
        }
        
        usiMoves.append(usiMove);
    }
    
    return usiMoves;
}

QStringList KifuExportController::sfenRecordToUsiMoves() const
{
    QStringList usiMoves;
    
    if (!m_deps.sfenRecord || m_deps.sfenRecord->size() < 2) {
        return usiMoves;
    }
    
    auto expandBoard = [](const QString& boardStr) -> QVector<QVector<QString>> {
        QVector<QVector<QString>> board(9, QVector<QString>(9));
        const QStringList ranks = boardStr.split(QLatin1Char('/'));
        for (qsizetype rank = 0; rank < qMin(ranks.size(), qsizetype(9)); ++rank) {
            const QString& rankStr = ranks[rank];
            int file = 0;
            bool promoted = false;
            for (qsizetype k = 0; k < rankStr.size() && file < 9; ++k) {
                QChar c = rankStr[k];
                if (c == QLatin1Char('+')) {
                    promoted = true;
                } else if (c.isDigit()) {
                    int skip = c.toLatin1() - '0';
                    for (int s = 0; s < skip && file < 9; ++s) {
                        board[rank][file++] = QString();
                    }
                    promoted = false;
                } else {
                    QString piece = promoted ? QStringLiteral("+") + QString(c) : QString(c);
                    board[rank][file++] = piece;
                    promoted = false;
                }
            }
        }
        return board;
    };
    
    for (int i = 1; i < m_deps.sfenRecord->size(); ++i) {
        const QString& prevSfen = m_deps.sfenRecord->at(i - 1);
        const QString& currSfen = m_deps.sfenRecord->at(i);
        
        const QStringList prevParts = prevSfen.split(QLatin1Char(' '));
        const QStringList currParts = currSfen.split(QLatin1Char(' '));
        
        if (prevParts.size() < 3 || currParts.size() < 3) {
            usiMoves.append(QString());
            continue;
        }
        
        QVector<QVector<QString>> prevBoardArr = expandBoard(prevParts[0]);
        QVector<QVector<QString>> currBoardArr = expandBoard(currParts[0]);
        
        QPoint fromPos(-1, -1);
        QPoint toPos(-1, -1);
        bool isDrop = false;
        bool isPromotion = false;
        
        QVector<QPoint> emptyPositions;
        QVector<QPoint> filledPositions;
        
        for (int rank = 0; rank < 9; ++rank) {
            for (int file = 0; file < 9; ++file) {
                const QString& prev = prevBoardArr[rank][file];
                const QString& curr = currBoardArr[rank][file];
                
                if (prev != curr) {
                    if (!prev.isEmpty() && curr.isEmpty()) {
                        emptyPositions.append(QPoint(file, rank));
                    } else if (!curr.isEmpty()) {
                        filledPositions.append(QPoint(file, rank));
                    }
                }
            }
        }
        
        QString movedPiece;
        if (emptyPositions.size() == 1 && filledPositions.size() == 1) {
            fromPos = emptyPositions[0];
            toPos = filledPositions[0];
            movedPiece = prevBoardArr[fromPos.y()][fromPos.x()];
            
            const QString& movedPieceFinal = currBoardArr[toPos.y()][toPos.x()];
            QString baseFrom = movedPiece.startsWith(QLatin1Char('+')) ? movedPiece.mid(1) : movedPiece;
            QString baseTo = movedPieceFinal.startsWith(QLatin1Char('+')) ? movedPieceFinal.mid(1) : movedPieceFinal;
            if (baseFrom.toUpper() == baseTo.toUpper() &&
                movedPieceFinal.startsWith(QLatin1Char('+')) && !movedPiece.startsWith(QLatin1Char('+'))) {
                isPromotion = true;
            }
        } else if (emptyPositions.isEmpty() && filledPositions.size() == 1) {
            isDrop = true;
            toPos = filledPositions[0];
        }
        
        QString usiMove;
        if (isDrop && toPos.x() >= 0) {
            QString droppedPiece = currBoardArr[toPos.y()][toPos.x()];
            QChar pieceChar = droppedPiece.isEmpty() ? QLatin1Char('P') : droppedPiece[0].toUpper();
            int toFileNum = 9 - toPos.x();
            QChar toRankChar = QChar('a' + toPos.y());
            usiMove = QStringLiteral("%1*%2%3").arg(pieceChar).arg(toFileNum).arg(toRankChar);
        } else if (fromPos.x() >= 0 && toPos.x() >= 0) {
            int fromFileNum = 9 - fromPos.x();
            int toFileNum = 9 - toPos.x();
            QChar fromRankChar = QChar('a' + fromPos.y());
            QChar toRankChar = QChar('a' + toPos.y());
            usiMove = QStringLiteral("%1%2%3%4").arg(fromFileNum).arg(fromRankChar).arg(toFileNum).arg(toRankChar);
            if (isPromotion) {
                usiMove += QLatin1Char('+');
            }
        }
        
        usiMoves.append(usiMove);
    }
    
    return usiMoves;
}
