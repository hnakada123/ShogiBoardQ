/// @file kifuexportcontroller.cpp
/// @brief 棋譜の書き出し（ファイル保存・上書き・自動保存・クリップボードへのコピー）

#include "kifuexportcontroller.h"

#include "appstate.h"
#include "bodtextgenerator.h"
#include "errorbus.h"
#include "gameinfopane.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifuexportmetadata.h"
#include "kifuioservice.h"
#include "kifunavigationstate.h"
#include "kifusavecoordinator.h"
#include "logcategories.h"
#include "matchtimerecord.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "usiexporter.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QWidget>

KifuExportController::KifuExportController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.gameRecord);
}

// --------------------------------------------------------
// 書き出しの文脈
// --------------------------------------------------------

GameRecordModel::ExportContext KifuExportController::buildExportContext() const
{
    GameRecordModel::ExportContext ctx;
    if (m_deps.gameInfoPane) {
        ctx.gameInfoItems = m_deps.gameInfoPane->gameInfo();
        ctx.gameInfoProvided = true;
    }
    ctx.startSfen = m_deps.gameRecord->initialSfenForExport(SfenUtils::hirateSfen());
    if (m_deps.state) ctx.players = m_deps.state->players;
    if (const MatchTimeRecord* record = m_deps.timeRecord) {
        ctx.timeControl = record->timeControl();
        ctx.gameStartDateTime = record->startDateTime();
        ctx.gameEndDateTime = record->endDateTime();
    }
    return ctx;
}

QStringList KifuExportController::exportLines(KifuFormat format) const
{
    const GameRecordModel::ExportContext ctx = buildExportContext();
    GameRecordModel* record = m_deps.gameRecord;
    switch (format) {
    case KifuFormat::Ki2: return record->toKi2Lines(ctx);
    case KifuFormat::Csa: return record->toCsaLines(ctx);
    case KifuFormat::Jkf: return record->toJkfLines(ctx);
    case KifuFormat::Usen: return record->toUsenLines(ctx);
    case KifuFormat::Usi: return record->toUsiLines(ctx);
    case KifuFormat::Kif:
    case KifuFormat::Sfen:
    case KifuFormat::Bod:
        break;
    }
    return record->toKifLines(ctx);
}

// --------------------------------------------------------
// ファイル保存
// --------------------------------------------------------

void KifuExportController::markSaved(const QString& message)
{
    if (m_deps.gameInfoPane) m_deps.gameInfoPane->applyChanges();
    m_deps.gameRecord->clearDirty();
    emit statusMessage(message, 5000);
}

QString KifuExportController::saveToFile()
{
    if (m_deps.gameInfoPane) m_deps.gameInfoPane->commitPendingEditor();

    const KifuBranchTree* tree = m_deps.gameRecord->branchTree();
    const bool hasBranches = tree && tree->lineCount() > 1;
    bool hasTimeInfo = false;
    if (tree) {
        for (const auto* node : tree->mainLine()) {
            if (!node->timeText().isEmpty()) {
                hasTimeInfo = true;
                break;
            }
        }
    }

    QString error;
    const MatchPlayers players = m_deps.state ? m_deps.state->players : MatchPlayers{};
    const QString path = KifuSaveCoordinator::saveViaDialog(
        m_deps.parentWidget,
        [this](KifuFormat format) { return exportLines(format); },
        KifuIoService::makeDefaultSaveFileName(players, QDateTime::currentDateTime()),
        hasBranches, hasTimeInfo, &error);
    if (!error.isEmpty()) QMessageBox::warning(m_deps.parentWidget, tr("KIF Save Error"), error);
    if (!path.isEmpty()) markSaved(tr("棋譜を保存しました: %1").arg(path));
    return path;
}

bool KifuExportController::overwriteFile(const QString& filePath)
{
    QString error;
    const bool ok = writeToPath(filePath, m_deps.parentWidget, &error);
    // 確認ダイアログでキャンセルした場合は error が空
    if (!ok && !error.isEmpty()) QMessageBox::warning(m_deps.parentWidget, tr("KIF Save Error"), error);
    return ok;
}

bool KifuExportController::saveToPathWithoutDialogs(const QString& filePath, QString* errorText)
{
    return writeToPath(filePath, nullptr, errorText);
}

bool KifuExportController::writeToPath(const QString& filePath, QWidget* dialogParent, QString* errorText)
{
    if (errorText) errorText->clear();
    if (m_deps.gameInfoPane) m_deps.gameInfoPane->commitPendingEditor();
    if (filePath.isEmpty()) return false;

    // 名前を付けて保存と同じ基準（拡張子）で保存形式を決める。
    // KIF 固定で書くと .csa などに保存したファイルが KIF 内容で上書きされてしまう。
    const QStringList lines = exportLines(KifuSaveFile::saveFormatForPath(filePath));
    const bool ok = KifuSaveCoordinator::overwriteExisting(dialogParent, filePath, lines, errorText);
    if (ok) markSaved(tr("棋譜を上書き保存しました: %1").arg(filePath));
    return ok;
}

std::optional<QString> KifuExportController::autoSaveToDir(const QString& saveDir, const QString& extension)
{
    if (m_deps.gameInfoPane) m_deps.gameInfoPane->commitPendingEditor();
    if (saveDir.isEmpty() || !QDir().mkpath(saveDir)) {
        emit statusMessage(tr("棋譜の自動保存先を作成できません: %1").arg(saveDir), 5000);
        return std::nullopt;
    }
    // 名前は対局者と日時から作る。同じ秒に終わった対局があっても上書きしない
    const MatchPlayers players = m_deps.state ? m_deps.state->players : MatchPlayers{};
    const QDir dir(saveDir);
    const QString fileName = KifuIoService::makeDefaultSaveFileName(players, QDateTime::currentDateTime(), extension);
    QString filePath = dir.filePath(fileName);
    const QFileInfo info(fileName);
    for (int serial = 2; QFileInfo::exists(filePath); ++serial)
        filePath = dir.filePath(QStringLiteral("%1_%2.%3").arg(info.completeBaseName()).arg(serial).arg(info.suffix()));

    if (exportLines(KifuSaveFile::saveFormatForPath(filePath)).isEmpty()) {
        emit statusMessage(tr("棋譜データが空のため自動保存をスキップしました"), 3000);
        return std::nullopt;
    }
    QString errorText;
    if (writeToPath(filePath, nullptr, &errorText)) {
        markSaved(tr("棋譜を自動保存しました: %1").arg(filePath));
        return filePath;
    }
    qCWarning(lcKifu).noquote() << "autoSaveToDir failed:" << errorText;
    ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Error, tr("棋譜の自動保存に失敗しました: %1").arg(errorText));
    return std::nullopt;
}

// --------------------------------------------------------
// クリップボードへのコピー
// --------------------------------------------------------

bool KifuExportController::setClipboardText(const QString& text, const QString& successMsg)
{
    if (QClipboard* clipboard = QApplication::clipboard()) {
        clipboard->setText(text);
        emit statusMessage(successMsg, 3000);
        return true;
    }
    emit statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

bool KifuExportController::copyRecord(KifuFormat format)
{
    const KifuBranchTree* tree = m_deps.gameRecord->branchTree();
    const bool mainLineOnly = tree && tree->lineCount() > 1;
    struct Messages {
        QString empty;
        QString copied;
    };
    Messages m;
    switch (format) {
    case KifuFormat::Kif:
    case KifuFormat::Sfen:
    case KifuFormat::Bod:
        m = {tr("KIF形式の棋譜データがありません"), tr("KIF形式の棋譜をクリップボードにコピーしました")};
        break;
    case KifuFormat::Ki2:
        m = {tr("KI2形式の棋譜データがありません"), tr("KI2形式の棋譜をクリップボードにコピーしました")};
        break;
    case KifuFormat::Csa:
        // CSA・USI 形式は分岐を書けない
        m = {tr("CSA形式の棋譜データがありません"),
             mainLineOnly ? tr("CSA形式の棋譜をクリップボードにコピーしました（CSA形式は分岐に対応していないため、本譜のみです）")
                          : tr("CSA形式の棋譜をクリップボードにコピーしました")};
        break;
    case KifuFormat::Usi:
        m = {tr("USI形式の棋譜データがありません"),
             mainLineOnly ? tr("USI形式の棋譜をクリップボードにコピーしました（USI形式は分岐に対応していないため、本譜のみです）")
                          : tr("USI形式の棋譜をクリップボードにコピーしました")};
        break;
    case KifuFormat::Jkf:
        m = {tr("JKF形式の棋譜データがありません"), tr("JKF形式の棋譜をクリップボードにコピーしました")};
        break;
    case KifuFormat::Usen:
        m = {tr("USEN形式の棋譜データがありません"), tr("USEN形式の棋譜をクリップボードにコピーしました")};
        break;
    }
    const QStringList lines = exportLines(format);
    if (lines.isEmpty()) {
        emit statusMessage(m.empty, 3000);
        return false;
    }
    return setClipboardText(lines.join(QStringLiteral("\n")), m.copied);
}

bool KifuExportController::copyUsiCurrentToClipboard()
{
    // 表示中の手順の、表示中の手までの指し手（終局の行まで来ていればその終局）
    QList<KifuBranchNode*> nodes;
    TerminalType terminal = TerminalType::None;
    if (const KifuNavigationState* nav = m_deps.navState) {
        const int limit = nav->currentPly();
        for (KifuBranchNode* node : nav->selectedPath()) {
            if (node->ply() > limit) break;
            if (node->isTerminal()) {
                terminal = node->terminalType();
                break;
            }
            nodes.append(node);
        }
    }
    const QStringList moves = KifuBranchTree::toRecordLine(nodes).usiMoves();
    const QStringList lines = UsiExporter::exportPosition(buildExportContext().startSfen, moves, terminal);
    if (lines.isEmpty()) {
        emit statusMessage(tr("USI形式の棋譜データがありません"), 3000);
        return false;
    }
    return setClipboardText(lines.join(QStringLiteral("\n")),
                            tr("USI形式（現在の指し手まで）の棋譜をクリップボードにコピーしました"));
}

QString KifuExportController::boardSfen() const
{
    // 局面編集による変更も含め、盤に出している局面・手番・持ち駒をコピーする
    const ShogiBoard* board = m_deps.gameController ? m_deps.gameController->board() : nullptr;
    if (!board) return m_deps.navState ? m_deps.navState->currentSfen() : QString();
    QString stand = board->convertStandToSfen();
    if (stand.isEmpty()) stand = QStringLiteral("-");
    // 手数は表示中の手の局面の手数（無ければ表示中の手数の次）
    const int ply = m_deps.navState ? m_deps.navState->currentPly() : 0;
    bool ok = false;
    int moveNumber = m_deps.navState ? m_deps.navState->currentSfen().section(QLatin1Char(' '), 3, 3).toInt(&ok) : 0;
    if (!ok) moveNumber = ply + 1;
    return QStringLiteral("%1 %2 %3 %4")
        .arg(board->convertBoardToSfen(), turnToSfen(board->currentPlayer()), stand, QString::number(moveNumber));
}

bool KifuExportController::copySfenToClipboard()
{
    const QString sfen = boardSfen();
    if (sfen.isEmpty()) {
        emit statusMessage(tr("SFEN形式の局面データがありません"), 3000);
        return false;
    }
    return setClipboardText(sfen, tr("SFEN形式の局面をクリップボードにコピーしました"));
}

bool KifuExportController::copyBodToClipboard()
{
    const QString sfen = boardSfen();
    if (sfen.isEmpty()) {
        emit statusMessage(tr("BOD形式の局面データがありません"), 3000);
        return false;
    }
    // 最終手は表示中の手（局面図の「手数＝N ▲７六歩 まで」）
    const KifuBranchNode* node = m_deps.navState ? m_deps.navState->currentNode() : nullptr;
    const int ply = node ? node->ply() : 0;
    const QString lastMove = node && node->isActualMove() ? node->displayText() : QString();
    // 駒落ちは KIF の局面図と同じく、対局情報の見出しに合わせて下手・上手と呼ぶ
    const GameRecordModel::ExportContext ctx = buildExportContext();
    const bool handicapNames =
        KifuExportMetadataBuilder::usesHandicapNames(GameRecordModel::collectGameInfo(ctx), ctx.startSfen);
    const QString bodText = BodTextGenerator::generate(sfen, ply, lastMove, handicapNames);
    if (bodText.isEmpty()) {
        emit statusMessage(tr("SFEN形式の解析に失敗しました"), 3000);
        return false;
    }
    return setClipboardText(bodText, tr("BOD形式の局面をクリップボードにコピーしました"));
}
