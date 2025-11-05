#ifndef TSUMEPOSITIONUTIL_H
#define TSUMEPOSITIONUTIL_H

#include <QString>
#include <QStringList>

// 詰み探索用に SFEN を「手番を明示」へ正規化し、"position sfen <...>" を返すユーティリティ。
class TsumePositionUtil {
public:
    static QString buildPositionForMate(const QStringList* sfenRecord,
                                        const QString& startSfenStr,
                                        const QStringList& positionStrList,
                                        int selectedIndex)
    {
        const int sel = qMax(0, selectedIndex);
        QString baseSfen;

        if (sfenRecord && !sfenRecord->isEmpty()) {
            const int safe = qBound(0, sel, sfenRecord->size() - 1);
            baseSfen = sfenRecord->at(safe);
        } else if (!startSfenStr.isEmpty()) {
            baseSfen = startSfenStr;
        } else if (!positionStrList.isEmpty()) {
            const int safe = qBound(0, sel, positionStrList.size() - 1);
            const QString pos = positionStrList.at(safe).trimmed();
            if (pos.startsWith(QStringLiteral("position sfen"))) {
                QString t = pos.mid(14).trimmed(); // "position sfen" を剥がす
                const int m = t.indexOf(QStringLiteral(" moves "));
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
