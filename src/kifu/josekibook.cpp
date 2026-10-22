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

#include <optional>
#include <utility>

namespace {

/// SFEN の手数（無い・読めなければ nullopt。startpos は1手目）
std::optional<int> plyOf(const QString &sfen)
{
    const QString field =
        SfenUtils::normalizePositionLikeSfen(sfen).section(QLatin1Char(' '), 3, 3, QString::SectionSkipEmpty);
    bool ok = false;
    const int ply = field.toInt(&ok);
    if (!ok || ply < 1) return std::nullopt;
    return ply;
}

/// 1つの局面の登録のうち、sfen と同じ手数のもの。無ければ最初のもの（registrations は空でない）
qsizetype resolveIndex(const QList<JosekiPosition> &registrations, const QString &sfen)
{
    if (registrations.size() > 1) {
        if (const std::optional<int> ply = plyOf(sfen)) {
            for (qsizetype i = 0; i < registrations.size(); ++i) {
                if (registrations.at(i).ply == *ply) return i;
            }
        }
    }
    return 0;
}

/// 定跡手の位置（無ければ -1）
qsizetype indexOfMove(const QList<JosekiMove> &moves, const QString &usiMove)
{
    for (qsizetype i = 0; i < moves.size(); ++i) {
        if (moves.at(i).move == usiMove) return i;
    }
    return -1;
}

/// ファイルの sfen 行に書く手数付きの SFEN
QString sfenLine(const QString &key, int ply)
{
    return key + QLatin1Char(' ') + QString::number(ply);
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

JosekiBook JosekiBook::fromPositions(QMap<QString, QList<JosekiPosition>> positions)
{
    for (auto it = positions.begin(); it != positions.end();) {
        it->removeIf([](const JosekiPosition &position) { return position.moves.isEmpty(); });
        if (it->isEmpty()) it = positions.erase(it);
        else ++it;
    }
    JosekiBook book;
    book.m_positions = std::move(positions);
    return book;
}

const JosekiPosition *JosekiBook::find(const QString &sfen) const
{
    const auto it = m_positions.constFind(positionKey(sfen));
    return it == m_positions.constEnd() ? nullptr : &it->at(resolveIndex(*it, sfen));
}

QString JosekiBook::registeredSfen(const QString &sfen) const
{
    const QString key = positionKey(sfen);
    const auto it = m_positions.constFind(key);
    return it == m_positions.constEnd() ? QString() : sfenLine(key, it->at(resolveIndex(*it, sfen)).ply);
}

QList<int> JosekiBook::registeredPlies(const QString &sfen) const
{
    QList<int> plies;
    const auto it = m_positions.constFind(positionKey(sfen));
    if (it == m_positions.constEnd()) return plies;
    for (const JosekiPosition &position : *it) plies.append(position.ply);
    return plies;
}

QList<JosekiMove> JosekiBook::movesAt(const QString &sfen) const
{
    const JosekiPosition *position = find(sfen);
    return position ? position->moves : QList<JosekiMove>();
}

bool JosekiBook::hasMove(const QString &sfen, const QString &usiMove) const
{
    const JosekiPosition *position = find(sfen);
    return position && indexOfMove(position->moves, usiMove) >= 0;
}

JosekiPosition *JosekiBook::findForWrite(const QString &key, const QString &sfen)
{
    const auto it = m_positions.find(key);
    return it == m_positions.end() ? nullptr : &(*it)[resolveIndex(*it, sfen)];
}

JosekiPosition &JosekiBook::findOrCreate(const QString &key, const QString &sfen)
{
    if (JosekiPosition *existing = findForWrite(key, sfen)) return *existing;
    QList<JosekiPosition> &registrations = m_positions[key];
    registrations.append(JosekiPosition{plyOf(sfen).value_or(1), {}});
    return registrations.last();
}

void JosekiBook::putMove(const QString &sfen, const JosekiMove &move)
{
    QList<JosekiMove> &moves = findOrCreate(positionKey(sfen), sfen).moves;
    const qsizetype index = indexOfMove(moves, move.move);
    if (index < 0) moves.append(move);
    else moves[index] = move;
}

bool JosekiBook::updateMove(const QString &sfen, const JosekiMove &move)
{
    JosekiPosition *position = findForWrite(positionKey(sfen), sfen);
    const qsizetype index = position ? indexOfMove(position->moves, move.move) : -1;
    if (index < 0) return false;
    JosekiMove &existing = position->moves[index];
    if (!move.nextMove.isNull()) existing.nextMove = move.nextMove;
    existing.value = move.value;
    existing.depth = move.depth;
    existing.frequency = move.frequency;
    existing.comment = move.comment;
    return true;
}

bool JosekiBook::removeMove(const QString &sfen, const QString &usiMove)
{
    const auto it = m_positions.find(positionKey(sfen));
    if (it == m_positions.end()) return false;
    const qsizetype index = resolveIndex(*it, sfen);
    QList<JosekiMove> &moves = (*it)[index].moves;
    const qsizetype removed = moves.removeIf([&usiMove](const JosekiMove &m) { return m.move == usiMove; });
    if (moves.isEmpty()) it->removeAt(index);
    if (it->isEmpty()) m_positions.erase(it);
    return removed > 0;
}

void JosekiBook::registerMergeMove(const QString &sfen, const QString &usiMove)
{
    const QString key = positionKey(sfen);
    m_mergeRegistered.insert({key, usiMove});
    QList<JosekiMove> &moves = findOrCreate(key, sfen).moves;
    const qsizetype index = indexOfMove(moves, usiMove);
    if (index >= 0) {
        moves[index].frequency += 1;
        return;
    }
    JosekiMove added;
    added.move = usiMove;
    added.nextMove = QStringLiteral("none");
    added.frequency = 1;
    moves.append(added);
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

    QMap<QString, QList<JosekiPosition>> positions;
    // 読んでいる登録（sfen 行の局面と手数）。局面のキーは sfen 行ごとに1回だけ作る
    auto current = positions.end();
    qsizetype currentIndex = -1;
    // コメント行を直前の定跡手に付けるか（重なって読み捨てた指し手の後のコメントは付けない）
    bool attachComment = false;
    QTextStream in(&file);
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
            if (attachComment) {
                QString &comment = (*current)[currentIndex].moves.last().comment;
                const QString text = line.mid(1);
                comment = comment.isEmpty() ? text : comment + QLatin1Char(' ') + text;
            }
            continue;
        }

        hasContentLine = true;
        if (line.startsWith(QStringLiteral("sfen "))) {
            const QString sfen = line.mid(5).trimmed();
            // 以前のこのアプリが書いた持ち駒の並び（先後交互）も、標準の並びの局面と同じキーにする
            const QString key = JosekiBook::positionKey(sfen);
            current = positions.find(key);
            if (current == positions.end()) current = positions.insert(key, {});
            // 同じ局面でも手数が違えば別の登録（同じ手数の sfen 行が重なっていれば続きとして読む）
            const int ply = plyOf(sfen).value_or(1);
            currentIndex = current->size();
            for (qsizetype i = 0; i < current->size(); ++i) {
                if (current->at(i).ply == ply) currentIndex = i;
            }
            if (currentIndex == current->size()) current->append(JosekiPosition{ply, {}});
            attachComment = false;
            hasSfenLine = true;
            continue;
        }
        if (current == positions.end()) continue;

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
        hasMoveLine = true;
        QList<JosekiMove> &moves = (*current)[currentIndex].moves;
        // 1つの登録に同じ指し手が重なっていれば、最初の行を使う
        attachComment = indexOfMove(moves, parts.at(0)) < 0;
        if (!attachComment) continue;
        JosekiMove move;
        move.move = parts.at(0);
        move.nextMove = parts.at(1);
        move.value = parts.at(2).toInt();
        move.depth = parts.at(3).toInt();
        move.frequency = parts.at(4).toInt();
        if (parts.size() > 5) move.comment = parts.mid(5).join(QLatin1Char(' '));
        moves.append(move);
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

    result.book = JosekiBook::fromPositions(std::move(positions));
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
    // 手数違いの登録も、それぞれの手数の sfen 行で書く（やねうら王は既定では手数まで一致する登録だけを使う）
    int savedCount = 0;
    const QMap<QString, QList<JosekiPosition>> &positions = book.positions();
    for (auto it = positions.cbegin(); it != positions.cend(); ++it) {
        for (const JosekiPosition &position : *it) {
            out << QStringLiteral("sfen ") << sfenLine(it.key(), position.ply) << QLatin1Char('\n');
            for (const JosekiMove &move : position.moves) {
                out << move.move << QLatin1Char(' ') << move.nextMove << QLatin1Char(' ') << move.value << QLatin1Char(' ')
                    << move.depth << QLatin1Char(' ') << move.frequency << QLatin1Char('\n');
                if (!move.comment.isEmpty()) out << QLatin1Char('#') << move.comment << QLatin1Char('\n');
            }
            ++savedCount;
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
    result.savedCount = savedCount;
    return result;
}

} // namespace JosekiBookFile
