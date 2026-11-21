//! 数と頻度の上限の小さな部品（相手ごとにも数える同時の処理の枠・接続元の2段の上限・トークンバケット）
//!
//! 届いた対局ストリームの最初のフレームを待つ枠（node/streams）、協力ノードが待ち合わせの要求を処理する枠
//! （meet）、対局ストリームのフレームの頻度（game）、協力ノードが接続元ごとに数える要求の頻度（meetstore）、
//! 接続元ごとの接続の数（behaviours・node/peerlinks）で使う。

use std::collections::HashMap;
use std::time::Instant;

use libp2p::PeerId;

use crate::addr::SourceKey;

/// 接続元ごとの上限（2段。addr::SourceKey）。per_source は1つの接続元（strict）、per_prefix は同じアドレス帯（wide）
/// から、いくつまで数に入れるか
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TierLimits {
    pub per_source: usize,
    pub per_prefix: usize,
}

impl TierLimits {
    /// 数えてある接続元（counted）に key をもう1つ足しても、どちらの段も上限の内か
    pub fn admits<'a>(self, counted: impl IntoIterator<Item = &'a SourceKey>, key: &SourceKey) -> bool {
        let mut same_source: usize = 0;
        let mut same_prefix: usize = 0;
        for k in counted {
            if k.strict == key.strict {
                same_source = same_source.saturating_add(1);
            }
            if k.wide == key.wide {
                same_prefix = same_prefix.saturating_add(1);
            }
        }
        same_source < self.per_source && same_prefix < self.per_prefix
    }
}

fn count_up(map: &mut HashMap<Vec<u8>, usize>, key: &[u8]) {
    let n = map.get(key).copied().unwrap_or(0).saturating_add(1);
    map.insert(key.to_vec(), n);
}

fn count_down(map: &mut HashMap<Vec<u8>, usize>, key: &[u8]) {
    let left = map.get(key).copied().unwrap_or(0).saturating_sub(1);
    if left == 0 {
        map.remove(key);
    } else {
        map.insert(key.to_vec(), left);
    }
}

/// 接続元の2段ごとの数（数えたものが無くなった値は消す）
#[derive(Default)]
pub struct TierCounts {
    per_source: HashMap<Vec<u8>, usize>,
    per_prefix: HashMap<Vec<u8>, usize>,
}

impl TierCounts {
    /// key の接続元と、同じアドレス帯の数
    pub fn of(&self, key: &SourceKey) -> (usize, usize) {
        (
            self.per_source.get(&key.strict).copied().unwrap_or(0),
            self.per_prefix.get(&key.wide).copied().unwrap_or(0),
        )
    }

    /// もう1つ数えても、どちらの段も上限の内か
    pub fn admits(&self, key: &SourceKey, limits: TierLimits) -> bool {
        let (source, prefix) = self.of(key);
        source < limits.per_source && prefix < limits.per_prefix
    }

    pub fn up(&mut self, key: &SourceKey) {
        count_up(&mut self.per_source, &key.strict);
        count_up(&mut self.per_prefix, &key.wide);
    }

    pub fn down(&mut self, key: &SourceKey) {
        count_down(&mut self.per_source, &key.strict);
        count_down(&mut self.per_prefix, &key.wide);
    }
}

/// 同時に処理しているものの数（全体と、接続元の2段。上限は使う側が決める）
#[derive(Default)]
pub struct SourceSlots {
    total: usize,
    counts: TierCounts,
}

impl SourceSlots {
    /// 全体と接続元の2段の上限の内なら、枠を1つ使って true
    pub fn enter(&mut self, key: &SourceKey, max_total: usize, limits: TierLimits) -> bool {
        if self.total >= max_total || !self.counts.admits(key, limits) {
            return false;
        }
        self.total = self.total.saturating_add(1);
        self.counts.up(key);
        true
    }

    /// 使った枠を1つ戻す
    pub fn leave(&mut self, key: &SourceKey) {
        self.total = self.total.saturating_sub(1);
        self.counts.down(key);
    }
}

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
        if !self.has_one() {
            return false;
        }
        self.spend_one();
        true
    }

    /// 1件分が残っているか（戻すのは refill。いくつかのバケットから同時に使うときに、先に全部を確かめる）
    pub fn has_one(&self) -> bool {
        self.tokens_milli >= 1000
    }

    /// 1件分を使う（残っていなければ 0 のまま）
    pub fn spend_one(&mut self) {
        self.tokens_milli = self.tokens_milli.saturating_sub(1000);
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

    fn source(strict: &[u8], wide: &[u8]) -> SourceKey {
        SourceKey {
            strict: strict.to_vec(),
            wide: wide.to_vec(),
        }
    }

    /// 2段の上限: 1つの接続元は per_source まで、同じアドレス帯は（接続元が違っても）per_prefix まで
    #[test]
    fn tier_limits_count_both_tiers() {
        let limits = TierLimits {
            per_source: 2,
            per_prefix: 3,
        };
        let a = source(&[1, 2, 3, 4], &[1, 2, 3]);
        let b = source(&[1, 2, 3, 5], &[1, 2, 3]);
        let other = source(&[9, 9, 9, 9], &[9, 9, 9]);
        let mut counted: Vec<SourceKey> = Vec::new();
        for key in [&a, &a] {
            assert!(limits.admits(&counted, key));
            counted.push(key.clone());
        }
        // 同じ接続元は2つまで
        assert!(!limits.admits(&counted, &a));
        // 同じアドレス帯の別の接続元は、帯の上限（3）まで
        assert!(limits.admits(&counted, &b));
        counted.push(b.clone());
        assert!(!limits.admits(&counted, &b));
        assert!(limits.admits(&counted, &other));
    }

    #[test]
    fn source_slots_are_limited_in_total_and_per_tier() {
        let limits = TierLimits {
            per_source: 2,
            per_prefix: 3,
        };
        let mut slots = SourceSlots::default();
        let a = source(&[1, 2, 3, 4], &[1, 2, 3]);
        let b = source(&[1, 2, 3, 5], &[1, 2, 3]);
        let c = source(&[5, 5, 5, 5], &[5, 5, 5]);
        assert!(slots.enter(&a, 4, limits));
        assert!(slots.enter(&a, 4, limits));
        assert!(!slots.enter(&a, 4, limits));
        assert!(slots.enter(&b, 4, limits));
        // 同じアドレス帯は3つまで（b の接続元にはまだ空きがある）
        assert!(!slots.enter(&b, 4, limits));
        assert!(slots.enter(&c, 4, limits));
        // 全体の上限
        assert!(!slots.enter(&source(&[7, 7, 7, 7], &[7, 7, 7]), 4, limits));
        slots.leave(&a);
        assert!(slots.enter(&b, 4, limits));
        // 使っていない枠を戻しても、数は負にならない
        let mut empty = SourceSlots::default();
        empty.leave(&a);
        assert!(empty.enter(&a, 1, limits));
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
