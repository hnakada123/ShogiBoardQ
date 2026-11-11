//! 招待文字列（protocol/README.md 3章）

use base64::engine::general_purpose::URL_SAFE_NO_PAD;
use base64::Engine;

use crate::cbor::{decode, encode, labeled, Value};
use crate::hash::{hex, sha256};
use crate::peerid;

/// 招待文字列の接頭辞
pub const PREFIX: &str = "sbqinv1.";
/// 招待文字列全体の上限（バイト）
pub const MAX_STRING_BYTES: usize = 1024;
/// 到達候補の数の上限
pub const MAX_ADDRS: usize = 8;
/// 到達候補1件の上限（バイト）
pub const MAX_ADDR_BYTES: usize = 100;
/// 有効期間の上限（秒）
pub const MAX_LIFETIME_SECS: u64 = 600;

/// 招待の内容
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Invite {
    pub network_id: [u8; 32],
    /// 招待した側の PeerId（バイナリ）
    pub peer_id: Vec<u8>,
    /// 到達候補（multiaddr のバイナリ。/p2p/ を含めない）
    pub addrs: Vec<Vec<u8>>,
    pub token: [u8; 32],
    pub expires_at: u64,
}

/// 招待文字列の誤り
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum InviteError {
    /// 読めない（形・検査の値・型）
    Invalid,
    /// 期限切れ
    Expired,
    /// 別のネットワーク
    WrongNetwork,
    /// 自分の招待
    Own,
}

impl std::fmt::Display for InviteError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            InviteError::Invalid => "invalid invite",
            InviteError::Expired => "invite expired",
            InviteError::WrongNetwork => "invite for another network",
            InviteError::Own => "own invite",
        };
        f.write_str(s)
    }
}

impl std::error::Error for InviteError {}

impl Invite {
    fn body_value(&self) -> Value {
        Value::Array(vec![
            Value::text("SBQ-INVITE"),
            Value::Uint(1),
            Value::bytes(&self.network_id),
            Value::bytes(&self.peer_id),
            Value::Array(self.addrs.iter().map(|a| Value::bytes(a)).collect()),
            Value::bytes(&self.token),
            Value::Uint(self.expires_at),
        ])
    }

    /// 招待文字列にする（上限を超えれば None）
    pub fn encode(&self) -> Option<String> {
        if self.addrs.len() > MAX_ADDRS || self.addrs.iter().any(|a| a.is_empty() || a.len() > MAX_ADDR_BYTES) {
            return None;
        }
        if !peerid::is_ed25519(&self.peer_id) {
            return None;
        }
        let body = encode(&self.body_value());
        let checksum = sha256(&body);
        let s = format!(
            "{}{}.{}",
            PREFIX,
            URL_SAFE_NO_PAD.encode(&body),
            hex(checksum.get(..4)?)
        );
        if s.len() > MAX_STRING_BYTES {
            return None;
        }
        Some(s)
    }

    /// 招待文字列を読む（前後の空白は除く）。形だけを確かめ、期限・ネットワークは `check` で確かめる
    pub fn decode(input: &str) -> Result<Invite, InviteError> {
        let s = input.trim_matches(|c: char| c == ' ' || c == '\t' || c == '\r' || c == '\n');
        if s.len() > MAX_STRING_BYTES {
            return Err(InviteError::Invalid);
        }
        let rest = s.strip_prefix(PREFIX).ok_or(InviteError::Invalid)?;
        let (body_text, checksum_text) = rest.split_once('.').ok_or(InviteError::Invalid)?;
        if checksum_text.len() != 8
            || !checksum_text
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
        {
            return Err(InviteError::Invalid);
        }
        let body = URL_SAFE_NO_PAD.decode(body_text).map_err(|_| InviteError::Invalid)?;
        let checksum = sha256(&body);
        if hex(checksum.get(..4).ok_or(InviteError::Invalid)?) != checksum_text {
            return Err(InviteError::Invalid);
        }
        let value = decode(&body).map_err(|_| InviteError::Invalid)?;
        let items = labeled(&value, "SBQ-INVITE", 1, 7).ok_or(InviteError::Invalid)?;
        let network_id = items
            .first()
            .and_then(Value::as_fixed_bytes::<32>)
            .ok_or(InviteError::Invalid)?;
        let peer_id = items
            .get(1)
            .and_then(Value::as_bytes)
            .ok_or(InviteError::Invalid)?
            .to_vec();
        if !peerid::is_ed25519(&peer_id) {
            return Err(InviteError::Invalid);
        }
        let addr_values = items.get(2).and_then(Value::as_array).ok_or(InviteError::Invalid)?;
        if addr_values.len() > MAX_ADDRS {
            return Err(InviteError::Invalid);
        }
        let mut addrs = Vec::with_capacity(addr_values.len());
        for a in addr_values {
            let bytes = a.as_bytes().ok_or(InviteError::Invalid)?;
            if bytes.is_empty() || bytes.len() > MAX_ADDR_BYTES {
                return Err(InviteError::Invalid);
            }
            addrs.push(bytes.to_vec());
        }
        let token = items
            .get(3)
            .and_then(Value::as_fixed_bytes::<32>)
            .ok_or(InviteError::Invalid)?;
        let expires_at = items.get(4).and_then(Value::as_uint).ok_or(InviteError::Invalid)?;
        Ok(Invite {
            network_id,
            peer_id,
            addrs,
            token,
            expires_at,
        })
    }

    /// 今のネットワーク・時刻・自分の PeerId に対して使えるか
    pub fn check(&self, now: u64, network_id: &[u8; 32], own_peer_id: &[u8]) -> Result<(), InviteError> {
        if &self.network_id != network_id {
            return Err(InviteError::WrongNetwork);
        }
        if self.expires_at <= now || self.expires_at > now.saturating_add(MAX_LIFETIME_SECS).saturating_add(60) {
            // 有効期限が長すぎる招待も読まない（時刻のずれとして 60 秒は許す）
            return Err(InviteError::Expired);
        }
        if self.peer_id == own_peer_id {
            return Err(InviteError::Own);
        }
        Ok(())
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::indexing_slicing, clippy::arithmetic_side_effects)]
mod tests {
    use super::*;

    fn sample() -> Invite {
        let mut peer = vec![0x00, 0x24, 0x08, 0x01, 0x12, 0x20];
        peer.extend_from_slice(&[7u8; 32]);
        Invite {
            network_id: [1u8; 32],
            peer_id: peer,
            addrs: vec![vec![0x04, 127, 0, 0, 1, 0x06, 0x12, 0x34]],
            token: [9u8; 32],
            expires_at: 1_000_600,
        }
    }

    #[test]
    fn roundtrip() {
        let inv = sample();
        let s = inv.encode().unwrap();
        assert!(s.starts_with(PREFIX));
        assert_eq!(Invite::decode(&format!("  {s}\n")).unwrap(), inv);
        assert!(inv.check(1_000_000, &[1u8; 32], &[0u8; 38]).is_ok());
        assert_eq!(inv.check(1_000_600, &[1u8; 32], &[0u8; 38]), Err(InviteError::Expired));
        assert_eq!(
            inv.check(1_000_000, &[2u8; 32], &[0u8; 38]),
            Err(InviteError::WrongNetwork)
        );
        assert_eq!(inv.check(1_000_000, &[1u8; 32], &inv.peer_id), Err(InviteError::Own));
    }

    #[test]
    fn rejects_tampering() {
        let s = sample().encode().unwrap();
        let mut bytes = s.into_bytes();
        let idx = PREFIX.len() + 3;
        bytes[idx] = if bytes[idx] == b'A' { b'B' } else { b'A' };
        let tampered = String::from_utf8(bytes).unwrap();
        assert_eq!(Invite::decode(&tampered), Err(InviteError::Invalid));
        assert_eq!(Invite::decode("sbqinv1.AAAA"), Err(InviteError::Invalid));
        assert_eq!(Invite::decode("hello"), Err(InviteError::Invalid));
        assert_eq!(Invite::decode(&"x".repeat(2000)), Err(InviteError::Invalid));
    }
}
