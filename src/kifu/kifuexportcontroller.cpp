/// @file kifuexportcontroller.cpp
/// @brief 棋譜エクスポートコントローラクラスの実装

#include "kifuexportcontroller.h"

#include <QWidget>
#include <QClipboard>
#include <QApplication>
#include <QStatusBar>
#include <QTableWidget>
#include "logcategories.h"
#include <QMessageBox>
#include <QDir>
#include <QDateTime>

#include "errorbus.h"
#include "gamerecordmodel.h"
#include "kifurecordlistmodel.h"
#include "gameinfopanecontroller.h"
#include "timecontrolcontroller.h"
#include "kifuloadcoordinator.h"
#include "gamerecordpresenter.h"  // GameRecordPresenter
#include "matchcoordinator.h"
#include "replaycontroller.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "kifusavecoordinator.h"
#include "kifucontentbuilder.h"
#include "kifuclipboardservice.h"
#include "shogimove.h"
#include "kifuioservice.h"
#include "bodtextgenerator.h"
#include "usimoveconverter.h"

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

void KifuExportController::setPrepareCallback(std::function<void()> callback)
{
    m_prepareCallback = std::move(callback);
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
            qCDebug(lcKifu).noquote() << "kifuUsiMoves from KifuLoadCoordinator, size =" << moves.size();
            return moves;
        }
    }
    
    // 3. SFENレコードから生成
    if (m_deps.sfenRecord && m_deps.sfenRecord->size() > 1) {
        QStringList moves = sfenRecordToUsiMoves();
        qCDebug(lcKifu).noquote() << "usiMoves from sfenRecord, size =" << moves.size();
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
        ctx.gameEndDateTime = m_deps.timeController->gameEndDateTime();
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
    if (m_prepareCallback) m_prepareCallback();

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
    
    qCDebug(lcKifu).noquote() << "saveToFile: generated"
                              << kifLines.size() << "KIF,"
                              << ki2Lines.size() << "KI2,"
                              << csaLines.size() << "CSA,"
                              << jkfLines.size() << "JKF,"
                              << usenLines.size() << "USEN,"
                              << usiLines.size() << "USI lines";
    
    m_kifuDataList = kifLines;

    // 分岐の有無を判定
    const bool hasBranches = m_deps.gameRecord->branchTree()
                             && m_deps.gameRecord->branchTree()->lineCount() > 1;

    // 消費時間の有無を判定
    bool hasTimeInfo = false;
    if (KifuBranchTree* tree = m_deps.gameRecord->branchTree()) {
        const auto mainNodes = tree->mainLine();
        for (const auto* node : std::as_const(mainNodes)) {
            if (!node->timeText().isEmpty()) {
                hasTimeInfo = true;
                break;
            }
        }
    }

    const QString path = KifuSaveCoordinator::saveViaDialogWithUsi(
        m_parentWidget,
        kifLines, ki2Lines, csaLines,
        jkfLines, usenLines, usiLines,
        m_deps.playMode,
        m_deps.humanName1, m_deps.humanName2,
        m_deps.engineName1, m_deps.engineName2,
        hasBranches, hasTimeInfo);
    
    if (!path.isEmpty()) {
        if (m_deps.gameRecord) {
            m_deps.gameRecord->clearDirty();
        }
        Q_EMIT statusMessage(tr("棋譜を保存しました: %1").arg(path), 5000);
    }
    
    return path;
}

bool KifuExportController::overwriteFile(const QString& filePath)
{
    if (m_prepareCallback) m_prepareCallback();

    if (filePath.isEmpty()) {
        return false;
    }
    
    QStringList kifLines;
    
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        kifLines = m_deps.gameRecord->toKifLines(ctx);
        qCDebug(lcKifu).noquote() << "overwriteFile: generated" << kifLines.size() << "lines";
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
    } else {
        QMessageBox::warning(m_parentWidget, tr("KIF Save Error"), error);
    }

    return ok;
}

std::optional<QString> KifuExportController::autoSaveToDir(const QString& saveDir)
{
    if (m_prepareCallback) m_prepareCallback();

    if (saveDir.isEmpty()) {
        Q_EMIT statusMessage(tr("自動保存先ディレクトリが指定されていません"), 3000);
        return std::nullopt;
    }

    if (!m_deps.gameRecord) {
        Q_EMIT statusMessage(tr("棋譜データがありません（自動保存をスキップ）"), 3000);
        return std::nullopt;
    }

    GameRecordModel::ExportContext ctx = buildExportContext();
    QStringList kifLines = m_deps.gameRecord->toKifLines(ctx);
    if (kifLines.isEmpty()) {
        Q_EMIT statusMessage(tr("棋譜データが空のため自動保存をスキップしました"), 3000);
        return std::nullopt;
    }

    // ファイル名生成
    const QString fileName = KifuIoService::makeDefaultSaveFileName(
        m_deps.playMode,
        m_deps.humanName1, m_deps.humanName2,
        m_deps.engineName1, m_deps.engineName2,
        QDateTime::currentDateTime());

    const QString filePath = QDir(saveDir).filePath(fileName);

    QString errorText;
    const bool ok = KifuIoService::writeKifuFile(filePath, kifLines, &errorText);
    if (ok) {
        if (m_deps.gameRecord) {
            m_deps.gameRecord->clearDirty();
        }
        Q_EMIT statusMessage(tr("棋譜を自動保存しました: %1").arg(filePath), 5000);
        return filePath;
    }

    qCWarning(lcKifu).noquote() << "autoSaveToDir failed:" << errorText;
    ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Error,
        tr("棋譜の自動保存に失敗しました: %1").arg(errorText));
    return std::nullopt;
}

// --------------------------------------------------------
// クリップボードコピー
// --------------------------------------------------------

bool KifuExportController::copyKifToClipboard()
{
    if (m_prepareCallback) m_prepareCallback();

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
    if (m_prepareCallback) m_prepareCallback();

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
    if (m_prepareCallback) m_prepareCallback();

    QStringList usiMovesForCsa = resolveUsiMoves();
    
    QStringList csaLines;
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        csaLines = m_deps.gameRecord->toCsaLines(ctx, usiMovesForCsa);
        qCDebug(lcKifu).noquote() << "copyCsaToClipboard: generated" << csaLines.size() << "lines";
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
    if (m_prepareCallback) m_prepareCallback();

    QStringList usiMovesForOutput = resolveUsiMoves();
    
    QStringList usiLines;
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        usiLines = m_deps.gameRecord->toUsiLines(ctx, usiMovesForOutput);
        qCDebug(lcKifu).noquote() << "copyUsiToClipboard: generated" << usiLines.size() << "lines";
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
    if (m_prepareCallback) m_prepareCallback();

    QStringList usiMovesForOutput = resolveUsiMoves();
    
    // 現在の手数まで制限
    const int limit = m_deps.currentMoveIndex;
    if (limit >= 0 && limit < usiMovesForOutput.size()) {
        usiMovesForOutput = usiMovesForOutput.mid(0, limit);
        qCDebug(lcKifu).noquote() << "copyUsiCurrentToClipboard: limited to" << limit << "moves";
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
    if (m_prepareCallback) m_prepareCallback();

    QStringList jkfLines;
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        jkfLines = m_deps.gameRecord->toJkfLines(ctx);
        qCDebug(lcKifu).noquote() << "copyJkfToClipboard: generated" << jkfLines.size() << "lines";
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
    if (m_prepareCallback) m_prepareCallback();

    QStringList usiMovesForOutput = resolveUsiMoves();
    
    QStringList usenLines;
    if (m_deps.gameRecord) {
        GameRecordModel::ExportContext ctx = buildExportContext();
        usenLines = m_deps.gameRecord->toUsenLines(ctx, usiMovesForOutput);
        qCDebug(lcKifu).noquote() << "copyUsenToClipboard: generated" << usenLines.size() << "lines";
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
        const QString turn = turnToSfen(board->currentPlayer());
        const int moveNum = qMax(1, currentPly() + 1);
        data.sfenStr = QStringLiteral("%1 %2 %3 %4").arg(boardSfen, turn, standSfen, QString::number(moveNum));
        data.moveIndex = currentPly();
    }
    
    return data;
}

bool KifuExportController::copySfenToClipboard()
{
    if (m_prepareCallback) m_prepareCallback();

    PositionData pos = getCurrentPositionData();
    
    if (pos.sfenStr.isEmpty()) {
        Q_EMIT statusMessage(tr("SFEN形式の局面データがありません"), 3000);
        return false;
    }
    
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(pos.sfenStr);
        Q_EMIT statusMessage(tr("SFEN形式の局面をクリップボードにコピーしました"), 3000);
        qCDebug(lcKifu).noquote() << "copySfenToClipboard: copied" << pos.sfenStr.size() << "chars";
        return true;
    }
    
    Q_EMIT statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

QString KifuExportController::generateBodText(const PositionData& pos) const
{
    return BodTextGenerator::generate(pos.sfenStr, pos.moveIndex, pos.lastMoveStr);
}

bool KifuExportController::copyBodToClipboard()
{
    if (m_prepareCallback) m_prepareCallback();

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
        qCDebug(lcKifu).noquote() << "copyBodToClipboard: copied" << bodText.size() << "chars";
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
    return UsiMoveConverter::fromGameMoves(moves);
}

QStringList KifuExportController::sfenRecordToUsiMoves() const
{
    if (!m_deps.sfenRecord || m_deps.sfenRecord->size() < 2) {
        return QStringList();
    }
    return UsiMoveConverter::fromSfenRecord(*m_deps.sfenRecord);
}
