/// @file jkftosfenconverter.cpp
/// @brief JKF形式棋譜コンバータクラスの実装
///
/// 指し手変換・初期局面構築は JkfMoveParser に委譲。
/// 本ファイルはファイルI/O・パーサ状態管理・オーケストレーションを担当する。

#include "jkftosfenconverter.h"
#include "handicaps.h"
#include "jkfmoveparser.h"
#include "kiflinebuilder.h"
#include "terminalcodes.h"
#include "shogiposition.h"

#include <array>

#include <QJsonDocument>

namespace {
int normalizedColorIndex(const QJsonObject& moveObject, int plyNumber, ParseReport& report)
{
    const QJsonValue colorValue = moveObject.value(QStringLiteral("color"));
    if (!colorValue.isDouble()) {
        report.warn(QStringLiteral("JKF: move %1 has invalid color type; fallback to 0").arg(plyNumber));
        return 0;
    }

    const int color = colorValue.toInt(-1);
    if (color != 0 && color != 1) {
        report.warn(QStringLiteral("JKF: move %1 has out-of-range color %2; fallback to 0").arg(plyNumber).arg(color));
        return 0;
    }
    return color;
}
} // namespace

// ========== 解析 ==========

KifuParseOutcome JkfToSfenConverter::parse(QStringView text, const ParseOptions& options)
{
    KifuParseOutcome outcome;
    ParseReport report(options);
    QJsonObject root;
    QString jsonError;
    if (!parseJson(text.toUtf8(), root, &jsonError)) {
        outcome.warning = jsonError;
        return outcome;
    }
    const QString initial = buildInitialSfen(root, &outcome.startKind);
    const std::optional<Position> start = Position::fromSfen(initial);
    if (!start) {
        outcome.warning = QStringLiteral("JKF: invalid initial position");
        return outcome;
    }
    if (!readRecord(root[QStringLiteral("moves")].toArray(), *start, outcome.record, report)) {
        outcome.record = {};
        outcome.warning = report.warnings();
        return outcome;
    }
    outcome.success = true;
    outcome.gameInfo = gameInfoFromRoot(root);
    outcome.warning = report.warnings();
    return outcome;
}

QList<KifGameInfoItem> JkfToSfenConverter::gameInfoFromRoot(const QJsonObject& root)
{
    QList<KifGameInfoItem> ordered;
    if (!root.contains(QStringLiteral("header"))) return ordered;

    const QJsonObject header = root[QStringLiteral("header")].toObject();
    const QStringList keys = header.keys();
    for (const QString& key : std::as_const(keys)) ordered.append({key, header[key].toString()});
    return ordered;
}

// ========== private ヘルパ ==========

bool JkfToSfenConverter::parseJson(const QByteArray& rawData, QJsonObject& root, QString* warn)
{
    // UTF-8 の BOM は JSON の解析前に除く
    const QByteArray data = rawData.startsWith("\xEF\xBB\xBF") ? rawData.mid(3) : rawData;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        if (warn) *warn = QStringLiteral("JSON parse error: %1").arg(parseError.errorString());
        return false;
    }

    if (!doc.isObject()) {
        if (warn) *warn = QStringLiteral("JKF root is not an object");
        return false;
    }

    root = doc.object();
    if (!root.value(QStringLiteral("moves")).isArray()) {
        if (warn) *warn = QStringLiteral("JKF: 'moves' must be an array");
        return false;
    }
    return true;
}

QString JkfToSfenConverter::buildInitialSfen(const QJsonObject& root, StartKind* kind)
{
    if (!root.contains(QStringLiteral("initial"))) {
        if (kind) *kind = StartKind::Default;
        return JkfMoveParser::presetToSfen(QStringLiteral("HIRATE"));
    }

    const QJsonObject initial = root[QStringLiteral("initial")].toObject();
    const QString preset = initial[QStringLiteral("preset")].toString();

    if (preset == QStringLiteral("OTHER")) {
        if (initial.contains(QStringLiteral("data"))) {
            if (kind) *kind = StartKind::Diagram;
            const QJsonObject data = initial[QStringLiteral("data")].toObject();
            return JkfMoveParser::buildSfenFromInitialData(data);
        }
        return {};
    }

    const Handicaps::Handicap* handicap = Handicaps::byJkfPreset(preset);
    if (kind) *kind = StartKind::Handicap;
    // 知らない preset は開始局面が分からない
    return handicap ? QString::fromLatin1(handicap->sfen) : QString();
}

namespace {
/// 読んだ手順の途中にある分岐（forks）。分岐の手を指す前の両者の累計時間を引き継ぐ
struct JkfFork {
    int ply = 1;
    QJsonArray moves;
    std::array<qint64, 2> totals{0, 0};
};

/// 1本の手順を読む。途中の分岐は forks に集め、手順を読み終えてから足す。形式が壊れていれば false
bool readJkfLine(const QJsonArray& moves, bool isMainline, KifLineBuilder& builder, QList<JkfFork>& forks,
                 ParseReport& report)
{
    for (const auto& value : moves) {
        if (!value.isObject()) {
            report.fail(QStringLiteral("JKF: move entry must be an object"));
            return false;
        }
        const QJsonObject obj = value.toObject();
        if (obj.contains(QStringLiteral("forks")) && !obj[QStringLiteral("forks")].isArray()) {
            report.fail(QStringLiteral("JKF: forks must be an array"));
            return false;
        }
        const int ply = builder.nextPly();
        const QJsonArray forkArrays = obj[QStringLiteral("forks")].toArray();
        for (const auto& fork : forkArrays) {
            if (!fork.isArray()) {
                report.fail(QStringLiteral("JKF: fork must be an array of moves"));
                return false;
            }
            forks.append(JkfFork{ply, fork.toArray(), builder.totals()});
        }

        const QString comment = JkfMoveParser::extractCommentsFromMoveObj(obj);
        const bool hasTime = obj.contains(QStringLiteral("time"));
        const JkfMoveParser::Time time = JkfMoveParser::timeOf(obj[QStringLiteral("time")].toObject());
        if (obj.contains(QStringLiteral("special"))) {
            const Turn side = builder.sideToMove();
            TerminalType terminal = TerminalCodes::fromCsaSpecial(obj[QStringLiteral("special")].toString(), side);
            if (terminal == TerminalType::None) terminal = TerminalType::Error;
            builder.addTerminal(terminal, side,
                                hasTime ? builder.timeFor(side, time.elapsedMs, time.totalMs) : KifLineBuilder::zeroTime());
            if (!comment.isEmpty()) builder.appendComment(comment);
            break;
        }
        if (!obj.contains(QStringLiteral("move"))) {
            // 手のない項目は、本譜の最初の項目（開始局面のコメント）だけ読む
            if (isMainline && builder.line().entries.isEmpty() && !comment.isEmpty()) builder.appendComment(comment);
            continue;
        }

        const QJsonObject moveObject = obj[QStringLiteral("move")].toObject();
        const std::optional<UsiMove> move = JkfMoveParser::toUsiMove(moveObject);
        if (!move) {
            report.fail(QStringLiteral("JKF: invalid move at ply %1").arg(ply));
            return false;
        }
        RecordTime recordTime = KifLineBuilder::zeroTime();
        if (hasTime) {
            const Turn side = normalizedColorIndex(moveObject, ply, report) == 0 ? Turn::Black : Turn::White;
            recordTime = builder.timeFor(side, time.elapsedMs, time.totalMs);
        }
        // 指せない手は、その手の手前で手順を打ち切る（全形式で同じ）
        if (!builder.addMove(*move, recordTime, JkfMoveParser::notesOf(moveObject))) {
            if (!report.rejectMove(ply, builder.cannotRestore(QStringLiteral("JKF"), ply)))
                return false;
            builder.stop();
            break;
        }
        if (!comment.isEmpty()) builder.appendComment(comment);
    }
    return true;
}

/// 手順 parent（-1 は本譜）の分岐を、書かれた順に（分岐の中の分岐も続けて）足す
bool addJkfForks(KifParseResult& out, int parent, const QList<JkfFork>& forks, ParseReport& report)
{
    for (const auto& fork : forks) {
        const auto from = out.branchPoint(parent, fork.ply);
        const std::optional<Position> start = from ? from->position : std::nullopt;
        if (!start) {
            report.fail(QStringLiteral("JKF: invalid fork at ply %1").arg(fork.ply));
            return false;
        }
        KifLineBuilder builder(*start, fork.ply, from->previousMove, KifLineBuilder::LineKind::Variation);
        builder.setTotals(fork.totals);
        QList<JkfFork> nested;
        if (!readJkfLine(fork.moves, false, builder, nested, report)) return false;
        out.addVariation(*from, builder.take());
        if (!addJkfForks(out, static_cast<int>(out.variations.size()) - 1, nested, report)) return false;
    }
    return true;
}
} // namespace

bool JkfToSfenConverter::readRecord(const QJsonArray& moves, const Position& start, KifParseResult& out,
                                    ParseReport& report)
{
    KifLineBuilder builder(start);
    QList<JkfFork> forks;
    if (!readJkfLine(moves, true, builder, forks, report)) return false;
    out.mainline = builder.take();
    return addJkfForks(out, -1, forks, report);
}
