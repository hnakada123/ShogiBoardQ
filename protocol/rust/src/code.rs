//! 合言葉（同じ LAN の招待の短い形。protocol/README.md 3.1節）
//!
//! 合言葉には、招待した側の LAN の中のアドレスと短い秘密だけを入れる。招待した側の PeerId は入らないので、
//! つないだ相手が合言葉を知っていることは、秘密から作った鍵と双方の PeerId で作るトークンで確かめ、
//! 応えた相手（招待した側）が合言葉を知っていることは、同じ鍵から作る確認の値で確かめる
//! （秘密そのものは回線に流さない）。

use std::net::Ipv4Addr;

use crate::cbor::Value;
use crate::hash::h;

/// 合言葉の文字（Crockford の Base32。紛らわしい I・L・O・U を使わない）
pub const ALPHABET: &[u8; 32] = b"0123456789ABCDEFGHJKMNPQRSTVWXYZ";
/// 合言葉に入れるアドレスの上限
pub const MAX_ADDRS: usize = 4;
/// 秘密のビット数の下限（全体を5ビットの倍数にするための余りを足すので、34〜38ビットになる）
pub const SECRET_MIN_BITS: u32 = 34;
/// 合言葉の文字数の範囲（区切りを除く）
pub const MIN_CHARS: usize = 9;
pub const MAX_CHARS: usize = 33;
/// 読む文字列の上限（区切りと空白を含む。バイト）
pub const MAX_INPUT_BYTES: usize = 96;
/// 鍵の導出（Argon2id）のメモリ（KiB）と回数。libsodium の crypto_pwhash の INTERACTIVE と同じ
pub const KDF_MEMORY_KIB: u32 = 65_536;
pub const KDF_PASSES: u32 = 2;

/// 合言葉の内容
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Code {
    /// 招待した側のアドレス（私設アドレスか 127.0.0.1。1〜4件）
    pub addrs: Vec<Ipv4Addr>,
    /// None なら決まったポート（TCP と QUIC）。Some なら、そのポートの QUIC だけ
    pub port: Option<u16>,
    /// 秘密（secret_bits ビット）
    pub secret: u64,
}

/// アドレスの種類の番号・ホスト部・ホスト部のビット数
fn addr_fields(ip: Ipv4Addr) -> Option<(u64, u64, u32)> {
    let [a, b, c, d] = ip.octets();
    if a == 192 && b == 168 {
        Some((0, u64::from(u16::from_be_bytes([c, d])), 16))
    } else if a == 172 && (16..=31).contains(&b) {
        Some((1, u64::from(u32::from_be_bytes([0, b & 0x0f, c, d])), 20))
    } else if a == 10 {
        Some((2, u64::from(u32::from_be_bytes([0, b, c, d])), 24))
    } else if ip == Ipv4Addr::LOCALHOST {
        Some((3, 0, 0))
    } else {
        None
    }
}

/// 種類ごとのホスト部のビット数
fn host_bits(kind: u64) -> Option<usize> {
    match kind {
        0 => Some(16),
        1 => Some(20),
        2 => Some(24),
        3 => Some(0),
        _ => None,
    }
}

fn addr_from(kind: u64, host: u64) -> Option<Ipv4Addr> {
    let [_, b, c, d] = u32::try_from(host).ok()?.to_be_bytes();
    match kind {
        0 => Some(Ipv4Addr::new(192, 168, c, d)),
        1 => Some(Ipv4Addr::new(172, 16 | (b & 0x0f), c, d)),
        2 => Some(Ipv4Addr::new(10, b, c, d)),
        3 => Some(Ipv4Addr::LOCALHOST),
        _ => None,
    }
}

/// 秘密より前のビット数
fn fixed_bits(addrs: &[Ipv4Addr], port: Option<u16>) -> Option<u32> {
    let mut bits: u32 = 2;
    for ip in addrs {
        let (_, _, n) = addr_fields(*ip)?;
        bits = bits.checked_add(n)?.checked_add(3)?;
    }
    if port.is_some() {
        bits = bits.checked_add(16)?;
    }
    Some(bits)
}

/// 全体（検査の5ビットを含む）を5ビットの倍数にする秘密のビット数
fn secret_bits_after(fixed: u32) -> Option<u32> {
    let base = fixed.checked_add(SECRET_MIN_BITS)?;
    let pad = 5u32.checked_sub(base % 5)? % 5;
    SECRET_MIN_BITS.checked_add(pad)
}

struct BitWriter {
    bits: Vec<bool>,
}

impl BitWriter {
    fn push(&mut self, value: u64, count: u32) {
        for i in (0..count).rev() {
            self.bits.push(value.checked_shr(i).is_some_and(|v| v & 1 == 1));
        }
    }
}

struct BitReader<'a> {
    bits: &'a [bool],
    pos: usize,
}

impl BitReader<'_> {
    fn take(&mut self, count: usize) -> Option<u64> {
        let end = self.pos.checked_add(count)?;
        let slice = self.bits.get(self.pos..end)?;
        self.pos = end;
        let mut value = 0u64;
        for bit in slice {
            value = value.wrapping_shl(1) | u64::from(*bit);
        }
        Some(value)
    }

    fn remaining(&self) -> usize {
        self.bits.len().saturating_sub(self.pos)
    }
}

/// 検査の文字（それより前の文字から決まる）
fn check_char(body: &str) -> Option<u8> {
    labeled_check_char("SBQ-CODE-CHECK", body)
}

/// 検査の文字（label は合言葉の種類ごとの名前。同じ LAN の合言葉と、協力ノードを使う合言葉で分ける）
pub(crate) fn labeled_check_char(label: &str, body: &str) -> Option<u8> {
    let digest = h(&Value::Array(vec![
        Value::text(label),
        Value::Uint(1),
        Value::text(body),
    ]));
    let index = usize::from(digest.first()?.checked_shr(3)?);
    ALPHABET.get(index).copied()
}

/// 合言葉の文字を、文字の表の番号（5ビット）の列にする。空白と "-" は読み飛ばし、大文字と小文字を区別しない。
/// O は 0、I と L は 1 として読む。表にない文字がある・長すぎるなら None
pub(crate) fn read_symbols(input: &str) -> Option<Vec<u8>> {
    if input.len() > MAX_INPUT_BYTES {
        return None;
    }
    let mut symbols: Vec<u8> = Vec::new();
    for c in input.chars() {
        if matches!(c, ' ' | '-' | '\t' | '\r' | '\n') {
            continue;
        }
        let upper = c.to_ascii_uppercase();
        let canonical = match upper {
            'O' => '0',
            'I' | 'L' => '1',
            other => other,
        };
        let byte = u8::try_from(canonical).ok()?;
        let index = ALPHABET.iter().position(|a| *a == byte)?;
        symbols.push(u8::try_from(index).ok()?);
    }
    Some(symbols)
}

/// 文字の表の番号の列を文字にする
pub(crate) fn symbols_text(symbols: &[u8]) -> Option<String> {
    let mut text = String::new();
    for s in symbols {
        text.push(char::from(*ALPHABET.get(usize::from(*s))?));
    }
    Some(text)
}

impl Code {
    /// 秘密のビット数（アドレスとポートで決まる。34〜38）
    pub fn secret_bits(&self) -> Option<u32> {
        secret_bits_after(fixed_bits(&self.addrs, self.port)?)
    }

    /// アドレス・ポートと乱数から合言葉を作る（乱数は秘密のビット数に切り詰める）。入れられないアドレスが
    /// あれば None
    pub fn new(addrs: Vec<Ipv4Addr>, port: Option<u16>, random: u64) -> Option<Code> {
        if addrs.is_empty() || addrs.len() > MAX_ADDRS || port == Some(0) {
            return None;
        }
        let bits = secret_bits_after(fixed_bits(&addrs, port)?)?;
        let mask = 1u64.checked_shl(bits)?.checked_sub(1)?;
        Some(Code {
            addrs,
            port,
            secret: random & mask,
        })
    }

    /// 文字列にする（4文字ごとに "-" で区切る）
    pub fn encode(&self) -> Option<String> {
        if self.addrs.is_empty() || self.addrs.len() > MAX_ADDRS || self.port == Some(0) {
            return None;
        }
        let secret_bits = self.secret_bits()?;
        if self.secret.checked_shr(secret_bits)? != 0 {
            return None;
        }
        let mut w = BitWriter { bits: Vec::new() };
        w.push(0, 1);
        w.push(u64::from(self.port.is_some()), 1);
        let last = self.addrs.len().checked_sub(1)?;
        for (i, ip) in self.addrs.iter().enumerate() {
            let (kind, host, n) = addr_fields(*ip)?;
            w.push(kind, 2);
            w.push(host, n);
            w.push(u64::from(i < last), 1);
        }
        if let Some(port) = self.port {
            w.push(u64::from(port), 16);
        }
        w.push(self.secret, secret_bits);
        if w.bits.len() % 5 != 0 {
            return None;
        }
        let mut body = String::new();
        for chunk in w.bits.chunks(5) {
            let mut index = 0usize;
            for bit in chunk {
                index = index.wrapping_shl(1) | usize::from(*bit);
            }
            body.push(char::from(*ALPHABET.get(index)?));
        }
        let check = check_char(&body)?;
        body.push(char::from(check));
        let mut out = String::new();
        for (i, c) in body.chars().enumerate() {
            if i > 0 && i % 4 == 0 {
                out.push('-');
            }
            out.push(c);
        }
        Some(out)
    }

    /// 文字列を読む。空白と "-" は読み飛ばし、大文字と小文字を区別しない。O は 0、I と L は 1 として読む。
    /// 形・検査の文字が合わなければ None
    pub fn decode(input: &str) -> Option<Code> {
        let symbols = read_symbols(input)?;
        if !(MIN_CHARS..=MAX_CHARS).contains(&symbols.len()) {
            return None;
        }
        let (check, body_symbols) = symbols.split_last()?;
        let body = symbols_text(body_symbols)?;
        if check_char(&body)? != *ALPHABET.get(usize::from(*check))? {
            return None;
        }
        let mut bits: Vec<bool> = Vec::new();
        for s in body_symbols {
            for i in (0..5u32).rev() {
                bits.push(s.checked_shr(i).is_some_and(|v| v & 1 == 1));
            }
        }
        let mut r = BitReader { bits: &bits, pos: 0 };
        if r.take(1)? != 0 {
            return None;
        }
        let has_port = r.take(1)? == 1;
        let mut addrs: Vec<Ipv4Addr> = Vec::new();
        loop {
            let kind = r.take(2)?;
            let host = r.take(host_bits(kind)?)?;
            let ip = addr_from(kind, host)?;
            if addrs.contains(&ip) {
                return None;
            }
            addrs.push(ip);
            if r.take(1)? == 0 {
                break;
            }
            if addrs.len() >= MAX_ADDRS {
                return None;
            }
        }
        let port = if has_port {
            let value = u16::try_from(r.take(16)?).ok()?;
            if value == 0 {
                return None;
            }
            Some(value)
        } else {
            None
        };
        let secret_bits = secret_bits_after(fixed_bits(&addrs, port)?)?;
        if r.remaining() != usize::try_from(secret_bits).ok()? {
            return None;
        }
        let secret = r.take(r.remaining())?;
        Some(Code { addrs, port, secret })
    }

    /// 鍵の導出のソルト（16バイト。ネットワーク・ポート・アドレスで決まる）
    pub fn salt(&self, network_id: &[u8; 32]) -> Option<[u8; 16]> {
        let digest = h(&Value::Array(vec![
            Value::text("SBQ-CODE-SALT"),
            Value::Uint(1),
            Value::bytes(network_id),
            Value::Uint(self.port.map_or(0, u64::from)),
            Value::Array(self.addrs.iter().map(|ip| Value::bytes(&ip.octets())).collect()),
        ]));
        <[u8; 16]>::try_from(digest.get(..16)?).ok()
    }

    /// 合言葉から作る鍵（Argon2id。パスワードは秘密の8バイトのビッグエンディアン。時間とメモリを使うので、
    /// 主ループの外で呼ぶ）
    pub fn derive_key(&self, network_id: &[u8; 32]) -> Option<[u8; 32]> {
        let salt = self.salt(network_id)?;
        let params = argon2::Params::new(KDF_MEMORY_KIB, KDF_PASSES, 1, Some(32)).ok()?;
        let kdf = argon2::Argon2::new(argon2::Algorithm::Argon2id, argon2::Version::V0x13, params);
        let mut key = [0u8; 32];
        kdf.hash_password_into(&self.secret.to_be_bytes(), &salt, &mut key).ok()?;
        Some(key)
    }
}

/// 合言葉でつなぐ側が INVITE_TOKEN で送るトークン（鍵と双方の PeerId で決まる。別の相手へは使えない）
pub fn token(key: &[u8; 32], inviter_peer_id: &[u8], invitee_peer_id: &[u8]) -> [u8; 32] {
    labeled_value("SBQ-CODE-TOKEN", key, inviter_peer_id, invitee_peer_id)
}

/// 招待した側が、合言葉のトークンを照合できた後に、同じストリームの最初のフレーム（INVITE_TOKEN の形）で返す
/// 確認の値。つなぐ側は、これを確かめてから接続を開いたものとする（合言葉の宛先で応えた相手が、合言葉を
/// 知っていることを確かめる。protocol/README.md 3.1節）。トークンとはラベルで分けるので、トークンからは作れない
pub fn confirmation(key: &[u8; 32], inviter_peer_id: &[u8], invitee_peer_id: &[u8]) -> [u8; 32] {
    labeled_value("SBQ-CODE-CONFIRM", key, inviter_peer_id, invitee_peer_id)
}

/// H(CBOR([label, 1, key, 招待した側の PeerId, つなぐ側の PeerId]))
fn labeled_value(label: &str, key: &[u8; 32], inviter_peer_id: &[u8], invitee_peer_id: &[u8]) -> [u8; 32] {
    h(&Value::Array(vec![
        Value::text(label),
        Value::Uint(1),
        Value::bytes(key),
        Value::bytes(inviter_peer_id),
        Value::bytes(invitee_peer_id),
    ]))
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::indexing_slicing, clippy::arithmetic_side_effects)]
mod tests {
    use super::*;

    fn ip(text: &str) -> Ipv4Addr {
        text.parse().unwrap()
    }

    #[test]
    fn home_lan_code_is_twelve_characters() {
        let code = Code::new(vec![ip("192.168.1.23")], None, u64::MAX).unwrap();
        assert_eq!(code.secret_bits(), Some(34));
        let text = code.encode().unwrap();
        assert_eq!(text.len(), 14);
        assert_eq!(text.matches('-').count(), 2);
        assert_eq!(Code::decode(&text).unwrap(), code);
    }

    #[test]
    fn roundtrip_for_every_kind_of_address() {
        let cases: Vec<(Vec<Ipv4Addr>, Option<u16>)> = vec![
            (vec![ip("192.168.0.1")], None),
            (vec![ip("172.16.0.1")], None),
            (vec![ip("172.31.255.254")], None),
            (vec![ip("10.200.30.4")], None),
            (vec![ip("127.0.0.1")], Some(40001)),
            (vec![ip("192.168.1.5"), ip("172.17.0.1")], None),
            (
                vec![ip("10.0.0.1"), ip("10.1.2.3"), ip("192.168.56.1"), ip("172.20.1.1")],
                Some(65535),
            ),
        ];
        for (addrs, port) in cases {
            for random in [0u64, 1, 0x1234_5678_9abc_def0, u64::MAX] {
                let code = Code::new(addrs.clone(), port, random).unwrap();
                let bits = code.secret_bits().unwrap();
                assert!((34..=38).contains(&bits));
                let text = code.encode().unwrap();
                let symbols = text.chars().filter(|c| *c != '-').count();
                assert!((MIN_CHARS..=MAX_CHARS).contains(&symbols), "{text}");
                assert_eq!(Code::decode(&text).unwrap(), code, "{text}");
            }
        }
    }

    #[test]
    fn reads_lowercase_spaces_and_lookalike_letters() {
        let code = Code::new(vec![ip("192.168.1.23")], None, 0x2_0000_0001).unwrap();
        let text = code.encode().unwrap();
        let plain: String = text.chars().filter(|c| *c != '-').collect();
        assert_eq!(Code::decode(&plain.to_lowercase()).unwrap(), code);
        assert_eq!(Code::decode(&format!("  {} \n", text.replace('-', " "))).unwrap(), code);
        // 0 と 1 は、O と I・L でも読める
        let lookalike = plain.replace('0', "o").replace('1', "l");
        assert_eq!(Code::decode(&lookalike).unwrap(), code);
        assert_eq!(Code::decode(&plain.replace('1', "I")).unwrap(), code);
    }

    #[test]
    fn rejects_other_shapes() {
        let text = Code::new(vec![ip("192.168.1.23")], None, 7).unwrap().encode().unwrap();
        let plain: String = text.chars().filter(|c| *c != '-').collect();
        // 1文字の違いは、検査の文字でほぼ見つかる（5ビットなので、32通りに1つは通る）
        let mut caught = 0;
        let mut total = 0;
        for i in 0..plain.len() {
            for a in ALPHABET {
                let mut bytes = plain.clone().into_bytes();
                if bytes[i] == *a {
                    continue;
                }
                bytes[i] = *a;
                total += 1;
                if Code::decode(&String::from_utf8(bytes).unwrap()).is_none() {
                    caught += 1;
                }
            }
        }
        assert!(caught * 100 >= total * 90, "{caught}/{total}");
        for bad in ["", "K7MP", "U7MP-2QXD-9F3A", "K7MP-2QXD-9F3!", "あいうえお", &"0".repeat(40)] {
            assert!(Code::decode(bad).is_none(), "{bad}");
        }
        assert!(Code::decode(&"0".repeat(200)).is_none());
        // 入れられないアドレス・件数・ポート
        assert!(Code::new(vec![ip("8.8.8.8")], None, 0).is_none());
        assert!(Code::new(vec![ip("100.64.0.1")], None, 0).is_none());
        assert!(Code::new(vec![ip("127.0.0.2")], None, 0).is_none());
        assert!(Code::new(Vec::new(), None, 0).is_none());
        assert!(Code::new(vec![ip("10.0.0.1"); 5], None, 0).is_none());
        assert!(Code::new(vec![ip("10.0.0.1")], Some(0), 0).is_none());
    }

    #[test]
    fn token_depends_on_key_and_both_peers() {
        let key = [3u8; 32];
        let t = token(&key, b"inviter", b"invitee");
        assert_ne!(t, token(&[4u8; 32], b"inviter", b"invitee"));
        assert_ne!(t, token(&key, b"invitee", b"inviter"));
        assert_ne!(t, token(&key, b"inviter", b"other"));
        assert_eq!(t, token(&key, b"inviter", b"invitee"));
    }

    /// 確認の値も鍵と双方の PeerId で決まり、トークンとは別の値になる（トークンを受けた側が、確認の値を作れない）
    #[test]
    fn confirmation_differs_from_the_token() {
        let key = [3u8; 32];
        let c = confirmation(&key, b"inviter", b"invitee");
        assert_ne!(c, token(&key, b"inviter", b"invitee"));
        assert_ne!(c, confirmation(&[4u8; 32], b"inviter", b"invitee"));
        assert_ne!(c, confirmation(&key, b"invitee", b"inviter"));
        assert_ne!(c, confirmation(&key, b"inviter", b"other"));
        assert_eq!(c, confirmation(&key, b"inviter", b"invitee"));
    }

    #[test]
    fn key_depends_on_secret_network_and_addresses() {
        let code = Code::new(vec![ip("192.168.1.23")], None, 5).unwrap();
        let key = code.derive_key(&[1u8; 32]).unwrap();
        assert_eq!(key, code.derive_key(&[1u8; 32]).unwrap());
        assert_ne!(key, code.derive_key(&[2u8; 32]).unwrap());
        let other_secret = Code::new(vec![ip("192.168.1.23")], None, 6).unwrap();
        assert_ne!(key, other_secret.derive_key(&[1u8; 32]).unwrap());
        let other_addr = Code::new(vec![ip("192.168.1.24")], None, 5).unwrap();
        assert_ne!(key, other_addr.derive_key(&[1u8; 32]).unwrap());
    }
}
