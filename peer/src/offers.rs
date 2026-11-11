//! 募集の検査と保持（protocol/README.md 4章）

use std::collections::{HashMap, VecDeque};
use std::time::{Duration, Instant};

use libp2p::identity::{ed25519, Keypair, PublicKey};
use libp2p::PeerId;
use sbq_protocol::hash::hex;
use sbq_protocol::offer::{Availability, OfferBody, SignedOffer};
use sbq_protocol::peerid;
use serde_json::{json, Value};

/// 保持する募集の上限
pub const MAX_OFFERS: usize = 1000;
/// 同じ発信者の更新の最短の間隔
pub const MIN_UPDATE_INTERVAL: Duration = Duration::from_secs(10);
/// 見た (offerId, sequence) を覚えておく時間と件数
pub const SEEN_TTL: Duration = Duration::from_secs(600);
pub const MAX_SEEN: usize = 20_000;
/// 時刻のずれとして許す秒数
pub const MAX_CLOCK_SKEW_SECS: u64 = 30;

/// 検査の結果
#[derive(Debug, PartialEq, Eq)]
pub enum Verdict {
    /// 形・署名が正しくない（転送しない。送ってきたピアを減点する）
    Reject,
    /// 正しいが受け取らない（期限切れ・古い sequence・頻度・上限。減点しない）
    Ignore,
    /// 時刻が先に進みすぎている
    Skewed,
}

/// 署名と、ネットワーク・ルール・期限を確かめる
pub fn verify(signed: &SignedOffer, network_id: &[u8; 32], rules_hash: &[u8; 32], now: u64) -> Result<PeerId, Verdict> {
    let body = &signed.body;
    if &body.network_id != network_id || &body.rules_hash != rules_hash {
        return Err(Verdict::Reject);
    }
    let key_bytes = peerid::ed25519_public_key(&body.peer_id).ok_or(Verdict::Reject)?;
    let key = ed25519::PublicKey::try_from_bytes(&key_bytes).map_err(|_| Verdict::Reject)?;
    let public = PublicKey::from(key);
    if !public.verify(&signed.body_bytes, &signed.signature) {
        return Err(Verdict::Reject);
    }
    let peer = PeerId::from_bytes(&body.peer_id).map_err(|_| Verdict::Reject)?;
    if peer != public.to_peer_id() {
        return Err(Verdict::Reject);
    }
    if body.created_at > now.saturating_add(MAX_CLOCK_SKEW_SECS) {
        return Err(Verdict::Skewed);
    }
    if body.expires_at <= now {
        return Err(Verdict::Ignore);
    }
    Ok(peer)
}

/// 自分の募集に署名する
pub fn sign(keypair: &Keypair, body: OfferBody) -> Option<SignedOffer> {
    let bytes = body.encode();
    let signature = keypair.sign(&bytes).ok()?;
    Some(SignedOffer::new(body, signature))
}

/// IPC の Offer
pub fn to_json(body: &OfferBody, peer: &PeerId) -> Value {
    json!({
        "offerId": hex(&body.offer_id),
        "sequence": body.sequence.to_string(),
        "peerId": peer.to_base58(),
        "displayName": body.display_name,
        "rulesHash": hex(&body.rules_hash),
        "mainMs": body.main_ms.to_string(),
        "byoyomiMs": body.byoyomi_ms.to_string(),
        "sidePreference": body.side_preference.name(),
        "createdAt": body.created_at.to_string(),
        "expiresAt": body.expires_at.to_string(),
    })
}

struct Stored {
    signed: SignedOffer,
    peer: PeerId,
    last_update: Instant,
}

/// 保持している募集（発信者ごとに Open の募集1件）
#[derive(Default)]
pub struct OfferStore {
    by_peer: HashMap<PeerId, Stored>,
    seen: HashMap<([u8; 32], u64), Instant>,
    seen_order: VecDeque<(([u8; 32], u64), Instant)>,
    added: Vec<Value>,
    removed: Vec<String>,
}

impl OfferStore {
    fn remember(&mut self, key: ([u8; 32], u64)) -> bool {
        let now = Instant::now();
        while let Some((k, at)) = self.seen_order.front() {
            if now.saturating_duration_since(*at) < SEEN_TTL && self.seen_order.len() < MAX_SEEN {
                break;
            }
            let k = *k;
            self.seen_order.pop_front();
            self.seen.remove(&k);
        }
        if self.seen.contains_key(&key) {
            return false;
        }
        self.seen.insert(key, now);
        self.seen_order.push_back((key, now));
        true
    }

    /// 検査済みの募集を取り込む。転送してよいもの（新しい内容）なら Ok(true)、重複なら Ok(false)
    pub fn insert(&mut self, signed: SignedOffer, peer: PeerId) -> Result<bool, Verdict> {
        let key = (signed.body.offer_id, signed.body.sequence);
        if self.seen.contains_key(&key) {
            return Ok(false);
        }
        let withdrawn = signed.body.availability == Availability::Withdrawn;
        match self.by_peer.get(&peer) {
            Some(existing) => {
                let same_offer = existing.signed.body.offer_id == signed.body.offer_id;
                if same_offer && signed.body.sequence <= existing.signed.body.sequence {
                    return Err(Verdict::Ignore);
                }
                if !withdrawn && existing.last_update.elapsed() < MIN_UPDATE_INTERVAL {
                    return Err(Verdict::Ignore);
                }
                if withdrawn && !same_offer {
                    // 保持していない募集の取り下げは何も変えない
                    self.remember(key);
                    return Ok(false);
                }
            }
            None => {
                if withdrawn {
                    self.remember(key);
                    return Ok(false);
                }
                if self.by_peer.len() >= MAX_OFFERS {
                    return Err(Verdict::Ignore);
                }
            }
        }
        self.remember(key);
        if let Some(old) = self.by_peer.remove(&peer) {
            // 同じ募集の更新は added だけで知らせる（offerId で置き換える）。別の募集に替わったときと取り下げは removed
            if withdrawn || old.signed.body.offer_id != signed.body.offer_id {
                self.removed.push(hex(&old.signed.body.offer_id));
            }
        }
        if !withdrawn {
            self.added.push(to_json(&signed.body, &peer));
            self.by_peer.insert(
                peer,
                Stored {
                    signed,
                    peer,
                    last_update: Instant::now(),
                },
            );
        }
        Ok(true)
    }

    /// 期限切れを除く
    pub fn expire(&mut self, now: u64) {
        let expired: Vec<PeerId> = self
            .by_peer
            .iter()
            .filter(|(_, s)| s.signed.body.expires_at <= now)
            .map(|(p, _)| *p)
            .collect();
        for p in expired {
            if let Some(old) = self.by_peer.remove(&p) {
                self.removed.push(hex(&old.signed.body.offer_id));
            }
        }
    }

    /// 発信者がいなくなった（期限切れのほかに、ここでは使わない）
    pub fn find_peer(&self, offer_id: &[u8; 32]) -> Option<PeerId> {
        self.by_peer
            .values()
            .find(|s| &s.signed.body.offer_id == offer_id)
            .map(|s| s.peer)
    }

    /// 一覧の応答に入れる募集（cursor 番目から最大 n 件）と次の cursor
    pub fn page(&self, cursor: u64, n: usize, own: Option<&SignedOffer>) -> (Vec<SignedOffer>, u64) {
        let mut all: Vec<&SignedOffer> = Vec::new();
        if let Some(o) = own {
            all.push(o);
        }
        let mut stored: Vec<&Stored> = self.by_peer.values().collect();
        stored.sort_by_key(|s| s.signed.body.offer_id);
        all.extend(stored.into_iter().map(|s| &s.signed));
        let start = usize::try_from(cursor).unwrap_or(usize::MAX);
        let page: Vec<SignedOffer> = all.iter().skip(start).take(n).map(|s| (*s).clone()).collect();
        let next = start.saturating_add(page.len());
        let next_cursor = if next < all.len() {
            u64::try_from(next).unwrap_or(0)
        } else {
            0
        };
        (page, next_cursor)
    }

    /// 保持している募集の数
    pub fn len(&self) -> usize {
        self.by_peer.len()
    }

    /// 前回から変わったもの（追加・削除）を取り出す
    pub fn take_changes(&mut self) -> (Vec<Value>, Vec<String>) {
        (std::mem::take(&mut self.added), std::mem::take(&mut self.removed))
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::indexing_slicing, clippy::arithmetic_side_effects)]
mod tests {
    use super::*;
    use sbq_protocol::offer::SidePreference;

    fn body(kp: &Keypair, offer_id: u8, seq: u64, now: u64) -> OfferBody {
        OfferBody {
            network_id: [1; 32],
            offer_id: [offer_id; 32],
            sequence: seq,
            peer_id: kp.public().to_peer_id().to_bytes(),
            display_name: "太郎".into(),
            rules_hash: [2; 32],
            main_ms: 600_000,
            byoyomi_ms: 30_000,
            side_preference: SidePreference::None,
            created_at: now,
            expires_at: now + 120,
            availability: Availability::Open,
        }
    }

    #[test]
    fn verify_signature_and_store() {
        let kp = Keypair::generate_ed25519();
        let now = 1_000_000;
        let signed = sign(&kp, body(&kp, 7, 1, now)).unwrap();
        let peer = verify(&signed, &[1; 32], &[2; 32], now).unwrap();
        assert_eq!(peer, kp.public().to_peer_id());
        assert_eq!(verify(&signed, &[9; 32], &[2; 32], now), Err(Verdict::Reject));
        assert_eq!(verify(&signed, &[1; 32], &[2; 32], now + 200), Err(Verdict::Ignore));
        assert_eq!(verify(&signed, &[1; 32], &[2; 32], now - 100), Err(Verdict::Skewed));

        let mut tampered = signed.clone();
        tampered.signature[0] ^= 1;
        assert_eq!(verify(&tampered, &[1; 32], &[2; 32], now), Err(Verdict::Reject));

        let mut store = OfferStore::default();
        assert_eq!(store.insert(signed.clone(), peer), Ok(true));
        assert_eq!(store.insert(signed.clone(), peer), Ok(false));
        let (added, removed) = store.take_changes();
        assert_eq!(added.len(), 1);
        assert!(removed.is_empty());
        // 10秒以内の更新は受け取らない
        let update = sign(&kp, body(&kp, 7, 2, now)).unwrap();
        assert_eq!(store.insert(update, peer), Err(Verdict::Ignore));
        // 取り下げはすぐに効く
        let mut w = body(&kp, 7, 3, now);
        w.availability = Availability::Withdrawn;
        assert_eq!(store.insert(sign(&kp, w).unwrap(), peer), Ok(true));
        let (_, removed) = store.take_changes();
        assert_eq!(removed.len(), 1);
    }
}
