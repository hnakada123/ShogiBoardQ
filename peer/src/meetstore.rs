//! 協力ノードが待ち合わせで預かるもの（--meet。protocol/README.md 2.2節）
//!
//! 利用者が預けた小さなデータを、短い時間だけ、番号で引けるように持つ。メモリにだけ持ち、ディスクにもログにも
//! 書かない（番号も書かない）。番号の意味も、預かったものの中身も、協力ノードには分からない。
//! 件数と頻度は、接続元のアドレス帯ごとに絞る（PeerId は起動のたびに変えられるので、PeerId では絞らない）。

use std::collections::HashMap;
use std::time::{Duration, Instant};

use libp2p::PeerId;
use sbq_protocol::meet::{Request, Response, Status};

/// 預かる件数の上限（全体）
pub const MAX_ENTRIES: usize = 4096;
/// 預かる件数の上限（接続元ごと）
pub const MAX_ENTRIES_PER_SOURCE: usize = 256;
/// 続けて受け付ける要求の数（接続元ごと）
pub const REQUEST_BURST: u64 = 64;
/// 1秒あたりに戻る要求の数（接続元ごと）
pub const REQUEST_RATE: u64 = 4;
/// 頻度を数える接続元の上限（超えたら、新しい接続元の要求を断る）
const MAX_SOURCES: usize = 1024;

struct Entry {
    blob: Vec<u8>,
    /// 預けたピア（置き換え・消去ができる）
    owner: PeerId,
    /// 預けたピアの接続元（件数を数える）
    source: Vec<u8>,
    expires: Instant,
    one_time: bool,
}

/// 要求の頻度（トークンバケット。値はミリ単位のトークン）
struct Bucket {
    tokens_milli: u64,
    last: Instant,
}

const BUCKET_FULL: u64 = REQUEST_BURST * 1000;

impl Bucket {
    fn refill(&mut self, now: Instant) {
        let elapsed_ms = u64::try_from(now.saturating_duration_since(self.last).as_millis()).unwrap_or(u64::MAX);
        self.last = now;
        self.tokens_milli = self
            .tokens_milli
            .saturating_add(elapsed_ms.saturating_mul(REQUEST_RATE))
            .min(BUCKET_FULL);
    }
}

/// 預かっているもの
#[derive(Default)]
pub struct MeetStore {
    entries: HashMap<[u8; 32], Entry>,
    /// 接続元ごとの、預かっている件数
    per_source: HashMap<Vec<u8>, usize>,
    /// 接続元ごとの、要求の頻度
    buckets: HashMap<Vec<u8>, Bucket>,
}

impl MeetStore {
    /// 預かっている件数
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// 要求を受け付けてよい頻度か（よければ1回分を使う）
    fn allow(&mut self, source: &[u8], now: Instant) -> bool {
        if !self.buckets.contains_key(source) {
            if self.buckets.len() >= MAX_SOURCES {
                return false;
            }
            self.buckets.insert(
                source.to_vec(),
                Bucket {
                    tokens_milli: BUCKET_FULL,
                    last: now,
                },
            );
        }
        let Some(bucket) = self.buckets.get_mut(source) else {
            return false;
        };
        bucket.refill(now);
        if bucket.tokens_milli < 1000 {
            return false;
        }
        bucket.tokens_milli = bucket.tokens_milli.saturating_sub(1000);
        true
    }

    fn remove(&mut self, slot: &[u8; 32]) -> Option<Entry> {
        let entry = self.entries.remove(slot)?;
        let left = self
            .per_source
            .get(&entry.source)
            .copied()
            .unwrap_or(0)
            .saturating_sub(1);
        if left == 0 {
            self.per_source.remove(&entry.source);
        } else {
            self.per_source.insert(entry.source.clone(), left);
        }
        Some(entry)
    }

    /// 期限の過ぎていない、その番号の預かりもの（期限が過ぎていれば消す）
    fn live(&mut self, slot: &[u8; 32], now: Instant) -> Option<&Entry> {
        if self.entries.get(slot).is_some_and(|e| e.expires <= now) {
            self.remove(slot);
        }
        self.entries.get(slot)
    }

    /// 要求を処理する。peer は要求したピア、source はその接続元（アドレス帯）
    pub fn handle(&mut self, peer: &PeerId, source: &[u8], request: Request, now: Instant) -> Response {
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
                match self.live(&slot, now) {
                    // 同じ番号に預けたものは、預けたピアだけが置き換えられる
                    Some(existing) if &existing.owner != peer => return Response::status(Status::Refused),
                    Some(_) => {
                        self.remove(&slot);
                    }
                    None => {}
                }
                let mine = self.per_source.get(source).copied().unwrap_or(0);
                if self.entries.len() >= MAX_ENTRIES || mine >= MAX_ENTRIES_PER_SOURCE {
                    return Response::status(Status::Limit);
                }
                self.per_source.insert(source.to_vec(), mine.saturating_add(1));
                self.entries.insert(
                    slot,
                    Entry {
                        blob,
                        owner: *peer,
                        source: source.to_vec(),
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
        // 満杯まで戻った頻度の記録は、消しても同じ（次の要求で満杯から始まる）
        self.buckets.retain(|_, bucket| {
            bucket.refill(now);
            bucket.tokens_milli < BUCKET_FULL
        });
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::arithmetic_side_effects, clippy::indexing_slicing)]
mod tests {
    use super::*;

    const HOME: &[u8] = &[203, 0, 113];
    const OTHER: &[u8] = &[198, 51, 100];

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

    #[test]
    fn deposit_is_returned_until_it_expires() {
        let mut store = MeetStore::default();
        let owner = PeerId::random();
        let friend = PeerId::random();
        let t0 = Instant::now();
        assert_eq!(store.handle(&friend, OTHER, get(1), t0).status, Status::None);
        assert_eq!(store.handle(&owner, HOME, put(1, 7, 60, false), t0).status, Status::Ok);
        assert_eq!(store.len(), 1);
        // 1回限りでなければ、期限まで何度でも引き取れる
        for _ in 0..2 {
            let got = store.handle(&friend, OTHER, get(1), t0 + Duration::from_secs(59));
            assert_eq!((got.status, got.blob), (Status::Ok, blob(7)));
        }
        assert_eq!(store.handle(&friend, OTHER, get(2), t0).status, Status::None);
        // 期限が過ぎたら引き取れない
        assert_eq!(
            store
                .handle(&friend, OTHER, get(1), t0 + Duration::from_secs(60))
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
        assert_eq!(store.handle(&owner, HOME, put(1, 7, 600, true), t0).status, Status::Ok);
        assert_eq!(store.handle(&PeerId::random(), OTHER, get(1), t0).blob, blob(7));
        assert_eq!(store.handle(&PeerId::random(), OTHER, get(1), t0).status, Status::None);
        assert_eq!(store.len(), 0);
    }

    #[test]
    fn only_the_depositor_replaces_or_deletes() {
        let mut store = MeetStore::default();
        let owner = PeerId::random();
        let stranger = PeerId::random();
        let t0 = Instant::now();
        assert_eq!(store.handle(&owner, HOME, put(1, 7, 600, false), t0).status, Status::Ok);
        // 別のピアは、同じ接続元からでも置き換え・消去ができない
        assert_eq!(
            store.handle(&stranger, HOME, put(1, 9, 600, false), t0).status,
            Status::Refused
        );
        let delete = Request::Delete { slot: [1; 32] };
        assert_eq!(
            store.handle(&stranger, HOME, delete.clone(), t0).status,
            Status::Refused
        );
        assert_eq!(store.handle(&stranger, OTHER, get(1), t0).blob, blob(7));
        // 預けたピアは置き換えられる（件数は増えない）
        assert_eq!(store.handle(&owner, HOME, put(1, 8, 600, false), t0).status, Status::Ok);
        assert_eq!(store.len(), 1);
        assert_eq!(store.handle(&stranger, OTHER, get(1), t0).blob, blob(8));
        assert_eq!(store.handle(&owner, HOME, delete.clone(), t0).status, Status::Ok);
        assert_eq!(store.len(), 0);
        // 無いものを消しても失敗にしない
        assert_eq!(store.handle(&owner, HOME, delete, t0).status, Status::Ok);
        // 期限が過ぎた番号は、別のピアが使える
        assert_eq!(store.handle(&owner, HOME, put(2, 7, 10, false), t0).status, Status::Ok);
        let later = t0 + Duration::from_secs(10);
        assert_eq!(
            store.handle(&stranger, OTHER, put(2, 9, 10, false), later).status,
            Status::Ok
        );
    }

    #[test]
    fn deposits_are_limited_per_source() {
        let mut store = MeetStore::default();
        let t0 = Instant::now();
        let mut now = t0;
        let put_slot = |store: &mut MeetStore, i: usize, source: &[u8], now: Instant| {
            let mut slot = [0u8; 32];
            slot[..8].copy_from_slice(&(i as u64).to_be_bytes());
            slot[8] = source[0];
            let request = Request::Put {
                slot,
                blob: blob(1),
                ttl_secs: 600,
                one_time: false,
            };
            // ピアを変えても、同じ接続元なら同じ枠
            store.handle(&PeerId::random(), source, request, now).status
        };
        for i in 0..MAX_ENTRIES_PER_SOURCE {
            // 頻度の上限に掛からないように時間を進める
            now += Duration::from_secs(1);
            assert_eq!(put_slot(&mut store, i, HOME, now), Status::Ok, "{i}");
        }
        now += Duration::from_secs(1);
        assert_eq!(put_slot(&mut store, 9999, HOME, now), Status::Limit);
        // ほかの接続元は預けられる
        assert_eq!(put_slot(&mut store, 0, OTHER, now), Status::Ok);
        // 期限が過ぎれば、また預けられる
        let later = now + Duration::from_secs(601);
        store.expire(later);
        assert_eq!(store.len(), 0);
        assert_eq!(put_slot(&mut store, 1, HOME, later), Status::Ok);
    }

    #[test]
    fn requests_are_rate_limited_per_source() {
        let mut store = MeetStore::default();
        let peer = PeerId::random();
        let t0 = Instant::now();
        for _ in 0..REQUEST_BURST {
            assert_eq!(store.handle(&peer, HOME, get(1), t0).status, Status::None);
        }
        assert_eq!(store.handle(&peer, HOME, get(1), t0).status, Status::Limit);
        // ほかの接続元は別に数える
        assert_eq!(store.handle(&peer, OTHER, get(1), t0).status, Status::None);
        // 時間がたつと、1秒に REQUEST_RATE 回分ずつ戻る
        let later = t0 + Duration::from_secs(1);
        for _ in 0..REQUEST_RATE {
            assert_eq!(store.handle(&peer, HOME, get(1), later).status, Status::None);
        }
        assert_eq!(store.handle(&peer, HOME, get(1), later).status, Status::Limit);
        // 満杯まで戻った記録は消える（消えても、次は満杯から始まる）
        store.expire(later + Duration::from_secs(3600));
        assert!(store.buckets.is_empty());
        assert_eq!(
            store
                .handle(&peer, HOME, get(1), later + Duration::from_secs(3600))
                .status,
            Status::None
        );
    }
}
