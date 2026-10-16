/// @file thinkinginfoformatter.cpp
/// @brief エンジンの info 行を表示用の値に整える処理の実装

#include "thinkinginfoformatter.h"
#include "kifupresentation.h"
#include "pvnotationformatter.h"
#include "usimove.h"

#include <QTimer>
#include <memory>
#include <utility>

ThinkingInfoFormatter::ThinkingInfoFormatter(QObject* parent)
    : QObject(parent)
    , m_locale(QLocale::English)
    , m_formatter(std::make_unique<PvNotationFormatter>())
{
}

ThinkingInfoFormatter::~ThinkingInfoFormatter() = default;

// ============================================================
// 探索の局面
// ============================================================

void ThinkingInfoFormatter::setSearchPosition(const UsiPosition& position)
{
    m_baseSfen = position.sfen();
    const std::optional<UsiMove>& last = position.lastMove();
    m_lastUsiMove = last ? last->toString() : QString();
    m_previousTo = last ? last->to : BoardSquare{};
    // 読み筋の手番の印（▲△）は、この局面の手番から決まる
    m_formatter->setPosition(m_baseSfen);
}

// ============================================================
// info処理
// ============================================================

void ThinkingInfoFormatter::onInfoReceived(const UsiInfo& info)
{
    m_infoBuffer.append(info);
    if (!m_flushScheduled) {
        m_flushScheduled = true;
        QTimer::singleShot(50, this, &ThinkingInfoFormatter::flushInfoBuffer);
    }
}

void ThinkingInfoFormatter::flushInfoBuffer()
{
    m_flushScheduled = false;
    QList<UsiInfo> batch;
    batch.swap(m_infoBuffer);
    for (const UsiInfo& info : std::as_const(batch)) processInfo(info);
}

void ThinkingInfoFormatter::processInfo(const UsiInfo& info)
{
    // 読み筋・探索手を、思考を始めた局面の上で漢字表記にする
    m_formatter->setPreviousMove(m_previousTo);
    m_formatter->format(info);

    emitSearchedHand();
    emitStatus(info);

    ThinkingInfoRow row;
    row.timeMs = info.timeMs;
    row.depth = info.depth;
    row.nodes = info.nodes;
    row.score = info.score;
    row.pvKanji = m_formatter->pvKanjiStr();
    if (row.timeMs < 0 && row.depth < 0 && row.nodes < 0 && row.score.isNone() && row.pvKanji.isEmpty()) return;
    row.usiPv = m_formatter->pvUsiStr();
    row.enginePv = info.pv;
    row.baseSfen = m_baseSfen;
    row.lastUsiMove = m_lastUsiMove;
    row.multipv = qMax(1, info.multipv);
    emit thinkingInfoUpdated(row);
}

void ThinkingInfoFormatter::requestClearThinkingInfo()
{
    m_infoBuffer.clear();
    emit clearThinkingInfoRequested();
}

void ThinkingInfoFormatter::presentCheckmate(const UsiCheckmate& result)
{
    if (result.result != UsiCheckmate::Result::Mate || result.pv.isEmpty()) return;
    flushInfoBuffer();
    UsiInfo info;
    info.score = UsiScore::mate(static_cast<int>(result.pv.size()));
    info.pv = result.pv;
    processInfo(info);
}

// ============================================================
// 情報欄
// ============================================================

void ThinkingInfoFormatter::emitSearchedHand()
{
    const QString hand = m_formatter->searchedHand();
    if (hand.isEmpty()) return;

    // info ごとに呼ばれ、同じ手が何度も届く。英語表記では局面を読み直して表記を作るので、
    // 局面と表記の設定が同じ間は、手ごとに作った表示を使う
    constexpr qsizetype kMaxSearchedLabels = 128;
    const auto style = KifuPresentation::options();
    const int notation = static_cast<int>(style.notation);
    if (m_baseSfen != m_searchedSfenSource || notation != m_searchedNotation
        || style.alwaysOrigin != m_searchedAlwaysOrigin || m_searchedLabels.size() >= kMaxSearchedLabels) {
        m_searchedLabels.clear();
        m_searchedSfenSource = m_baseSfen;
        m_searchedNotation = notation;
        m_searchedAlwaysOrigin = style.alwaysOrigin;
    }
    const QString usi = m_formatter->searchedUsi();
    const QString key = hand + QChar(0) + usi;
    const auto it = m_searchedLabels.constFind(key);
    if (it != m_searchedLabels.cend()) {
        emit searchedMoveUpdated(it.value());
        return;
    }
    const QString label = KifuPresentation::label(hand, m_baseSfen, usi);
    m_searchedLabels.insert(key, label);
    emit searchedMoveUpdated(label);
}

void ThinkingInfoFormatter::emitStatus(const UsiInfo& info)
{
    if (info.depth >= 0) {
        QString depth = QString::number(info.depth);
        if (info.seldepth >= 0) depth += QLatin1Char('/') + QString::number(info.seldepth);
        emit searchDepthUpdated(depth);
    }
    if (info.nodes >= 0) emit nodeCountUpdated(m_locale.toString(info.nodes));
    if (info.nps >= 0) emit npsUpdated(m_locale.toString(info.nps));
    emit hashUsageUpdated(info.hashfull < 0 ? QString() : QString::number(info.hashfull / 10) + QLatin1Char('%'));
}
