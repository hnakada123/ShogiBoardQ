//! 数と頻度の上限の小さな部品（相手ごとにも数える同時の処理の枠・トークンバケット）
//!
//! 届いた対局ストリームの最初のフレームを待つ枠（node/streams）と、協力ノードが待ち合わせの要求を処理する枠
//! （meet）、対局ストリームのフレームの頻度（game）と、協力ノードが接続元ごとに数える要求の頻度（meetstore）で使う。

use std::collections::HashMap;
use std::time::Instant;

use libp2p::PeerId;

/// 同時に処理しているものの数（全体と相手ごと。上限は使う側が決める）
#[derive(Default)]
pub struct PeerSlots {
    total: usize,
    per_peer: HashMap<PeerId, usize>,
}

impl PeerSlots {
    /// 全体と相手ごとの上限の内なら、枠を1つ使って true
    pub fn enter(&mut self, peer: &PeerId, max_total: usize, max_per_peer: usize) -> bool {
        let mine = self.per_peer.get(peer).copied().unwrap_or(0);
        if self.total >= max_total || mine >= max_per_peer {
            return false;
        }
        self.total = self.total.saturating_add(1);
        self.per_peer.insert(*peer, mine.saturating_add(1));
        true
    }

    /// 使った枠を1つ戻す
    pub fn leave(&mut self, peer: &PeerId) {
        self.total = self.total.saturating_sub(1);
        let left = self.per_peer.get(peer).copied().unwrap_or(0).saturating_sub(1);
        if left == 0 {
            self.per_peer.remove(peer);
        } else {
            self.per_peer.insert(*peer, left);
        }
    }
}

/// 頻度の上限（トークンバケット。値はミリ単位のトークン）。rate は1秒あたりに戻る件数、burst は続けて使える件数
/// （呼ぶたびに渡すので、途中で上限を変えられる）
pub struct TokenBucket {
    tokens_milli: u64,
    last: Instant,
}

impl TokenBucket {
    /// burst 件分を持って始める
    pub fn full(burst: u64, now: Instant) -> TokenBucket {
        TokenBucket {
            tokens_milli: burst.saturating_mul(1000),
            last: now,
        }
    }

    /// 前の時刻からの分だけ戻す（burst 件分まで）
    pub fn refill(&mut self, now: Instant, rate: u64, burst: u64) {
        let elapsed_ms = u64::try_from(now.saturating_duration_since(self.last).as_millis()).unwrap_or(u64::MAX);
        self.last = now;
        self.tokens_milli = self
            .tokens_milli
            .saturating_add(elapsed_ms.saturating_mul(rate))
            .min(burst.saturating_mul(1000));
    }

    /// 戻してから1件分を使う。足りなければ使わずに false
    pub fn take(&mut self, now: Instant, rate: u64, burst: u64) -> bool {
        self.refill(now, rate, burst);
        if self.tokens_milli < 1000 {
            return false;
        }
        self.tokens_milli = self.tokens_milli.saturating_sub(1000);
        true
    }

    /// burst 件分まで戻っているか（戻っていれば、捨てて満杯から始め直しても同じ）
    pub fn is_full(&self, burst: u64) -> bool {
        self.tokens_milli >= burst.saturating_mul(1000)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::Duration;

    #[test]
    fn slots_are_limited_in_total_and_per_peer() {
        let mut slots = PeerSlots::default();
        let (a, b, c) = (PeerId::random(), PeerId::random(), PeerId::random());
        assert!(slots.enter(&a, 3, 2));
        assert!(slots.enter(&a, 3, 2));
        assert!(!slots.enter(&a, 3, 2));
        assert!(slots.enter(&b, 3, 2));
        assert!(!slots.enter(&c, 3, 2));
        slots.leave(&a);
        assert!(slots.enter(&c, 3, 2));
        // 使っていない枠を戻しても、数は負にならない
        let mut empty = PeerSlots::default();
        empty.leave(&a);
        assert!(empty.enter(&a, 1, 1));
        assert!(!empty.enter(&b, 1, 1));
    }

    #[test]
    fn bucket_allows_a_burst_then_the_rate() {
        let t0 = Instant::now();
        let mut bucket = TokenBucket::full(10, t0);
        let allowed = (0..20).filter(|_| bucket.take(t0, 5, 10)).count();
        assert_eq!(allowed, 10);
        assert!(!bucket.is_full(10));
        // 1秒で rate 件分戻る
        let t1 = t0.checked_add(Duration::from_secs(1)).unwrap_or(t0);
        let allowed = (0..20).filter(|_| bucket.take(t1, 5, 10)).count();
        assert_eq!(allowed, 5);
        // burst 件分より多くは戻らない
        let later = t0.checked_add(Duration::from_secs(60)).unwrap_or(t0);
        bucket.refill(later, 5, 10);
        assert!(bucket.is_full(10));
        let allowed = (0..20).filter(|_| bucket.take(later, 5, 10)).count();
        assert_eq!(allowed, 10);
    }
}
