/// @file tsumelinemetrics.cpp
/// @brief 詰将棋の局面と詰み手順の評価の実装

#include "tsumelinemetrics.h"

#include <position.h>
#include <types.h>

#include <QSet>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace TsumeLineMetrics {
namespace {

using shogi::Color;
using shogi::PieceType;

int countImmediateMatesInPos(const shogi::Position& position)
{
    if (position.side_to_move() != Color::Black || position.is_in_check(Color::White)) return 0;
    shogi::Position probe = position;
    std::vector<shogi::Move> checks;
    probe.generate_checking_moves(checks);
    int mates = 0;
    for (const auto& move : checks) {
        shogi::MoveUndo undo;
        probe.make_move(move, undo);
        const bool mated = !probe.has_legal_move();
        probe.unmake_move(move, undo);
        if (mated) ++mates;
    }
    return mates;
}

} // namespace

int countImmediateMates(const QString& sfen)
{
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return 0;
    return countImmediateMatesInPos(pos);
}

std::optional<Metrics> analyzeLine(const QString& sfen, const QStringList& pv)
{
    if (pv.isEmpty() || pv.size() % 2 == 0) return std::nullopt;
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return std::nullopt;
    if (pos.side_to_move() != Color::Black || pos.is_in_check(Color::White)) return std::nullopt;
    const int initialKing = pos.find_king(Color::White);
    if (initialKing < 0 || pos.find_king(Color::Black) >= 0) return std::nullopt;

    Metrics m;
    m.plies = static_cast<int>(pv.size());

    // 開始局面の駒数カウント
    for (int sq = 0; sq < shogi::kSquareCount; ++sq) {
        const int piece = pos.piece_at(sq);
        if (piece > 0) ++m.attackPieces;
        else if (piece < 0 && shogi::piece_type(piece) != PieceType::King) ++m.defendBoardPieces;
    }
    for (int kind = 1; kind <= 7; ++kind) {
        m.attackPieces += pos.hand_count(Color::Black, static_cast<PieceType>(kind));
    }

    QSet<int> kingSquares;
    kingSquares.insert(initialKing);
    int prevKing = initialKing;

    for (int i = 0; i < pv.size(); ++i) {
        if (i == pv.size() - 1) {
            m.finalImmediateMates = countImmediateMatesInPos(pos);
        }
        const QString& moveStr = pv.at(i);
        const bool isAttack = (i % 2 == 0);
        const bool isDrop = moveStr.size() >= 4 && moveStr[1] == QLatin1Char('*');
        const auto toSq = moveStr.size() >= 4 ? shogi::square_from_usi(moveStr.mid(2, 2).toStdString())
                                              : std::nullopt;
        const bool defenderCapturesAttacker = !isAttack && !isDrop && toSq && pos.piece_at(*toSq) > 0;

        if (!pos.apply_usi_move(moveStr.toStdString())) return std::nullopt;

        if (isAttack) {
            if (!pos.is_in_check(Color::White)) return std::nullopt;
        } else {
            if (pos.is_in_check(Color::White)) return std::nullopt;
            if (isDrop) ++m.interpositions;
            if (defenderCapturesAttacker) ++m.sacrifices;
            const int curKing = pos.find_king(Color::White);
            if (curKing != prevKing && curKing >= 0) {
                ++m.kingMoves;
                m.kingDistance += std::abs(shogi::square_row(curKing) - shogi::square_row(prevKing))
                    + std::abs(shogi::square_col(curKing) - shogi::square_col(prevKing));
                kingSquares.insert(curKing);
                prevKing = curKing;
            }
        }
    }

    if (pos.side_to_move() != Color::White || !pos.is_in_check(Color::White) || pos.has_legal_move()) {
        return std::nullopt;
    }

    m.distinctKingSquares = static_cast<int>(kingSquares.size());
    for (int kind = 1; kind <= 7; ++kind) {
        m.surplusCount += pos.hand_count(Color::Black, static_cast<PieceType>(kind));
    }
    return m;
}

double scoreNode(const Metrics& metrics, int targetMoves, int expansionCount,
                 bool firstMoveUnique, bool strictNoSurplus, bool strictFinalMove)
{
    // 1. 手数の進捗が最優先（目標に近づくほど高評価、超過も接尾辞切り出しで有用）
    const int cappedPlies = std::min(metrics.plies, targetMoves);
    double score = static_cast<double>(cappedPlies) * 14.0;

    // 2. 駒余りなしを強く優遇（厳密条件を満たす系統を育てる）
    if (metrics.surplusCount == 0) {
        score += strictNoSurplus ? 35.0 : 12.0;
    } else {
        score -= static_cast<double>(metrics.surplusCount) * (strictNoSurplus ? 22.0 : 6.0);
    }

    // 2b. 最終手一意を強く優遇（最終手余詰の系統を育てても厳密証明で棄却されるため）
    if (strictFinalMove) {
        if (metrics.finalImmediateMates == 1) score += 25.0;
        else if (metrics.finalImmediateMates > 1) score -= 55.0;
    }

    // 3. 初手一意ボーナス
    if (firstMoveUnique) score += 20.0;

    // 4. 長手数になりやすい構造特徴（案B：玉の広がり・捨駒・合駒）
    score += std::min(metrics.distinctKingSquares, 25) * 2.2;
    score += std::min(metrics.kingDistance, 60) * 0.6;
    score += std::min(metrics.sacrifices, 20) * 2.5;
    score += std::min(metrics.interpositions, 20) * 2.0;

    // 5. 盤上の過密ペナルティ（手数に対して駒が多すぎると余詰爆発・探索低速化しやすい）
    const int idealTotal = std::clamp(4 + metrics.plies / 3, 5, 24);
    const int actualTotal = metrics.attackPieces + metrics.defendBoardPieces;
    if (actualTotal > idealTotal) {
        score -= static_cast<double>(actualTotal - idealTotal) * 1.4;
    }

    // 6. 展開回数ペナルティ（行き詰まった枝に固執せず自動バックトラック）
    score -= std::log1p(static_cast<double>(std::max(0, expansionCount))) * 12.0;
    return score;
}

QString structuralSignature(const QString& sfen, const QStringList& pv)
{
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return sfen;
    const int kingSq = pos.find_king(Color::White);
    const int kr = kingSq >= 0 ? shogi::square_row(kingSq) : -1;
    const int kc = kingSq >= 0 ? shogi::square_col(kingSq) : -1;

    // 玉の領域（3x3ブロック）と先頭最大4手の座標・駒種で系統をまとめる
    QString sig = QStringLiteral("k%1%2|p%3|").arg(kr / 2).arg(kc / 2).arg(pv.size());
    const int prefix = std::min<int>(static_cast<int>(pv.size()), 4);
    for (int i = 0; i < prefix; ++i) {
        if (i > 0) sig += QLatin1Char(',');
        sig += pv.at(i);
    }
    return sig;
}

QStringList diversityKeys(const QString& sfen, const QStringList& pv)
{
    QStringList keys;
    const int n = static_cast<int>(pv.size());
    if (n < 1 || n % 2 == 0) return keys;
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return keys;
    const int initKing = pos.find_king(Color::White);
    if (initKing < 0) return keys;

    auto moveTokens = [](const QStringList& moves, int originFile, int originRank, int sign) -> QStringList {
        QStringList out;
        out.reserve(moves.size());
        for (const QString& m : moves) {
            if (m.size() < 4) return {};
            if (m[1] == QLatin1Char('*')) {
                const int df = sign * ((m[2].unicode() - '0') - originFile);
                const int dr = (m[3].unicode() - 'a') - originRank;
                out.append(QStringLiteral("D%1:%2,%3").arg(m[0]).arg(df).arg(dr));
            } else {
                const int ff = sign * ((m[0].unicode() - '0') - originFile);
                const int fr = (m[1].unicode() - 'a') - originRank;
                const int tf = sign * ((m[2].unicode() - '0') - originFile);
                const int tr = (m[3].unicode() - 'a') - originRank;
                const bool pro = m.endsWith(QLatin1Char('+'));
                out.append(QStringLiteral("M%1,%2>%3,%4%5").arg(ff).arg(fr).arg(tf).arg(tr).arg(pro ? 1 : 0));
            }
        }
        return out;
    };

    auto canonicalJoined = [&](const QStringList& moves, int originFile, int originRank) -> QString {
        const QString a = moveTokens(moves, originFile, originRank, 1).join(QLatin1Char(';'));
        const QString b = moveTokens(moves, originFile, originRank, -1).join(QLatin1Char(';'));
        return a < b ? a : b;
    };

    const int kf = 9 - shogi::square_col(initKing);
    const int kr = shogi::square_row(initKing);
    const QStringList prefixMoves = pv.mid(0, n - 1);
    keys.append(QStringLiteral("%1|P|").arg(n) + canonicalJoined(prefixMoves, kf, kr));
    keys.append(QStringLiteral("%1|A|").arg(n) + canonicalJoined(prefixMoves, 5, 0));

    if (n >= 9) {
        shogi::Position after2 = pos;
        if (after2.apply_usi_move(pv.at(0).toStdString()) && after2.apply_usi_move(pv.at(1).toStdString())) {
            const int k2 = after2.find_king(Color::White);
            if (k2 >= 0) {
                const int k2f = 9 - shogi::square_col(k2);
                const int k2r = shogi::square_row(k2);
                const QStringList coreMoves = pv.mid(2, n - 3);
                const QStringList t1 = moveTokens(coreMoves, k2f, k2r, 1);
                const QStringList t2 = moveTokens(coreMoves, k2f, k2r, -1);
                const int cLen = static_cast<int>(t1.size());
                if (cLen == t2.size()) {
                    for (int i = 0; i < cLen; ++i) {
                        QStringList m1 = t1;
                        QStringList m2 = t2;
                        m1[i] = QStringLiteral("*");
                        m2[i] = QStringLiteral("*");
                        const QString s1 = m1.join(QLatin1Char(';'));
                        const QString s2 = m2.join(QLatin1Char(';'));
                        keys.append(QStringLiteral("%1|C%2|").arg(n).arg(i) + (s1 < s2 ? s1 : s2));
                    }
                }
            }
        }
    }
    return keys;
}

} // namespace TsumeLineMetrics
