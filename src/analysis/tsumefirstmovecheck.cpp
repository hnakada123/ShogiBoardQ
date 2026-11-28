/// @file tsumefirstmovecheck.cpp
/// @brief 成長中の候補局面の、軽い初手余詰の検査の実装

#include "tsumefirstmovecheck.h"

#include "tsumelocalscreen.h"

#include <position.h>

#include <algorithm>
#include <utility>
#include <vector>

TsumeFirstMoveCheck::TsumeFirstMoveCheck(const QString& sfen, const QStringList& pv,
                                         const TsumeLineMetrics::Metrics& metrics, bool fromRepair)
    : m_sfen(sfen)
    , m_mainPv(pv)
    , m_metrics(metrics)
    , m_fromRepair(fromRepair)
{
}

std::optional<TsumeFirstMoveCheck> TsumeFirstMoveCheck::begin(const QString& sfen, const QStringList& pv,
                                                              const TsumeLineMetrics::Metrics& metrics,
                                                              bool fromRepair)
{
    TsumeFirstMoveCheck check(sfen, pv, metrics, fromRepair);

    // 63手以下ならまずローカル Hayanagi で瞬時に明白な初手余詰を検出
    const QString quickCook = TsumeLocalScreen::quickCookCheck(sfen, pv.first(), metrics.plies,
                                                               metrics.plies <= 15 ? 6 : 4);
    if (!quickCook.isEmpty()) {
        check.m_verdict = Verdict{false, quickCook};
        return check;
    }

    // 15手以下の成長足場はローカル Hayanagi の初手余詰検査だけで十分（外部エンジン問い合わせを温存）
    if (metrics.plies <= 15) {
        check.m_verdict = Verdict{true, QString()};
        return check;
    }

    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return std::nullopt;

    check.m_maxQueries = std::clamp(24 + metrics.plies / 2, 24, 80);
    for (const auto& move : pos.generate_checking_moves()) {
        const QString usi = QString::fromStdString(pos.move_to_usi(move));
        if (usi == pv.first()) continue;
        shogi::Position child = pos;
        child.do_move(move);
        if (!child.has_legal_move()) {
            // 別の即詰がある＝即座に初手余詰確定
            check.m_verdict = Verdict{false, usi};
            return check;
        }
        check.m_altMoves.append(usi);
    }
    return check;
}

bool TsumeFirstMoveCheck::expandDefenses()
{
    shogi::Position pos;
    if (!pos.set_sfen(m_sfen.toStdString(), true)
        || !pos.apply_usi_move(m_altMoves.at(m_altMoveIndex).toStdString())) {
        return false;
    }
    const auto legals = pos.generate_legal_moves();
    std::vector<std::pair<int, QString>> ordered;
    ordered.reserve(legals.size());
    for (const auto& def : legals) {
        shogi::Position after = pos;
        after.do_move(def);
        // 1手詰の応手は反駁にならないので外部に聞く必要がない
        if (TsumeLocalScreen::hasImmediateMate(after)) continue;
        ordered.push_back({TsumeLocalScreen::defensePriority(pos, def), QString::fromStdString(after.to_sfen())});
    }
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& item : ordered) m_defenses.append(item.second);
    return true;
}

std::optional<QString> TsumeFirstMoveCheck::nextQuery(const CacheLookup& cached)
{
    while (!m_verdict) {
        if (m_altMoveIndex >= m_altMoves.size()) {
            m_verdict = Verdict{!m_altUnresolved, QString()};
            break;
        }

        // 現在の別攻手の応手リストがまだ無ければ展開する
        if (m_defenses.isEmpty() && m_defenseIndex == 0) {
            if (!expandDefenses()) {
                ++m_altMoveIndex;
                continue;
            }
            if (m_defenses.isEmpty()) {
                // 全応手が1手詰＝確実に詰む別初手（初手余詰！）
                m_verdict = Verdict{false, m_altMoves.at(m_altMoveIndex)};
                break;
            }
        }

        if (m_defenseIndex >= m_defenses.size()) {
            if (!m_altUnresolved) {
                // 全応手が詰むことが確認できた＝初手余詰確定
                m_verdict = Verdict{false, m_altMoves.at(m_altMoveIndex)};
                break;
            }
            // 未解決が残ったので次へ
            m_defenses.clear();
            m_defenseIndex = 0;
            ++m_altMoveIndex;
            continue;
        }

        const QString defSfen = m_defenses.at(m_defenseIndex);
        if (const MateAnswer* answer = cached(defSfen)) {
            submit(*answer);
            continue;
        }

        if (m_queriesUsed >= m_maxQueries) {
            // 成長段階の軽量チェック上限に達したら未確定としてビームに委ねる
            m_verdict = Verdict{false, QString()};
            break;
        }

        ++m_queriesUsed;
        return defSfen;
    }
    return std::nullopt;
}

void TsumeFirstMoveCheck::submit(const MateAnswer& answer)
{
    if (answer.kind == MateAnswer::Kind::NoMate) {
        // この別攻手には逃れがある（不詰確定）→ 次の別攻手へ
        m_defenses.clear();
        m_defenseIndex = 0;
        ++m_altMoveIndex;
        return;
    }
    if (answer.kind == MateAnswer::Kind::Unknown) {
        m_altUnresolved = true;
    }
    ++m_defenseIndex;
}
