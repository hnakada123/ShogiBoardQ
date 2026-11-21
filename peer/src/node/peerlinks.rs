//! 相手との libp2p の接続（相手ごとの接続と中継経由か・接続ができた時刻・受け付けた接続の接続元のアドレス帯）
//!
//! 数えるのは、確立して受け付けた接続だけ。確立した直後に閉じた接続（接続元のアドレス帯の上限・中継の上で
//! 対局ストリームを開いた後の直接の接続）は数えず、その接続が閉じた知らせでは、相手との接続がなくなったと
//! みなさない（同じ相手へ開く途中の対局ストリームを失敗にしない。監査 2026-11-21 U2）。

use std::collections::HashMap;
use std::time::{Duration, Instant};

use libp2p::swarm::ConnectionId;
use libp2p::PeerId;

use super::outbound::{self, Links};
use crate::ipc::Route;

/// 同じアドレス帯（ローカルでは同じ IP）から受け付ける接続の上限
pub const MAX_INBOUND_PER_PREFIX: usize = 8;

/// 相手との接続
#[derive(Default)]
pub struct PeerLinks {
    /// 相手ごとの接続と、中継経由か
    by_peer: HashMap<PeerId, HashMap<ConnectionId, bool>>,
    /// 接続ができた時刻（中継経由の接続で、直接の接続を待つ時間を測る）
    since: HashMap<ConnectionId, Instant>,
    /// 受け付けた接続の接続元のアドレス帯
    inbound_prefixes: HashMap<ConnectionId, Vec<u8>>,
}

impl PeerLinks {
    /// 届いた直接の接続を受け付けるか。同じアドレス帯（key）から受け付けた接続が多すぎなければ数に入れる。
    /// アドレス帯の分からない接続は受け付ける
    pub fn admit_inbound(&mut self, id: ConnectionId, key: Option<Vec<u8>>) -> bool {
        let Some(prefix) = key else {
            return true;
        };
        let count = self.inbound_prefixes.values().filter(|p| **p == prefix).count();
        if count >= MAX_INBOUND_PER_PREFIX {
            return false;
        }
        self.inbound_prefixes.insert(id, prefix);
        true
    }

    /// 受け付けた接続の接続元のアドレス帯
    pub fn source_of(&self, id: ConnectionId) -> Option<&Vec<u8>> {
        self.inbound_prefixes.get(&id)
    }

    /// 接続を受け付けた（数に入れる）
    pub fn established(&mut self, peer: PeerId, id: ConnectionId, relayed: bool, now: Instant) {
        self.by_peer.entry(peer).or_default().insert(id, relayed);
        self.since.insert(id, now);
    }

    /// 接続が閉じた。数に入れていた接続なら、相手との接続がもう無いか（Some(true) で無い）。数に入れていなかった
    /// 接続（受け付けずに閉じた接続）は None
    pub fn closed(&mut self, peer: &PeerId, id: ConnectionId) -> Option<bool> {
        self.inbound_prefixes.remove(&id);
        self.since.remove(&id);
        let links = self.by_peer.get_mut(peer)?;
        links.remove(&id)?;
        if !links.is_empty() {
            return Some(false);
        }
        self.by_peer.remove(peer);
        Some(true)
    }

    /// 相手と接続しているか
    pub fn contains(&self, peer: &PeerId) -> bool {
        self.by_peer.contains_key(peer)
    }

    /// 接続している相手の数
    pub fn peer_count(&self) -> usize {
        self.by_peer.len()
    }

    /// 相手との接続から見た経路（1つでも直接の接続があれば直接。接続が無ければ直接）。届いた対局ストリームは、
    /// どの接続の上にあるか分からないので、これで決める
    pub fn route_of(&self, peer: &PeerId) -> Route {
        match self.by_peer.get(peer) {
            Some(links) if links.values().any(|relayed| !relayed) => Route::Direct,
            Some(links) if !links.is_empty() => Route::Relay,
            _ => Route::Direct,
        }
    }

    /// 相手との中継経由の接続
    pub fn relayed_ids(&self, peer: &PeerId) -> Vec<ConnectionId> {
        self.by_peer
            .get(peer)
            .into_iter()
            .flatten()
            .filter(|(_, relayed)| **relayed)
            .map(|(id, _)| *id)
            .collect()
    }

    /// 相手との接続の様子（対局ストリームを開く前に見る。hole_punching は直接の接続（穴あけ）を待つか）
    pub fn summary(&self, peer: &PeerId, hole_punching: bool, now: Instant) -> Links {
        let mut links = Links {
            direct: 0,
            relayed: 0,
            relayed_for: Duration::ZERO,
            hole_punching,
        };
        for (id, relayed) in self.by_peer.get(peer).into_iter().flatten() {
            if *relayed {
                links.relayed = links.relayed.saturating_add(1);
                // 時刻を覚えていない接続は、待ち終えたものとして扱う
                let age = self
                    .since
                    .get(id)
                    .map_or(outbound::HOLE_PUNCH_WAIT, |since| now.saturating_duration_since(*since));
                links.relayed_for = links.relayed_for.max(age);
            } else {
                links.direct = links.direct.saturating_add(1);
            }
        }
        links
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn id(n: usize) -> ConnectionId {
        ConnectionId::new_unchecked(n)
    }

    /// 1つでも直接の接続があれば直接、中継経由だけなら中継。接続が無ければ直接
    #[test]
    fn route_follows_the_links() {
        let now = Instant::now();
        let peer = PeerId::random();
        let mut links = PeerLinks::default();
        assert_eq!(links.route_of(&peer), Route::Direct);
        links.established(peer, id(1), true, now);
        assert_eq!(links.route_of(&peer), Route::Relay);
        assert_eq!(links.relayed_ids(&peer), vec![id(1)]);
        links.established(peer, id(2), false, now);
        assert_eq!(links.route_of(&peer), Route::Direct);
        assert_eq!(links.peer_count(), 1);
        assert_eq!(links.closed(&peer, id(2)), Some(false));
        assert_eq!(links.route_of(&peer), Route::Relay);
        assert_eq!(links.closed(&peer, id(1)), Some(true));
        assert!(!links.contains(&peer));
        assert_eq!(links.peer_count(), 0);
    }

    /// 確立した直後に閉じた（数に入れなかった）接続の知らせでは、相手との接続がなくなったとみなさない。
    /// 相手へ接続している途中に、同じ相手から届いた接続を受け付けずに閉じても、開く途中の対局ストリームを
    /// 失敗にしない（監査 2026-11-21 U2）
    #[test]
    fn closing_an_untracked_connection_does_not_drop_the_peer() {
        let now = Instant::now();
        let peer = PeerId::random();
        let mut links = PeerLinks::default();
        // 相手との接続がまだ無い（自分からの接続の結果を待っている）
        assert_eq!(links.closed(&peer, id(1)), None);
        // 相手との接続があるときも、数に入れていなかった接続は関係ない
        links.established(peer, id(2), true, now);
        assert_eq!(links.closed(&peer, id(3)), None);
        assert!(links.contains(&peer));
    }

    /// 同じアドレス帯から受け付ける接続は8まで。閉じたら空く。アドレス帯の分からない接続は数えない
    #[test]
    fn inbound_connections_are_limited_per_prefix() {
        let peer = PeerId::random();
        let mut links = PeerLinks::default();
        let home = vec![203, 0, 113];
        for n in 0..MAX_INBOUND_PER_PREFIX {
            assert!(links.admit_inbound(id(n), Some(home.clone())));
        }
        assert!(!links.admit_inbound(id(100), Some(home.clone())));
        assert!(links.admit_inbound(id(101), Some(vec![198, 51, 100])));
        assert!(links.admit_inbound(id(102), None));
        assert_eq!(links.source_of(id(0)), Some(&home));
        assert_eq!(links.source_of(id(102)), None);
        // 受け付けずに閉じた接続も、受け付けた接続も、閉じた知らせで忘れる
        assert_eq!(links.closed(&peer, id(0)), None);
        assert_eq!(links.source_of(id(0)), None);
        assert!(links.admit_inbound(id(103), Some(home)));
    }

    /// 中継経由の接続ができてからの時間（いちばん古いもの）と、接続の数
    #[test]
    fn summary_counts_the_links() {
        let now = Instant::now();
        let peer = PeerId::random();
        let mut links = PeerLinks::default();
        links.established(peer, id(1), true, now);
        links.established(peer, id(2), false, now);
        let later = now.checked_add(Duration::from_secs(2)).unwrap_or(now);
        links.established(peer, id(3), true, later);
        let summary = links.summary(&peer, true, later);
        assert_eq!(
            summary,
            Links {
                direct: 1,
                relayed: 2,
                relayed_for: later.saturating_duration_since(now),
                hole_punching: true
            }
        );
        let none = links.summary(&PeerId::random(), false, later);
        assert_eq!((none.direct, none.relayed, none.hole_punching), (0, 0, false));
    }
}
