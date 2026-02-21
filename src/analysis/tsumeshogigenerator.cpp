/// @file tsumeshogigenerator.cpp
/// @brief 詰将棋局面自動生成オーケストレータの実装

#include "tsumeshogigenerator.h"
#include "usi.h"
#include "playmode.h"

namespace {
/// エンジン無応答ガードの余裕時間(ms)
constexpr int kSafetyMarginMs = 5000;
/// プログレス更新間隔(ms)
constexpr int kProgressIntervalMs = 500;
/// 駒種数（P,L,N,S,G,B,R）
constexpr int kPieceTypeCount = 7;
}

TsumeshogiGenerator::TsumeshogiGenerator(QObject* parent)
    : QObject(parent)
{
    m_safetyTimer.setSingleShot(true);
    connect(&m_safetyTimer, &QTimer::timeout, this, &TsumeshogiGenerator::onSafetyTimeout);

    connect(&m_progressTimer, &QTimer::timeout, this, &TsumeshogiGenerator::onProgressTimerTimeout);
}

TsumeshogiGenerator::~TsumeshogiGenerator()
{
    stop();
}

void TsumeshogiGenerator::start(const Settings& settings)
{
    if (m_phase != Phase::Idle) return;

    m_settings = settings;
    m_triedCount = 0;
    m_foundCount = 0;
    m_phase = Phase::Searching;
    m_currentSfen.clear();

    m_positionGenerator.setSettings(settings.posGenSettings);

    // PlayMode を作成（Usiコンストラクタが参照を要求するため）
    m_playMode = std::make_unique<PlayMode>(PlayMode::TsumiSearchMode);

    // Usi インスタンスを作成（モデル不要、ゲームコントローラ不要）
    m_usi = std::make_unique<Usi>(nullptr, nullptr, nullptr, *m_playMode, this);

    // checkmate シグナルを接続
    connect(m_usi.get(), &Usi::checkmateSolved,
            this, &TsumeshogiGenerator::onCheckmateSolved);
    connect(m_usi.get(), &Usi::checkmateNoMate,
            this, &TsumeshogiGenerator::onCheckmateNoMate);
    connect(m_usi.get(), &Usi::checkmateNotImplemented,
            this, &TsumeshogiGenerator::onCheckmateNotImplemented);
    connect(m_usi.get(), &Usi::checkmateUnknown,
            this, &TsumeshogiGenerator::onCheckmateUnknown);
    connect(m_usi.get(), &Usi::errorOccurred,
            this, &TsumeshogiGenerator::errorOccurred);

    // エンジン起動
    m_usi->startAndInitializeEngine(settings.enginePath, settings.engineName);

    // タイマー開始
    m_elapsedTimer.start();
    m_progressTimer.start(kProgressIntervalMs);

    // 最初の局面を送信
    generateAndSendNext();
}

void TsumeshogiGenerator::stop()
{
    if (m_phase == Phase::Idle) return;
    m_phase = Phase::Idle;
    cleanup();
    emit finished();
}

bool TsumeshogiGenerator::isRunning() const
{
    return m_phase != Phase::Idle;
}

// ======================================================================
// エンジン応答スロット
// ======================================================================

void TsumeshogiGenerator::onCheckmateSolved(const QStringList& pv)
{
    if (m_phase == Phase::Idle) return;
    m_safetyTimer.stop();

    if (m_phase == Phase::Searching) {
        // 「ちょうどN手」のフィルタリング
        if (pv.size() == m_settings.targetMoves) {
            startTrimmingPhase(m_currentSfen, pv);
        } else {
            processResult(false);
        }
    } else { // Trimming
        if (pv.size() == m_settings.targetMoves) {
            // 除去しても同じ手数で詰む → 除去確定
            m_trimBaseSfen = m_trimTestSfen;
            m_trimBasePv = pv;
            // 候補リストを再構築し先頭から再開
            m_trimCandidates = enumerateRemovablePieces(m_trimBaseSfen);
            m_trimCandidateIndex = 0;
            tryNextTrimCandidate();
        } else {
            // 手数が変わった → 除去却下、次の候補へ
            m_trimCandidateIndex++;
            tryNextTrimCandidate();
        }
    }
}

void TsumeshogiGenerator::onCheckmateNoMate()
{
    if (m_phase == Phase::Idle) return;
    m_safetyTimer.stop();

    if (m_phase == Phase::Searching) {
        processResult(false);
    } else { // Trimming
        // 詰みなし → 除去却下、次の候補へ
        m_trimCandidateIndex++;
        tryNextTrimCandidate();
    }
}

void TsumeshogiGenerator::onCheckmateNotImplemented()
{
    if (m_phase == Phase::Idle) return;
    m_safetyTimer.stop();
    m_phase = Phase::Idle;
    cleanup();
    emit errorOccurred(tr("エンジンが詰み探索に対応していません。"));
    emit finished();
}

void TsumeshogiGenerator::onCheckmateUnknown()
{
    if (m_phase == Phase::Idle) return;
    m_safetyTimer.stop();

    if (m_phase == Phase::Searching) {
        processResult(false);
    } else { // Trimming
        // 不明 → 除去却下、次の候補へ
        m_trimCandidateIndex++;
        tryNextTrimCandidate();
    }
}

void TsumeshogiGenerator::onSafetyTimeout()
{
    if (m_phase == Phase::Idle) return;

    if (m_phase == Phase::Searching) {
        // エンジン無応答 → 次の局面に進む
        processResult(false);
    } else { // Trimming
        // タイムアウト → 除去却下、次の候補へ
        m_trimCandidateIndex++;
        tryNextTrimCandidate();
    }
}

void TsumeshogiGenerator::onProgressTimerTimeout()
{
    emit progressUpdated(m_triedCount, m_foundCount, m_elapsedTimer.elapsed());
}

// ======================================================================
// 検索フェーズ
// ======================================================================

void TsumeshogiGenerator::generateAndSendNext()
{
    if (m_phase != Phase::Searching) return;

    // 局面を生成
    m_currentSfen = m_positionGenerator.generate();

    // ThinkingInfoPresenter が info 行を処理する際に盤面データが必要
    // ダミーの盤面データ（81マス空欄）を設定してクラッシュを防ぐ
    QVector<QChar> dummyBoard(Usi::NUM_BOARD_SQUARES, QChar(' '));
    m_usi->setClonedBoardData(dummyBoard);

    // position コマンド用の文字列を構築
    QString positionStr = QStringLiteral("position sfen ") + m_currentSfen;

    // エンジンに送信
    m_usi->sendPositionAndGoMateCommands(m_settings.timeoutMs, positionStr);

    // 安全タイマーを設定
    m_safetyTimer.start(m_settings.timeoutMs + kSafetyMarginMs);
}

void TsumeshogiGenerator::processResult(bool found, const QStringList& pv)
{
    m_triedCount++;

    if (found) {
        m_foundCount++;
        emit positionFound(m_currentSfen, pv);

        // 上限チェック
        if (m_settings.maxPositionsToFind > 0 && m_foundCount >= m_settings.maxPositionsToFind) {
            m_phase = Phase::Idle;
            emit progressUpdated(m_triedCount, m_foundCount, m_elapsedTimer.elapsed());
            cleanup();
            emit finished();
            return;
        }
    }

    emit progressUpdated(m_triedCount, m_foundCount, m_elapsedTimer.elapsed());

    // 次の局面を送信
    generateAndSendNext();
}

void TsumeshogiGenerator::cleanup()
{
    m_safetyTimer.stop();
    m_progressTimer.stop();

    if (m_usi) {
        m_usi->cleanupEngineProcessAndThread();
        m_usi.reset();
    }
    m_playMode.reset();

    // トリミング状態をクリア
    m_trimBaseSfen.clear();
    m_trimBasePv.clear();
    m_trimCandidates.clear();
    m_trimCandidateIndex = 0;
    m_trimTestSfen.clear();
}

// ======================================================================
// トリミングフェーズ
// ======================================================================

void TsumeshogiGenerator::startTrimmingPhase(const QString& sfen, const QStringList& pv)
{
    m_phase = Phase::Trimming;
    m_trimBaseSfen = sfen;
    m_trimBasePv = pv;
    m_trimCandidates = enumerateRemovablePieces(sfen);
    m_trimCandidateIndex = 0;
    tryNextTrimCandidate();
}

void TsumeshogiGenerator::tryNextTrimCandidate()
{
    if (m_phase != Phase::Trimming) return;

    // 全候補を試し終わった → トリミング完了
    if (m_trimCandidateIndex >= m_trimCandidates.size()) {
        finishTrimmingPhase();
        return;
    }

    const TrimCandidate& candidate = m_trimCandidates.at(m_trimCandidateIndex);
    m_trimTestSfen = removePieceFromSfen(m_trimBaseSfen, candidate);
    sendTrimmingCheck(m_trimTestSfen);
}

void TsumeshogiGenerator::sendTrimmingCheck(const QString& modifiedSfen)
{
    QVector<QChar> dummyBoard(Usi::NUM_BOARD_SQUARES, QChar(' '));
    m_usi->setClonedBoardData(dummyBoard);

    QString positionStr = QStringLiteral("position sfen ") + modifiedSfen;
    m_usi->sendPositionAndGoMateCommands(m_settings.timeoutMs, positionStr);

    m_safetyTimer.start(m_settings.timeoutMs + kSafetyMarginMs);
}

void TsumeshogiGenerator::finishTrimmingPhase()
{
    // トリミング結果を出力
    m_currentSfen = m_trimBaseSfen;
    m_phase = Phase::Searching;
    processResult(true, m_trimBasePv);
}

// ======================================================================
// SFEN解析・再構築
// ======================================================================

TsumeshogiGenerator::ParsedSfen TsumeshogiGenerator::parseSfen(const QString& sfen) const
{
    ParsedSfen result;

    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 3) return result;

    // 盤面パース
    const QStringList ranks = parts[0].split(QLatin1Char('/'));
    if (ranks.size() != 9) return result;

    for (int r = 0; r < 9; ++r) {
        const QString& row = ranks[r];
        int c = 0;
        for (qsizetype i = 0; i < row.size() && c < 9; ++i) {
            const QChar ch = row.at(i);
            if (ch.isDigit()) {
                int n = ch.digitValue();
                while (n-- > 0 && c < 9) {
                    result.cells[r][c++].clear();
                }
            } else if (ch == QLatin1Char('+')) {
                if (i + 1 >= row.size()) break;
                const QChar p = row.at(++i);
                result.cells[r][c++] = QString(QLatin1Char('+')) + p;
            } else {
                result.cells[r][c++] = QString(ch);
            }
        }
        while (c < 9) result.cells[r][c++].clear();
    }

    // 持ち駒パース
    if (parts.size() >= 3) {
        const QString hands = parts[2];
        if (hands != QStringLiteral("-")) {
            int num = 0;
            for (QChar ch : hands) {
                if (ch.isDigit()) {
                    num = num * 10 + ch.digitValue();
                } else {
                    const bool isSente = ch.isUpper();
                    const int n = (num > 0) ? num : 1;
                    num = 0;
                    int idx = pieceCharToIndex(ch.toUpper());
                    if (idx >= 0) {
                        if (isSente) {
                            result.senteHand[idx] += n;
                        } else {
                            result.goteHand[idx] += n;
                        }
                    }
                }
            }
        }
    }

    return result;
}

QString TsumeshogiGenerator::buildSfenFromParsed(const ParsedSfen& parsed) const
{
    QString board;
    for (int r = 0; r < 9; ++r) {
        if (r > 0) board += QLatin1Char('/');
        int empty = 0;
        for (int c = 0; c < 9; ++c) {
            if (parsed.cells[r][c].isEmpty()) {
                empty++;
            } else {
                if (empty > 0) {
                    board += QString::number(empty);
                    empty = 0;
                }
                board += parsed.cells[r][c];
            }
        }
        if (empty > 0) {
            board += QString::number(empty);
        }
    }

    // 持ち駒構築
    QString hand;
    // 先手持駒（大文字）: R,B,G,S,N,L,P の順（SFEN慣例: 価値の高い順）
    static constexpr int kHandOrder[kPieceTypeCount] = {6, 5, 4, 3, 2, 1, 0};
    for (int o = 0; o < kPieceTypeCount; ++o) {
        int idx = kHandOrder[o];
        if (parsed.senteHand[idx] > 0) {
            if (parsed.senteHand[idx] > 1) {
                hand += QString::number(parsed.senteHand[idx]);
            }
            hand += indexToPieceChar(idx);
        }
    }
    // 後手持駒（小文字）
    for (int o = 0; o < kPieceTypeCount; ++o) {
        int idx = kHandOrder[o];
        if (parsed.goteHand[idx] > 0) {
            if (parsed.goteHand[idx] > 1) {
                hand += QString::number(parsed.goteHand[idx]);
            }
            hand += indexToPieceChar(idx).toLower();
        }
    }
    if (hand.isEmpty()) hand = QStringLiteral("-");

    // 「先手番 手数1」固定（詰将棋は常に攻方=先手番）
    return board + QStringLiteral(" b ") + hand + QStringLiteral(" 1");
}

// ======================================================================
// 駒種変換
// ======================================================================

int TsumeshogiGenerator::pieceCharToIndex(QChar upper)
{
    switch (upper.toLatin1()) {
    case 'P': return 0;
    case 'L': return 1;
    case 'N': return 2;
    case 'S': return 3;
    case 'G': return 4;
    case 'B': return 5;
    case 'R': return 6;
    default:  return -1;
    }
}

QChar TsumeshogiGenerator::indexToPieceChar(int idx)
{
    static constexpr char kChars[kPieceTypeCount] = {'P', 'L', 'N', 'S', 'G', 'B', 'R'};
    if (idx < 0 || idx >= kPieceTypeCount) return QChar();
    return QLatin1Char(kChars[idx]);
}

// ======================================================================
// トリミング操作
// ======================================================================

QVector<TsumeshogiGenerator::TrimCandidate>
TsumeshogiGenerator::enumerateRemovablePieces(const QString& sfen) const
{
    QVector<TrimCandidate> candidates;
    ParsedSfen parsed = parseSfen(sfen);

    // 盤上の駒を列挙（玉以外）
    for (int r = 0; r < 9; ++r) {
        for (int c = 0; c < 9; ++c) {
            const QString& cell = parsed.cells[r][c];
            if (cell.isEmpty()) continue;

            // 駒文字を取得（成駒の場合は '+' の次の文字）
            bool isPromoted = cell.startsWith(QLatin1Char('+'));
            QChar pieceChar = isPromoted ? cell.at(1) : cell.at(0);

            // 玉はスキップ（攻方玉は詰将棋では配置されないが念のため）
            QChar upper = pieceChar.toUpper();
            if (upper == QLatin1Char('K')) continue;

            // 守方持駒は除去対象外だが、盤上の守方駒は除去可
            TrimCandidate tc;
            tc.location = TrimCandidate::Board;
            tc.file = c;
            tc.rank = r;
            tc.pieceChar = pieceChar;
            tc.promoted = isPromoted;
            candidates.append(tc);
        }
    }

    // 攻方（先手）持駒を列挙
    for (int i = 0; i < kPieceTypeCount; ++i) {
        if (parsed.senteHand[i] > 0) {
            TrimCandidate tc;
            tc.location = TrimCandidate::Hand;
            tc.pieceChar = indexToPieceChar(i);
            candidates.append(tc);
        }
    }

    // 守方持駒は除去対象外（守方の防御資源を減らすのは不正）

    return candidates;
}

QString TsumeshogiGenerator::removePieceFromSfen(
    const QString& sfen, const TrimCandidate& candidate) const
{
    ParsedSfen parsed = parseSfen(sfen);

    // 除去する駒の生駒インデックス（持駒移動用）
    QChar upper = candidate.pieceChar.toUpper();
    int pieceIdx = pieceCharToIndex(upper);

    if (candidate.location == TrimCandidate::Board) {
        // 盤上の駒を除去
        parsed.cells[candidate.rank][candidate.file].clear();

        // 40駒保存則: 除去した駒は後手持駒に移動（生駒に戻す）
        if (pieceIdx >= 0) {
            parsed.goteHand[pieceIdx]++;
        }
    } else {
        // 攻方持駒から1枚除去 → 後手持駒に移動
        if (pieceIdx >= 0 && parsed.senteHand[pieceIdx] > 0) {
            parsed.senteHand[pieceIdx]--;
            parsed.goteHand[pieceIdx]++;
        }
    }

    return buildSfenFromParsed(parsed);
}
