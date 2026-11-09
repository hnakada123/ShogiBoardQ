/// @file positioncollectionfile.cpp
/// @brief 局面集ファイル（1行1局面）の読み取りの実装

#include "positioncollectionfile.h"

#include "boundedfile.h"
#include "sfenvalidator.h"
#include "usimove.h"
#include "usiposition.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace PositionCollectionFile {

QString Line::position() const
{
    if (!movesPlayed || moves.isEmpty()) return SfenValidator::validate(startSfen).normalizedSfen;
    return SfenValidator::applyUsiMoves(startSfen, moves);
}

LineKind parseLine(const QString& text, int lineNumber, Line* line)
{
    QString trimmed = text.trimmed();
    if (trimmed.startsWith(QChar(0xfeff))) trimmed = trimmed.mid(1).trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) return LineKind::Skipped;

    // 語の分け方は position コマンドと同じ（UsiPosition::split。"sfen"・"moves"・手数の欄は省ける）
    const std::optional<UsiPosition::Words> words = UsiPosition::split(trimmed);
    if (!words) return LineKind::Invalid;
    Line result;
    result.lineNumber = lineNumber;
    result.startSfen = words->completeStart();
    // position コマンドの行と startpos の行の手は指した後の局面、SFEN だけの行の手は参考手順
    result.movesPlayed = words->positionCommand || words->startpos;
    result.moves = words->moves;
    for (const QString& move : std::as_const(result.moves))
        if (!UsiMove::isExact(move)) return LineKind::Invalid;
    if (!SfenValidator::validate(result.startSfen).valid) return LineKind::Invalid;

    if (line) *line = std::move(result);
    return LineKind::Position;
}

Contents parse(const QString& text)
{
    Contents contents;
    const QStringList lines = text.split(QLatin1Char('\n'));
    const qsizetype count = qMin(lines.size(), kMaxLines);
    contents.unreadLines = lines.size() - count;
    for (qsizetype i = 0; i < count; ++i) {
        const int lineNumber = static_cast<int>(i + 1);
        Line line;
        switch (parseLine(lines.at(i), lineNumber, &line)) {
        case LineKind::Skipped: break;
        case LineKind::Position: contents.lines.append(std::move(line)); break;
        case LineKind::Invalid: contents.invalidLines.append(lineNumber); break;
        }
    }
    return contents;
}

File readFile(const QString& path)
{
    File file;
    const qint64 limit = BoundedFile::kMaxCollectionBytes;
    const QString name = QFileInfo(path).fileName();
    switch (BoundedFile::read(path, limit, file.bytes)) {
    case BoundedFile::Result::Ok:
        file.ok = true;
        file.text = QString::fromUtf8(file.bytes);
        break;
    case BoundedFile::Result::OpenFailed:
        file.error = QCoreApplication::translate("PositionCollectionFile", "ファイルを開けませんでした: %1").arg(name);
        break;
    case BoundedFile::Result::NotRegularFile:
        file.error = QCoreApplication::translate("PositionCollectionFile", "通常のファイルではありません: %1").arg(name);
        break;
    case BoundedFile::Result::TooLarge:
        file.error = QCoreApplication::translate("PositionCollectionFile", "ファイルが大きすぎます（%1 MB まで）: %2")
                         .arg(limit / (1024 * 1024))
                         .arg(name);
        break;
    }
    return file;
}

QString fileFilter()
{
    return QCoreApplication::translate("PositionCollectionFile", "局面集 (*.txt *.sfen *.usi);;すべてのファイル (*)");
}

} // namespace PositionCollectionFile
