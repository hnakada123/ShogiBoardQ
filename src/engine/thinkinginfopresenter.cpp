/// @file thinkinginfopresenter.cpp
/// @brief 思考情報GUI表示Presenterクラスの実装

#include "thinkinginfopresenter.h"
#include "kifupresentation.h"
#include "usi.h"
#include "pvnotationformatter.h"

#include <QTimer>
#include <memory>
#include <utility>

ThinkingInfoPresenter::ThinkingInfoPresenter(QObject* parent)
    : QObject(parent)
    , m_locale(QLocale::English)
    , m_formatter(std::make_unique<PvNotationFormatter>())
{
}

ThinkingInfoPresenter::~ThinkingInfoPresenter() = default;

// ============================================================
// 依存関係設定
// ============================================================


// ============================================================
// 状態設定
// ============================================================

void ThinkingInfoPresenter::setPreviousMove(int fileTo, int rankTo)
{
    m_previousFileTo = fileTo;
    m_previousRankTo = rankTo;
}

void ThinkingInfoPresenter::setBaseSfen(const QString& sfen)
{
    m_baseSfen = sfen;
    // 読み筋の手番の印（▲△）は、この局面の手番から決まる
    m_formatter->setPosition(sfen);
}

QString ThinkingInfoPresenter::baseSfen() const
{
    return m_baseSfen;
}

QString ThinkingInfoPresenter::mateScoreText(const QString& plies)
{
    return tr("%1手詰").arg(plies);
}

// ============================================================
// info処理
// ============================================================

void ThinkingInfoPresenter::onInfoReceived(const UsiInfo& info)
{
    m_infoBuffer.append(info);

    if (!m_flushScheduled) {
        m_flushScheduled = true;
        QTimer::singleShot(50, this, &ThinkingInfoPresenter::flushInfoBuffer);
    }
}

void ThinkingInfoPresenter::flushInfoBuffer()
{
    m_flushScheduled = false;

    QList<UsiInfo> batch;
    batch.swap(m_infoBuffer);

    for (const UsiInfo& info : std::as_const(batch)) {
        processInfo(info);
    }
}

void ThinkingInfoPresenter::processInfo(const UsiInfo& info)
{
    // 処理フロー:
    // 1. 思考開始局面の上で読み筋・探索手を日本語にする
    // 2. 探索手・深さ・ノード数・NPS・ハッシュ使用率のシグナルを発行
    // 3. 評価値/詰み手数を更新
    // 4. 有効な情報があれば思考タブ向けの行を発行

    qCDebug(lcEngine) << "processInfo: prevFile=" << m_previousFileTo << "prevRank=" << m_previousRankTo;

    m_formatter->setPreviousMove(BoardSquare{m_previousFileTo, m_previousRankTo});
    m_formatter->format(info);

    emitSearchedHand();
    emitDepth(info);
    emitNodes(info);
    emitNps(info);
    emitHashfull(info);

    QString scoreText;
    int scoreInt = 0;
    updateEvaluationInfo(info, scoreText, scoreInt);

    ThinkingInfoRow row;
    row.time = info.timeMs >= 0 ? QString::number(info.timeMs) : QString();
    row.depth = info.depth >= 0 ? QString::number(info.depth) : QString();
    row.nodes = info.nodes >= 0 ? QString::number(info.nodes) : QString();
    row.score = scoreText;
    row.pvKanji = m_formatter->pvKanjiStr();
    if (row.time.isEmpty() && row.depth.isEmpty() && row.nodes.isEmpty()
        && row.score.isEmpty() && row.pvKanji.isEmpty()) {
        return;
    }
    row.usiPv = m_formatter->pvUsiStr();
    row.enginePv = info.pv;
    row.baseSfen = m_baseSfen;
    row.multipv = qMax(1, info.multipv);
    row.scoreCp = scoreInt;
    emit thinkingInfoUpdated(row);
}

void ThinkingInfoPresenter::requestClearThinkingInfo()
{
    qCDebug(lcEngine) << "requestClearThinkingInfo";
    m_infoBuffer.clear();
    emit clearThinkingInfoRequested();
}

// ============================================================
// 通信ログ
// ============================================================

/// プレフィックス文字列（"[E1]..."等）からエンジン番号タグを抽出する
static QString extractEngineTag(const QString& prefix)
{
    // プレフィックスは "[E1]..." や "[E2]..." の形式
    // E1 または E2 を抽出
    if (prefix.contains(QStringLiteral("[E1"))) {
        return QStringLiteral("E1");
    } else if (prefix.contains(QStringLiteral("[E2"))) {
        return QStringLiteral("E2");
    }
    return QStringLiteral("E?");
}

void ThinkingInfoPresenter::logSentCommand(const QString& prefix, const QString& command)
{
    // エンジンタグ付きフォーマットで送信ログを通知
    const QString tag = extractEngineTag(prefix);
    emit commLogAppended(QStringLiteral("▶ ") + tag + QStringLiteral(": ") + command);
}

void ThinkingInfoPresenter::logReceivedData(const QString& prefix, const QString& data)
{
    // エンジンタグ付きフォーマットで受信ログを通知
    const QString tag = extractEngineTag(prefix);
    emit commLogAppended(QStringLiteral("◀ ") + tag + QStringLiteral(": ") + data);
}

void ThinkingInfoPresenter::logStderrData(const QString& prefix, const QString& data)
{
    // エンジンタグ付きフォーマットで標準エラーログを通知
    const QString tag = extractEngineTag(prefix);
    emit commLogAppended(QStringLiteral("⚠ ") + tag + QStringLiteral(": ") + data);
}

// ============================================================
// シグナル発行ヘルパメソッド
// ============================================================

void ThinkingInfoPresenter::emitSearchedHand()
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

void ThinkingInfoPresenter::emitDepth(const UsiInfo& info)
{
    if (info.depth < 0) return;

    QString depthStr = QString::number(info.depth);
    if (info.seldepth >= 0) {
        depthStr += QLatin1Char('/') + QString::number(info.seldepth);
    }
    emit searchDepthUpdated(depthStr);
}

void ThinkingInfoPresenter::emitNodes(const UsiInfo& info)
{
    if (info.nodes < 0) return;
    emit nodeCountUpdated(m_locale.toString(info.nodes));
}

void ThinkingInfoPresenter::emitNps(const UsiInfo& info)
{
    if (info.nps < 0) return;
    emit npsUpdated(m_locale.toString(info.nps));
}

void ThinkingInfoPresenter::emitHashfull(const UsiInfo& info)
{
    if (info.hashfull < 0) {
        emit hashUsageUpdated(QString());
    } else {
        emit hashUsageUpdated(QString::number(info.hashfull / 10) + QLatin1Char('%'));
    }
}

// ============================================================
// 評価値計算
// ============================================================

// やねうら王系エンジンの優等局面スコア（VALUE_SUPERIOR）に合わせた詰み評価値
// VALUE_SUPERIOR == 28000, PawnValue == 90, centi-pawn換算で100/90倍 → 約31111
static constexpr int SCORE_MATE_VALUE = 31111;

int ThinkingInfoPresenter::updateCpScore(const UsiInfo& info)
{
    const int cp = info.score.value;
    m_scoreStr = QString::number(cp);
    if (info.score.bound == UsiScore::Bound::Lower) {
        m_scoreStr += QStringLiteral("++");
    } else if (info.score.bound == UsiScore::Bound::Upper) {
        m_scoreStr += QStringLiteral("--");
    }
    return cp;
}

void ThinkingInfoPresenter::updateLastScore(int scoreInt)
{
    qCDebug(lcEngine) << "updateLastScore: scoreInt=" << scoreInt << "before=" << m_lastScoreCp;
    
    // 表示範囲外の値も保持し、チャート側で端のマーカーとして描画する。
    m_lastScoreCp = scoreInt;
}

void ThinkingInfoPresenter::updateEvaluationInfo(const UsiInfo& info, QString& scoreText, int& scoreInt)
{
    // 処理フロー:
    // 1. multipv 1 のみ評価値グラフ更新対象とする（2以降は思考タブの表示だけ）
    // 2. score mate → 詰み評価値（±31111）と「N手詰」の表示
    // 3. score cp → 解析モードに応じて符号反転し評価値を算出

    const bool isMultipv1 = info.multipv == 1;

    qCDebug(lcEngine) << "updateEvaluationInfo: multipv=" << info.multipv
                      << "before=" << m_lastScoreCp;

    if (info.score.isMate()) {
        scoreInt = info.score.mateSign() * SCORE_MATE_VALUE;
        const QString mate = info.score.mateText();
        scoreText = info.score.mateCountKnown ? mateScoreText(mate) : tr("詰み");

        if (isMultipv1) {
            m_scoreStr = mate;
            m_lastScoreCp = scoreInt;
            m_lastScoreMate = mate;
        }
    } else if (info.score.isCp()) {
        scoreInt = updateCpScore(info);
        scoreText = m_scoreStr;

        if (isMultipv1) {
            m_pvKanjiStr = m_formatter->pvKanjiStr();
            qCDebug(lcEngine) << "評価値更新: scoreInt=" << scoreInt << "scoreStr=" << m_scoreStr;
            m_lastScoreMate.clear();
            updateLastScore(scoreInt);
        }
    }
}

void ThinkingInfoPresenter::presentCheckmate(const QStringList& pv)
{
    // info を省略するエンジンでも詰み手順を履歴に残す。通信結果から表示への変換はここで行う。
    flushInfoBuffer();
    UsiInfo info;
    info.score.kind = UsiScore::Kind::Mate;
    info.score.value = static_cast<int>(pv.size());
    info.score.mateCountKnown = true;
    info.score.mateWin = true;
    info.pv = pv;
    processInfo(info);
}
