//! 決定的CBOR（protocol/README.md 1章の部分集合）
//!
//! 使う型は符号なし整数・byte string・text string・配列の4つだけ。長さと整数は最短の形、
//! 確定長、入れ子は16段まで、値は1つで余りのバイトなし。読む側はほかをすべて拒否する。

use std::fmt;

/// 入れ子の上限（最上位の値を1段目と数える）
pub const MAX_DEPTH: usize = 16;

/// CBOR の値
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Value {
    Uint(u64),
    Bytes(Vec<u8>),
    Text(String),
    Array(Vec<Value>),
}

impl Value {
    pub fn text(s: &str) -> Value {
        Value::Text(s.to_owned())
    }

    pub fn bytes(b: &[u8]) -> Value {
        Value::Bytes(b.to_vec())
    }

    pub fn as_uint(&self) -> Option<u64> {
        match self {
            Value::Uint(v) => Some(*v),
            _ => None,
        }
    }

    pub fn as_bytes(&self) -> Option<&[u8]> {
        match self {
            Value::Bytes(v) => Some(v),
            _ => None,
        }
    }

    pub fn as_text(&self) -> Option<&str> {
        match self {
            Value::Text(v) => Some(v),
            _ => None,
        }
    }

    pub fn as_array(&self) -> Option<&[Value]> {
        match self {
            Value::Array(v) => Some(v),
            _ => None,
        }
    }

    /// 長さ n の byte string なら、その中身
    pub fn as_fixed_bytes<const N: usize>(&self) -> Option<[u8; N]> {
        let b = self.as_bytes()?;
        <[u8; N]>::try_from(b).ok()
    }
}

/// 読み取りの失敗
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CborError {
    /// 入力が途中で終わった
    Truncated,
    /// 使わない型（負の整数・map・タグ・浮動小数・単純値）
    UnsupportedType,
    /// 最短でない長さ・整数
    NonCanonical,
    /// 不定長・予約の additional information
    Indefinite,
    /// text string が正しい UTF-8 でない
    InvalidUtf8,
    /// 入れ子が深すぎる
    TooDeep,
    /// 値の後に余りのバイトがある
    TrailingBytes,
}

impl fmt::Display for CborError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            CborError::Truncated => "truncated",
            CborError::UnsupportedType => "unsupported type",
            CborError::NonCanonical => "non-canonical encoding",
            CborError::Indefinite => "indefinite or reserved length",
            CborError::InvalidUtf8 => "invalid utf-8",
            CborError::TooDeep => "nesting too deep",
            CborError::TrailingBytes => "trailing bytes",
        };
        f.write_str(s)
    }
}

impl std::error::Error for CborError {}

fn write_head(out: &mut Vec<u8>, major: u8, value: u64) {
    let m = major << 5;
    if value < 24 {
        // value < 24 なので u8 に収まる
        out.push(m | (value as u8));
    } else if let Ok(v) = u8::try_from(value) {
        out.push(m | 24);
        out.push(v);
    } else if let Ok(v) = u16::try_from(value) {
        out.push(m | 25);
        out.extend_from_slice(&v.to_be_bytes());
    } else if let Ok(v) = u32::try_from(value) {
        out.push(m | 26);
        out.extend_from_slice(&v.to_be_bytes());
    } else {
        out.push(m | 27);
        out.extend_from_slice(&value.to_be_bytes());
    }
}

fn encode_into(value: &Value, out: &mut Vec<u8>) {
    match value {
        Value::Uint(v) => write_head(out, 0, *v),
        Value::Bytes(b) => {
            write_head(out, 2, b.len() as u64);
            out.extend_from_slice(b);
        }
        Value::Text(s) => {
            write_head(out, 3, s.len() as u64);
            out.extend_from_slice(s.as_bytes());
        }
        Value::Array(items) => {
            write_head(out, 4, items.len() as u64);
            for item in items {
                encode_into(item, out);
            }
        }
    }
}

/// 決定的CBORのバイト列にする
pub fn encode(value: &Value) -> Vec<u8> {
    let mut out = Vec::new();
    encode_into(value, &mut out);
    out
}

struct Reader<'a> {
    input: &'a [u8],
    pos: usize,
}

impl<'a> Reader<'a> {
    fn remaining(&self) -> usize {
        self.input.len().saturating_sub(self.pos)
    }

    fn take(&mut self, n: usize) -> Result<&'a [u8], CborError> {
        let end = self.pos.checked_add(n).ok_or(CborError::Truncated)?;
        let slice = self.input.get(self.pos..end).ok_or(CborError::Truncated)?;
        self.pos = end;
        Ok(slice)
    }

    fn byte(&mut self) -> Result<u8, CborError> {
        let b = *self.input.get(self.pos).ok_or(CborError::Truncated)?;
        self.pos = self.pos.checked_add(1).ok_or(CborError::Truncated)?;
        Ok(b)
    }

    /// 先頭バイトと続く長さ・整数を読む（major, value）
    fn head(&mut self) -> Result<(u8, u64), CborError> {
        let initial = self.byte()?;
        let major = initial >> 5;
        let info = initial & 0x1f;
        if major == 1 || major == 5 || major == 6 || major == 7 {
            return Err(CborError::UnsupportedType);
        }
        let value = match info {
            0..=23 => u64::from(info),
            24 => {
                let v = u64::from(self.byte()?);
                if v < 24 {
                    return Err(CborError::NonCanonical);
                }
                v
            }
            25 => {
                let b = self.take(2)?;
                let arr: [u8; 2] = b.try_into().map_err(|_| CborError::Truncated)?;
                let v = u64::from(u16::from_be_bytes(arr));
                if v <= 0xff {
                    return Err(CborError::NonCanonical);
                }
                v
            }
            26 => {
                let b = self.take(4)?;
                let arr: [u8; 4] = b.try_into().map_err(|_| CborError::Truncated)?;
                let v = u64::from(u32::from_be_bytes(arr));
                if v <= 0xffff {
                    return Err(CborError::NonCanonical);
                }
                v
            }
            27 => {
                let b = self.take(8)?;
                let arr: [u8; 8] = b.try_into().map_err(|_| CborError::Truncated)?;
                let v = u64::from_be_bytes(arr);
                if v <= 0xffff_ffff {
                    return Err(CborError::NonCanonical);
                }
                v
            }
            _ => return Err(CborError::Indefinite),
        };
        Ok((major, value))
    }

    fn value(&mut self, depth: usize) -> Result<Value, CborError> {
        if depth > MAX_DEPTH {
            return Err(CborError::TooDeep);
        }
        let (major, n) = self.head()?;
        match major {
            0 => Ok(Value::Uint(n)),
            2 | 3 => {
                let len = usize::try_from(n).map_err(|_| CborError::Truncated)?;
                if len > self.remaining() {
                    return Err(CborError::Truncated);
                }
                let data = self.take(len)?;
                if major == 2 {
                    Ok(Value::Bytes(data.to_vec()))
                } else {
                    let s = std::str::from_utf8(data).map_err(|_| CborError::InvalidUtf8)?;
                    Ok(Value::Text(s.to_owned()))
                }
            }
            4 => {
                let len = usize::try_from(n).map_err(|_| CborError::Truncated)?;
                // 各要素は1バイト以上なので、残りより多い要素数は読めない（先に確かめて領域を確保しすぎない）
                if len > self.remaining() {
                    return Err(CborError::Truncated);
                }
                let next_depth = depth.checked_add(1).ok_or(CborError::TooDeep)?;
                let mut items = Vec::with_capacity(len);
                for _ in 0..len {
                    items.push(self.value(next_depth)?);
                }
                Ok(Value::Array(items))
            }
            _ => Err(CborError::UnsupportedType),
        }
    }
}

/// 決定的CBORとして読む（値1つ、余りのバイトなし）
pub fn decode(input: &[u8]) -> Result<Value, CborError> {
    let mut reader = Reader { input, pos: 0 };
    let value = reader.value(1)?;
    if reader.remaining() != 0 {
        return Err(CborError::TrailingBytes);
    }
    // 規則を守った入力は符号化し直すと元に戻る（念のため確かめる）
    if encode(&value) != input {
        return Err(CborError::NonCanonical);
    }
    Ok(value)
}

/// 配列 `[label, version, ...]` の先頭を確かめ、残りの要素を返す
pub fn labeled<'a>(value: &'a Value, label: &str, version: u64, len: usize) -> Option<&'a [Value]> {
    let items = value.as_array()?;
    if items.len() != len {
        return None;
    }
    if items.first()?.as_text()? != label {
        return None;
    }
    if items.get(1)?.as_uint()? != version {
        return None;
    }
    items.get(2..)
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::indexing_slicing, clippy::arithmetic_side_effects)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip_integers() {
        for v in [
            0u64,
            23,
            24,
            255,
            256,
            65535,
            65536,
            0xffff_ffff,
            0x1_0000_0000,
            u64::MAX,
        ] {
            let bytes = encode(&Value::Uint(v));
            assert_eq!(decode(&bytes).unwrap(), Value::Uint(v));
        }
    }

    #[test]
    fn rejects_non_canonical() {
        assert_eq!(decode(&[0x18, 0x17]), Err(CborError::NonCanonical));
        assert_eq!(decode(&[0x19, 0x00, 0xff]), Err(CborError::NonCanonical));
        assert_eq!(decode(&[0x9f, 0xff]), Err(CborError::Indefinite));
    }

    #[test]
    fn rejects_deep_nesting() {
        let mut bytes = vec![0x81u8; 16];
        bytes.push(0x00);
        assert_eq!(decode(&bytes), Err(CborError::TooDeep));
        let mut ok = vec![0x81u8; 15];
        ok.push(0x00);
        assert!(decode(&ok).is_ok());
    }

    #[test]
    fn rejects_huge_lengths() {
        assert_eq!(
            decode(&[0x9b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]),
            Err(CborError::Truncated)
        );
        assert_eq!(decode(&[0x5a, 0x00, 0x01, 0x00, 0x00]), Err(CborError::Truncated));
    }
}
