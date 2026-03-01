/// @file josekirepository.cpp
/// @brief 定跡データのファイルI/Oを担当するリポジトリクラスの実装

#include "josekirepository.h"
#include "josekiwindow.h"  // JosekiMove 構造体
#include "logcategories.h"

#include <QFile>
#include <QTextStream>
#include <QStringView>

const QVector<JosekiMove> JosekiRepository::s_emptyMoves;

void JosekiRepository::clear()
{
    m_josekiData.clear();
    m_sfenWithPlyMap.clear();
    m_mergeRegisteredMoves.clear();
}

bool JosekiRepository::containsPosition(const QString &normalizedSfen) const
{
    return m_josekiData.contains(normalizedSfen);
}

const QVector<JosekiMove> &JosekiRepository::movesForPosition(const QString &normalizedSfen) const
{
    auto it = m_josekiData.constFind(normalizedSfen);
    if (it != m_josekiData.constEnd()) {
        return it.value();
    }
    return s_emptyMoves;
}

void JosekiRepository::addMove(const QString &normalizedSfen, const JosekiMove &move)
{
    m_josekiData[normalizedSfen].append(move);
}

void JosekiRepository::updateMove(const QString &normalizedSfen, const QString &usiMove,
                                   int value, int depth, int frequency, const QString &comment)
{
    if (!m_josekiData.contains(normalizedSfen)) return;

    QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
    for (int i = 0; i < moves.size(); ++i) {
        if (moves[i].move == usiMove) {
            moves[i].value = value;
            moves[i].depth = depth;
            moves[i].frequency = frequency;
            moves[i].comment = comment;
            break;
        }
    }
}

void JosekiRepository::deleteMove(const QString &normalizedSfen, int index)
{
    if (!m_josekiData.contains(normalizedSfen)) return;

    QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
    if (index < 0 || index >= moves.size()) return;

    moves.removeAt(index);

    // この局面の定跡手がなくなった場合はエントリも削除
    if (moves.isEmpty()) {
        m_josekiData.remove(normalizedSfen);
        m_sfenWithPlyMap.remove(normalizedSfen);
    }
}

void JosekiRepository::removeMoveByUsi(const QString &normalizedSfen, const QString &usiMove)
{
    if (!m_josekiData.contains(normalizedSfen)) return;

    QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
    for (int i = 0; i < moves.size(); ++i) {
        if (moves[i].move == usiMove) {
            moves.removeAt(i);
            break;
        }
    }
}

void JosekiRepository::registerMergeMove(const QString &normalizedSfen, const QString &sfenWithPly,
                                          const QString &usiMove)
{
    // 登録済みセットに追加
    QString key = normalizedSfen + QStringLiteral(":") + usiMove;
    m_mergeRegisteredMoves.insert(key);

    // 既存の定跡手があるか確認
    if (m_josekiData.contains(normalizedSfen)) {
        QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];

        // 同じ指し手が既にあるか確認
        for (int i = 0; i < moves.size(); ++i) {
            if (moves[i].move == usiMove) {
                // 既に登録済みの場合は出現頻度を増やす
                moves[i].frequency += 1;
                qCDebug(lcUi) << "Updated frequency for existing move:" << usiMove;
                return;
            }
        }

        // 新しい指し手として追加
        JosekiMove newMove;
        newMove.move = usiMove;
        newMove.nextMove = QStringLiteral("none");
        newMove.value = 0;
        newMove.depth = 0;
        newMove.frequency = 1;
        moves.append(newMove);
        qCDebug(lcUi) << "Added new move to existing position:" << usiMove;
    } else {
        // 新しい局面として追加
        JosekiMove newMove;
        newMove.move = usiMove;
        newMove.nextMove = QStringLiteral("none");
        newMove.value = 0;
        newMove.depth = 0;
        newMove.frequency = 1;

        QVector<JosekiMove> moves;
        moves.append(newMove);
        m_josekiData[normalizedSfen] = moves;

        // 手数付きSFENも保存
        ensureSfenWithPly(normalizedSfen, sfenWithPly);

        qCDebug(lcUi) << "Added new position with move:" << usiMove;
    }
}

void JosekiRepository::ensureSfenWithPly(const QString &normalizedSfen, const QString &sfenWithPly)
{
    if (!m_sfenWithPlyMap.contains(normalizedSfen)) {
        m_sfenWithPlyMap[normalizedSfen] = sfenWithPly;
    }
}

QString JosekiRepository::sfenWithPly(const QString &normalizedSfen) const
{
    return m_sfenWithPlyMap.value(normalizedSfen);
}

JosekiLoadResult JosekiRepository::parseFromFile(const QString &filePath)
{
    JosekiLoadResult result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = QStringLiteral("ファイルを開けませんでした: %1").arg(filePath);
        return result;
    }

    QTextStream in(&file);
    QString line;
    QString currentSfen;
    QString normalizedSfen;

    bool hasValidHeader = false;
    bool hasSfenLine = false;
    bool hasMoveLine = false;
    int lineNumber = 0;
    int invalidMoveLineCount = 0;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        lineNumber++;

        line.remove(QLatin1Char('\r'));

        if (line.isEmpty()) {
            continue;
        }

        if (line.startsWith(QLatin1Char('#'))) {
            if (line.contains(QStringLiteral("YANEURAOU")) ||
                line.contains(QStringLiteral("yaneuraou"))) {
                hasValidHeader = true;
                continue;
            }

            if (!normalizedSfen.isEmpty() && result.josekiData.contains(normalizedSfen)) {
                QVector<JosekiMove> &moves = result.josekiData[normalizedSfen];
                if (!moves.isEmpty()) {
                    QString commentText = line.mid(1);
                    if (!moves.last().comment.isEmpty()) {
                        moves.last().comment += QStringLiteral(" ") + commentText;
                    } else {
                        moves.last().comment = commentText;
                    }
                }
            }
            continue;
        }

        if (line.startsWith(QStringLiteral("sfen "))) {
            currentSfen = line.mid(5).trimmed();
            currentSfen.remove(QLatin1Char('\r'));
            const QStringList sfenParts = currentSfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (sfenParts.size() >= 3) {
                normalizedSfen = sfenParts[0] + QLatin1Char(' ') + sfenParts[1] + QLatin1Char(' ') + sfenParts[2];
            } else {
                normalizedSfen = currentSfen;
            }
            if (!result.sfenWithPlyMap.contains(normalizedSfen)) {
                result.sfenWithPlyMap[normalizedSfen] = currentSfen;
            }
            hasSfenLine = true;
            continue;
        }

        if (!normalizedSfen.isEmpty()) {
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                const QString &moveStr = parts[0];
                bool validMove = false;

                if (moveStr.size() >= 4 && moveStr.at(1) == QLatin1Char('*')) {
                    QChar piece = moveStr.at(0).toUpper();
                    if (QString("PLNSGBR").contains(piece)) {
                        validMove = true;
                    }
                }
                else if (moveStr.size() >= 4) {
                    bool validFormat = true;
                    if (moveStr.at(0) < QLatin1Char('1') || moveStr.at(0) > QLatin1Char('9')) validFormat = false;
                    if (moveStr.at(1) < QLatin1Char('a') || moveStr.at(1) > QLatin1Char('i')) validFormat = false;
                    if (moveStr.at(2) < QLatin1Char('1') || moveStr.at(2) > QLatin1Char('9')) validFormat = false;
                    if (moveStr.at(3) < QLatin1Char('a') || moveStr.at(3) > QLatin1Char('i')) validFormat = false;
                    validMove = validFormat;
                }

                if (!validMove) {
                    invalidMoveLineCount++;
                    qCWarning(lcUi) << "Invalid move format at line" << lineNumber << ":" << moveStr;
                }

                JosekiMove move;
                move.move = parts[0];
                move.nextMove = parts[1];
                move.value = parts[2].toInt();
                move.depth = parts[3].toInt();
                move.frequency = parts[4].toInt();

                if (parts.size() > 5) {
                    QStringList commentParts;
                    for (int i = 5; i < parts.size(); ++i) {
                        commentParts.append(parts[i]);
                    }
                    move.comment = commentParts.join(QLatin1Char(' '));
                }

                result.josekiData[normalizedSfen].append(move);
                hasMoveLine = true;
            } else if (parts.size() > 0) {
                invalidMoveLineCount++;
                qCWarning(lcUi) << "Invalid line format at line" << lineNumber
                                << ": expected 5+ fields, got" << parts.size();
            }
        }
    }

    file.close();

    if (!hasValidHeader) {
        result.errorMessage = QStringLiteral(
            "このファイルはやねうら王定跡フォーマット(YANEURAOU-DB2016)ではありません。\n"
            "ヘッダー行（#YANEURAOU-DB2016 等）が見つかりませんでした。\n\n"
            "ファイル: %1").arg(filePath);
        return result;
    }

    if (!hasSfenLine) {
        result.errorMessage = QStringLiteral(
            "定跡ファイルにSFEN行が見つかりませんでした。\n\n"
            "やねうら王定跡フォーマットでは「sfen 」で始まる局面行が必要です。\n\n"
            "ファイル: %1").arg(filePath);
        return result;
    }

    if (!hasMoveLine) {
        result.errorMessage = QStringLiteral(
            "定跡ファイルに有効な指し手行が見つかりませんでした。\n\n"
            "やねうら王定跡フォーマットでは指し手行に少なくとも5つのフィールド\n"
            "（指し手 予想応手 評価値 深さ 出現頻度）が必要です。\n\n"
            "ファイル: %1").arg(filePath);
        return result;
    }

    result.success = true;
    result.positionCount = static_cast<int>(result.josekiData.size());

    qCInfo(lcUi) << "Parsed" << result.positionCount << "positions from" << filePath;
    if (invalidMoveLineCount > 0) {
        qCWarning(lcUi) << invalidMoveLineCount << "lines had invalid format";
    }

    QString hirate = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");
    if (result.josekiData.contains(hirate)) {
        qCDebug(lcUi) << "Hirate position has" << result.josekiData[hirate].size() << "moves";
    }

    return result;
}

JosekiSaveResult JosekiRepository::serializeToFile(
    const QString &filePath,
    const QMap<QString, QVector<JosekiMove>> &josekiData,
    const QMap<QString, QString> &sfenWithPlyMap)
{
    JosekiSaveResult result;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.errorMessage = QStringLiteral("ファイルを保存できませんでした: %1").arg(filePath);
        return result;
    }

    QTextStream out(&file);

    out << QStringLiteral("#YANEURAOU-DB2016 1.00\n");

    QMapIterator<QString, QVector<JosekiMove>> it(josekiData);
    while (it.hasNext()) {
        it.next();
        const QString &normalizedSfen = it.key();
        const QVector<JosekiMove> &moves = it.value();

        QString sfenToWrite;
        if (sfenWithPlyMap.contains(normalizedSfen)) {
            sfenToWrite = sfenWithPlyMap[normalizedSfen];
        } else {
            sfenToWrite = normalizedSfen;
        }

        const QStringList parts = sfenToWrite.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() == 3) {
            sfenToWrite += QStringLiteral(" 1");
        }

        out << QStringLiteral("sfen ") << sfenToWrite << QStringLiteral("\n");

        for (const JosekiMove &move : moves) {
            out << move.move << QStringLiteral(" ")
                << move.nextMove << QStringLiteral(" ")
                << move.value << QStringLiteral(" ")
                << move.depth << QStringLiteral(" ")
                << move.frequency
                << QStringLiteral("\n");

            if (!move.comment.isEmpty()) {
                out << QStringLiteral("#") << move.comment << QStringLiteral("\n");
            }
        }
    }

    out.flush();
    if (out.status() != QTextStream::Ok) {
        result.errorMessage = QStringLiteral("ファイル書き込み中にエラーが発生しました: %1").arg(filePath);
        file.close();
        return result;
    }

    file.close();
    result.success = true;
    result.savedCount = static_cast<int>(josekiData.size());
    return result;
}

void JosekiRepository::applyLoadResult(JosekiLoadResult &&result)
{
    m_josekiData = std::move(result.josekiData);
    m_sfenWithPlyMap = std::move(result.sfenWithPlyMap);
    m_mergeRegisteredMoves.clear();
}

bool JosekiRepository::loadFromFile(const QString &filePath, QString *errorMessage)
{
    JosekiLoadResult result = parseFromFile(filePath);
    if (!result.success) {
        if (errorMessage) {
            *errorMessage = result.errorMessage;
        }
        return false;
    }
    applyLoadResult(std::move(result));
    return true;
}

bool JosekiRepository::saveToFile(const QString &filePath, QString *errorMessage) const
{
    JosekiSaveResult result = serializeToFile(filePath, m_josekiData, m_sfenWithPlyMap);
    if (!result.success) {
        if (errorMessage) {
            *errorMessage = result.errorMessage;
        }
        return false;
    }
    return true;
}
