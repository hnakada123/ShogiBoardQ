/// @file kifuexportcontroller.cpp
/// @brief 棋譜エクスポートコントローラクラスの実装

#include "kifuexportcontroller.h"
#include "kifuexportclipboard.h"

#include <QWidget>
#include <QStatusBar>
#include <QTableWidget>
#include "logcategories.h"
#include <QMessageBox>
#include <QDir>
#include <QDateTime>

#include "errorbus.h"
#include "gamerecordmodel.h"
#include "gameinfopanecontroller.h"
#include "timecontrolcontroller.h"
#include "kifuloadcoordinator.h"
#include "gamerecordpresenter.h"
#include "matchcoordinator.h"
#include "replaycontroller.h"
#include "kifusavecoordinator.h"
#include "kifucontentbuilder.h"
#include "shogimove.h"
#include "kifuioservice.h"
#include "usimoveconverter.h"

KifuExportController::KifuExportController(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
    , m_clipboard(new KifuExportClipboard(parentWidget, this))
{
    connect(m_clipboard, &KifuExportClipboard::statusMessage,
            this, &KifuExportController::statusMessage);
}

KifuExportController::~KifuExportController() = default;

static KifuExportClipboard::Deps toClipboardDeps(const KifuExportController::Dependencies& d)
{
    KifuExportClipboard::Deps cd;
    cd.gameRecord = d.gameRecord;
    cd.kifuRecordModel = d.kifuRecordModel;
    cd.gameInfoController = d.gameInfoController;
    cd.timeController = d.timeController;
    cd.kifuLoadCoordinator = d.kifuLoadCoordinator;
    cd.match = d.match;
    cd.replayController = d.replayController;
    cd.gameController = d.gameController;
    cd.sfenRecord = d.sfenRecord;
    cd.usiMoves = d.usiMoves;
    cd.startSfenStr = d.startSfenStr;
    cd.playMode = d.playMode;
    cd.humanName1 = d.humanName1;
    cd.humanName2 = d.humanName2;
    cd.engineName1 = d.engineName1;
    cd.engineName2 = d.engineName2;
    cd.currentMoveIndex = d.currentMoveIndex;
    cd.activePly = d.activePly;
    cd.currentSelectedPly = d.currentSelectedPly;
    return cd;
}

void KifuExportController::setDependencies(const Dependencies& deps)
{
    m_deps = deps;
    m_clipboard->setDependencies(toClipboardDeps(deps));
}

void KifuExportController::setPrepareCallback(std::function<void()> callback)
{
    m_prepareCallback = std::move(callback);
    m_clipboard->setPrepareCallback(m_prepareCallback);
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
    m_clipboard->setDependencies(toClipboardDeps(m_deps));
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
// クリップボードコピー（KifuExportClipboardへ委譲）
// --------------------------------------------------------

bool KifuExportController::copyKifToClipboard()         { return m_clipboard->copyKifToClipboard(); }
bool KifuExportController::copyKi2ToClipboard()         { return m_clipboard->copyKi2ToClipboard(); }
bool KifuExportController::copyCsaToClipboard()         { return m_clipboard->copyCsaToClipboard(); }
bool KifuExportController::copyUsiToClipboard()         { return m_clipboard->copyUsiToClipboard(); }
bool KifuExportController::copyUsiCurrentToClipboard()  { return m_clipboard->copyUsiCurrentToClipboard(); }
bool KifuExportController::copyJkfToClipboard()         { return m_clipboard->copyJkfToClipboard(); }
bool KifuExportController::copyUsenToClipboard()        { return m_clipboard->copyUsenToClipboard(); }
bool KifuExportController::copySfenToClipboard()        { return m_clipboard->copySfenToClipboard(); }
bool KifuExportController::copyBodToClipboard()         { return m_clipboard->copyBodToClipboard(); }

// --------------------------------------------------------
// USI指し手変換
// --------------------------------------------------------

QStringList KifuExportController::gameMovesToUsiMoves(const QList<ShogiMove>& moves)
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
