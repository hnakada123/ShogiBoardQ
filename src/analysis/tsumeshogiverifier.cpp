#include "tsumeshogiverifier.h"

#include <position.h>
#include <algorithm>
#include <map>
#include <optional>
#include <utility>
#include <vector>

struct TsumeshogiVerifier::Impl {
    struct Node {
        shogi::Position position;
        QStringList path;   ///< 根からの手順
        int remaining = 0;  ///< この局面から詰み上がりまでの手数（奇数）
    };
    /// 攻手に対する玉方の応手後の局面
    struct Child {
        Node node;
        int len = 0;        ///< エンジンが証明した応手後の詰み手数（奇数）
    };
    /// 全応手が詰む攻手
    struct Winner {
        QString usi;
        std::vector<Child> children; ///< 空なら即詰
    };
    struct Frame {
        Node node;
        std::vector<shogi::Move> moves;
        std::size_t moveIndex = 0;
        std::vector<Winner> winners;
        bool unresolved = false;
    };
    struct CachedReply {
        Reply reply = Reply::Unknown;
        int len = 0;
    };

    Result result;
    Options options;
    shogi::Color attacker = shogi::Color::Black;
    int target = 0;
    int visited = 0;
    std::vector<Node> nodes;
    std::optional<Frame> frame;
    bool moveActive = false;
    bool moveUnresolved = false;
    std::vector<Child> defenses;
    std::size_t defenseIndex = 0;
    QStringList movePath;
    std::optional<shogi::Position> request;
    // 完全なSFENをキーにする。ハッシュ衝突や異なる持駒の結果を流用しない。
    std::map<std::string, CachedReply> cache;

    void finishMove(bool mate, bool unresolved)
    {
        frame->unresolved |= unresolved;
        if (mate) {
            // 成・不成の違いも別の攻手として数える
            frame->winners.push_back({movePath.last(), std::move(defenses)});
            if (frame->winners.size() > 1) result.status = Status::Multiple;
        }
        defenses.clear();
        ++frame->moveIndex;
        moveActive = false;
    }

    void consume(const CachedReply& reply)
    {
        if (reply.reply == Reply::NoMate) {
            // 玉方に1つでも逃れがあれば、この攻手は手数に関係なく不詰。
            finishMove(false, false);
            return;
        }
        if (reply.reply == Reply::Unknown) {
            moveUnresolved = true;
        } else {
            defenses[defenseIndex].len = reply.len;
        }
        ++defenseIndex;
    }

    bool validPv(const QStringList& pv) const
    {
        if (!request || pv.isEmpty() || pv.size() % 2 != 1) return false;
        auto position = *request;
        for (const QString& text : pv) {
            const bool attack = position.side_to_move() == attacker;
            if (!position.apply_usi_move(text.toStdString())) return false;
            if (attack && !position.is_in_check(position.side_to_move())) return false;
        }
        return position.side_to_move() != attacker
            && position.is_in_check(position.side_to_move())
            && position.generate_legal_moves().empty();
    }

    /// 即詰の攻手を1つ返す（最終手の複数解を許容した終端の手順表示用）
    static std::optional<std::string> immediateMate(const shogi::Position& position)
    {
        for (const auto& move : position.generate_checking_moves()) {
            auto next = position;
            next.do_move(move);
            if (next.generate_legal_moves().empty()) return position.move_to_usi(move);
        }
        return std::nullopt;
    }

    void recordPv(const QStringList& pv)
    {
        if (result.pv.isEmpty()) result.pv = pv;
    }

    void finishFrame()
    {
        Frame& current = *frame;
        if (current.unresolved) {
            result.status = Status::Unknown;
        } else if (current.winners.empty()) {
            result.status = Status::NoMate;
        } else {
            finishUniqueFrame(current);
        }
        frame.reset();
    }

    void finishUniqueFrame(Frame& current)
    {
        Winner& winner = current.winners.front();
        int longest = -1;
        for (const auto& child : winner.children) longest = std::max(longest, child.len);
        const int total = winner.children.empty() ? 1 : 2 + longest;
        if (total != current.node.remaining) {
            // 根なら目標手数と不一致。途中ならエンジンの手数と木の手数が食い違う（判定不能）
            result.status = current.node.path.isEmpty() ? Status::WrongLength : Status::Unknown;
            return;
        }
        if (winner.children.empty()) {
            QStringList pv = current.node.path;
            pv.append(winner.usi);
            recordPv(pv);
            return;
        }
        // 最長抵抗の応手だけを主手順として検査する（同手数なら全部）。
        // 早く詰む変化での攻方の別の詰め方（変化別詰）は検査しない。
        std::vector<Node> mainLines;
        for (auto& child : winner.children) {
            if (child.len != longest) continue;
            if (longest == 1 && options.allowFinalMoveAlternatives) {
                if (!result.pv.isEmpty()) continue;
                const auto mate = immediateMate(child.node.position);
                if (!mate) {
                    result.status = Status::Unknown;
                    return;
                }
                QStringList pv = child.node.path;
                pv.append(QString::fromStdString(*mate));
                recordPv(pv);
                continue;
            }
            child.node.remaining = longest;
            mainLines.push_back(std::move(child.node));
        }
        // 生成順で先頭の変化を先に調べるため逆順に積む
        for (auto it = mainLines.rbegin(); it != mainLines.rend(); ++it) nodes.push_back(std::move(*it));
    }
};

TsumeshogiVerifier::TsumeshogiVerifier() : m_impl(std::make_unique<Impl>()) {}
TsumeshogiVerifier::~TsumeshogiVerifier() = default;

void TsumeshogiVerifier::start(const QString& sfen, int targetMoves, const Options& options)
{
    m_impl = std::make_unique<Impl>();
    auto& state = *m_impl;
    state.options = options;
    shogi::Position position;
    if (targetMoves < 1 || targetMoves > 99 || targetMoves % 2 != 1
        || !position.set_sfen(sfen.toStdString(), true)) return;
    state.attacker = position.side_to_move();
    const auto defender = shogi::opposite(state.attacker);
    if (position.find_king(defender) < 0 || position.is_in_check(defender)) return;
    state.target = targetMoves;
    state.nodes.push_back({position, {}, targetMoves});
    state.result.status = Status::Running;
}

QString TsumeshogiVerifier::nextPosition()
{
    auto& state = *m_impl;
    // キャッシュだけで進む場合もUIイベントループへ定期的に制御を戻す。
    int steps = 0;
    while (state.result.status == Status::Running && ++steps <= 64) {
        if (state.request) return QString::fromStdString(state.request->to_sfen());
        if (!state.frame) {
            if (state.nodes.empty()) {
                state.result.status = state.result.pv.size() == state.target
                    ? Status::Unique : Status::WrongLength;
                break;
            }
            // 資源上限は「判定不能」で終了する。唯一性の証明には使わない。
            if (++state.visited > 10000) {
                state.result.status = Status::Unknown;
                break;
            }
            Impl::Frame frame;
            frame.node = std::move(state.nodes.back());
            state.nodes.pop_back();
            frame.moves = frame.node.position.generate_checking_moves();
            state.frame = std::move(frame);
        }
        auto& frame = *state.frame;
        if (frame.moveIndex == frame.moves.size()) {
            state.finishFrame();
            continue;
        }
        if (!state.moveActive) {
            auto position = frame.node.position;
            const auto move = frame.moves[frame.moveIndex];
            state.movePath = frame.node.path;
            state.movePath.append(QString::fromStdString(position.move_to_usi(move)));
            position.do_move(move);
            state.defenses.clear();
            for (const auto& defense : position.generate_legal_moves()) {
                auto child = position;
                auto path = state.movePath;
                path.append(QString::fromStdString(position.move_to_usi(defense)));
                child.do_move(defense);
                state.defenses.push_back({{std::move(child), std::move(path), 0}, 0});
            }
            state.defenseIndex = 0;
            state.moveUnresolved = false;
            state.moveActive = true;
        }
        if (state.defenseIndex == state.defenses.size()) {
            state.finishMove(!state.moveUnresolved, state.moveUnresolved);
            continue;
        }
        const auto& child = state.defenses[state.defenseIndex].node.position;
        const auto found = state.cache.find(child.to_sfen());
        if (found != state.cache.end()) {
            state.consume(found->second);
        } else {
            if (state.cache.size() >= 10000) {
                state.result.status = Status::Unknown;
                break;
            }
            state.request = child;
        }
    }
    return {};
}

void TsumeshogiVerifier::submit(Reply reply, const QStringList& pv)
{
    auto& state = *m_impl;
    if (state.result.status != Status::Running || !state.request) return;
    if (reply == Reply::Mate && !state.validPv(pv)) reply = Reply::Unknown;
    const Impl::CachedReply cached{reply, reply == Reply::Mate ? static_cast<int>(pv.size()) : 0};
    state.cache.emplace(state.request->to_sfen(), cached);
    state.request.reset();
    state.consume(cached);
}

void TsumeshogiVerifier::abort()
{
    if (m_impl->result.status == Status::Running) m_impl->result.status = Status::Unknown;
}

const TsumeshogiVerifier::Result& TsumeshogiVerifier::result() const
{
    return m_impl->result;
}
