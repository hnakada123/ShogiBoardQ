//! 表示名の判定（実装仕様 3.2節、protocol/displayname-v1.txt）
//!
//! 文字の区分は Rust の Unicode の表を使わず、protocol/ の表だけで決める（C++ 側と同じ結果にする）。

use std::sync::OnceLock;

/// 表示名の表 v1（protocol/displayname-v1.txt）
pub const TABLE_V1: &str = include_str!("../../displayname-v1.txt");

/// UTF-8 のバイト数の上限
pub const MAX_BYTES: usize = 96;
/// 文字（コードポイント）の数の上限
pub const MAX_CHARS: usize = 32;

/// 表の1行を範囲にする（"XXXX..YYYY"）。読めない行は None
fn parse_range(line: &str) -> Option<(u32, u32)> {
    let (lo, hi) = line.split_once("..")?;
    let lo = u32::from_str_radix(lo.trim(), 16).ok()?;
    let hi = u32::from_str_radix(hi.trim(), 16).ok()?;
    if lo > hi {
        return None;
    }
    Some((lo, hi))
}

/// 表の文字列を範囲の一覧にする。`#` の行と空行を読み飛ばす。読めない行があれば None
pub fn parse_table(text: &str) -> Option<Vec<(u32, u32)>> {
    let mut ranges = Vec::new();
    for raw in text.lines() {
        let line = raw.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        ranges.push(parse_range(line)?);
    }
    Some(ranges)
}

fn table() -> &'static [(u32, u32)] {
    static TABLE: OnceLock<Vec<(u32, u32)>> = OnceLock::new();
    // 表は配布物に組み込んだ固定の文字列で、試験（table_parses）で読めることを確かめている。
    // 読めなければ空の表になり、すべての表示名を断る（安全な側に倒す）
    TABLE.get_or_init(|| parse_table(TABLE_V1).unwrap_or_default())
}

fn allowed(c: char) -> bool {
    let cp = u32::from(c);
    table().iter().any(|&(lo, hi)| lo <= cp && cp <= hi)
}

/// 表示名が規則に合うか
pub fn is_valid(name: &str) -> bool {
    if name.is_empty() || name.len() > MAX_BYTES {
        return false;
    }
    if name.chars().count() > MAX_CHARS {
        return false;
    }
    if name.starts_with(' ') || name.ends_with(' ') {
        return false;
    }
    name.chars().all(allowed)
}

/// UTF-8 のバイト列が規則に合う表示名か
pub fn is_valid_bytes(bytes: &[u8]) -> bool {
    match std::str::from_utf8(bytes) {
        Ok(s) => is_valid(s),
        Err(_) => false,
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::indexing_slicing, clippy::arithmetic_side_effects)]
mod tests {
    use super::*;

    #[test]
    fn table_parses() {
        let ranges = parse_table(TABLE_V1).unwrap();
        assert!(ranges.len() >= 5);
        assert!(ranges.contains(&(0x20, 0x7e)));
    }

    #[test]
    fn basic_names() {
        assert!(is_valid("Taro"));
        assert!(is_valid("将棋太郎"));
        assert!(is_valid("ショウギ"));
        assert!(!is_valid(""));
        assert!(!is_valid(" a"));
        assert!(!is_valid("a "));
        assert!(!is_valid("a\u{200B}b"));
        assert!(!is_valid("「名人」"));
    }
}
