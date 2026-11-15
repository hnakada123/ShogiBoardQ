//! 対局ストリームを、相手とのどの接続の上で開くか（つなぐ側。protocol/ipc.md 5章）
//!
//! 対局ストリームは、開いた接続の上に乗ったまま動かせない。中継でつながった直後に開くと、その後に直接の接続
//! （穴あけ）ができても、指し手は協力ノードを通り続ける（画面には「直接」と出るのに、使われない直接の接続は
//! しばらくして閉じる）。そこで、つなぐ側は次の順で開く:
//!
//! - 直接の接続があれば、中継経由の接続を閉じてから開く（libp2p-stream は、相手との接続のどれかを無作為に選んで
//!   ストリームを開くので、直接の接続だけを残す）
//! - 中継経由の接続しかなければ、直接の接続ができるのを少し待つ（自分が穴あけを使わないとき・穴あけを
//!   あきらめたときは待たない）。待っても直接の接続ができなければ、中継経由の接続の上で開く。相手が穴あけを
//!   使うかは前もって分からない（穴あけは中継経由の接続を受けた側が始め、受けた側は Identify で名乗らない）ので、
//!   穴あけを使わない相手（「直接接続を使わない」）とも、同じ時間だけ待つ
//!
//! 中継経由の接続の上で開いた後にできた直接の接続は、使わないので閉じる（node/events）。次につなぎ直すときに、
//! もう一度直接の接続を試す。

use std::time::Duration;

use libp2p::PeerId;
use tokio::sync::oneshot;

/// 中継でつながった相手との直接の接続（穴あけ）を待つ時間。対局・UI側は、切れた対局の相手へ5秒ごとに
/// つなぎ直す（P2pGameCoordinator）。前のつなぎ直しが開く前に次を始めないよう、それより短くする
pub const HOLE_PUNCH_WAIT: Duration = Duration::from_secs(3);

/// 対局ストリームを開くのを待っている接続
pub struct Opening {
    pub peer: PeerId,
    /// 開くタスクへの知らせ（true で開く、false でやめる）
    pub go: oneshot::Sender<bool>,
    /// 直接の接続を待たない（穴あけをあきらめた）
    pub no_hole_punch: bool,
}

/// 相手との接続の様子
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Links {
    /// 直接の接続の数
    pub direct: usize,
    /// 中継経由の接続の数
    pub relayed: usize,
    /// 中継経由の接続ができてからの時間（いちばん古いもの）
    pub relayed_for: Duration,
    /// 直接の接続（穴あけ）を試している
    pub hole_punching: bool,
}

/// 次にすること
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Step {
    /// まだ開かない
    Wait,
    /// 中継経由の接続を閉じる（閉じ終わってから、直接の接続の上で開く）
    CloseRelayed,
    /// 直接の接続の上で開く
    OpenDirect,
    /// 中継経由の接続の上で開く
    OpenRelayed,
}

pub fn next_step(links: Links) -> Step {
    if links.direct > 0 {
        return if links.relayed > 0 {
            Step::CloseRelayed
        } else {
            Step::OpenDirect
        };
    }
    if links.relayed == 0 {
        // まだつながっていない（接続の結果を待つ）
        return Step::Wait;
    }
    if links.hole_punching && links.relayed_for < HOLE_PUNCH_WAIT {
        Step::Wait
    } else {
        Step::OpenRelayed
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn links(direct: usize, relayed: usize, relayed_for_ms: u64, hole_punching: bool) -> Links {
        Links {
            direct,
            relayed,
            relayed_for: Duration::from_millis(relayed_for_ms),
            hole_punching,
        }
    }

    #[test]
    fn waits_for_a_connection() {
        assert_eq!(next_step(links(0, 0, 0, true)), Step::Wait);
        assert_eq!(next_step(links(0, 0, 0, false)), Step::Wait);
    }

    #[test]
    fn opens_on_the_direct_connection_alone() {
        assert_eq!(next_step(links(1, 0, 0, true)), Step::OpenDirect);
        // 中継経由の接続が残っていれば、先に閉じる（穴あけを待つ時間の内でも後でも）
        assert_eq!(next_step(links(1, 1, 100, true)), Step::CloseRelayed);
        assert_eq!(next_step(links(1, 2, 60_000, false)), Step::CloseRelayed);
    }

    #[test]
    fn waits_for_hole_punching_only_for_a_while() {
        let limit = u64::try_from(HOLE_PUNCH_WAIT.as_millis()).unwrap_or(u64::MAX);
        assert_eq!(next_step(links(0, 1, 0, true)), Step::Wait);
        assert_eq!(next_step(links(0, 1, limit.saturating_sub(1), true)), Step::Wait);
        assert_eq!(next_step(links(0, 1, limit, true)), Step::OpenRelayed);
        // 穴あけを使わない・あきらめたときは待たない
        assert_eq!(next_step(links(0, 1, 0, false)), Step::OpenRelayed);
    }
}
