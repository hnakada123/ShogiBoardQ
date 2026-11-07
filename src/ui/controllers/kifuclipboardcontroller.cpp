/// @file kifuclipboardcontroller.cpp
/// @brief 棋譜・局面のクリップボードへのコピーの実装

#include "kifuclipboardcontroller.h"

#include "bodtextgenerator.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "kifuexport.h"
#include "kifunavigationstate.h"
#include "kifusavedialogs.h"
#include "shogiboard.h"
#include "usiexporter.h"

#include <QApplication>
#include <QClipboard>

KifuClipboardController::KifuClipboardController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.document && m_deps.board && m_deps.navState);
}

bool KifuClipboardController::setClipboardText(const QString& text, const QString& successMsg)
{
    if (QClipboard* clipboard = QApplication::clipboard()) {
        clipboard->setText(text);
        emit statusMessage(successMsg, 3000);
        return true;
    }
    emit statusMessage(tr("クリップボードへのコピーに失敗しました"), 3000);
    return false;
}

bool KifuClipboardController::copyRecord(KifuFormat format)
{
    // 局面だけの形式は棋譜としてはコピーしない（局面のコピーは別の操作）
    if (format == KifuFormat::Sfen || format == KifuFormat::Bod) return false;
    const QString name = KifuSaveDialogs::formatName(format);
    const QStringList lines = KifuExport::lines(*m_deps.document, format, KifuEncoding::Utf8);
    if (lines.isEmpty()) {
        emit statusMessage(tr("%1形式の棋譜データがありません").arg(name), 3000);
        return false;
    }
    // 書き出しで確定した入力中のコメントも数える。失うものは保存の確認と同じ表（KifuExport::lossIn）で決める
    const KifuExport::Loss loss = KifuExport::lossIn(*m_deps.document->tree(), format);
    return setClipboardText(lines.join(QStringLiteral("\n")),
                            loss.isEmpty() ? tr("%1形式の棋譜をクリップボードにコピーしました").arg(name)
                                           : tr("%1形式の棋譜をクリップボードにコピーしました（%1形式は%2に対応していないため、含みません）")
                                                 .arg(name, KifuSaveDialogs::lostItemsText(loss)));
}

bool KifuClipboardController::copyUsiCurrentToClipboard()
{
    // 表示中の手順の、表示中の手までの指し手（終局の行まで来ていればその終局）
    QList<KifuBranchNode*> nodes;
    TerminalType terminal = TerminalType::None;
    const KifuNavigationState* nav = m_deps.navState;
    const int limit = nav->currentPly();
    for (KifuBranchNode* node : nav->selectedPath()) {
        if (node->ply() > limit) break;
        if (node->isTerminal()) {
            terminal = node->terminalType();
            break;
        }
        nodes.append(node);
    }
    const QStringList moves = KifuBranchTree::toRecordLine(nodes).usiMoves();
    const QStringList lines = UsiExporter::exportPosition(m_deps.document->startSfen(), moves, terminal);
    if (lines.isEmpty()) {
        emit statusMessage(tr("USI形式の棋譜データがありません"), 3000);
        return false;
    }
    return setClipboardText(lines.join(QStringLiteral("\n")),
                            tr("USI形式（現在の指し手まで）の棋譜をクリップボードにコピーしました"));
}

bool KifuClipboardController::copySfenToClipboard()
{
    // 局面編集による変更も含め、盤に出している局面・手番・持ち駒をコピーする
    return setClipboardText(m_deps.board->toSfen(), tr("SFEN形式の局面をクリップボードにコピーしました"));
}

bool KifuClipboardController::copyBodToClipboard()
{
    // 最終手は表示中の手（局面図の「手数＝N ▲７六歩 まで」）
    const KifuBranchNode* node = m_deps.navState->currentNode();
    const int ply = node ? node->ply() : 0;
    const QString lastMove = node && node->isActualMove() ? node->displayText() : QString();
    // 駒落ちは KIF の局面図と同じく、対局情報の見出しに合わせて下手・上手と呼ぶ
    const QString bodText = BodTextGenerator::generate(m_deps.board->position(), ply, lastMove, m_deps.document->usesHandicapNames());
    return setClipboardText(bodText, tr("BOD形式の局面をクリップボードにコピーしました"));
}
