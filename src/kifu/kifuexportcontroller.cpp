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

// --------------------------------------------------------
// ユーティリティ
// --------------------------------------------------------

QStringList KifuExportController::resolveUsiMoves() const
{
    // 表示テキストと同じ本譜を使う。対局用USI列は途中再開後の新規手だけの場合がある。
    if (m_deps.gameRecord && m_deps.gameRecord->branchTree()
        && !m_deps.gameRecord->branchTree()->isEmpty()) {
        return m_deps.gameRecord->collectMainlineUsiForExport();
    }

    // ツリーがない場合は対局用データにフォールバックする。
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
    ctx.startSfen = m_deps.gameRecord
        ? m_deps.gameRecord->initialSfenForExport(m_deps.startSfenStr) : m_deps.startSfenStr;
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

    QString error;
    const QString path = KifuSaveCoordinator::saveViaDialog(
        m_parentWidget,
        [this](KifuSaveCoordinator::SaveFormat format) { return linesForFormat(format); },
        m_deps.playMode,
        m_deps.humanName1, m_deps.humanName2,
        m_deps.engineName1, m_deps.engineName2,
        hasBranches, hasTimeInfo, &error);

    if (!error.isEmpty()) {
        QMessageBox::warning(m_parentWidget, tr("KIF Save Error"), error);
    }
    
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

    // 名前を付けて保存と同じ基準（拡張子）で保存形式を決める。
    // KIF 固定で書くと .csa などに保存したファイルが KIF 内容で上書きされてしまう。
    const KifuSaveCoordinator::SaveFormat format = KifuSaveCoordinator::saveFormatForPath(filePath);

    QStringList lines;
    if (m_deps.gameRecord) {
        lines = linesForFormat(format);
        qCDebug(lcKifu).noquote() << "overwriteFile: generated" << lines.size()
                                  << "lines for" << filePath;
    } else if (format == KifuSaveCoordinator::SaveFormat::Kif) {
        // フォールバック（KIF のみ）
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

        lines = KifuContentBuilder::buildKifuDataList(ctx);
    } else {
        Q_EMIT statusMessage(tr("棋譜データがありません"), 3000);
        return false;
    }

    QString error;
    const bool ok = KifuSaveCoordinator::overwriteExisting(filePath, lines, &error);

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

QStringList KifuExportController::exportLines(KifuSaveCoordinator::SaveFormat format)
{
    if (m_prepareCallback) m_prepareCallback();
    return linesForFormat(format);
}

QStringList KifuExportController::linesForFormat(KifuSaveCoordinator::SaveFormat format) const
{
    if (!m_deps.gameRecord) return QStringList();

    const GameRecordModel::ExportContext ctx = buildExportContext();
    switch (format) {
    case KifuSaveCoordinator::SaveFormat::Ki2:
        return m_deps.gameRecord->toKi2Lines(ctx);
    case KifuSaveCoordinator::SaveFormat::Csa:
        return m_deps.gameRecord->toCsaLines(ctx, resolveUsiMoves());
    case KifuSaveCoordinator::SaveFormat::Jkf:
        return m_deps.gameRecord->toJkfLines(ctx);
    case KifuSaveCoordinator::SaveFormat::Usen:
        return m_deps.gameRecord->toUsenLines(ctx, resolveUsiMoves());
    case KifuSaveCoordinator::SaveFormat::Usi:
        return m_deps.gameRecord->toUsiLines(ctx, resolveUsiMoves());
    case KifuSaveCoordinator::SaveFormat::Kif:
        break;
    }
    return m_deps.gameRecord->toKifLines(ctx);
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

    QString filePath = QDir(saveDir).filePath(fileName);

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

QStringList KifuExportController::sfenRecordToUsiMoves() const
{
    if (!m_deps.sfenRecord || m_deps.sfenRecord->size() < 2) {
        return QStringList();
    }
    return UsiMoveConverter::fromSfenRecord(*m_deps.sfenRecord);
}
