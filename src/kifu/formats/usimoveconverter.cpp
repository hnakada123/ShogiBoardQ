/// @file usimoveconverter.cpp
/// @brief USI指し手変換クラスの実装

#include "usimoveconverter.h"

#include <QPoint>
#include <QVector>

#include "shogimove.h"
#include "shogitypes.h"

QStringList UsiMoveConverter::fromGameMoves(const QVector<ShogiMove>& moves)
{
    QStringList usiMoves;
    usiMoves.reserve(moves.size());

    for (const ShogiMove& mv : moves) {
        QString usiMove;

        if (mv.fromSquare.x() >= 9) {
            // 駒打ち
            QChar pieceChar = pieceToChar(toBlack(mv.movingPiece));
            int toFile = mv.toSquare.x() + 1;
            int toRank = mv.toSquare.y() + 1;
            QChar toRankChar = QChar('a' + toRank - 1);
            usiMove = QStringLiteral("%1*%2%3").arg(pieceChar).arg(toFile).arg(toRankChar);
        } else {
            // 通常移動
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

QStringList UsiMoveConverter::fromSfenRecord(const QStringList& sfenRecord)
{
    QStringList usiMoves;

    if (sfenRecord.size() < 2) {
        return usiMoves;
    }

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

        QVector<QVector<QString>> prevBoardArr = expandBoard(prevParts[0]);
        QVector<QVector<QString>> currBoardArr = expandBoard(currParts[0]);

        QPoint fromPos(-1, -1);
        QPoint toPos(-1, -1);
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

        QString movedPiece;
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
