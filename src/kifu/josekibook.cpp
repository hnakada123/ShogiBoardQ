/// @file josekibook.cpp
/// @brief 定跡のデータの値型と、やねうら王定跡形式の読み書きの実装

#include "josekibook.h"

#include "logcategories.h"
#include "sfenutils.h"
#include "usimove.h"

#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>

namespace {

/// 手数のない SFEN（3つの欄）は1手目にする
QString withPly(const QString &sfen)
{
    const QString trimmed = sfen.trimmed();
    return trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts).size() == 3 ? trimmed + QStringLiteral(" 1") : trimmed;
}

QString translated(const char *source)
{
    return QCoreApplication::translate("JosekiBook", source);
}

} // namespace

// ============================================================
// データ
// ============================================================

QString JosekiBook::positionKey(const QString &sfen)
{
    return SfenUtils::normalizeSfenKey(sfen);
}

const JosekiPosition *JosekiBook::find(const QString &sfen) const
{
    const auto it = m_positions.constFind(positionKey(sfen));
    return it == m_positions.constEnd() ? nullptr : &it.value();
}

QList<JosekiMove> JosekiBook::movesAt(const QString &sfen) const
{
    const JosekiPosition *position = find(sfen);
    return position ? position->moves : QList<JosekiMove>();
}

bool JosekiBook::hasMove(const QString &sfen, const QString &usiMove) const
{
    const JosekiPosition *position = find(sfen);
    return position && std::any_of(position->moves.cbegin(), position->moves.cend(),
                                   [&usiMove](const JosekiMove &m) { return m.move == usiMove; });
}

void JosekiBook::putMove(const QString &sfen, const JosekiMove &move)
{
    JosekiPosition &position = m_positions[positionKey(sfen)];
    if (position.sfen.isEmpty()) position.sfen = withPly(sfen);
    position.moves.removeIf([&move](const JosekiMove &m) { return m.move == move.move; });
    position.moves.append(move);
}

bool JosekiBook::updateMove(const QString &sfen, const JosekiMove &move)
{
    const auto it = m_positions.find(positionKey(sfen));
    if (it == m_positions.end()) return false;
    for (JosekiMove &existing : it->moves) {
        if (existing.move != move.move) continue;
        if (!move.nextMove.isNull()) existing.nextMove = move.nextMove;
        existing.value = move.value;
        existing.depth = move.depth;
        existing.frequency = move.frequency;
        existing.comment = move.comment;
        return true;
    }
    return false;
}

bool JosekiBook::removeMove(const QString &sfen, const QString &usiMove)
{
    const QString key = positionKey(sfen);
    const auto it = m_positions.find(key);
    if (it == m_positions.end()) return false;
    const qsizetype removed = it->moves.removeIf([&usiMove](const JosekiMove &m) { return m.move == usiMove; });
    if (it->moves.isEmpty()) m_positions.remove(key);
    return removed > 0;
}

void JosekiBook::registerMergeMove(const QString &sfen, const QString &usiMove)
{
    const QString key = positionKey(sfen);
    m_mergeRegistered.insert({key, usiMove});
    JosekiPosition &position = m_positions[key];
    if (position.sfen.isEmpty()) position.sfen = withPly(sfen);
    for (JosekiMove &existing : position.moves) {
        if (existing.move != usiMove) continue;
        existing.frequency += 1;
        return;
    }
    JosekiMove added;
    added.move = usiMove;
    added.nextMove = QStringLiteral("none");
    added.frequency = 1;
    position.moves.append(added);
}

bool JosekiBook::isMergeRegistered(const QString &sfen, const QString &usiMove) const
{
    return m_mergeRegistered.contains({positionKey(sfen), usiMove});
}

void JosekiBook::clear()
{
    m_positions.clear();
    m_mergeRegistered.clear();
}

// ============================================================
// ファイル
// ============================================================

namespace JosekiBookFile {

JosekiLoadResult load(const QString &filePath)
{
    JosekiLoadResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = translated(QT_TRANSLATE_NOOP("JosekiBook", "ファイルを開けませんでした: %1")).arg(filePath);
        return result;
    }

    QMap<QString, JosekiPosition> positions;
    QTextStream in(&file);
    QString currentKey;
    QString currentSfen;
    bool hasValidHeader = false;
    bool hasSfenLine = false;
    bool hasMoveLine = false;
    bool hasContentLine = false;
    int lineNumber = 0;
    int invalidLineCount = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        ++lineNumber;
        line.remove(QLatin1Char('\r'));
        if (line.isEmpty()) continue;

        if (line.startsWith(QLatin1Char('#'))) {
            if (line.contains(QStringLiteral("YANEURAOU"), Qt::CaseInsensitive)) {
                hasValidHeader = true;
                continue;
            }
            // コメント行は直前の定跡手のコメント
            const auto it = positions.find(currentKey);
            if (!currentKey.isEmpty() && it != positions.end() && !it->moves.isEmpty()) {
                QString &comment = it->moves.last().comment;
                const QString text = line.mid(1);
                comment = comment.isEmpty() ? text : comment + QLatin1Char(' ') + text;
            }
            continue;
        }

        hasContentLine = true;
        if (line.startsWith(QStringLiteral("sfen "))) {
            currentSfen = line.mid(5).trimmed();
            // 以前のこのアプリが書いた持ち駒の並び（先後交互）も、標準の並びの局面と同じキーにする
            currentKey = JosekiBook::positionKey(currentSfen);
            hasSfenLine = true;
            continue;
        }
        if (currentKey.isEmpty()) continue;

        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 5) {
            ++invalidLineCount;
            qCWarning(lcUi) << "Invalid line format at line" << lineNumber << ": expected 5+ fields, got" << parts.size();
            continue;
        }
        if (!UsiMove::parse(parts.at(0))) {
            ++invalidLineCount;
            qCWarning(lcUi) << "Invalid move format at line" << lineNumber << ":" << parts.at(0);
        }
        JosekiMove move;
        move.move = parts.at(0);
        move.nextMove = parts.at(1);
        move.value = parts.at(2).toInt();
        move.depth = parts.at(3).toInt();
        move.frequency = parts.at(4).toInt();
        if (parts.size() > 5) move.comment = parts.mid(5).join(QLatin1Char(' '));
        JosekiPosition &position = positions[currentKey];
        if (position.sfen.isEmpty()) position.sfen = currentSfen;
        position.moves.append(move);
        hasMoveLine = true;
    }

    if (!hasValidHeader) {
        result.errorMessage = translated(QT_TRANSLATE_NOOP("JosekiBook",
            "このファイルはやねうら王定跡フォーマット(YANEURAOU-DB2016)ではありません。\n"
            "ヘッダー行（#YANEURAOU-DB2016 等）が見つかりませんでした。\n\n"
            "ファイル: %1")).arg(filePath);
        return result;
    }
    if (!hasSfenLine && hasContentLine) {
        result.errorMessage = translated(QT_TRANSLATE_NOOP("JosekiBook",
            "定跡ファイルにSFEN行が見つかりませんでした。\n\n"
            "やねうら王定跡フォーマットでは「sfen 」で始まる局面行が必要です。\n\n"
            "ファイル: %1")).arg(filePath);
        return result;
    }
    if (hasSfenLine && !hasMoveLine) {
        result.errorMessage = translated(QT_TRANSLATE_NOOP("JosekiBook",
            "定跡ファイルに有効な指し手行が見つかりませんでした。\n\n"
            "やねうら王定跡フォーマットでは指し手行に少なくとも5つのフィールド\n"
            "（指し手 予想応手 評価値 深さ 出現頻度）が必要です。\n\n"
            "ファイル: %1")).arg(filePath);
        return result;
    }

    for (auto it = positions.cbegin(); it != positions.cend(); ++it)
        for (const JosekiMove &move : it->moves) result.book.putMove(it->sfen, move);
    result.success = true;
    qCInfo(lcUi) << "Parsed" << result.book.positionCount() << "positions from" << filePath;
    if (invalidLineCount > 0) qCWarning(lcUi) << invalidLineCount << "lines had invalid format";
    return result;
}

JosekiSaveResult save(const QString &filePath, const JosekiBook &book)
{
    JosekiSaveResult result;
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.errorMessage = translated(QT_TRANSLATE_NOOP("JosekiBook", "ファイルを保存できませんでした: %1")).arg(filePath);
        return result;
    }

    QTextStream out(&file);
    out << QStringLiteral("#YANEURAOU-DB2016 1.00\n");
    const QMap<QString, JosekiPosition> &positions = book.positions();
    for (auto it = positions.cbegin(); it != positions.cend(); ++it) {
        out << QStringLiteral("sfen ") << withPly(it->sfen.isEmpty() ? it.key() : it->sfen) << QLatin1Char('\n');
        for (const JosekiMove &move : it->moves) {
            out << move.move << QLatin1Char(' ') << move.nextMove << QLatin1Char(' ') << move.value << QLatin1Char(' ')
                << move.depth << QLatin1Char(' ') << move.frequency << QLatin1Char('\n');
            if (!move.comment.isEmpty()) out << QLatin1Char('#') << move.comment << QLatin1Char('\n');
        }
    }

    out.flush();
    if (out.status() != QTextStream::Ok) {
        result.errorMessage =
            translated(QT_TRANSLATE_NOOP("JosekiBook", "ファイル書き込み中にエラーが発生しました: %1")).arg(filePath);
        file.cancelWriting();
        return result;
    }
    if (!file.commit()) {
        result.errorMessage = translated(QT_TRANSLATE_NOOP("JosekiBook", "ファイルを保存できませんでした: %1")).arg(filePath);
        return result;
    }
    result.success = true;
    result.savedCount = book.positionCount();
    return result;
}

} // namespace JosekiBookFile
