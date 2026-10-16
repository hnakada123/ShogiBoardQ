/// @file jkftosfenconverter.cpp
/// @brief JKF形式棋譜コンバータクラスの実装
///
/// 指し手変換・初期局面構築は JkfMoveParser に委譲。
/// 本ファイルはファイルI/O・パーサ状態管理・オーケストレーションを担当する。

#include "jkftosfenconverter.h"
#include "handicaps.h"
#include "jkfmoveparser.h"
#include "parsecommon.h"
#include "terminalcodes.h"
#include "sfenutils.h"

#include <array>

#include <QJsonDocument>

namespace {
void appendWarning(QString* warn, const QString& message)
{
    if (!warn) {
        return;
    }
    if (!warn->isEmpty()) {
        *warn += QLatin1Char('\n');
    }
    *warn += message;
}

int normalizedColorIndex(const QJsonObject& moveObject, int plyNumber, QString* warn)
{
    const QJsonValue colorValue = moveObject.value(QStringLiteral("color"));
    if (!colorValue.isDouble()) {
        appendWarning(warn,
                      QStringLiteral("JKF: move %1 has invalid color type; fallback to 0")
                          .arg(plyNumber));
        return 0;
    }

    const int color = colorValue.toInt(-1);
    if (color != 0 && color != 1) {
        appendWarning(warn,
                      QStringLiteral("JKF: move %1 has out-of-range color %2; fallback to 0")
                          .arg(plyNumber)
                          .arg(color));
        return 0;
    }
    return color;
}
} // namespace

// ========== public API ==========

QString JkfToSfenConverter::mapPresetToSfen(const QString& preset)
{
    return JkfMoveParser::presetToSfen(preset);
}

QString JkfToSfenConverter::detectInitialSfenFromText(const QString& text, QString* detectedLabel)
{
    QJsonObject root;
    QString warn;
    if (!parseJson(text.toUtf8(), root, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return JkfMoveParser::presetToSfen(QStringLiteral("HIRATE"));
    }
    return buildInitialSfen(root, detectedLabel);
}

bool JkfToSfenConverter::parseText(const QString& text, KifParseResult& out, QString* errorMessage)
{
    out = KifParseResult{};
    QJsonObject root;
    if (!parseJson(text.toUtf8(), root, errorMessage)) return false;
    return parseRoot(root, out, errorMessage);
}

bool JkfToSfenConverter::parseRoot(const QJsonObject& root, KifParseResult& out, QString* errorMessage)
{
    QString detectedLabel;
    out.mainline.baseSfen = buildInitialSfen(root, &detectedLabel);
    out.mainline.startPly = 1;
    if (out.mainline.baseSfen.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("JKF: invalid initial position");
        return false;
    }

    if (!root.contains(QStringLiteral("moves"))) {
        if (errorMessage) *errorMessage = QStringLiteral("JKF: 'moves' array not found");
        return false;
    }

    const QJsonArray movesArray = root[QStringLiteral("moves")].toArray();
    if (parseMovesArray(movesArray, out.mainline.baseSfen, out, errorMessage)) return true;
    out = KifParseResult{};
    return false;
}

QList<KifGameInfoItem> JkfToSfenConverter::extractGameInfoFromText(const QString& text)
{
    QJsonObject root;
    QString warn;
    if (!parseJson(text.toUtf8(), root, &warn)) return {};
    return gameInfoFromRoot(root);
}

QList<KifGameInfoItem> JkfToSfenConverter::gameInfoFromRoot(const QJsonObject& root)
{
    QList<KifGameInfoItem> ordered;

    if (!root.contains(QStringLiteral("header"))) {
        return ordered;
    }

    const QJsonObject header = root[QStringLiteral("header")].toObject();
    const QStringList keys = header.keys();

    for (const QString& key : std::as_const(keys)) {
        const QString value = header[key].toString();
        ordered.append({key, value});
    }

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

QString JkfToSfenConverter::buildInitialSfen(const QJsonObject& root, QString* detectedLabel)
{
    if (!root.contains(QStringLiteral("initial"))) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手");
        return JkfMoveParser::presetToSfen(QStringLiteral("HIRATE"));
    }

    const QJsonObject initial = root[QStringLiteral("initial")].toObject();
    const QString preset = initial[QStringLiteral("preset")].toString();

    if (preset == QStringLiteral("OTHER")) {
        if (initial.contains(QStringLiteral("data"))) {
            if (detectedLabel) *detectedLabel = QStringLiteral("任意局面");
            const QJsonObject data = initial[QStringLiteral("data")].toObject();
            return JkfMoveParser::buildSfenFromInitialData(data);
        }
        return {};
    }

    const Handicaps::Handicap* handicap = Handicaps::byJkfPreset(preset);
    if (detectedLabel) *detectedLabel = handicap ? QString(QStringView(handicap->label)) : preset;
    // 知らない preset は開始局面が分からない
    return handicap ? QString::fromLatin1(handicap->sfen) : QString();
}

namespace {
/// 読んだ手順の途中にある分岐（forks）。分岐の手を指す前の移動先と累計時間を引き継ぐ
struct JkfFork {
    int ply = 1;
    QJsonArray moves;
    int prevToX = 0;
    int prevToY = 0;
    std::array<qint64, 2> cumSec{0, 0};
};

/// 1本の手順を読む。途中の分岐は forks に集め、手順を読み終えてから足す
bool readJkfLine(const QJsonArray& moves, const KifBranchPoint& from, int prevToX, int prevToY,
                 std::array<qint64, 2> cumSec, bool isMainline, KifLine& outLine, QList<JkfFork>& forks, QString* warn)
{
    KifuParseCommon::KifLineDraft line;
    line.baseSfen = from.sfen;
    line.startPly = from.startPly;
    line.previousUsi = from.previousUsi;
    Turn side = SfenUtils::sideToMove(from.sfen);
    int ply = from.startPly;
    if (isMainline) line.disp.append(KifuParseCommon::createOpeningDisplayItem({}, {}));

    for (const auto& value : moves) {
        if (!value.isObject()) {
            appendWarning(warn, QStringLiteral("JKF: move entry must be an object"));
            return false;
        }
        const QJsonObject obj = value.toObject();
        if (obj.contains(QStringLiteral("forks")) && !obj[QStringLiteral("forks")].isArray()) {
            appendWarning(warn, QStringLiteral("JKF: forks must be an array"));
            return false;
        }
        const QJsonArray forkArrays = obj[QStringLiteral("forks")].toArray();
        for (const auto& fork : forkArrays) {
            if (!fork.isArray()) {
                appendWarning(warn, QStringLiteral("JKF: fork must be an array of moves"));
                return false;
            }
            forks.append(JkfFork{ply, fork.toArray(), prevToX, prevToY, cumSec});
        }

        const QString comment = JkfMoveParser::extractCommentsFromMoveObj(obj);
        if (obj.contains(QStringLiteral("special"))) {
            const QString special = obj[QStringLiteral("special")].toString();
            TerminalType terminal = TerminalCodes::fromCsaSpecial(special, side);
            if (terminal == TerminalType::None) terminal = TerminalType::Error;
            auto item = KifuParseCommon::createTerminalDisplayItem(ply, terminal, side);
            item.comment = comment;
            if (obj.contains(QStringLiteral("time"))) {
                item.timeText = JkfMoveParser::formatTimeText(obj[QStringLiteral("time")].toObject(), cumSec[side == Turn::Black ? 0 : 1]);
            }
            line.disp.append(item);
            line.endsWithTerminal = true;
            break;
        }
        if (!obj.contains(QStringLiteral("move"))) {
            if (isMainline && ply == from.startPly) line.disp[0].comment = comment;
            continue;
        }

        const QJsonObject move = obj[QStringLiteral("move")].toObject();
        const BoardSquare previousTo{prevToX, prevToY};
        const QString usi = JkfMoveParser::convertMoveToUsi(move, prevToX, prevToY);
        const auto usiMove = UsiMove::parse(usi);
        if (!usiMove) {
            appendWarning(warn, QStringLiteral("JKF: invalid move at ply %1").arg(ply));
            return false;
        }
        const QString pretty = JkfMoveParser::convertMoveToPretty(move, side, *usiMove, previousTo);
        QString time;
        if (obj.contains(QStringLiteral("time"))) {
            const int color = normalizedColorIndex(move, ply, warn);
            time = JkfMoveParser::formatTimeText(obj[QStringLiteral("time")].toObject(), cumSec[static_cast<size_t>(color)]);
        }
        auto item = KifuParseCommon::createMoveDisplayItem(ply, pretty, time);
        item.comment = comment;
        line.disp.append(item);
        line.usiMoves.append(usi);
        side = opponent(side);
        ++ply;
    }
    // 指し手が局面に合うかは resolve が盤で確かめる
    return line.resolve(outLine, warn);
}

/// 手順 parent（-1 は本譜）の分岐を、書かれた順に（分岐の中の分岐も続けて）足す
bool addJkfForks(KifParseResult& out, int parent, const QList<JkfFork>& forks, QString* warn)
{
    for (const auto& fork : forks) {
        const auto from = out.branchPoint(parent, fork.ply);
        if (!from) {
            appendWarning(warn, QStringLiteral("JKF: invalid fork at ply %1").arg(fork.ply));
            return false;
        }
        KifLine line;
        QList<JkfFork> nested;
        if (!readJkfLine(fork.moves, *from, fork.prevToX, fork.prevToY, fork.cumSec, false, line, nested, warn))
            return false;
        out.addVariation(*from, std::move(line));
        if (!addJkfForks(out, static_cast<int>(out.variations.size()) - 1, nested, warn)) return false;
    }
    return true;
}
} // namespace

bool JkfToSfenConverter::parseMovesArray(const QJsonArray& movesArray, const QString& baseSfen,
                                         KifParseResult& out, QString* warn)
{
    KifBranchPoint start;
    start.sfen = baseSfen;
    QList<JkfFork> forks;
    if (!readJkfLine(movesArray, start, 0, 0, {0, 0}, true, out.mainline, forks, warn)) return false;
    return addJkfForks(out, -1, forks, warn);
}
