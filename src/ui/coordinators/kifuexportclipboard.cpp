/// @file kifuexportclipboard.cpp
/// @brief 棋譜クリップボードコピー機能の実装

#include "kifuexportclipboard.h"
#include "appstate.h"
#include "kifunavigationstate.h"

#include <QClipboard>
#include <QApplication>
#include "logcategories.h"

#include "gamerecordmodel.h"
#include "kifubranchtree.h"
#include "kifurecordlistmodel.h"
#include "gameinfopanecontroller.h"
#include "timecontrolcontroller.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "kifuclipboardservice.h"
#include "bodtextgenerator.h"
#include "kifuexportmetadata.h"
#include "usiexporter.h"

KifuExportClipboard::KifuExportClipboard(QObject* parent)
    : QObject(parent)
{
}

void KifuExportClipboard::setDependencies(const Deps& deps)
{
    m_deps = deps;
}

// --------------------------------------------------------
// ユーティリティ
// --------------------------------------------------------

QStringList KifuExportClipboard::resolveUsiMoves() const
{
    // 棋譜の正本（分岐ツリー）の本譜。表示テキストと同じ手順になる
    return m_deps.gameRecord ? m_deps.gameRecord->collectMainlineUsiForExport() : QStringList();
}

GameRecordModel::ExportContext KifuExportClipboard::buildExportContext() const
{
    GameRecordModel::ExportContext ctx;
    if (m_deps.gameInfoController) {
        ctx.gameInfoItems = m_deps.gameInfoController->gameInfo();
        ctx.gameInfoProvided = true;
    }
    ctx.startSfen = m_deps.gameRecord
        ? m_deps.gameRecord->initialSfenForExport(m_deps.state->startSfenStr) : m_deps.state->startSfenStr;
    ctx.players = m_deps.state->players;

    // 時間制御情報
    if (m_deps.timeController) {
        ctx.timeControl = m_deps.timeController->timeControl();
        ctx.gameStartDateTime = m_deps.timeController->gameStartDateTime();
        ctx.gameEndDateTime = m_deps.timeController->gameEndDateTime();
    }

    return ctx;
}

bool KifuExportClipboard::hasBranches() const
{
    const KifuBranchTree* tree = m_deps.gameRecord ? m_deps.gameRecord->branchTree() : nullptr;
    return tree != nullptr && !tree->isEmpty() && tree->lineCount() > 1;
}

int KifuExportClipboard::currentPly() const
{
    // 表示中の手数（分岐ツリーのカーソル）。対局中はカーソルが最新の手を指している
    return m_deps.navState ? qMax(0, m_deps.navState->currentPly()) : 0;
}

QString KifuExportClipboard::lineSfenAt(int ply) const
{
    // 表示中の手順（分岐ツリーのライン）の ply 行目の局面
    const KifuBranchTree* tree = m_deps.gameRecord ? m_deps.gameRecord->branchTree() : nullptr;
    if (tree == nullptr) return {};
    const auto lines = tree->allLines();
    const int line = m_deps.gameRecord->activeRow();
    if (line < 0 || line >= lines.size() || ply < 0 || ply >= lines[line].nodes.size()) return {};
    return lines[line].nodes[ply]->sfen();
}

KifuExportClipboard::PositionData KifuExportClipboard::currentPositionData() const
{
    PositionData data;
    data.moveIndex = currentPly();

    // SFEN履歴は対局用で、読み込んだ棋譜や分岐の表示局面と一致するとは限らない。
    // 局面編集による変更も含め、実際に表示している盤面・手番・持ち駒をコピーする。
    if (m_deps.gameController && m_deps.gameController->board()) {
        ShogiBoard* board = m_deps.gameController->board();
        const QString boardSfen = board->convertBoardToSfen();
        QString standSfen = board->convertStandToSfen();
        if (standSfen.isEmpty()) standSfen = QStringLiteral("-");
        const QString turn = turnToSfen(board->currentPlayer());
        int moveNum = data.moveIndex + 1;
        bool ok = false;
        const int number = lineSfenAt(data.moveIndex).section(QLatin1Char(' '), 3, 3).toInt(&ok);
        if (ok) moveNum = number;
        data.sfenStr = QStringLiteral("%1 %2 %3 %4").arg(boardSfen, turn, standSfen, QString::number(moveNum));
    } else {
        data.sfenStr = lineSfenAt(data.moveIndex);
    }

    // BODの最終手表示にも閲覧中の行を使用する。
    if (data.moveIndex > 0 && m_deps.kifuRecordModel
        && data.moveIndex < m_deps.kifuRecordModel->rowCount()) {
        const QModelIndex modelIdx = m_deps.kifuRecordModel->index(data.moveIndex, 0);
        data.lastMoveStr = m_deps.kifuRecordModel->data(modelIdx, Qt::DisplayRole).toString();
    }

    return data;
}

QString KifuExportClipboard::generateBodText(const PositionData& pos) const
{
    // 駒落ちは KIF の局面図と同じく、対局情報の見出しに合わせて下手・上手と呼ぶ
    const GameRecordModel::ExportContext ctx = buildExportContext();
    const bool handicapNames = KifuExportMetadataBuilder::usesHandicapNames(
        GameRecordModel::collectGameInfo(ctx), ctx.startSfen);
    return BodTextGenerator::generate(pos.sfenStr, pos.moveIndex, pos.lastMoveStr, handicapNames);
}

// --------------------------------------------------------
// クリップボードコピー
// --------------------------------------------------------

bool KifuExportClipboard::setClipboardText(const QString& text, const QString& successMsg)
{
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
        Q_EMIT statusMessage(successMsg, 3000);
        return true;
    }
    Q_EMIT statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

bool KifuExportClipboard::copyKifToClipboard()
{

    if (!m_deps.gameRecord) {
        Q_EMIT statusMessage(tr("KIF形式の棋譜データがありません"), 3000);
        return false;
    }

    if (KifuClipboardService::copyKif(*m_deps.gameRecord, buildExportContext())) {
        Q_EMIT statusMessage(tr("KIF形式の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    }
    Q_EMIT statusMessage(tr("KIF形式の棋譜データがありません"), 3000);
    return false;
}

bool KifuExportClipboard::copyKi2ToClipboard()
{

    if (!m_deps.gameRecord) {
        Q_EMIT statusMessage(tr("KI2形式の棋譜データがありません"), 3000);
        return false;
    }

    if (KifuClipboardService::copyKi2(*m_deps.gameRecord, buildExportContext())) {
        Q_EMIT statusMessage(tr("KI2形式の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    }
    Q_EMIT statusMessage(tr("KI2形式の棋譜データがありません"), 3000);
    return false;
}

bool KifuExportClipboard::copyCsaToClipboard()
{

    QStringList csaLines;
    if (m_deps.gameRecord) {
        csaLines = m_deps.gameRecord->toCsaLines(buildExportContext(), resolveUsiMoves());
        qCDebug(lcKifu).noquote() << "copyCsaToClipboard: generated" << csaLines.size() << "lines";
    }
    if (csaLines.isEmpty()) {
        Q_EMIT statusMessage(tr("CSA形式の棋譜データがありません"), 3000);
        return false;
    }
    return setClipboardText(csaLines.join(QStringLiteral("\n")),
                            hasBranches() ? tr("CSA形式の棋譜をクリップボードにコピーしました（CSA形式は分岐に対応していないため、本譜のみです）")
                                          : tr("CSA形式の棋譜をクリップボードにコピーしました"));
}

bool KifuExportClipboard::copyUsiToClipboard()
{

    QStringList usiLines;
    if (m_deps.gameRecord) {
        usiLines = m_deps.gameRecord->toUsiLines(buildExportContext(), resolveUsiMoves());
        qCDebug(lcKifu).noquote() << "copyUsiToClipboard: generated" << usiLines.size() << "lines";
    }
    if (usiLines.isEmpty()) {
        Q_EMIT statusMessage(tr("USI形式の棋譜データがありません"), 3000);
        return false;
    }
    return setClipboardText(usiLines.join(QStringLiteral("\n")),
                            hasBranches() ? tr("USI形式の棋譜をクリップボードにコピーしました（USI形式は分岐に対応していないため、本譜のみです）")
                                          : tr("USI形式の棋譜をクリップボードにコピーしました"));
}

bool KifuExportClipboard::copyUsiCurrentToClipboard()
{

    QStringList usiLines;
    if (m_deps.gameRecord) {
        const int limit = currentPly();
        QStringList moves = resolveUsiMoves().mid(0, limit);
        TerminalType terminal = TerminalType::None;
        if (auto* tree = m_deps.gameRecord->branchTree(); tree && !tree->isEmpty()) {
            const auto lines = tree->allLines();
            const int line = m_deps.gameRecord->activeRow();
            if (line >= 0 && line < lines.size()) {
                QList<KifuBranchNode*> nodes;
                for (KifuBranchNode* node : lines[line].nodes) {
                    if (node->ply() > limit) break;
                    if (node->isTerminal()) { terminal = node->terminalType(); break; }
                    nodes.append(node);
                }
                moves = KifuBranchTree::toRecordLine(nodes).usiMoves();
            }
        } else {
            const auto disp = m_deps.gameRecord->collectMainlineForExport();
            if (!disp.isEmpty() && disp.last().ply <= limit) terminal = disp.last().terminalType;
        }
        usiLines = UsiExporter::exportPosition(buildExportContext().startSfen, moves, terminal);
    }
    if (usiLines.isEmpty()) {
        Q_EMIT statusMessage(tr("USI形式の棋譜データがありません"), 3000);
        return false;
    }
    return setClipboardText(usiLines.join(QStringLiteral("\n")),
                            tr("USI形式（現在の指し手まで）の棋譜をクリップボードにコピーしました"));
}

bool KifuExportClipboard::copyJkfToClipboard()
{

    QStringList jkfLines;
    if (m_deps.gameRecord) {
        jkfLines = m_deps.gameRecord->toJkfLines(buildExportContext());
        qCDebug(lcKifu).noquote() << "copyJkfToClipboard: generated" << jkfLines.size() << "lines";
    }
    if (jkfLines.isEmpty()) {
        Q_EMIT statusMessage(tr("JKF形式の棋譜データがありません"), 3000);
        return false;
    }
    return setClipboardText(jkfLines.join(QStringLiteral("\n")),
                            tr("JKF形式の棋譜をクリップボードにコピーしました"));
}

bool KifuExportClipboard::copyUsenToClipboard()
{

    QStringList usenLines;
    if (m_deps.gameRecord) {
        usenLines = m_deps.gameRecord->toUsenLines(buildExportContext(), resolveUsiMoves());
        qCDebug(lcKifu).noquote() << "copyUsenToClipboard: generated" << usenLines.size() << "lines";
    }
    if (usenLines.isEmpty()) {
        Q_EMIT statusMessage(tr("USEN形式の棋譜データがありません"), 3000);
        return false;
    }
    return setClipboardText(usenLines.join(QStringLiteral("\n")),
                            tr("USEN形式の棋譜をクリップボードにコピーしました"));
}

bool KifuExportClipboard::copySfenToClipboard()
{

    PositionData pos = currentPositionData();
    if (pos.sfenStr.isEmpty()) {
        Q_EMIT statusMessage(tr("SFEN形式の局面データがありません"), 3000);
        return false;
    }
    qCDebug(lcKifu).noquote() << "copySfenToClipboard: copied" << pos.sfenStr.size() << "chars";
    return setClipboardText(pos.sfenStr,
                            tr("SFEN形式の局面をクリップボードにコピーしました"));
}

bool KifuExportClipboard::copyBodToClipboard()
{

    PositionData pos = currentPositionData();
    if (pos.sfenStr.isEmpty()) {
        Q_EMIT statusMessage(tr("BOD形式の局面データがありません"), 3000);
        return false;
    }

    QString bodText = generateBodText(pos);
    if (bodText.isEmpty()) {
        Q_EMIT statusMessage(tr("SFEN形式の解析に失敗しました"), 3000);
        return false;
    }
    qCDebug(lcKifu).noquote() << "copyBodToClipboard: copied" << bodText.size() << "chars";
    return setClipboardText(bodText,
                            tr("BOD形式の局面をクリップボードにコピーしました"));
}
