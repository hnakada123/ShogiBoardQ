/// @file kifuclipboardservice.cpp
/// @brief 棋譜クリップボードサービスの実装

#include "kifuclipboardservice.h"
#include "gamerecordmodel.h"
#include "logcategories.h"
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

QStringList gameMovesToUsiMoves(const QList<ShogiMove>& moves)
{
    QStringList usiMoves;
    usiMoves.reserve(moves.size());

    for (const ShogiMove& mv : moves) {
        QString usiMove;

        // 駒打ち判定: x >= 9 は駒打ち
        if (mv.fromSquare.x() >= 9) {
            // 駒打ち: "P*5e" 形式
            QChar pieceChar = pieceToChar(toBlack(mv.movingPiece));
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
    auto expandBoard = [](const QString& boardStr) -> QList<QList<QString>> {
        QList<QList<QString>> board(9, QList<QString>(9));
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

        QList<QList<QString>> prevBoardArr = expandBoard(prevBoard);
        QList<QList<QString>> currBoardArr = expandBoard(currBoard);

        QPoint fromPos(-1, -1);
        QPoint toPos(-1, -1);
        QString movedPiece;
        bool isDrop = false;
        bool isPromotion = false;

        QList<QPoint> emptyPositions;
        QList<QPoint> filledPositions;

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

} // namespace KifuClipboardService
