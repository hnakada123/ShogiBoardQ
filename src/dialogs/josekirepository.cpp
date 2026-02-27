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

bool JosekiRepository::loadFromFile(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ファイルを開けませんでした: %1").arg(filePath);
        }
        return false;
    }

    m_josekiData.clear();
    m_sfenWithPlyMap.clear();
    m_mergeRegisteredMoves.clear();

    QTextStream in(&file);
    QString line;
    QString currentSfen;
    QString normalizedSfen;

    // フォーマット検証用フラグ
    bool hasValidHeader = false;
    bool hasSfenLine = false;
    bool hasMoveLine = false;
    int lineNumber = 0;
    int invalidMoveLineCount = 0;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        lineNumber++;

        // Windows改行コード(\r)を除去
        line.remove(QLatin1Char('\r'));

        // 空行をスキップ
        if (line.isEmpty()) {
            continue;
        }

        // コメント行（#で始まる行）の処理
        if (line.startsWith(QLatin1Char('#'))) {
            // やねうら王定跡フォーマットのヘッダーを確認
            if (line.contains(QStringLiteral("YANEURAOU")) ||
                line.contains(QStringLiteral("yaneuraou"))) {
                hasValidHeader = true;
                continue;
            }

            // ヘッダー以外の#行は直前の指し手のコメントとして扱う
            if (!normalizedSfen.isEmpty() && m_josekiData.contains(normalizedSfen)) {
                QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
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

        // sfen行の処理
        if (line.startsWith(QStringLiteral("sfen "))) {
            currentSfen = line.mid(5).trimmed();
            currentSfen.remove(QLatin1Char('\r'));
            // SFEN文字列を正規化（手数部分を除去）
            const QStringList sfenParts = currentSfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (sfenParts.size() >= 3) {
                normalizedSfen = sfenParts[0] + QLatin1Char(' ') + sfenParts[1] + QLatin1Char(' ') + sfenParts[2];
            } else {
                normalizedSfen = currentSfen;
            }
            // 元のSFEN（手数付き）を保持
            if (!m_sfenWithPlyMap.contains(normalizedSfen)) {
                m_sfenWithPlyMap[normalizedSfen] = currentSfen;
            }
            hasSfenLine = true;
            continue;
        }

        // 指し手行の処理
        if (!normalizedSfen.isEmpty()) {
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                // 指し手の形式を簡易チェック
                const QString &moveStr = parts[0];
                bool validMove = false;

                // 駒打ち形式: X*YZ
                if (moveStr.size() >= 4 && moveStr.at(1) == QLatin1Char('*')) {
                    QChar piece = moveStr.at(0).toUpper();
                    if (QString("PLNSGBR").contains(piece)) {
                        validMove = true;
                    }
                }
                // 通常移動形式: XYZW または XYZW+
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

                // コメントがあれば取得
                if (parts.size() > 5) {
                    QStringList commentParts;
                    for (int i = 5; i < parts.size(); ++i) {
                        commentParts.append(parts[i]);
                    }
                    move.comment = commentParts.join(QLatin1Char(' '));
                }

                m_josekiData[normalizedSfen].append(move);
                hasMoveLine = true;
            } else if (parts.size() > 0) {
                invalidMoveLineCount++;
                qCWarning(lcUi) << "Invalid line format at line" << lineNumber
                                << ": expected 5+ fields, got" << parts.size();
            }
        }
    }

    file.close();

    // フォーマット検証
    if (!hasValidHeader) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "このファイルはやねうら王定跡フォーマット(YANEURAOU-DB2016)ではありません。\n"
                "ヘッダー行（#YANEURAOU-DB2016 等）が見つかりませんでした。\n\n"
                "ファイル: %1").arg(filePath);
        }
        m_josekiData.clear();
        return false;
    }

    if (!hasSfenLine) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "定跡ファイルにSFEN行が見つかりませんでした。\n\n"
                "やねうら王定跡フォーマットでは「sfen 」で始まる局面行が必要です。\n\n"
                "ファイル: %1").arg(filePath);
        }
        m_josekiData.clear();
        return false;
    }

    if (!hasMoveLine) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "定跡ファイルに有効な指し手行が見つかりませんでした。\n\n"
                "やねうら王定跡フォーマットでは指し手行に少なくとも5つのフィールド\n"
                "（指し手 予想応手 評価値 深さ 出現頻度）が必要です。\n\n"
                "ファイル: %1").arg(filePath);
        }
        m_josekiData.clear();
        return false;
    }

    qCInfo(lcUi) << "Loaded" << m_josekiData.size() << "positions from" << filePath;
    if (invalidMoveLineCount > 0) {
        qCWarning(lcUi) << invalidMoveLineCount << "lines had invalid format";
    }

    // デバッグ：平手初期局面があるか確認
    QString hirate = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");
    if (m_josekiData.contains(hirate)) {
        qCDebug(lcUi) << "Hirate position has" << m_josekiData[hirate].size() << "moves";
    }

    return true;
}

bool JosekiRepository::saveToFile(const QString &filePath, QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ファイルを保存できませんでした: %1").arg(filePath);
        }
        return false;
    }

    QTextStream out(&file);

    // ヘッダーを書き込み
    out << QStringLiteral("#YANEURAOU-DB2016 1.00\n");

    // 各局面と定跡手を書き込み
    QMapIterator<QString, QVector<JosekiMove>> it(m_josekiData);
    while (it.hasNext()) {
        it.next();
        const QString &normalizedSfen = it.key();
        const QVector<JosekiMove> &moves = it.value();

        // 元のSFEN（手数付き）を取得、なければ正規化SFENを使用
        QString sfenToWrite;
        if (m_sfenWithPlyMap.contains(normalizedSfen)) {
            sfenToWrite = m_sfenWithPlyMap[normalizedSfen];
        } else {
            sfenToWrite = normalizedSfen;
        }

        // 手数が含まれているか確認し、なければデフォルト値1を追加
        const QStringList parts = sfenToWrite.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() == 3) {
            sfenToWrite += QStringLiteral(" 1");
        }

        // SFEN行を書き込み
        out << QStringLiteral("sfen ") << sfenToWrite << QStringLiteral("\n");

        // 各指し手を書き込み
        for (const JosekiMove &move : moves) {
            out << move.move << QStringLiteral(" ")
                << move.nextMove << QStringLiteral(" ")
                << move.value << QStringLiteral(" ")
                << move.depth << QStringLiteral(" ")
                << move.frequency
                << QStringLiteral("\n");

            // コメントがあれば次の行に#プレフィックス付きで追加
            if (!move.comment.isEmpty()) {
                out << QStringLiteral("#") << move.comment << QStringLiteral("\n");
            }
        }
    }

    // 書き込みステータスを確認
    out.flush();
    if (out.status() != QTextStream::Ok) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ファイル書き込み中にエラーが発生しました: %1").arg(filePath);
        }
        file.close();
        return false;
    }

    file.close();
    return true;
}
