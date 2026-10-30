/// @file josekibook.cpp
/// @brief 定跡のデータの値型と、やねうら王定跡形式の読み書きの実装

#include "josekibook.h"

#include "kifuexportcommon.h"
#include "logcategories.h"
#include "sfenutils.h"
#include "usimove.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <utility>

namespace {

/// 指し手の番号: kNoMove は "none"、それより大きく kTokenBase より小さい番号は USI の指し手、
/// kTokenBase から JosekiBook::kHeader の手前までは USI として読めない文字列の表の番号
constexpr quint16 kNoMove = 0;
constexpr quint16 kTokenBase = 0x8000;
/// 駒打ちの移動元の番号（盤の81マスの後に、打つ駒の種類 歩〜飛を並べる）
constexpr int kDropFromBase = 81;
/// 駒打ちの駒の文字（PieceKind の歩〜飛の順）
constexpr char kDropLetters[] = "PLNSGBR";

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

int squareIndex(BoardSquare square)
{
    return (square.file - 1) * 9 + (square.rank - 1);
}

/// USI の書き方どおりの指し手の番号（違えば nullopt。駒打ちの駒は大文字だけを USI の書き方とする）
std::optional<quint16> usiMoveCode(QStringView text)
{
    const std::optional<UsiMove> move = UsiMove::parse(text);
    if (!move || !move->isValid() || text.trimmed().size() != text.size()) return std::nullopt;
    if (move->isDrop() && !text.at(0).isUpper()) return std::nullopt;
    const int from = move->isDrop() ? kDropFromBase + static_cast<int>(pieceKind(move->drop)) : squareIndex(move->from);
    return static_cast<quint16>(1 + (from * 81 + squareIndex(move->to)) * 2 + (move->promote ? 1 : 0));
}

/// USI の指し手の番号を文字にして out の後ろに足す
void appendUsiMove(QByteArray &out, quint16 code)
{
    const int value = code - 1;
    const int from = value / 2 / 81;
    const int to = value / 2 % 81;
    if (from >= kDropFromBase) {
        out += kDropLetters[from - kDropFromBase];
        out += '*';
    } else {
        out += static_cast<char>('1' + from / 9);
        out += static_cast<char>('a' + from % 9);
    }
    out += static_cast<char>('1' + to / 9);
    out += static_cast<char>('a' + to % 9);
    if (value % 2 != 0) out += '+';
}

QString translated(const char *source)
{
    return QCoreApplication::translate("JosekiBook", source);
}

} // namespace

// ============================================================
// 詰めた形との変換
// ============================================================

QString JosekiBook::positionKey(const QString &sfen)
{
    return SfenUtils::normalizeSfenKey(sfen);
}

QByteArray JosekiBook::keyOf(const QString &sfen)
{
    // toUtf8 は長さの3倍の領域を取るので、表に入れるキーは詰める
    QByteArray key = positionKey(sfen).toUtf8();
    key.squeeze();
    return key;
}

QByteArray JosekiBook::keyOf(const Position &position)
{
    QByteArray key = position.positionKey().toUtf8();
    key.squeeze();
    return key;
}

QList<JosekiBook::Range> JosekiBook::rangesOf(const Records &records)
{
    QList<Range> ranges;
    for (qsizetype i = 0; i < records.size(); ++i) {
        if (records.at(i).move != kHeader) continue;
        if (!ranges.isEmpty()) ranges.last().end = i;
        ranges.append(Range{i, records.size()});
    }
    return ranges;
}

JosekiBook::Range JosekiBook::resolve(const Records &records, const QString &sfen)
{
    return resolve(records, plyOf(sfen));
}

JosekiBook::Range JosekiBook::resolve(const Records &records, std::optional<int> ply)
{
    const QList<Range> ranges = rangesOf(records);
    if (ranges.size() > 1 && ply) {
        for (const Range &range : ranges) {
            if (records.at(range.header).value == *ply) return range;
        }
    }
    return ranges.first();
}

qsizetype JosekiBook::indexOf(const Records &records, Range range, quint16 move)
{
    for (qsizetype i = range.header + 1; i < range.end; ++i) {
        if (records.at(i).move == move) return i;
    }
    return -1;
}

quint16 JosekiBook::encodeMove(QStringView text)
{
    if (const std::optional<quint16> code = findMoveCode(text)) return *code;
    // USI として読めない文字列は、ファイルの内容を失わないように表に入れて番号にする
    if (kTokenBase + m_tokens.size() >= kHeader) {
        qCWarning(lcUi) << "Too many unreadable book moves; stored as none:" << text;
        return kNoMove;
    }
    const auto code = static_cast<quint16>(kTokenBase + m_tokens.size());
    m_tokens.append(text.toString());
    m_tokenCodes.insert(m_tokens.last(), code);
    return code;
}

std::optional<quint16> JosekiBook::findMoveCode(QStringView text) const
{
    if (text.isEmpty() || text == QLatin1String("none")) return kNoMove;
    if (const std::optional<quint16> code = usiMoveCode(text)) return code;
    const auto it = m_tokenCodes.constFind(text.toString());
    if (it == m_tokenCodes.constEnd()) return std::nullopt;
    return *it;
}

void JosekiBook::appendMoveText(QByteArray &out, quint16 code) const
{
    if (code == kNoMove) out += "none";
    else if (code >= kTokenBase) out += m_tokens.value(code - kTokenBase).toUtf8();
    else appendUsiMove(out, code);
}

QString JosekiBook::decodeMove(quint16 code) const
{
    if (code >= kTokenBase) return m_tokens.value(code - kTokenBase);
    QByteArray text;
    appendMoveText(text, code);
    return QString::fromLatin1(text);
}

quint32 JosekiBook::storeComment(const QString &comment, quint32 index)
{
    if (comment.isEmpty()) return 0;
    if (index != 0) {
        m_comments[index] = comment;
        return index;
    }
    m_comments.append(comment);
    return static_cast<quint32>(m_comments.size() - 1);
}

void JosekiBook::assign(PackedMove &target, const JosekiMove &move)
{
    target.next = move.nextMove ? encodeMove(*move.nextMove) : kNoMove;
    target.value = move.value;
    target.depth = move.depth;
    target.frequency = move.frequency;
    target.comment = storeComment(move.comment, target.comment);
}

JosekiMove JosekiBook::unpack(const PackedMove &packed) const
{
    JosekiMove move;
    move.move = decodeMove(packed.move);
    if (packed.next != kNoMove) move.nextMove = decodeMove(packed.next);
    move.value = packed.value;
    move.depth = packed.depth;
    move.frequency = packed.frequency;
    move.comment = m_comments.value(packed.comment);
    return move;
}

JosekiBook::Range JosekiBook::findOrCreate(Records &records, const QString &sfen)
{
    if (!records.isEmpty()) return resolve(records, sfen);
    PackedMove header;
    header.move = kHeader;
    header.value = plyOf(sfen).value_or(1);
    records.append(header);
    return Range{0, 1};
}

// ============================================================
// データ
// ============================================================

bool JosekiBook::contains(const QString &sfen) const
{
    return m_positions.contains(keyOf(sfen));
}

QString JosekiBook::registeredSfen(const QString &sfen) const
{
    const QByteArray key = keyOf(sfen);
    const auto it = m_positions.constFind(key);
    if (it == m_positions.constEnd()) return QString();
    return QString::fromUtf8(key) + QLatin1Char(' ') + QString::number(it->at(resolve(*it, sfen).header).value);
}

QList<int> JosekiBook::registeredPlies(const QString &sfen) const
{
    QList<int> plies;
    const auto it = m_positions.constFind(keyOf(sfen));
    if (it == m_positions.constEnd()) return plies;
    const QList<Range> ranges = rangesOf(*it);
    for (const Range &range : ranges) plies.append(it->at(range.header).value);
    return plies;
}

QList<JosekiMove> JosekiBook::movesAt(const QString &sfen) const
{
    const auto it = m_positions.constFind(keyOf(sfen));
    if (it == m_positions.constEnd()) return {};
    return movesIn(*it, resolve(*it, sfen));
}

QList<JosekiMove> JosekiBook::movesAt(const Position &position) const
{
    const auto it = m_positions.constFind(keyOf(position));
    if (it == m_positions.constEnd()) return {};
    return movesIn(*it, resolve(*it, position.moveNumber()));
}

QList<JosekiMove> JosekiBook::movesIn(const Records &records, Range range) const
{
    QList<JosekiMove> moves;
    for (qsizetype i = range.header + 1; i < range.end; ++i) moves.append(unpack(records.at(i)));
    return moves;
}

QString JosekiBook::leadingMove(const QString &sfen) const
{
    const auto it = m_positions.constFind(keyOf(sfen));
    if (it == m_positions.constEnd()) return QString();
    // やねうら王は局面の定跡手を出現頻度、次に評価値の高い順に並べ替える（同じなら元の順）。深さは見ない
    const Range range = resolve(*it, sfen);
    const PackedMove *best = nullptr;
    for (qsizetype i = range.header + 1; i < range.end; ++i) {
        const PackedMove &move = it->at(i);
        if (!best || move.frequency > best->frequency
            || (move.frequency == best->frequency && move.value > best->value))
            best = &move;
    }
    return best ? decodeMove(best->move) : QString();
}

bool JosekiBook::hasMove(const QString &sfen, const QString &usiMove) const
{
    const auto it = m_positions.constFind(keyOf(sfen));
    const std::optional<quint16> code = findMoveCode(usiMove);
    return it != m_positions.constEnd() && code && indexOf(*it, resolve(*it, sfen), *code) >= 0;
}

void JosekiBook::putMove(const QString &sfen, const JosekiMove &move)
{
    Records &records = m_positions[keyOf(sfen)];
    const Range range = findOrCreate(records, sfen);
    const quint16 code = encodeMove(move.move);
    const qsizetype index = indexOf(records, range, code);
    if (index >= 0) {
        assign(records[index], move);
        return;
    }
    PackedMove added;
    added.move = code;
    assign(added, move);
    records.insert(range.end, added);
}

bool JosekiBook::updateMove(const QString &sfen, const JosekiMove &move, NextMoveUpdate next)
{
    const auto it = m_positions.find(keyOf(sfen));
    const std::optional<quint16> code = findMoveCode(move.move);
    if (it == m_positions.end() || !code) return false;
    const qsizetype index = indexOf(*it, resolve(*it, sfen), *code);
    if (index < 0) return false;
    PackedMove &existing = (*it)[index];
    if (next == NextMoveUpdate::Replace) existing.next = move.nextMove ? encodeMove(*move.nextMove) : kNoMove;
    existing.value = move.value;
    existing.depth = move.depth;
    existing.frequency = move.frequency;
    existing.comment = storeComment(move.comment, existing.comment);
    return true;
}

bool JosekiBook::moveTo(const QString &sfen, const QString &usiMove, int index)
{
    const auto it = m_positions.find(keyOf(sfen));
    const std::optional<quint16> code = findMoveCode(usiMove);
    if (it == m_positions.end() || !code) return false;
    Records &records = *it;
    const Range range = resolve(records, sfen);
    const qsizetype from = indexOf(records, range, *code);
    if (from < 0) return false;
    const qsizetype to = range.header + 1 + qBound(qsizetype(0), qsizetype(index), range.end - range.header - 2);
    if (from == to) return false;
    records.move(from, to);
    return true;
}

bool JosekiBook::sortMoves(const QString &sfen, SortKey key)
{
    const auto it = m_positions.find(keyOf(sfen));
    if (it == m_positions.end()) return false;
    Records &records = *it;
    const Range range = resolve(records, sfen);
    const auto first = records.begin() + range.header + 1;
    const auto last = records.begin() + range.end;
    const auto higher = [key](const PackedMove &a, const PackedMove &b) {
        const auto rank = [key](const PackedMove &m) {
            return key == SortKey::Frequency ? std::pair<qint32, qint32>{m.frequency, m.value}
                                             : std::pair<qint32, qint32>{m.value, m.frequency};
        };
        return rank(a) > rank(b);
    };
    if (std::is_sorted(first, last, higher)) return false;
    std::stable_sort(first, last, higher);
    return true;
}

bool JosekiBook::removeMove(const QString &sfen, const QString &usiMove)
{
    const auto it = m_positions.find(keyOf(sfen));
    const std::optional<quint16> code = findMoveCode(usiMove);
    if (it == m_positions.end() || !code) return false;
    const Range range = resolve(*it, sfen);
    const qsizetype index = indexOf(*it, range, *code);
    if (index < 0) return false;
    it->removeAt(index);
    // 登録の最後の定跡手なら見出しも消し、登録が無くなれば局面も消す
    if (range.end - range.header == 2) it->removeAt(range.header);
    if (it->isEmpty()) m_positions.erase(it);
    return true;
}

bool JosekiBook::addPlayedMove(const QString &sfen, const QString &usiMove, int count, const QString &comment)
{
    Records &records = m_positions[keyOf(sfen)];
    const Range range = findOrCreate(records, sfen);
    const quint16 code = encodeMove(usiMove);
    const qsizetype index = indexOf(records, range, code);
    if (index >= 0) {
        PackedMove &existing = records[index];
        existing.frequency += count;
        if (existing.comment == 0) existing.comment = storeComment(comment);
        return false;
    }
    PackedMove added;
    added.move = code;
    added.next = kNoMove;
    added.frequency = count;
    added.comment = storeComment(comment);
    records.insert(range.end, added);
    return true;
}

JosekiBook::MergeStats JosekiBook::mergeFrom(const JosekiBook &other, const MergeOptions &options)
{
    MergeStats stats;
    // USI の指し手の番号はどの定跡でも同じ。読めない文字列の番号だけを、この定跡の表の番号に直す
    const auto remap = [this, &other](quint16 code) {
        return code >= kTokenBase && code != kHeader ? encodeMove(other.decodeMove(code)) : code;
    };
    for (auto it = other.m_positions.constBegin(); it != other.m_positions.constEnd(); ++it) {
        const Records &incoming = it.value();
        const auto existing = m_positions.find(it.key());
        if (existing == m_positions.end()) {
            Records copied;
            copied.reserve(incoming.size());
            for (const PackedMove &packed : incoming) {
                PackedMove move = packed;
                if (packed.move != kHeader) {
                    move.move = remap(packed.move);
                    move.next = remap(packed.next);
                    move.comment = storeComment(other.m_comments.value(packed.comment));
                    ++stats.addedMoves;
                }
                copied.append(move);
            }
            m_positions.insert(it.key(), copied);
            ++stats.addedPositions;
            continue;
        }
        Records &records = *existing;
        const QString key = QString::fromUtf8(it.key());
        const QList<Range> ranges = rangesOf(incoming);
        for (const Range &from : ranges) {
            // 同じ手数の登録（無ければ最初の登録）に足す
            const QString sfen = key + QLatin1Char(' ') + QString::number(incoming.at(from.header).value);
            for (qsizetype i = from.header + 1; i < from.end; ++i) {
                const PackedMove &packed = incoming.at(i);
                const Range range = resolve(records, sfen);
                const quint16 code = remap(packed.move);
                const qsizetype index = indexOf(records, range, code);
                const QString comment = other.m_comments.value(packed.comment);
                if (index < 0) {
                    PackedMove added = packed;
                    added.move = code;
                    added.next = remap(packed.next);
                    added.comment = storeComment(comment);
                    records.insert(range.end, added);
                    ++stats.addedMoves;
                    continue;
                }
                PackedMove &target = records[index];
                const int frequency = options.sumFrequency ? target.frequency + packed.frequency
                                      : options.preferIncoming ? packed.frequency
                                                               : target.frequency;
                if (options.preferIncoming) {
                    target.next = remap(packed.next);
                    target.value = packed.value;
                    target.depth = packed.depth;
                    if (!comment.isEmpty()) target.comment = storeComment(comment, target.comment);
                }
                target.frequency = frequency;
                ++stats.updatedMoves;
            }
        }
    }
    return stats;
}

void JosekiBook::clear()
{
    m_positions.clear();
    m_tokens.clear();
    m_tokenCodes.clear();
    m_comments = QStringList{QString()};
}

// ============================================================
// ファイル
// ============================================================

namespace JosekiBookFile {

namespace {
/// 定跡ファイルの拡張子（やねうら王の定跡ファイルと同じ）
constexpr char kBookSuffix[] = ".db";
} // namespace

bool hasBookSuffix(const QString &filePath)
{
    return filePath.endsWith(QLatin1String(kBookSuffix), Qt::CaseInsensitive);
}

QString withBookSuffix(const QString &filePath)
{
    if (filePath.isEmpty() || hasBookSuffix(filePath)) return filePath;
    return filePath + QLatin1String(kBookSuffix);
}

QString fileFilter()
{
    return QCoreApplication::translate("JosekiBookFile", "定跡ファイル (*.db);;すべてのファイル (*)");
}

JosekiLoadResult load(const QString &filePath)
{
    JosekiLoadResult result;
    QFile file(filePath);
    // 通常のファイルだけを読む（FIFO・デバイスは読み終わらない。セキュリティ監査 S14）。大きな定跡も読めるよう、大きさは制限しない
    if (!QFileInfo(filePath).isFile() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = translated(QT_TRANSLATE_NOOP("JosekiBook", "ファイルを開けませんでした: %1")).arg(filePath);
        return result;
    }

    JosekiBook &book = result.book;
    // 読んでいる sfen 行の局面と手数。定跡手は pending にためて、次の sfen 行かファイルの終わりで表に入れる
    QByteArray currentKey;
    int currentPly = 1;
    bool hasCurrent = false;
    JosekiBook::Records pending;
    QSet<quint16> pendingMoves;   // pending の指し手（同じ指し手の2行目以降は読み捨てる）
    // コメント行を直前の定跡手に付けるか（重なって読み捨てた指し手の後のコメントは付けない）
    bool attachComment = false;

    // 同じ局面の sfen 行が前にもあった局面の登録（手数ごと。ファイルの順）。読み終えてから表に入れる
    // （行ごとに表の中の登録を探し直すと、同じ局面の行の数の二乗の時間になる。セキュリティ監査 S6）
    struct Registration {
        int ply = 1;
        JosekiBook::Records moves;
        QSet<quint16> codes;
    };
    struct RepeatedPosition {
        QList<Registration> registrations;
        QHash<int, qsizetype> byPly;   // 手数 → registrations の位置
    };
    QHash<QByteArray, RepeatedPosition> repeated;

    const auto flush = [&book, &currentKey, &currentPly, &pending, &pendingMoves, &repeated]() {
        if (pending.isEmpty()) return;
        JosekiBook::PackedMove header;
        header.move = JosekiBook::kHeader;
        header.value = currentPly;
        const auto it = book.m_positions.find(currentKey);
        if (it == book.m_positions.end()) {
            // ほとんどの局面はここ。ちょうどの大きさの配列で持つ
            JosekiBook::Records records;
            records.reserve(pending.size() + 1);
            records.append(header);
            records.append(pending);
            book.m_positions.insert(currentKey, records);
        } else {
            // 同じ局面の sfen 行が前にもあった。手数違いなら別の登録、同じ手数ならその続き（同じ指し手は前の行を使う）
            RepeatedPosition &position = repeated[currentKey];
            if (position.registrations.isEmpty()) {
                for (const JosekiBook::Range &range : JosekiBook::rangesOf(*it)) {
                    Registration registration;
                    registration.ply = it->at(range.header).value;
                    for (qsizetype i = range.header + 1; i < range.end; ++i) {
                        registration.moves.append(it->at(i));
                        registration.codes.insert(it->at(i).move);
                    }
                    position.byPly.insert(registration.ply, position.registrations.size());
                    position.registrations.append(registration);
                }
            }
            const auto same = position.byPly.constFind(currentPly);
            if (same == position.byPly.constEnd()) {
                position.byPly.insert(currentPly, position.registrations.size());
                position.registrations.append(Registration{currentPly, pending, pendingMoves});
            } else {
                Registration &registration = position.registrations[*same];
                for (const JosekiBook::PackedMove &move : std::as_const(pending)) {
                    if (registration.codes.contains(move.move)) continue;
                    registration.moves.append(move);
                    registration.codes.insert(move.move);
                }
            }
        }
        pending.clear();
        pendingMoves.clear();
    };
    // 同じ局面の行が何度もあった局面の登録を表に入れる
    const auto mergeRepeated = [&book, &repeated]() {
        for (auto it = repeated.cbegin(); it != repeated.cend(); ++it) {
            JosekiBook::Records records;
            for (const Registration &registration : it->registrations) {
                JosekiBook::PackedMove header;
                header.move = JosekiBook::kHeader;
                header.value = registration.ply;
                records.append(header);
                records.append(registration.moves);
            }
            book.m_positions.insert(it.key(), records);
        }
    };

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
                JosekiBook::PackedMove &last = pending.last();
                const QString text = line.mid(1);
                if (last.comment == 0) last.comment = book.storeComment(text);
                else book.m_comments[last.comment] += QLatin1Char(' ') + text;
            }
            continue;
        }

        hasContentLine = true;
        if (line.startsWith(QStringLiteral("sfen "))) {
            flush();
            const QString sfen = line.mid(5).trimmed();
            // 以前のこのアプリが書いた持ち駒の並び（先後交互）も、標準の並びの局面と同じキーにする
            currentKey = JosekiBook::keyOf(sfen);
            currentPly = plyOf(sfen).value_or(1);
            hasCurrent = true;
            attachComment = false;
            hasSfenLine = true;
            continue;
        }
        if (!hasCurrent) continue;

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
        const quint16 code = book.encodeMove(parts.at(0));
        // 1つの登録に同じ指し手が重なっていれば、最初の行を使う
        attachComment = !pendingMoves.contains(code);
        if (!attachComment) continue;
        pendingMoves.insert(code);
        JosekiBook::PackedMove move;
        move.move = code;
        move.next = book.encodeMove(parts.at(1));
        move.value = parts.at(2).toInt();
        move.depth = parts.at(3).toInt();
        move.frequency = parts.at(4).toInt();
        if (parts.size() > 5) move.comment = book.storeComment(parts.mid(5).join(QLatin1Char(' ')));
        pending.append(move);
    }
    flush();
    mergeRepeated();

    if (!hasValidHeader) {
        result.errorMessage = translated(QT_TRANSLATE_NOOP("JosekiBook",
            "このファイルはやねうら王定跡フォーマット(YANEURAOU-DB2016)ではありません。\n"
            "ヘッダー行（#YANEURAOU-DB2016 等）が見つかりませんでした。\n\n"
            "ファイル: %1")).arg(filePath);
        result.book.clear();
        return result;
    }
    if (!hasSfenLine && hasContentLine) {
        result.errorMessage = translated(QT_TRANSLATE_NOOP("JosekiBook",
            "定跡ファイルにSFEN行が見つかりませんでした。\n\n"
            "やねうら王定跡フォーマットでは「sfen 」で始まる局面行が必要です。\n\n"
            "ファイル: %1")).arg(filePath);
        result.book.clear();
        return result;
    }
    if (hasSfenLine && !hasMoveLine) {
        result.errorMessage = translated(QT_TRANSLATE_NOOP("JosekiBook",
            "定跡ファイルに有効な指し手行が見つかりませんでした。\n\n"
            "やねうら王定跡フォーマットでは指し手行に少なくとも5つのフィールド\n"
            "（指し手 予想応手 評価値 深さ 出現頻度）が必要です。\n\n"
            "ファイル: %1")).arg(filePath);
        result.book.clear();
        return result;
    }

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

    // やねうら王の定跡ファイルと同じく sfen 行の文字の順に書く（キーの順。同じ局面の手数違いの登録はファイルの順）。
    // 手数違いの登録も、それぞれの手数の sfen 行で書く（やねうら王は既定では手数まで一致する登録だけを使う）
    QList<QByteArray> keys = book.m_positions.keys();
    std::sort(keys.begin(), keys.end());
    constexpr qsizetype kChunkSize = 1 << 20;
    QByteArray out;
    out.reserve(kChunkSize + 4096);
    out += "#YANEURAOU-DB2016 1.00\n";
    bool written = true;
    int savedCount = 0;
    for (const QByteArray &key : std::as_const(keys)) {
        const JosekiBook::Records &records = *book.m_positions.constFind(key);
        for (const JosekiBook::PackedMove &packed : records) {
            if (packed.move == JosekiBook::kHeader) {
                out += "sfen " + key + ' ' + QByteArray::number(packed.value) + '\n';
                ++savedCount;
                continue;
            }
            book.appendMoveText(out, packed.move);
            out += ' ';
            book.appendMoveText(out, packed.next);
            out += ' ' + QByteArray::number(packed.value) + ' ' + QByteArray::number(packed.depth) + ' '
                + QByteArray::number(packed.frequency) + '\n';
            // コメントは1行に書く（改行が残ると、次の行が定跡手として読まれる）
            const QString comment = KifuExportCommon::singleLine(book.m_comments.value(packed.comment));
            if (!comment.isEmpty()) out += '#' + comment.toUtf8() + '\n';
        }
        if (out.size() >= kChunkSize) {
            written = file.write(out) == out.size();
            out.clear();
            if (!written) break;
        }
    }
    if (written) written = file.write(out) == out.size();

    if (!written) {
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
