/// @file kifuexportclipboard.cpp
/// @brief 棋譜クリップボードコピー機能の実装

#include "kifuexportclipboard.h"

#include <QClipboard>
#include <QApplication>
#include "logcategories.h"

#include "gamerecordmodel.h"
#include "kifurecordlistmodel.h"
#include "gameinfopanecontroller.h"
#include "timecontrolcontroller.h"
#include "kifuloadcoordinator.h"
#include "matchcoordinator.h"
#include "replaycontroller.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "kifuclipboardservice.h"
#include "bodtextgenerator.h"
#include "usimoveconverter.h"

KifuExportClipboard::KifuExportClipboard(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
{
}

void KifuExportClipboard::setDependencies(const Deps& deps)
{
    m_deps = deps;
}

void KifuExportClipboard::setPrepareCallback(std::function<void()> callback)
{
    m_prepareCallback = std::move(callback);
}

// --------------------------------------------------------
// ユーティリティ
// --------------------------------------------------------

QStringList KifuExportClipboard::resolveUsiMoves() const
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
        QStringList moves = UsiMoveConverter::fromSfenRecord(*m_deps.sfenRecord);
        qCDebug(lcKifu).noquote() << "usiMoves from sfenRecord, size =" << moves.size();
        return moves;
    }

    return QStringList();
}

GameRecordModel::ExportContext KifuExportClipboard::buildExportContext() const
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

bool KifuExportClipboard::isCurrentlyPlaying() const
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

int KifuExportClipboard::currentPly() const
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

KifuExportClipboard::PositionData KifuExportClipboard::getCurrentPositionData() const
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

QString KifuExportClipboard::generateBodText(const PositionData& pos) const
{
    return BodTextGenerator::generate(pos.sfenStr, pos.moveIndex, pos.lastMoveStr);
}

// --------------------------------------------------------
// クリップボードコピー
// --------------------------------------------------------

KifuClipboardService::ExportContext KifuExportClipboard::buildClipboardContext() const
{
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
    return ctx;
}

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
    if (m_prepareCallback) m_prepareCallback();

    if (!m_deps.gameRecord) {
        Q_EMIT statusMessage(tr("KIF形式の棋譜データがありません"), 3000);
        return false;
    }

    if (KifuClipboardService::copyKif(buildClipboardContext())) {
        Q_EMIT statusMessage(tr("KIF形式の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    }
    Q_EMIT statusMessage(tr("KIF形式の棋譜データがありません"), 3000);
    return false;
}

bool KifuExportClipboard::copyKi2ToClipboard()
{
    if (m_prepareCallback) m_prepareCallback();

    if (!m_deps.gameRecord) {
        Q_EMIT statusMessage(tr("KI2形式の棋譜データがありません"), 3000);
        return false;
    }

    if (KifuClipboardService::copyKi2(buildClipboardContext())) {
        Q_EMIT statusMessage(tr("KI2形式の棋譜をクリップボードにコピーしました"), 3000);
        return true;
    }
    Q_EMIT statusMessage(tr("KI2形式の棋譜データがありません"), 3000);
    return false;
}

bool KifuExportClipboard::copyCsaToClipboard()
{
    if (m_prepareCallback) m_prepareCallback();

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
                            tr("CSA形式の棋譜をクリップボードにコピーしました"));
}

bool KifuExportClipboard::copyUsiToClipboard()
{
    if (m_prepareCallback) m_prepareCallback();

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
                            tr("USI形式の棋譜をクリップボードにコピーしました"));
}

bool KifuExportClipboard::copyUsiCurrentToClipboard()
{
    if (m_prepareCallback) m_prepareCallback();

    QStringList usiMovesForOutput = resolveUsiMoves();
    const int limit = m_deps.currentMoveIndex;
    if (limit >= 0 && limit < usiMovesForOutput.size()) {
        usiMovesForOutput = usiMovesForOutput.mid(0, limit);
        qCDebug(lcKifu).noquote() << "copyUsiCurrentToClipboard: limited to" << limit << "moves";
    }

    QStringList usiLines;
    if (m_deps.gameRecord) {
        usiLines = m_deps.gameRecord->toUsiLines(buildExportContext(), usiMovesForOutput);
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
    if (m_prepareCallback) m_prepareCallback();

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
    if (m_prepareCallback) m_prepareCallback();

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
    if (m_prepareCallback) m_prepareCallback();

    PositionData pos = getCurrentPositionData();
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
    qCDebug(lcKifu).noquote() << "copyBodToClipboard: copied" << bodText.size() << "chars";
    return setClipboardText(bodText,
                            tr("BOD形式の局面をクリップボードにコピーしました"));
}
