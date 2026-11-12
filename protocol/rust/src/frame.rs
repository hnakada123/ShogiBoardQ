//! 対局フレームの外枠（protocol/README.md 2章）
//!
//! 通信プロセスは外枠だけを確かめ、payload の意味は読まない。例外は INVITE_TOKEN。

use crate::cbor::{decode, encode, Value};

/// 対局フレームの libp2p プロトコル
pub const GAME_PROTOCOL: &str = "/shogiboardq/game/1.0.0";
/// フレームの長さの上限（長さの前置き4バイトを含めない）
pub const MAX_FRAME_BYTES: usize = 64 * 1024;
/// 外枠の version
pub const FRAME_VERSION: u64 = 1;

pub const TYPE_HELLO: u16 = 1;
pub const TYPE_START_AGREEMENT: u16 = 2;
pub const TYPE_START_SIGNATURE: u16 = 3;
pub const TYPE_STATE_PROPOSE: u16 = 4;
pub const TYPE_STATE_ACCEPT: u16 = 5;
pub const TYPE_CONTROL: u16 = 6;
pub const TYPE_SYNC_REQUEST: u16 = 7;
pub const TYPE_SYNC_CHUNK: u16 = 8;
pub const TYPE_RESULT_SIGNATURE: u16 = 9;
pub const TYPE_REJECT: u16 = 10;
pub const TYPE_INVITE_TOKEN: u16 = 11;

/// 外枠を確かめたフレーム
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FrameHeader {
    pub frame_type: u16,
    pub request_id: u64,
    pub game_id: [u8; 32],
    pub payload: Value,
}

/// 外枠の誤り
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FrameError {
    Empty,
    TooLarge,
    NotCbor,
    BadShape,
    BadVersion,
    BadType,
    BadGameId,
}

impl std::fmt::Display for FrameError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            FrameError::Empty => "empty frame",
            FrameError::TooLarge => "frame too large",
            FrameError::NotCbor => "not deterministic cbor",
            FrameError::BadShape => "frame is not a 5-element array",
            FrameError::BadVersion => "unsupported frame version",
            FrameError::BadType => "unknown frame type",
            FrameError::BadGameId => "gameId is not 32 bytes",
        };
        f.write_str(s)
    }
}

impl std::error::Error for FrameError {}

/// フレームの外枠を確かめる
pub fn check(bytes: &[u8]) -> Result<FrameHeader, FrameError> {
    if bytes.is_empty() {
        return Err(FrameError::Empty);
    }
    if bytes.len() > MAX_FRAME_BYTES {
        return Err(FrameError::TooLarge);
    }
    let value = decode(bytes).map_err(|_| FrameError::NotCbor)?;
    let items = value.as_array().ok_or(FrameError::BadShape)?;
    if items.len() != 5 {
        return Err(FrameError::BadShape);
    }
    let version = items.first().and_then(Value::as_uint).ok_or(FrameError::BadShape)?;
    if version != FRAME_VERSION {
        return Err(FrameError::BadVersion);
    }
    let frame_type = items.get(1).and_then(Value::as_uint).ok_or(FrameError::BadShape)?;
    let frame_type = u16::try_from(frame_type).map_err(|_| FrameError::BadType)?;
    if !(TYPE_HELLO..=TYPE_INVITE_TOKEN).contains(&frame_type) {
        return Err(FrameError::BadType);
    }
    let request_id = items.get(2).and_then(Value::as_uint).ok_or(FrameError::BadShape)?;
    let game_id_value = items.get(3).ok_or(FrameError::BadShape)?;
    if game_id_value.as_bytes().is_none() {
        return Err(FrameError::BadShape);
    }
    let game_id = game_id_value.as_fixed_bytes::<32>().ok_or(FrameError::BadGameId)?;
    let payload = items.get(4).ok_or(FrameError::BadShape)?;
    if payload.as_array().is_none() {
        return Err(FrameError::BadShape);
    }
    Ok(FrameHeader {
        frame_type,
        request_id,
        game_id,
        payload: payload.clone(),
    })
}

/// フレームを組み立てる
pub fn build(frame_type: u16, request_id: u64, game_id: &[u8; 32], payload: Vec<Value>) -> Vec<u8> {
    encode(&Value::Array(vec![
        Value::Uint(FRAME_VERSION),
        Value::Uint(u64::from(frame_type)),
        Value::Uint(request_id),
        Value::bytes(game_id),
        Value::Array(payload),
    ]))
}

/// INVITE_TOKEN のフレーム `[1, 11, 0, zero32, [token]]`
pub fn invite_token_frame(token: &[u8; 32]) -> Vec<u8> {
    build(TYPE_INVITE_TOKEN, 0, &[0u8; 32], vec![Value::bytes(token)])
}

/// INVITE_TOKEN のフレームならトークンを返す（形が違えば None）
pub fn invite_token(header: &FrameHeader) -> Option<[u8; 32]> {
    if header.frame_type != TYPE_INVITE_TOKEN || header.request_id != 0 || header.game_id != [0u8; 32] {
        return None;
    }
    let items = header.payload.as_array()?;
    if items.len() != 1 {
        return None;
    }
    items.first()?.as_fixed_bytes::<32>()
}

#[cfg(test)]
#[allow(clippy::unwrap_used)]
mod tests {
    use super::*;

    /// INVITE_TOKEN は `[1, 11, 0, zero32, [token]]` の形だけを読む（protocol/README.md 2.1節）
    #[test]
    fn invite_token_round_trip_and_shape() {
        let token = [7u8; 32];
        let header = check(&invite_token_frame(&token)).unwrap();
        assert_eq!(header.frame_type, TYPE_INVITE_TOKEN);
        assert_eq!(invite_token(&header), Some(token));
        // 型・requestId・gameId・payload の形が違えば読まない
        let other_type = check(&build(1, 0, &[0u8; 32], vec![Value::bytes(&token)])).unwrap();
        assert_eq!(invite_token(&other_type), None);
        let with_request = check(&build(TYPE_INVITE_TOKEN, 1, &[0u8; 32], vec![Value::bytes(&token)])).unwrap();
        assert_eq!(invite_token(&with_request), None);
        let with_game = check(&build(TYPE_INVITE_TOKEN, 0, &[1u8; 32], vec![Value::bytes(&token)])).unwrap();
        assert_eq!(invite_token(&with_game), None);
        let short = check(&build(TYPE_INVITE_TOKEN, 0, &[0u8; 32], vec![Value::bytes(&[7u8; 31])])).unwrap();
        assert_eq!(invite_token(&short), None);
        let two = check(&build(
            TYPE_INVITE_TOKEN,
            0,
            &[0u8; 32],
            vec![Value::bytes(&token), Value::bytes(&token)],
        ))
        .unwrap();
        assert_eq!(invite_token(&two), None);
    }
}
