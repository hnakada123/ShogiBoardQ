/// @file kifuclipboardservice.cpp
/// @brief 棋譜クリップボードサービスの実装

#include "kifuclipboardservice.h"
#include "gamerecordmodel.h"
#include "kifulogging.h"
#include "shogimove.h"

#include <QApplication>
#include <QClipboard>
#include <QTableWidget>

namespace KifuClipboardService {

namespace {

/// GameRecordModel用のExportContextを構築
GameRecordModel::ExportContext buildModelContext(const ExportContext& ctx)
{
    GameRecordModel::ExportContext modelCtx;
    modelCtx.gameInfoTable = ctx.gameInfoTable;
    modelCtx.recordModel   = ctx.recordModel;
    modelCtx.startSfen     = ctx.startSfen;
    modelCtx.playMode      = ctx.playMode;
    modelCtx.human1        = ctx.human1;
    modelCtx.human2        = ctx.human2;
    modelCtx.engine1       = ctx.engine1;
    modelCtx.engine2       = ctx.engine2;
    return modelCtx;
}

/// クリップボードにテキストを設定
bool setClipboardText(const QString& text)
{
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
        return true;
    }
    return false;
}

/// USI指し手リストを取得（必要に応じてSFENレコードから生成）
QStringList resolveUsiMoves(const ExportContext& ctx)
{
    QStringList moves = ctx.usiMoves;
    if (moves.isEmpty() && ctx.sfenRecord && ctx.sfenRecord->size() > 1) {
        moves = sfenRecordToUsiMoves(*ctx.sfenRecord);
        qCDebug(lcKifu).noquote() << "generated" << moves.size() << "USI moves from sfenRecord";
    }
    return moves;
}

} // anonymous namespace

bool copyKif(const ExportContext& ctx)
{
    if (!ctx.gameRecord) {
        qCWarning(lcKifu) << "copyKif: gameRecord is null";
        return false;
    }

    GameRecordModel::ExportContext modelCtx = buildModelContext(ctx);
    QStringList kifLines = ctx.gameRecord->toKifLines(modelCtx);

    if (kifLines.isEmpty()) {
        qCDebug(lcKifu).noquote() << "copyKif: no KIF data";
        return false;
    }

    const QString kifText = kifLines.join(QStringLiteral("\n"));
    if (setClipboardText(kifText)) {
        qCDebug(lcKifu).noquote() << "copyKif: copied" << kifText.size() << "chars";
        return true;
    }
    return false;
}

bool copyKi2(const ExportContext& ctx)
{
    if (!ctx.gameRecord) {
        qCWarning(lcKifu) << "copyKi2: gameRecord is null";
        return false;
    }

    GameRecordModel::ExportContext modelCtx = buildModelContext(ctx);
    QStringList ki2Lines = ctx.gameRecord->toKi2Lines(modelCtx);

    if (ki2Lines.isEmpty()) {
        qCDebug(lcKifu).noquote() << "copyKi2: no KI2 data";
        return false;
    }

    const QString ki2Text = ki2Lines.join(QStringLiteral("\n"));
    if (setClipboardText(ki2Text)) {
        qCDebug(lcKifu).noquote() << "copyKi2: copied" << ki2Text.size() << "chars";
        return true;
    }
    return false;
}

bool copyCsa(const ExportContext& ctx)
{
    if (!ctx.gameRecord) {
        qCWarning(lcKifu) << "copyCsa: gameRecord is null";
        return false;
    }

    QStringList usiMoves = resolveUsiMoves(ctx);
    GameRecordModel::ExportContext modelCtx = buildModelContext(ctx);
    QStringList csaLines = ctx.gameRecord->toCsaLines(modelCtx, usiMoves);

    if (csaLines.isEmpty()) {
        qCDebug(lcKifu).noquote() << "copyCsa: no CSA data";
        return false;
    }

    const QString csaText = csaLines.join(QStringLiteral("\n"));
    if (setClipboardText(csaText)) {
        qCDebug(lcKifu).noquote() << "copyCsa: copied" << csaText.size() << "chars";
        return true;
    }
    return false;
}

bool copyUsi(const ExportContext& ctx)
{
    if (!ctx.gameRecord) {
        qCWarning(lcKifu) << "copyUsi: gameRecord is null";
        return false;
    }

    QStringList usiMoves = resolveUsiMoves(ctx);
    GameRecordModel::ExportContext modelCtx = buildModelContext(ctx);
    QStringList usiLines = ctx.gameRecord->toUsiLines(modelCtx, usiMoves);

    if (usiLines.isEmpty()) {
        qCDebug(lcKifu).noquote() << "copyUsi: no USI data";
        return false;
    }

    const QString usiText = usiLines.join(QStringLiteral("\n"));
    if (setClipboardText(usiText)) {
        qCDebug(lcKifu).noquote() << "copyUsi: copied" << usiText.size() << "chars";
        return true;
    }
    return false;
}

bool copyUsiCurrent(const ExportContext& ctx, int currentMoveIndex)
{
    if (!ctx.gameRecord) {
        qCWarning(lcKifu) << "copyUsiCurrent: gameRecord is null";
        return false;
    }

    QStringList usiMoves = resolveUsiMoves(ctx);

    // 現在の手数まで制限
    if (currentMoveIndex >= 0 && currentMoveIndex < usiMoves.size()) {
        usiMoves = usiMoves.mid(0, currentMoveIndex);
        qCDebug(lcKifu).noquote() << "copyUsiCurrent: limited to" << currentMoveIndex << "moves";
    }

    GameRecordModel::ExportContext modelCtx = buildModelContext(ctx);
    QStringList usiLines = ctx.gameRecord->toUsiLines(modelCtx, usiMoves);

    if (usiLines.isEmpty()) {
        qCDebug(lcKifu).noquote() << "copyUsiCurrent: no USI data";
        return false;
    }

    const QString usiText = usiLines.join(QStringLiteral("\n"));
    if (setClipboardText(usiText)) {
        qCDebug(lcKifu).noquote() << "copyUsiCurrent: copied" << usiText.size() << "chars";
        return true;
    }
    return false;
}

bool copyJkf(const ExportContext& ctx)
{
    if (!ctx.gameRecord) {
        qCWarning(lcKifu) << "copyJkf: gameRecord is null";
        return false;
    }

    GameRecordModel::ExportContext modelCtx = buildModelContext(ctx);
    QStringList jkfLines = ctx.gameRecord->toJkfLines(modelCtx);

    if (jkfLines.isEmpty()) {
        qCDebug(lcKifu).noquote() << "copyJkf: no JKF data";
        return false;
    }

    const QString jkfText = jkfLines.join(QStringLiteral("\n"));
    if (setClipboardText(jkfText)) {
        qCDebug(lcKifu).noquote() << "copyJkf: copied" << jkfText.size() << "chars";
        return true;
    }
    return false;
}

bool copyUsen(const ExportContext& ctx)
{
    if (!ctx.gameRecord) {
        qCWarning(lcKifu) << "copyUsen: gameRecord is null";
        return false;
    }

    QStringList usiMoves = resolveUsiMoves(ctx);
    GameRecordModel::ExportContext modelCtx = buildModelContext(ctx);
    QStringList usenLines = ctx.gameRecord->toUsenLines(modelCtx, usiMoves);

    if (usenLines.isEmpty()) {
        qCDebug(lcKifu).noquote() << "copyUsen: no USEN data";
        return false;
    }

    const QString usenText = usenLines.join(QStringLiteral("\n"));
    if (setClipboardText(usenText)) {
        qCDebug(lcKifu).noquote() << "copyUsen: copied" << usenText.size() << "chars";
        return true;
    }
    return false;
}

bool copySfen(const ExportContext& ctx)
{
    QString sfenStr;

    if (ctx.sfenRecord && !ctx.sfenRecord->isEmpty()) {
        int idx = ctx.isPlaying
                      ? static_cast<int>(ctx.sfenRecord->size() - 1)
                      : ctx.currentPly;

        // 範囲チェック
        if (idx < 0) {
            idx = 0;
        } else if (idx >= ctx.sfenRecord->size()) {
            idx = static_cast<int>(ctx.sfenRecord->size() - 1);
        }

        sfenStr = ctx.sfenRecord->at(idx);
        qCDebug(lcKifu).noquote() << "copySfen: idx=" << idx << "sfen=" << sfenStr;
    }

    if (sfenStr.isEmpty()) {
        qCDebug(lcKifu).noquote() << "copySfen: no SFEN data";
        return false;
    }

    if (setClipboardText(sfenStr)) {
        qCDebug(lcKifu).noquote() << "copySfen: copied" << sfenStr.size() << "chars";
        return true;
    }
    return false;
}

bool copyBod(const ExportContext& ctx)
{
    // BOD形式はMainWindow側で直接処理する（toBodLinesメソッドがGameRecordModelにないため）
    Q_UNUSED(ctx);
    qCDebug(lcKifu).noquote() << "copyBod: not implemented in service, use MainWindow directly";
    return false;
}

QStringList gameMovesToUsiMoves(const QVector<ShogiMove>& moves)
{
    QStringList usiMoves;
    usiMoves.reserve(moves.size());

    for (const ShogiMove& mv : moves) {
        QString usiMove;

        // 駒打ち判定: x >= 9 は駒打ち
        if (mv.fromSquare.x() >= 9) {
            // 駒打ち: "P*5e" 形式
            QChar pieceChar = mv.movingPiece.toUpper();
            int toFile = mv.toSquare.x() + 1;
            int toRank = mv.toSquare.y() + 1;
            QChar toRankChar = QChar('a' + toRank - 1);
            usiMove = QStringLiteral("%1*%2%3").arg(pieceChar).arg(toFile).arg(toRankChar);
        } else {
            // 通常移動: "7g7f" 形式
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

QStringList sfenRecordToUsiMoves(const QStringList& sfenRecord)
{
    QStringList usiMoves;

    if (sfenRecord.size() < 2) {
        return usiMoves;
    }

    // 盤面展開関数
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

    for (int i = 1; i < sfenRecord.size(); ++i) {
        const QString& prevSfen = sfenRecord.at(i - 1);
        const QString& currSfen = sfenRecord.at(i);

        const QStringList prevParts = prevSfen.split(QLatin1Char(' '));
        const QStringList currParts = currSfen.split(QLatin1Char(' '));

        if (prevParts.size() < 3 || currParts.size() < 3) {
            usiMoves.append(QString());
            continue;
        }

        const QString prevBoard = prevParts[0];
        const QString currBoard = currParts[0];

        QVector<QVector<QString>> prevBoardArr = expandBoard(prevBoard);
        QVector<QVector<QString>> currBoardArr = expandBoard(currBoard);

        QPoint fromPos(-1, -1);
        QPoint toPos(-1, -1);
        QString movedPiece;
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

QString getClipboardText()
{
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        return clipboard->text();
    }
    return QString();
}

KifuFormat detectFormat(const QString& text)
{
    if (text.isEmpty()) {
        return KifuFormat::Unknown;
    }

    const QString trimmed = text.trimmed();

    // JKF: JSON形式
    if (trimmed.startsWith(QLatin1Char('{')) || trimmed.startsWith(QLatin1Char('['))) {
        return KifuFormat::Jkf;
    }

    // SFEN: "position sfen" または "sfen" で始まる、または標準SFEN形式
    if (trimmed.startsWith(QLatin1String("position ")) ||
        trimmed.startsWith(QLatin1String("sfen ")) ||
        trimmed.startsWith(QLatin1String("startpos"))) {
        return KifuFormat::Sfen;
    }

    // USEN: Base64エンコードされた文字列（英数字と+/=のみ）
    static const QRegularExpression usenPattern(
        QStringLiteral("^[A-Za-z0-9+/=]{20,}$"));
    if (usenPattern.match(trimmed).hasMatch()) {
        return KifuFormat::Usen;
    }

    // CSA: バージョン行またはCSA形式の指し手
    if (trimmed.startsWith(QLatin1String("V2")) ||
        trimmed.startsWith(QLatin1String("N+")) ||
        trimmed.startsWith(QLatin1String("N-")) ||
        trimmed.startsWith(QLatin1String("P1")) ||
        trimmed.contains(QRegularExpression(QStringLiteral("^[+-]\\d{4}\\w{2}")))) {
        return KifuFormat::Csa;
    }

    // USI: "position" で始まる、または "moves" を含む
    if (trimmed.contains(QLatin1String("moves "))) {
        return KifuFormat::Usi;
    }

    // KIF/KI2: 日本語を含む
    if (trimmed.contains(QRegularExpression(QStringLiteral("[\\x{3040}-\\x{309F}\\x{30A0}-\\x{30FF}\\x{4E00}-\\x{9FFF}]")))) {
        // KI2: "▲" "△" で始まる指し手行が多い
        if (trimmed.contains(QRegularExpression(QStringLiteral("^[▲△]")))) {
            return KifuFormat::Ki2;
        }
        // KIF: "手数" や数字で始まる行
        if (trimmed.contains(QStringLiteral("手数")) ||
            trimmed.contains(QRegularExpression(QStringLiteral("^\\s*\\d+\\s+")))) {
            return KifuFormat::Kif;
        }
        // デフォルトはKIF
        return KifuFormat::Kif;
    }

    return KifuFormat::Unknown;
}

} // namespace KifuClipboardService
