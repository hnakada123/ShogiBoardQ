/// @file josekiimport.cpp
/// @brief 複数の棋譜ファイルから定跡に登録する手を集める処理の実装

#include "josekiimport.h"

#include "josekibook.h"
#include "kifparsetypes.h"
#include "kifuformat.h"
#include "kifuloadparser.h"
#include "logcategories.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <algorithm>

namespace JosekiImport {

namespace {

bool whiteToMove(const QString &sfen)
{
    return sfen.section(QLatin1Char(' '), 1, 1) == QLatin1String("w");
}

/// 本譜の勝った側（勝敗の付かない終局や、終局の行が無ければ nullopt）
std::optional<Turn> winnerOf(const RecordLine &line)
{
    const RecordEntry *terminal = line.terminal();
    if (!terminal) return std::nullopt;
    // 終局の行の局面は最後の手の後。その手番が投了・宣言などをした側
    const Turn mover = whiteToMove(terminal->sfen) ? Turn::White : Turn::Black;
    const Turn opponent = mover == Turn::Black ? Turn::White : Turn::Black;
    switch (terminal->terminal) {
    case TerminalType::Resign:
    case TerminalType::Checkmate:
    case TerminalType::Timeout:
    case TerminalType::IllegalLoss:
    case TerminalType::Forfeit:
        return opponent;
    case TerminalType::IllegalWin:
    case TerminalType::DeclarationWin:
    case TerminalType::ForfeitWin:
        return mover;
    default:
        return std::nullopt;
    }
}

} // namespace

bool isKifuFile(const QString &path)
{
    const std::optional<KifuFormat> format = KifuFormats::fromPath(path);
    return format && KifuFormats::isSavable(*format);
}

QStringList expandPaths(const QStringList &paths)
{
    QStringList files;
    QSet<QString> seen;
    const auto add = [&files, &seen](const QString &path) {
        const QString canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty() || seen.contains(canonical)) return;
        seen.insert(canonical);
        files.append(path);
    };
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isDir()) {
            QStringList inFolder;
            QDirIterator it(path, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString file = it.next();
                if (isKifuFile(file)) inFolder.append(file);
            }
            std::sort(inFolder.begin(), inFolder.end());
            for (const QString &file : std::as_const(inFolder)) add(file);
        } else if (info.isFile()) {
            add(path);
        }
    }
    return files;
}

Collector::Collector(const Options &options)
    : m_options(options)
{
}

void Collector::addGame(const KifParseResult &record)
{
    const std::optional<Turn> winner = winnerOf(record.mainline);
    if (m_options.sides == Sides::WinnerOnly && !winner) return;
    QSet<QString> seenInGame;
    addLine(record.mainline.startSfen, record.mainline, winner, seenInGame);
    if (!m_options.includeVariations) return;
    for (int i = 0; i < record.variations.size(); ++i) {
        const KifVariation &variation = record.variations.at(i);
        QString start = variation.line.startSfen;
        if (start.isEmpty()) {
            if (const auto point = record.branchPoint(variation.parentVariation, variation.startPly)) start = point->sfen;
        }
        addLine(start, variation.line, winner, seenInGame);
    }
}

void Collector::addLine(const QString &startSfen, const RecordLine &line, std::optional<Turn> winner,
                        QSet<QString> &seenInGame)
{
    QString before = startSfen;
    int ply = line.startPly;
    for (const RecordEntry &entry : line.entries) {
        if (entry.isTerminal() || ply > m_options.maxPly || before.isEmpty()) break;
        const Turn mover = whiteToMove(before) ? Turn::White : Turn::Black;
        bool take = true;
        switch (m_options.sides) {
        case Sides::Both: break;
        case Sides::BlackOnly: take = mover == Turn::Black; break;
        case Sides::WhiteOnly: take = mover == Turn::White; break;
        case Sides::WinnerOnly: take = winner && *winner == mover; break;
        }
        const QString usiMove = entry.move.move.toString();
        if (take && !usiMove.isEmpty()) {
            const QString key = JosekiBook::positionKey(before) + QLatin1Char(' ') + usiMove;
            if (!seenInGame.contains(key)) {
                seenInGame.insert(key);
                // 定跡のコメントは1行で書くので、改行は空白にする
                const QString comment = m_options.useComments ? entry.comment.simplified() : QString();
                const auto it = m_index.constFind(key);
                if (it == m_index.constEnd()) {
                    m_index.insert(key, m_moves.size());
                    m_moves.append(Move{before, usiMove, 1, comment});
                } else {
                    Move &move = m_moves[*it];
                    ++move.count;
                    if (move.comment.isEmpty()) move.comment = comment;
                }
            }
        }
        before = entry.sfen;
        ++ply;
    }
}

QList<Move> Collector::moves() const
{
    QList<Move> moves;
    for (const Move &move : m_moves) {
        if (move.count >= m_options.minCount) moves.append(move);
    }
    return moves;
}

Result collect(const QStringList &files, const Options &options, const std::function<bool(int)> &progress)
{
    Result result;
    Collector collector(options);
    for (qsizetype i = 0; i < files.size(); ++i) {
        const QString &file = files.at(i);
        const KifuLoadResult loaded = KifuLoadParser::parseFile(file);
        if (loaded.success) {
            collector.addGame(loaded.record);
            ++result.gameCount;
        } else {
            qCDebug(lcKifu) << "JosekiImport::collect: failed to read" << file << loaded.error;
            result.failedFiles.append(file);
        }
        if (progress && !progress(static_cast<int>(i + 1))) {
            result.canceled = true;
            return result;
        }
    }
    result.moves = collector.moves();
    return result;
}

} // namespace JosekiImport
