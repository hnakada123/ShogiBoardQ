//! 協力ノードが待ち合わせで預かるもの（--meet。protocol/README.md 2.2節）
//!
//! 利用者が預けた小さなデータを、短い時間だけ、番号で引けるように持つ。メモリにだけ持ち、ディスクにもログにも
//! 書かない（番号も書かない）。番号の意味も、預かったものの中身も、協力ノードには分からない。
//! 件数と頻度は、接続元ごとに2段で絞る（1つの接続元（IPv4 のアドレス・IPv6 の /64）と、同じアドレス帯（IPv4 の
//! /24・IPv6 の /48）。addr::SourceKey）。PeerId は起動のたびに変えられるので、PeerId では絞らない。
//!
//! 上限の目安: 友だちが 64 人いる利用者は、在席の知らせを 64 件まで預け（期間の変わり目には前の期間の分が少し残る）、
//! 「相手」タブを開いている間は、預け直しと引き取りで毎秒 2〜4 件の要求を出す。CGNAT（IPv4）では、別の利用者どうしが
//! 1つのアドレスを分け合うので、1つの接続元の上限も、そういう利用者が数人いて足りる大きさにする。

use std::collections::HashMap;
use std::time::{Duration, Instant};

use libp2p::PeerId;
use sbq_protocol::meet::{Request, Response, Status};

use crate::addr::SourceKey;
use crate::limits::{TierCounts, TierLimits, TokenBucket};

/// 預かる件数の上限（全体。1件 1 KiB ほどなので、全部で 20 MiB ほど）
pub const MAX_ENTRIES: usize = 16_384;
/// 預かる件数の上限（1つの接続元・同じアドレス帯）
pub const ENTRIES_PER_SOURCE: TierLimits = TierLimits {
    per_source: 512,
    per_prefix: 2048,
};

/// 要求の頻度の上限（1つの接続元・同じアドレス帯。続けて受け付ける数と、1秒あたりに戻る数）
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RequestLimits {
    pub source_burst: u64,
    pub source_rate: u64,
    pub prefix_burst: u64,
    pub prefix_rate: u64,
}

pub const REQUEST_LIMITS: RequestLimits = RequestLimits {
    source_burst: 256,
    source_rate: 16,
    prefix_burst: 1024,
    prefix_rate: 64,
};
/// 頻度を数える接続元・アドレス帯の上限（それぞれ。超えたら、新しい接続元の要求を断る）
const MAX_SOURCES: usize = 4096;

struct Entry {
    blob: Vec<u8>,
    /// 預けたピア（置き換え・消去ができる）
    owner: PeerId,
    /// 預けたピアの接続元（件数を数える）
    source: SourceKey,
    expires: Instant,
    one_time: bool,
}

/// 頻度の記録（1つの段）
#[derive(Default)]
struct Buckets {
    map: HashMap<Vec<u8>, TokenBucket>,
}

impl Buckets {
    /// key の記録（無ければ満杯から始める。記録の数が上限なら None）
    fn get(&mut self, key: &[u8], burst: u64, now: Instant) -> Option<&mut TokenBucket> {
        if !self.map.contains_key(key) {
            if self.map.len() >= MAX_SOURCES {
                return None;
            }
            self.map.insert(key.to_vec(), TokenBucket::full(burst, now));
        }
        self.map.get_mut(key)
    }

    /// 満杯まで戻った記録を消す（消しても、次の要求で満杯から始まるので同じ）
    fn prune(&mut self, now: Instant, rate: u64, burst: u64) {
        self.map.retain(|_, bucket| {
            bucket.refill(now, rate, burst);
            !bucket.is_full(burst)
        });
    }
}

/// 預かっているもの
#[derive(Default)]
pub struct MeetStore {
    entries: HashMap<[u8; 32], Entry>,
    /// 接続元ごとの、預かっている件数
    counts: TierCounts,
    /// 接続元ごと・アドレス帯ごとの、要求の頻度
    source_buckets: Buckets,
    prefix_buckets: Buckets,
}

impl MeetStore {
    /// 預かっている件数
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// 要求を受け付けてよい頻度か（よければ、どちらの段からも1回分を使う）
    fn allow(&mut self, source: &SourceKey, now: Instant) -> bool {
        let l = REQUEST_LIMITS;
        let Some(strict) = self.source_buckets.get(&source.strict, l.source_burst, now) else {
            return false;
        };
        strict.refill(now, l.source_rate, l.source_burst);
        if !strict.has_one() {
            return false;
        }
        let Some(wide) = self.prefix_buckets.get(&source.wide, l.prefix_burst, now) else {
            return false;
        };
        wide.refill(now, l.prefix_rate, l.prefix_burst);
        if !wide.has_one() {
            return false;
        }
        strict.spend_one();
        wide.spend_one();
        true
    }

    fn remove(&mut self, slot: &[u8; 32]) -> Option<Entry> {
        let entry = self.entries.remove(slot)?;
        self.counts.down(&entry.source);
        Some(entry)
    }

    /// 期限の過ぎていない、その番号の預かりもの（期限が過ぎていれば消す）
    fn live(&mut self, slot: &[u8; 32], now: Instant) -> Option<&Entry> {
        if self.entries.get(slot).is_some_and(|e| e.expires <= now) {
            self.remove(slot);
        }
        self.entries.get(slot)
    }

    /// 要求を処理する。peer は要求したピア、source はその接続元
    pub fn handle(&mut self, peer: &PeerId, source: &SourceKey, request: Request, now: Instant) -> Response {
        if !self.allow(source, now) {
            return Response::status(Status::Limit);
        }
        match request {
            Request::Put {
                slot,
                blob,
                ttl_secs,
                one_time,
            } => {
                let Some(expires) = now.checked_add(Duration::from_secs(ttl_secs)) else {
                    return Response::status(Status::Invalid);
                };
                // 同じ番号に預けたものは、預けたピアだけが置き換えられる。置き換えは件数を増やさない（同じ接続元
                // からなら、その接続元の件数も増やさない）
                let replaced_source = match self.live(&slot, now) {
                    Some(existing) if &existing.owner != peer => return Response::status(Status::Refused),
                    Some(existing) => Some(existing.source.clone()),
                    None => None,
                };
                let total = self
                    .entries
                    .len()
                    .saturating_sub(usize::from(replaced_source.is_some()));
                // 同じ接続元・同じアドレス帯からの置き換えは、その段の件数を増やさない
                let (source_count, prefix_count) = self.counts.of(source);
                let mine_source = source_count.saturating_sub(usize::from(
                    replaced_source.as_ref().is_some_and(|r| r.strict == source.strict),
                ));
                let mine_prefix = prefix_count.saturating_sub(usize::from(
                    replaced_source.as_ref().is_some_and(|r| r.wide == source.wide),
                ));
                // 上限を調べてから古いものを消す（断ったときに、前に預けたものを失わない。監査 2026-11-21 U3）
                if total >= MAX_ENTRIES
                    || mine_source >= ENTRIES_PER_SOURCE.per_source
                    || mine_prefix >= ENTRIES_PER_SOURCE.per_prefix
                {
                    return Response::status(Status::Limit);
                }
                if replaced_source.is_some() {
                    self.remove(&slot);
                }
                self.counts.up(source);
                self.entries.insert(
                    slot,
                    Entry {
                        blob,
                        owner: *peer,
                        source: source.clone(),
                        expires,
                        one_time,
                    },
                );
                Response::status(Status::Ok)
            }
            Request::Get { slot } => {
                let Some(entry) = self.live(&slot, now) else {
                    return Response::status(Status::None);
                };
                let blob = entry.blob.clone();
                if entry.one_time {
                    self.remove(&slot);
                }
                Response {
                    status: Status::Ok,
                    blob,
                }
            }
            Request::Delete { slot } => match self.live(&slot, now) {
                Some(existing) if &existing.owner != peer => Response::status(Status::Refused),
                Some(_) => {
                    self.remove(&slot);
                    Response::status(Status::Ok)
                }
                None => Response::status(Status::Ok),
            },
        }
    }

    /// 期限の過ぎた預かりものと、使われていない頻度の記録を消す
    pub fn expire(&mut self, now: Instant) {
        let expired: Vec<[u8; 32]> = self
            .entries
            .iter()
            .filter(|(_, e)| e.expires <= now)
            .map(|(slot, _)| *slot)
            .collect();
        for slot in expired {
            self.remove(&slot);
        }
        let l = REQUEST_LIMITS;
        self.source_buckets.prune(now, l.source_rate, l.source_burst);
        self.prefix_buckets.prune(now, l.prefix_rate, l.prefix_burst);
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::arithmetic_side_effects, clippy::indexing_slicing)]
mod tests {
    use super::*;
    use sbq_protocol::network::Profile;

    fn source(ip: [u8; 4]) -> SourceKey {
        SourceKey::of_ip(ip.into(), Profile::Internet)
    }

    /// 預ける側の接続元
    fn home() -> SourceKey {
        source([203, 0, 114, 7])
    }

    /// 引き取る側など、別のアドレス帯の接続元
    fn other() -> SourceKey {
        source([198, 51, 101, 9])
    }

    fn blob(n: u8) -> Vec<u8> {
        vec![n; 48]
    }

    fn put(slot: u8, n: u8, ttl_secs: u64, one_time: bool) -> Request {
        Request::Put {
            slot: [slot; 32],
            blob: blob(n),
            ttl_secs,
            one_time,
        }
    }

    fn get(slot: u8) -> Request {
        Request::Get { slot: [slot; 32] }
    }

    /// 番号 i の預け入れ（tag で、別の試験の番号と分ける）
    fn numbered_put(i: usize, tag: u8, n: u8) -> Request {
        let mut slot = [tag; 32];
        slot[..8].copy_from_slice(&(i as u64).to_be_bytes());
        Request::Put {
            slot,
            blob: blob(n),
            ttl_secs: 600,
            one_time: false,
        }
    }

    #[test]
    fn deposit_is_returned_until_it_expires() {
        let mut store = MeetStore::default();
        let owner = PeerId::random();
        let friend = PeerId::random();
        let t0 = Instant::now();
        assert_eq!(store.handle(&friend, &other(), get(1), t0).status, Status::None);
        assert_eq!(
            store.handle(&owner, &home(), put(1, 7, 60, false), t0).status,
            Status::Ok
        );
        assert_eq!(store.len(), 1);
        // 1回限りでなければ、期限まで何度でも引き取れる
        for _ in 0..2 {
            let got = store.handle(&friend, &other(), get(1), t0 + Duration::from_secs(59));
            assert_eq!((got.status, got.blob), (Status::Ok, blob(7)));
        }
        assert_eq!(store.handle(&friend, &other(), get(2), t0).status, Status::None);
        // 期限が過ぎたら引き取れない
        assert_eq!(
            store
                .handle(&friend, &other(), get(1), t0 + Duration::from_secs(60))
                .status,
            Status::None
        );
        assert_eq!(store.len(), 0);
    }

    #[test]
    fn one_time_deposit_is_returned_once() {
        let mut store = MeetStore::default();
        let owner = PeerId::random();
        let t0 = Instant::now();
        assert_eq!(
            store.handle(&owner, &home(), put(1, 7, 600, true), t0).status,
            Status::Ok
        );
        assert_eq!(store.handle(&PeerId::random(), &other(), get(1), t0).blob, blob(7));
        assert_eq!(
            store.handle(&PeerId::random(), &other(), get(1), t0).status,
            Status::None
        );
        assert_eq!(store.len(), 0);
    }

    #[test]
    fn only_the_depositor_replaces_or_deletes() {
        let mut store = MeetStore::default();
        let owner = PeerId::random();
        let stranger = PeerId::random();
        let t0 = Instant::now();
        assert_eq!(
            store.handle(&owner, &home(), put(1, 7, 600, false), t0).status,
            Status::Ok
        );
        // 別のピアは、同じ接続元からでも置き換え・消去ができない
        assert_eq!(
            store.handle(&stranger, &home(), put(1, 9, 600, false), t0).status,
            Status::Refused
        );
        let delete = Request::Delete { slot: [1; 32] };
        assert_eq!(
            store.handle(&stranger, &home(), delete.clone(), t0).status,
            Status::Refused
        );
        assert_eq!(store.handle(&stranger, &other(), get(1), t0).blob, blob(7));
        // 預けたピアは置き換えられる（件数は増えない）
        assert_eq!(
            store.handle(&owner, &home(), put(1, 8, 600, false), t0).status,
            Status::Ok
        );
        assert_eq!(store.len(), 1);
        assert_eq!(store.handle(&stranger, &other(), get(1), t0).blob, blob(8));
        assert_eq!(store.handle(&owner, &home(), delete.clone(), t0).status, Status::Ok);
        assert_eq!(store.len(), 0);
        // 無いものを消しても失敗にしない
        assert_eq!(store.handle(&owner, &home(), delete, t0).status, Status::Ok);
        // 期限が過ぎた番号は、別のピアが使える
        assert_eq!(
            store.handle(&owner, &home(), put(2, 7, 10, false), t0).status,
            Status::Ok
        );
        let later = t0 + Duration::from_secs(10);
        assert_eq!(
            store.handle(&stranger, &other(), put(2, 9, 10, false), later).status,
            Status::Ok
        );
    }

    /// 預かる件数は、1つの接続元ごとに数え、PeerId を替えても同じ枠。同じアドレス帯の別の接続元は、帯の上限まで預けられる
    #[test]
    fn deposits_are_limited_per_source() {
        let mut store = MeetStore::default();
        let mut now = Instant::now();
        let per_source = ENTRIES_PER_SOURCE.per_source;
        // 頻度の上限に掛からないように、少しずつ時間を進める
        let step = |now: &mut Instant| *now += Duration::from_millis(100);
        for i in 0..per_source {
            step(&mut now);
            let status = store
                .handle(&PeerId::random(), &home(), numbered_put(i, 1, 1), now)
                .status;
            assert_eq!(status, Status::Ok, "{i}");
        }
        step(&mut now);
        assert_eq!(
            store
                .handle(&PeerId::random(), &home(), numbered_put(9999, 1, 1), now)
                .status,
            Status::Limit
        );
        // 同じ /24 の別の接続元（CGNAT の別の利用者など）は預けられる。ほかの接続元も預けられる
        let neighbour = source([203, 0, 114, 8]);
        assert_eq!(
            store
                .handle(&PeerId::random(), &neighbour, numbered_put(0, 2, 1), now)
                .status,
            Status::Ok
        );
        assert_eq!(
            store
                .handle(&PeerId::random(), &other(), numbered_put(0, 3, 1), now)
                .status,
            Status::Ok
        );
        // 期限が過ぎれば、また預けられる
        let later = now + Duration::from_secs(601);
        store.expire(later);
        assert_eq!(store.len(), 0);
        assert_eq!(
            store
                .handle(&PeerId::random(), &home(), numbered_put(1, 1, 1), later)
                .status,
            Status::Ok
        );
    }

    /// 同じアドレス帯の預かる件数は、接続元を替えても帯の上限まで（1人が /24・/48 の中でアドレスを替えて逃れられない）
    #[test]
    fn deposits_are_limited_per_prefix() {
        let mut store = MeetStore::default();
        let mut now = Instant::now();
        let per_source = ENTRIES_PER_SOURCE.per_source;
        let per_prefix = ENTRIES_PER_SOURCE.per_prefix;
        let mut stored = 0usize;
        let mut host: u8 = 1;
        while stored < per_prefix {
            let from = source([203, 0, 114, host]);
            for i in 0..per_source.min(per_prefix - stored) {
                // 1つの接続元の頻度（1秒に 16 回分）に掛からない間隔
                now += Duration::from_millis(70);
                let status = store
                    .handle(&PeerId::random(), &from, numbered_put(i, host, 1), now)
                    .status;
                assert_eq!(status, Status::Ok, "{host} {i}");
                stored += 1;
            }
            host += 1;
        }
        now += Duration::from_millis(70);
        let fresh = source([203, 0, 114, 200]);
        assert_eq!(
            store
                .handle(&PeerId::random(), &fresh, numbered_put(0, 200, 1), now)
                .status,
            Status::Limit
        );
        // 別の /24 は預けられる
        assert_eq!(
            store
                .handle(&PeerId::random(), &other(), numbered_put(0, 201, 1), now)
                .status,
            Status::Ok
        );
    }

    /// 預けたピアが別の接続元から同じ番号を置き換えようとして、その接続元が件数の上限なら、断る。
    /// 断ったときに、前に預けたものを失わない（上限を調べる前に消さない）
    #[test]
    fn refused_replacement_keeps_the_old_deposit() {
        let mut store = MeetStore::default();
        let owner = PeerId::random();
        let t0 = Instant::now();
        let per_source = ENTRIES_PER_SOURCE.per_source;
        assert_eq!(
            store.handle(&owner, &home(), put(1, 7, 600, false), t0).status,
            Status::Ok
        );
        // 別の接続元を、件数の上限まで埋める（頻度の上限に掛からないように時間を進める）
        let filler = PeerId::random();
        let mut now = t0;
        for i in 0..per_source {
            now += Duration::from_millis(100);
            assert_eq!(
                store.handle(&filler, &other(), numbered_put(i, 0xff, 1), now).status,
                Status::Ok
            );
        }
        now += Duration::from_millis(100);
        assert_eq!(
            store.handle(&owner, &other(), put(1, 9, 300, false), now).status,
            Status::Limit
        );
        let got = store.handle(&PeerId::random(), &home(), get(1), now);
        assert_eq!((got.status, got.blob), (Status::Ok, blob(7)));
        assert_eq!(store.len(), per_source + 1);
        // 同じ接続元からの置き換えは、その接続元が上限でもできる（件数は増えない）
        assert_eq!(
            store.handle(&filler, &other(), numbered_put(0, 0xff, 2), now).status,
            Status::Ok
        );
        assert_eq!(store.len(), per_source + 1);
        // 上限でない接続元へ移る置き換えは、できる（件数は増えず、数える接続元が替わる）
        assert_eq!(
            store.handle(&filler, &home(), numbered_put(0, 0xff, 3), now).status,
            Status::Ok
        );
        assert_eq!(store.len(), per_source + 1);
        assert_eq!(
            store.handle(&owner, &other(), put(2, 9, 300, false), now).status,
            Status::Ok
        );
    }

    /// 要求の頻度は、1つの接続元ごとと、同じアドレス帯ごとの2段で絞る
    #[test]
    fn requests_are_rate_limited_per_source_and_prefix() {
        let l = REQUEST_LIMITS;
        let mut store = MeetStore::default();
        let peer = PeerId::random();
        let t0 = Instant::now();
        for _ in 0..l.source_burst {
            assert_eq!(store.handle(&peer, &home(), get(1), t0).status, Status::None);
        }
        assert_eq!(store.handle(&peer, &home(), get(1), t0).status, Status::Limit);
        // 同じ /24 の別の接続元と、ほかの接続元は、別に数える
        assert_eq!(
            store.handle(&peer, &source([203, 0, 114, 8]), get(1), t0).status,
            Status::None
        );
        assert_eq!(store.handle(&peer, &other(), get(1), t0).status, Status::None);
        // 時間がたつと、1秒に source_rate 回分ずつ戻る
        let later = t0 + Duration::from_secs(1);
        for _ in 0..l.source_rate {
            assert_eq!(store.handle(&peer, &home(), get(1), later).status, Status::None);
        }
        assert_eq!(store.handle(&peer, &home(), get(1), later).status, Status::Limit);
        // 同じ /24 の接続元を替えながら送っても、帯の上限で止まる
        let mut allowed = 0u64;
        for host in 10..=250u8 {
            let from = source([203, 0, 114, host]);
            for _ in 0..l.source_burst {
                if store.handle(&peer, &from, get(1), later).status == Status::None {
                    allowed += 1;
                }
            }
        }
        assert!(allowed < l.prefix_burst, "{allowed}");
        // 満杯まで戻った記録は消える（消えても、次は満杯から始まる）
        let much_later = later + Duration::from_secs(3600);
        store.expire(much_later);
        assert!(store.source_buckets.map.is_empty());
        assert!(store.prefix_buckets.map.is_empty());
        assert_eq!(store.handle(&peer, &home(), get(1), much_later).status, Status::None);
    }
}
