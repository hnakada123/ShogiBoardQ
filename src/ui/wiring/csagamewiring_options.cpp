/// @file csagamewiring_options.cpp
/// @brief CSA通信対局配線の補助処理（接続ダイアログの詳細設定・棋譜の自動保存・盤の反転・棋譜行の追記）

#include "csagamewiring.h"

#include "csagamecoordinator.h"
#include "csagamedialog.h"
#include "gamerecordupdateservice.h"
#include "kifudisplay.h"
#include "kifurecordlistmodel.h"
#include "logcategories.h"
#include "playmode.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStatusBar>

namespace {
/// ファイル名に使えない文字を取り除く
QString sanitizeFileNamePart(const QString& text)
{
    static const QRegularExpression forbidden(QStringLiteral("[\\\\/:*?\"<>|\\s]+"));
    QString out = text;
    out.replace(forbidden, QStringLiteral("_"));
    return out.isEmpty() ? QStringLiteral("player") : out;
}
} // namespace

CsaGameCoordinator::StartOptions CsaGameWiring::buildStartOptions(const CsaGameDialog* dialog)
{
    CsaGameCoordinator::StartOptions options;
    options.host = dialog->host();
    options.transport = dialog->useTls() ? CsaClient::Transport::Tls : CsaClient::Transport::Plain;
    options.port = dialog->port();
    options.username = dialog->loginId();
    // shogi-server のゲーム名と手番の希望は LOGIN のパスワード欄に載せる
    options.password = dialog->loginPassword();

    if (dialog->isHuman()) {
        options.playerType = CsaGameCoordinator::PlayerType::Human;
    } else {
        options.playerType = CsaGameCoordinator::PlayerType::Engine;
        options.engineName = dialog->engineName();
        options.enginePath = dialog->engineList().value(dialog->engineNumber()).path;
    }

    options.keepAliveEnabled = dialog->keepAliveEnabled();
    options.keepAliveIntervalMs = dialog->keepAliveIntervalSec() * 1000;
    options.tcpKeepAliveEnabled = dialog->tcpKeepAliveEnabled();
    options.thinkingMarginMs = dialog->thinkingMarginSec() * 1000;
    options.gameLimit = dialog->gameLimit();
    options.reconnectAfterGame = dialog->reconnectAfterGame();

    m_autoSaveEnabled = dialog->autoSaveEnabled();
    m_autoSaveDir = dialog->autoSaveDir();
    m_autoSaveFormat = dialog->autoSaveFormat();
    m_autoFlipBoard = dialog->autoFlipBoard();
    return options;
}

void CsaGameWiring::applyAutoFlip()
{
    if (!m_autoFlipBoard || !m_coordinator || !m_setBoardFlipped) return;
    m_setBoardFlipped(!m_coordinator->isBlackSide());
}

void CsaGameWiring::autoSaveRecord()
{
    if (!m_autoSaveEnabled || !m_coordinator) return;
    if (!m_saveKifuToPath) {
        qCWarning(lcUi) << "autoSaveRecord: no save callback";
        return;
    }
    if (m_autoSaveDir.isEmpty() || !QDir().mkpath(m_autoSaveDir)) {
        const QString text = tr("棋譜の自動保存先を作成できません: %1").arg(m_autoSaveDir);
        qCWarning(lcUi) << text;
        if (m_statusBar) m_statusBar->showMessage(text, 5000);
        return;
    }

    const CsaClient::GameSummary& summary = m_coordinator->gameSummary();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString base = QStringLiteral("%1_%2_vs_%3")
                             .arg(stamp, sanitizeFileNamePart(summary.blackName),
                                  sanitizeFileNamePart(summary.whiteName));
    const QString extension = m_autoSaveFormat.isEmpty() ? QStringLiteral("kif") : m_autoSaveFormat;
    // 同じ秒に終わった対局があっても上書きしない
    const QDir dir(m_autoSaveDir);
    QString path = dir.filePath(base + QLatin1Char('.') + extension);
    for (int serial = 2; QFileInfo::exists(path); ++serial) {
        path = dir.filePath(QStringLiteral("%1_%2.%3").arg(base).arg(serial).arg(extension));
    }

    QString error;
    if (m_saveKifuToPath(path, &error)) {
        qCDebug(lcUi) << "autoSaveRecord: saved" << path;
        if (m_statusBar) m_statusBar->showMessage(tr("棋譜を自動保存しました: %1").arg(path), 5000);
    } else {
        const QString text = tr("棋譜の自動保存に失敗しました: %1").arg(error.isEmpty() ? path : error);
        qCWarning(lcUi) << text;
        if (m_statusBar) m_statusBar->showMessage(text, 5000);
    }
}

void CsaGameWiring::onPlayModeChangedInternal(int mode)
{
    if (m_playMode) {
        *m_playMode = static_cast<PlayMode>(mode);
    }
    qCDebug(lcUi) << "onPlayModeChangedInternal: mode=" << mode;
}

void CsaGameWiring::showGameEndDialogInternal(const QString& title, const QString& message)
{
    // 対局を続けるときは、結果の表示が次の対局の開始を止めないようにする
    if (m_coordinator && m_coordinator->moreGamesWanted()) {
        auto* box = new QMessageBox(QMessageBox::Information, title, message, QMessageBox::Ok, m_parentWidget);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setModal(false);
        box->show();
        return;
    }
    QMessageBox::information(m_parentWidget, title, message);
}

void CsaGameWiring::appendInitialKifuLine(const QString& prettyMove, const RecordTime& time, const QString& sfen)
{
    const QString trimmed = prettyMove.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    const int beforeRows = m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1;
    if (m_recordService) {
        RecordEntry entry;
        entry.displayText = trimmed;
        entry.sfen = sfen;
        entry.time = time;
        if (m_gameMoves && !m_gameMoves->isEmpty()) entry.move = m_gameMoves->last();
        m_recordService->appendRecord(entry);
    }

    const bool appendedByService = m_kifuRecordModel && m_kifuRecordModel->rowCount() > beforeRows;
    if (!appendedByService && m_kifuRecordModel) {
        m_kifuRecordModel->appendItem(new KifuDisplay(buildNumberedKifuLine(trimmed), time.toText()));
    }
}

QString CsaGameWiring::buildNumberedKifuLine(const QString& prettyMove) const
{
    const int moveRows = m_kifuRecordModel ? qMax(0, m_kifuRecordModel->rowCount() - 1) : 0;

    const int nextMoveNumber = moveRows + 1;
    const QString moveNumberStr = QString::number(nextMoveNumber);
    const QString spaces = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));
    return spaces + moveNumberStr + QLatin1Char(' ') + prettyMove;
}
