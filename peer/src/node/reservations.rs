//! 協力ノードの中継の予約（予約を頼んだ協力ノード・取れた予約・取り直す時刻）
//!
//! 予約は、協力ノードが自分のアドレスを知らないうち（起動した直後。到達性の確認がまだ済んでいない）は
//! 断られ、協力ノードとの接続が切れたときにも失われる。どちらも中継の待受が閉じるので、少し待ってから
//! 頼み直す（頼み直さないと、つなぎ直すまで招待に中継経由の宛先が入らない）。

use std::collections::{HashMap, HashSet};
use std::time::{Duration, Instant};

use libp2p::core::transport::ListenerId;
use libp2p::PeerId;

/// 中継の予約の数
pub const MAX_RESERVATIONS: usize = 2;
/// 予約が取れなかった・失われたときに、頼み直すまでの時間
pub const RETRY_AFTER: Duration = Duration::from_secs(30);
/// 覚える中継のできる協力ノードの上限（指定できる協力ノードの数）
const MAX_HOP_NODES: usize = crate::ipc::MAX_BOOTSTRAP;

#[derive(Default)]
pub struct Reservations {
    /// 中継のできる協力ノード（利用者が指定し、Identify で中継の hop を名乗ったノード）
    hop_nodes: HashSet<PeerId>,
    /// 予約を頼んでいる協力ノードと、その中継の待受
    requested: HashMap<PeerId, ListenerId>,
    /// 予約が取れた協力ノード
    accepted: HashSet<PeerId>,
    /// 頼み直す時刻（予約が取れなかった・失われた協力ノード）
    retry_at: HashMap<PeerId, Instant>,
}

impl Reservations {
    /// 中継のできる協力ノードとして覚える
    pub fn add_hop_node(&mut self, peer: PeerId) {
        if self.hop_nodes.len() < MAX_HOP_NODES || self.hop_nodes.contains(&peer) {
            self.hop_nodes.insert(peer);
        }
    }

    /// 予約を頼んでよいか（中継のできる協力ノードで、まだ頼んでおらず、数の上限の内で、頼み直す時刻を過ぎた）
    pub fn may_request(&self, peer: &PeerId, now: Instant) -> bool {
        let retry_due = match self.retry_at.get(peer) {
            Some(at) => now >= *at,
            None => true,
        };
        self.hop_nodes.contains(peer)
            && !self.requested.contains_key(peer)
            && self.requested.len() < MAX_RESERVATIONS
            && retry_due
    }

    /// 予約を頼んだ（中継の待受を始めた）
    pub fn requested(&mut self, peer: PeerId, listener: ListenerId) {
        self.requested.insert(peer, listener);
        self.retry_at.remove(&peer);
    }

    /// 予約が取れた
    pub fn accepted(&mut self, peer: PeerId) {
        if self.requested.contains_key(&peer) {
            self.accepted.insert(peer);
        }
    }

    /// 待受が閉じた。中継の待受なら（予約が取れなかった・協力ノードとの接続が切れた）、頼み直す時刻を決めて true
    pub fn closed(&mut self, listener: ListenerId, now: Instant) -> bool {
        let Some(peer) = self
            .requested
            .iter()
            .find(|(_, id)| **id == listener)
            .map(|(peer, _)| *peer)
        else {
            return false;
        };
        self.requested.remove(&peer);
        self.accepted.remove(&peer);
        self.retry_at
            .insert(peer, now.checked_add(RETRY_AFTER).unwrap_or(now));
        true
    }

    /// 頼み直す時刻を過ぎた協力ノード
    pub fn due(&self, now: Instant) -> Vec<PeerId> {
        self.retry_at
            .keys()
            .filter(|peer| self.may_request(peer, now))
            .copied()
            .collect()
    }

    /// 取れている予約の数
    pub fn accepted_count(&self) -> usize {
        self.accepted.len()
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::arithmetic_side_effects, clippy::indexing_slicing)]
mod tests {
    use super::*;

    /// 頼んだ予約が断られる（協力ノードが起動した直後）か、協力ノードとの接続が切れて中継の待受が閉じたら、
    /// 少し待ってから頼み直す。待つ間は頼み直さない
    #[test]
    fn refused_or_lost_reservation_is_requested_again_later() {
        let node = PeerId::random();
        let listener = ListenerId::next();
        let now = Instant::now();
        let mut r = Reservations::default();
        // 中継のできる協力ノードでなければ頼まない
        assert!(!r.may_request(&node, now));
        r.add_hop_node(node);
        assert!(r.may_request(&node, now));
        r.requested(node, listener);
        assert!(!r.may_request(&node, now));
        // ほかの待受が閉じても関係ない
        assert!(!r.closed(ListenerId::next(), now));
        // 断られた（中継の待受が閉じた）
        assert!(r.closed(listener, now));
        assert_eq!(r.accepted_count(), 0);
        assert!(!r.may_request(&node, now));
        assert!(r.due(now + RETRY_AFTER - Duration::from_secs(1)).is_empty());
        assert_eq!(r.due(now + RETRY_AFTER), vec![node]);
        // 頼み直したら、待つ時刻は消える
        let again = ListenerId::next();
        r.requested(node, again);
        r.accepted(node);
        assert_eq!(r.accepted_count(), 1);
        assert!(r.due(now + RETRY_AFTER * 10).is_empty());
        // 取れていた予約が失われても（接続が切れた）、同じく頼み直す
        assert!(r.closed(again, now));
        assert_eq!(r.accepted_count(), 0);
        assert_eq!(r.due(now + RETRY_AFTER), vec![node]);
    }

    /// 予約は2つまで。頼んでいる数が上限なら、ほかの協力ノードには頼まない
    #[test]
    fn at_most_two_reservations() {
        let now = Instant::now();
        let mut r = Reservations::default();
        let nodes: Vec<PeerId> = (0..3).map(|_| PeerId::random()).collect();
        for node in &nodes {
            r.add_hop_node(*node);
        }
        r.requested(nodes[0], ListenerId::next());
        r.requested(nodes[1], ListenerId::next());
        assert!(!r.may_request(&nodes[2], now));
    }

    /// 取れたと知らされても、頼んでいない協力ノードは数えない
    #[test]
    fn acceptance_without_request_is_ignored() {
        let mut r = Reservations::default();
        r.accepted(PeerId::random());
        assert_eq!(r.accepted_count(), 0);
    }
}
