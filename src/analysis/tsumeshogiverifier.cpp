#include "tsumeshogiverifier.h"

#include <position.h>
#include <map>
#include <optional>
#include <utility>
#include <vector>

struct TsumeshogiVerifier::Impl {
    struct Node {
        shogi::Position position;
        QStringList path;
    };
    struct Frame {
        Node node;
        std::vector<shogi::Move> moves;
        std::size_t moveIndex = 0;
        int wins = 0;
        bool unresolved = false;
        std::vector<Node> continuations;
        QStringList terminal;
    };

    Result result;
    shogi::Color attacker = shogi::Color::Black;
    int target = 0;
    int visited = 0;
    std::vector<Node> nodes;
    std::optional<Frame> frame;
    bool moveActive = false;
    bool moveUnresolved = false;
    std::vector<Node> defenses;
    std::size_t defenseIndex = 0;
    QStringList movePath;
    std::optional<shogi::Position> request;
    // 完全なSFENをキーにする。ハッシュ衝突や異なる持駒の結果を流用しない。
    std::map<std::string, Reply> cache;

    void finishMove(bool mate, bool unresolved = false)
    {
        frame->unresolved |= unresolved;
        if (mate) {
            if (++frame->wins > 1) {
                result.status = Status::Multiple;
            } else {
                frame->continuations = defenses;
                frame->terminal = defenses.empty() ? movePath : QStringList{};
            }
        }
        ++frame->moveIndex;
        moveActive = false;
    }

    void consume(Reply reply)
    {
        if (reply == Reply::NoMate) {
            // 玉方に1つでも逃れがあれば、この攻手は手数に関係なく不詰。
            finishMove(false);
        } else {
            moveUnresolved |= reply == Reply::Unknown;
            ++defenseIndex;
        }
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

    void finishFrame()
    {
        if (frame->unresolved) {
            result.status = Status::Unknown;
        } else if (frame->wins == 0) {
            result.status = Status::NoMate;
        } else if (!frame->terminal.isEmpty()) {
            if (frame->terminal.size() > result.pv.size()) result.pv = frame->terminal;
        } else {
            for (auto& child : frame->continuations) {
                if (child.path.size() >= target) {
                    result.status = Status::WrongLength;
                    break;
                }
                nodes.push_back(std::move(child));
            }
        }
        frame.reset();
    }
};

TsumeshogiVerifier::TsumeshogiVerifier() : m_impl(std::make_unique<Impl>()) {}
TsumeshogiVerifier::~TsumeshogiVerifier() = default;

void TsumeshogiVerifier::start(const QString& sfen, int targetMoves)
{
    m_impl = std::make_unique<Impl>();
    auto& state = *m_impl;
    shogi::Position position;
    if (targetMoves < 1 || targetMoves > 99 || targetMoves % 2 != 1
        || !position.set_sfen(sfen.toStdString(), true)) return;
    state.attacker = position.side_to_move();
    const auto defender = shogi::opposite(state.attacker);
    if (position.find_king(defender) < 0 || position.is_in_check(defender)) return;
    state.target = targetMoves;
    state.nodes.push_back({position, {}});
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
                state.defenses.push_back({std::move(child), std::move(path)});
            }
            state.defenseIndex = 0;
            state.moveUnresolved = false;
            state.moveActive = true;
        }
        if (state.defenseIndex == state.defenses.size()) {
            state.finishMove(!state.moveUnresolved, state.moveUnresolved);
            continue;
        }
        const auto& child = state.defenses[state.defenseIndex].position;
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
    state.cache.emplace(state.request->to_sfen(), reply);
    state.request.reset();
    state.consume(reply);
}

void TsumeshogiVerifier::abort()
{
    if (m_impl->result.status == Status::Running) m_impl->result.status = Status::Unknown;
}

const TsumeshogiVerifier::Result& TsumeshogiVerifier::result() const
{
    return m_impl->result;
}
