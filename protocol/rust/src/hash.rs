//! ハッシュ H（SHA-256）と16進表記

use sha2::{Digest, Sha256};

use crate::cbor::{encode, Value};

/// H(CBOR(value))
pub fn h(value: &Value) -> [u8; 32] {
    sha256(&encode(value))
}

/// バイト列の SHA-256
pub fn sha256(bytes: &[u8]) -> [u8; 32] {
    let digest = Sha256::digest(bytes);
    let mut out = [0u8; 32];
    out.copy_from_slice(&digest);
    out
}

/// 小文字の16進
pub fn hex(bytes: &[u8]) -> String {
    const DIGITS: &[u8; 16] = b"0123456789abcdef";
    let mut s = String::with_capacity(bytes.len().saturating_mul(2));
    for b in bytes {
        for nibble in [b >> 4, b & 0x0f] {
            if let Some(&d) = DIGITS.get(usize::from(nibble)) {
                s.push(char::from(d));
            }
        }
    }
    s
}

/// 16進（大文字・小文字とも）をバイト列にする。長さが奇数・16進でない文字があれば None
pub fn from_hex(s: &str) -> Option<Vec<u8>> {
    let bytes = s.as_bytes();
    if bytes.len() % 2 != 0 {
        return None;
    }
    let mut out = Vec::with_capacity(bytes.len() / 2);
    for pair in bytes.chunks_exact(2) {
        let hi = char::from(*pair.first()?).to_digit(16)?;
        let lo = char::from(*pair.get(1)?).to_digit(16)?;
        out.push(u8::try_from(hi.checked_mul(16)?.checked_add(lo)?).ok()?);
    }
    Some(out)
}
