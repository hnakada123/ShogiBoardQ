#ifndef TSUMEPOSITIONUTIL_H
#define TSUMEPOSITIONUTIL_H

/// @file tsumepositionutil.h
/// @brief 詰み探索用のUSI positionコマンド構築ユーティリティ

#include <QString>
#include <QStringList>

/**
 * @brief 詰み探索用に局面コマンドを構築するユーティリティ
 *
 * usiMovesが利用可能な場合は "position startpos moves ..." 形式を使用し、
 * そうでない場合は "position sfen ..." 形式にフォールバックする。
 *
 */
class TsumePositionUtil {
public:
    // --- 公開API ---

    /**
     * @brief USI形式の指し手リストを使用してpositionコマンドを構築（推奨）
     * @param usiMoves USI形式の指し手リスト（例: ["7g7f", "3c3d", ...]）
     * @param startPositionCmd 開始局面コマンド（"startpos" または "sfen ..."）
     * @param selectedIndex 現在の手数（0始まり）
     * @return "position startpos moves 7g7f 3c3d ..." または "position sfen ... moves ..."
     */
    static QString buildPositionWithMoves(const QStringList* usiMoves,
                                          const QString& startPositionCmd,
                                          int selectedIndex)
    {
        if (!usiMoves || usiMoves->isEmpty() || selectedIndex <= 0) {
            // 指し手がない場合は開始局面を返す
            if (startPositionCmd.isEmpty()) {
                return QStringLiteral("position startpos");
            }
            if (startPositionCmd == QStringLiteral("startpos")) {
                return QStringLiteral("position startpos");
            }
            // "sfen ..." の場合は "position sfen ..." にする
            if (startPositionCmd.startsWith(QStringLiteral("sfen "))) {
                return QStringLiteral("position ") + startPositionCmd;
            }
            return QStringLiteral("position sfen ") + startPositionCmd;
        }

        // selectedIndex手目までの指し手を取得
        const int moveCount = qMin(selectedIndex, static_cast<int>(usiMoves->size()));
        QStringList moves;
        for (int i = 0; i < moveCount; ++i) {
            moves.append(usiMoves->at(i));
        }

        QString base;
        if (startPositionCmd.isEmpty() || startPositionCmd == QStringLiteral("startpos")) {
            base = QStringLiteral("position startpos");
        } else if (startPositionCmd.startsWith(QStringLiteral("sfen "))) {
            base = QStringLiteral("position ") + startPositionCmd;
        } else {
            base = QStringLiteral("position sfen ") + startPositionCmd;
        }

        if (moves.isEmpty()) {
            return base;
        }
        return base + QStringLiteral(" moves ") + moves.join(QLatin1Char(' '));
    }

    /**
     * @brief SFEN形式で局面コマンドを構築（フォールバック用）
     *
     * 玉の有無から攻方を推定し、手番を強制設定して返す。
     */
    static QString buildPositionForMate(const QStringList* sfenRecord,
                                        const QString& startSfenStr,
                                        const QStringList& positionStrList,
                                        int selectedIndex)
    {
        // 処理フロー:
        // 1. 複数のデータソースから基本SFENを取得
        // 2. 玉の有無から攻方（手番）を推定
        // 3. 手番を強制設定してpositionコマンドを構築

        const int sel = qMax(0, selectedIndex);
        QString baseSfen;

        if (sfenRecord && !sfenRecord->isEmpty()) {
            const int safe = static_cast<int>(qBound(qsizetype(0), qsizetype(sel), sfenRecord->size() - 1));
            baseSfen = sfenRecord->at(safe);
        } else if (!startSfenStr.isEmpty()) {
            baseSfen = startSfenStr;
        } else if (!positionStrList.isEmpty()) {
            const int safe = static_cast<int>(qBound(qsizetype(0), qsizetype(sel), positionStrList.size() - 1));
            const QString pos = positionStrList.at(safe).trimmed();
            if (pos.startsWith(QStringLiteral("position sfen"))) {
                QString t = pos.mid(14).trimmed(); // "position sfen" を剥がす
                const int m = static_cast<int>(t.indexOf(QStringLiteral(" moves ")));
                baseSfen = (m >= 0) ? t.left(m).trimmed() : t;
            }
        }
        if (baseSfen.isEmpty()) return QString();

        // 玉の有無から攻方を推定 → b/w を強制
        auto decideTurnFromSfen = [](const QString& sfen)->QChar {
            const QStringList toks = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (toks.isEmpty()) return QLatin1Char('b');
            const QString& board = toks.at(0);
            int cntK = 0, cntk = 0;
            for (const QChar ch : board) {
                if (ch == QLatin1Char('K')) ++cntK;
                else if (ch == QLatin1Char('k')) ++cntk;
            }
            if ((cntK > 0) ^ (cntk > 0)) return (cntk > 0) ? QLatin1Char('b') : QLatin1Char('w');
            if (toks.size() >= 2) {
                if (toks[1] == QLatin1String("b")) return QLatin1Char('b');
                if (toks[1] == QLatin1String("w")) return QLatin1Char('w');
            }
            return QLatin1Char('b');
        };

        auto forceTurnInSfen = [](const QString& sfen, QChar turn)->QString {
            QStringList toks = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (toks.size() >= 2) {
                toks[1] = QString(turn);
                if (toks.size() >= 4) toks[3] = QStringLiteral("1");
                return toks.join(QLatin1Char(' '));
            }
            return sfen + QLatin1Char(' ') + QString(turn) + QLatin1String(" - 1");
        };

        const QChar desiredTurn = decideTurnFromSfen(baseSfen);
        const QString forced    = forceTurnInSfen(baseSfen, desiredTurn);
        return QStringLiteral("position sfen %1").arg(forced);
    }
};

#endif // TSUMEPOSITIONUTIL_H
