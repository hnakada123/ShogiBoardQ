//! 募集と一覧取得（protocol/README.md 4章・5章）
//!
//! ここでは形と範囲だけを確かめる。署名（libp2p 識別鍵）・期限・発信者の一致は通信プロセスが確かめる。

use crate::cbor::{decode, encode, labeled, Value};
use crate::displayname;
use crate::peerid;

/// SignedOffer 全体の上限（バイト）
pub const MAX_SIGNED_OFFER_BYTES: usize = 1024;
/// 募集の寿命の上限（秒）
pub const MAX_LIFETIME_SECS: u64 = 120;
/// mainMs の上限
pub const MAX_MAIN_MS: u64 = 86_400_000;
/// byoyomiMs の上限
pub const MAX_BYOYOMI_MS: u64 = 600_000;
/// 一覧の応答1回の件数の上限
pub const MAX_LIST_ITEMS: usize = 50;
/// 一覧取得のプロトコル
pub const LOBBY_PROTOCOL: &str = "/shogiboardq/lobby/1.0.0";
/// 一覧取得の要求・応答の上限（バイト）
pub const MAX_LIST_MESSAGE_BYTES: usize = 64 * 1024;

/// 先後の希望
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum SidePreference {
    None,
    Black,
    White,
}

impl SidePreference {
    pub fn code(self) -> u64 {
        match self {
            SidePreference::None => 0,
            SidePreference::Black => 1,
            SidePreference::White => 2,
        }
    }

    pub fn from_code(code: u64) -> Option<Self> {
        match code {
            0 => Some(SidePreference::None),
            1 => Some(SidePreference::Black),
            2 => Some(SidePreference::White),
            _ => None,
        }
    }

    /// IPC の表記（none・black・white）
    pub fn name(self) -> &'static str {
        match self {
            SidePreference::None => "none",
            SidePreference::Black => "black",
            SidePreference::White => "white",
        }
    }

    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "none" => Some(SidePreference::None),
            "black" => Some(SidePreference::Black),
            "white" => Some(SidePreference::White),
            _ => None,
        }
    }
}

/// 募集の状態
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Availability {
    Open,
    Withdrawn,
}

impl Availability {
    pub fn code(self) -> u64 {
        match self {
            Availability::Open => 1,
            Availability::Withdrawn => 2,
        }
    }

    pub fn from_code(code: u64) -> Option<Self> {
        match code {
            1 => Some(Availability::Open),
            2 => Some(Availability::Withdrawn),
            _ => None,
        }
    }
}

/// 持ち時間が v1 で使えるか
pub fn is_valid_time_control(main_ms: u64, byoyomi_ms: u64) -> bool {
    main_ms <= MAX_MAIN_MS && byoyomi_ms <= MAX_BYOYOMI_MS && !(main_ms == 0 && byoyomi_ms == 0)
}

/// 募集の本体
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct OfferBody {
    pub network_id: [u8; 32],
    pub offer_id: [u8; 32],
    pub sequence: u64,
    pub peer_id: Vec<u8>,
    pub display_name: String,
    pub rules_hash: [u8; 32],
    pub main_ms: u64,
    pub byoyomi_ms: u64,
    pub side_preference: SidePreference,
    pub created_at: u64,
    pub expires_at: u64,
    pub availability: Availability,
}

/// 募集の誤り
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum OfferError {
    /// 形・型が違う
    Malformed,
    /// 範囲・規則に合わない値
    OutOfRange,
    /// 表示名が規則に合わない
    DisplayName,
    /// 大きすぎる
    TooLarge,
}

impl std::fmt::Display for OfferError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            OfferError::Malformed => "malformed offer",
            OfferError::OutOfRange => "offer value out of range",
            OfferError::DisplayName => "invalid display name",
            OfferError::TooLarge => "offer too large",
        };
        f.write_str(s)
    }
}

impl std::error::Error for OfferError {}

impl OfferBody {
    pub fn to_value(&self) -> Value {
        Value::Array(vec![
            Value::text("SBQ-OFFER"),
            Value::Uint(1),
            Value::bytes(&self.network_id),
            Value::bytes(&self.offer_id),
            Value::Uint(self.sequence),
            Value::bytes(&self.peer_id),
            Value::text(&self.display_name),
            Value::bytes(&self.rules_hash),
            Value::Array(vec![Value::Uint(self.main_ms), Value::Uint(self.byoyomi_ms)]),
            Value::Uint(self.side_preference.code()),
            Value::Uint(self.created_at),
            Value::Uint(self.expires_at),
            Value::Uint(self.availability.code()),
        ])
    }

    /// 決定的CBORのバイト列
    pub fn encode(&self) -> Vec<u8> {
        encode(&self.to_value())
    }

    /// 値の規則（型以外）を確かめる
    pub fn validate(&self) -> Result<(), OfferError> {
        if !peerid::is_ed25519(&self.peer_id) {
            return Err(OfferError::Malformed);
        }
        if !displayname::is_valid(&self.display_name) {
            return Err(OfferError::DisplayName);
        }
        if !is_valid_time_control(self.main_ms, self.byoyomi_ms) {
            return Err(OfferError::OutOfRange);
        }
        if self.created_at >= self.expires_at || self.expires_at > self.created_at.saturating_add(MAX_LIFETIME_SECS) {
            return Err(OfferError::OutOfRange);
        }
        Ok(())
    }

    pub fn from_value(value: &Value) -> Result<OfferBody, OfferError> {
        let items = labeled(value, "SBQ-OFFER", 1, 13).ok_or(OfferError::Malformed)?;
        let get = |i: usize| items.get(i).ok_or(OfferError::Malformed);
        let network_id = get(0)?.as_fixed_bytes::<32>().ok_or(OfferError::Malformed)?;
        let offer_id = get(1)?.as_fixed_bytes::<32>().ok_or(OfferError::Malformed)?;
        let sequence = get(2)?.as_uint().ok_or(OfferError::Malformed)?;
        let peer_id = get(3)?.as_bytes().ok_or(OfferError::Malformed)?.to_vec();
        let display_name = get(4)?.as_text().ok_or(OfferError::Malformed)?.to_owned();
        let rules_hash = get(5)?.as_fixed_bytes::<32>().ok_or(OfferError::Malformed)?;
        let time = get(6)?.as_array().ok_or(OfferError::Malformed)?;
        if time.len() != 2 {
            return Err(OfferError::Malformed);
        }
        let main_ms = time.first().and_then(Value::as_uint).ok_or(OfferError::Malformed)?;
        let byoyomi_ms = time.get(1).and_then(Value::as_uint).ok_or(OfferError::Malformed)?;
        let side_preference =
            SidePreference::from_code(get(7)?.as_uint().ok_or(OfferError::Malformed)?).ok_or(OfferError::OutOfRange)?;
        let created_at = get(8)?.as_uint().ok_or(OfferError::Malformed)?;
        let expires_at = get(9)?.as_uint().ok_or(OfferError::Malformed)?;
        let availability =
            Availability::from_code(get(10)?.as_uint().ok_or(OfferError::Malformed)?).ok_or(OfferError::OutOfRange)?;
        let body = OfferBody {
            network_id,
            offer_id,
            sequence,
            peer_id,
            display_name,
            rules_hash,
            main_ms,
            byoyomi_ms,
            side_preference,
            created_at,
            expires_at,
            availability,
        };
        body.validate()?;
        Ok(body)
    }

    pub fn decode(bytes: &[u8]) -> Result<OfferBody, OfferError> {
        let value = decode(bytes).map_err(|_| OfferError::Malformed)?;
        OfferBody::from_value(&value)
    }
}

/// 署名付きの募集。`body` は OfferBody の決定的CBOR、`signature` は libp2p 識別鍵の署名（64バイト）
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SignedOffer {
    pub body_bytes: Vec<u8>,
    pub signature: Vec<u8>,
    /// 読んだ body（`body_bytes` と同じ内容）
    pub body: OfferBody,
}

impl SignedOffer {
    pub fn new(body: OfferBody, signature: Vec<u8>) -> SignedOffer {
        SignedOffer {
            body_bytes: body.encode(),
            signature,
            body,
        }
    }

    pub fn to_value(&self) -> Value {
        Value::Array(vec![Value::bytes(&self.body_bytes), Value::bytes(&self.signature)])
    }

    pub fn encode(&self) -> Vec<u8> {
        encode(&self.to_value())
    }

    pub fn from_value(value: &Value) -> Result<SignedOffer, OfferError> {
        let items = value.as_array().ok_or(OfferError::Malformed)?;
        if items.len() != 2 {
            return Err(OfferError::Malformed);
        }
        let body_bytes = items
            .first()
            .and_then(Value::as_bytes)
            .ok_or(OfferError::Malformed)?
            .to_vec();
        let signature = items
            .get(1)
            .and_then(Value::as_bytes)
            .ok_or(OfferError::Malformed)?
            .to_vec();
        if signature.len() != 64 {
            return Err(OfferError::Malformed);
        }
        if encode(value).len() > MAX_SIGNED_OFFER_BYTES {
            return Err(OfferError::TooLarge);
        }
        let body = OfferBody::decode(&body_bytes)?;
        Ok(SignedOffer {
            body_bytes,
            signature,
            body,
        })
    }

    pub fn decode(bytes: &[u8]) -> Result<SignedOffer, OfferError> {
        if bytes.len() > MAX_SIGNED_OFFER_BYTES {
            return Err(OfferError::TooLarge);
        }
        let value = decode(bytes).map_err(|_| OfferError::Malformed)?;
        SignedOffer::from_value(&value)
    }
}

/// 一覧取得の要求
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ListRequest {
    pub network_id: [u8; 32],
    pub rules_hash: [u8; 32],
    pub cursor: u64,
}

impl ListRequest {
    pub fn encode(&self) -> Vec<u8> {
        encode(&Value::Array(vec![
            Value::text("SBQ-LIST"),
            Value::Uint(1),
            Value::bytes(&self.network_id),
            Value::bytes(&self.rules_hash),
            Value::Uint(self.cursor),
        ]))
    }

    pub fn decode(bytes: &[u8]) -> Option<ListRequest> {
        if bytes.len() > MAX_LIST_MESSAGE_BYTES {
            return None;
        }
        let value = decode(bytes).ok()?;
        let items = labeled(&value, "SBQ-LIST", 1, 5)?;
        Some(ListRequest {
            network_id: items.first()?.as_fixed_bytes::<32>()?,
            rules_hash: items.get(1)?.as_fixed_bytes::<32>()?,
            cursor: items.get(2)?.as_uint()?,
        })
    }
}

/// 一覧取得の応答
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ListResponse {
    pub offers: Vec<SignedOffer>,
    pub next_cursor: u64,
}

impl ListResponse {
    pub fn encode(&self) -> Vec<u8> {
        encode(&Value::Array(vec![
            Value::text("SBQ-LIST-RESP"),
            Value::Uint(1),
            Value::Array(
                self.offers
                    .iter()
                    .take(MAX_LIST_ITEMS)
                    .map(SignedOffer::to_value)
                    .collect(),
            ),
            Value::Uint(self.next_cursor),
        ]))
    }

    /// 応答を読む。形の合わない募集は捨てる（応答全体の形が違えば None）
    pub fn decode(bytes: &[u8]) -> Option<ListResponse> {
        if bytes.len() > MAX_LIST_MESSAGE_BYTES {
            return None;
        }
        let value = decode(bytes).ok()?;
        let items = labeled(&value, "SBQ-LIST-RESP", 1, 4)?;
        let offer_values = items.first()?.as_array()?;
        if offer_values.len() > MAX_LIST_ITEMS {
            return None;
        }
        let offers = offer_values
            .iter()
            .filter_map(|v| SignedOffer::from_value(v).ok())
            .collect();
        Some(ListResponse {
            offers,
            next_cursor: items.get(1)?.as_uint()?,
        })
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::indexing_slicing, clippy::arithmetic_side_effects)]
mod tests {
    use super::*;

    fn body() -> OfferBody {
        let mut peer = peerid::ED25519_PREFIX.to_vec();
        peer.extend_from_slice(&[5u8; 32]);
        OfferBody {
            network_id: [1; 32],
            offer_id: [2; 32],
            sequence: 3,
            peer_id: peer,
            display_name: "太郎".to_owned(),
            rules_hash: [4; 32],
            main_ms: 600_000,
            byoyomi_ms: 30_000,
            side_preference: SidePreference::None,
            created_at: 1000,
            expires_at: 1120,
            availability: Availability::Open,
        }
    }

    #[test]
    fn roundtrip() {
        let signed = SignedOffer::new(body(), vec![0u8; 64]);
        let bytes = signed.encode();
        assert!(bytes.len() < 400);
        assert_eq!(SignedOffer::decode(&bytes).unwrap(), signed);
        let resp = ListResponse {
            offers: vec![signed.clone()],
            next_cursor: 0,
        };
        assert_eq!(ListResponse::decode(&resp.encode()).unwrap(), resp);
        let req = ListRequest {
            network_id: [1; 32],
            rules_hash: [4; 32],
            cursor: 7,
        };
        assert_eq!(ListRequest::decode(&req.encode()).unwrap(), req);
    }

    #[test]
    fn rejects_bad_values() {
        let mut b = body();
        b.expires_at = 1121;
        assert_eq!(b.validate(), Err(OfferError::OutOfRange));
        let mut b = body();
        b.main_ms = 0;
        b.byoyomi_ms = 0;
        assert_eq!(b.validate(), Err(OfferError::OutOfRange));
        let mut b = body();
        b.display_name = " x".to_owned();
        assert_eq!(b.validate(), Err(OfferError::DisplayName));
    }
}
