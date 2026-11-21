//! 対局ストリーム（/shogiboardq/game/1.0.0）の読み書き
//!
//! 4バイトの長さ＋決定的CBORのフレーム。外枠だけを確かめ、payload は読まない（INVITE_TOKEN を除く）。

use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::Instant;

use futures::io::{ReadHalf, WriteHalf};
use futures::AsyncWriteExt;
use libp2p::Stream;
use sbq_protocol::frame::{self, FrameHeader, MAX_FRAME_BYTES, TYPE_INVITE_TOKEN, TYPE_START_SIGNATURE};
use tokio::sync::mpsc;

use crate::ipc;
use crate::limits::TokenBucket;
use crate::wire::{self, ReadError};

/// 接続ごとの受信キューの上限（バイト）
pub const MAX_PENDING_BYTES: usize = 256 * 1024;

/// 長さ付きのフレームを1つ読み、外枠を確かめる（外枠の違反は Invalid）
pub async fn read_frame<R: futures::AsyncRead + Unpin>(reader: &mut R) -> Result<(Vec<u8>, FrameHeader), ReadError> {
    let body = wire::read(reader, MAX_FRAME_BYTES).await?;
    let header = frame::check(&body).map_err(|_| ReadError::Invalid)?;
    Ok((body, header))
}

/// 長さ付きのフレームを書く
pub async fn write_frame<W: futures::AsyncWrite + Unpin>(writer: &mut W, frame: &[u8]) -> std::io::Result<()> {
    wire::write(writer, frame).await
}

/// 双方が START_SIGNATURE を送るまでの、1秒あたりのフレームの数（protocol/ipc.md 5章。バーストは2倍）。
/// 再接続で開いたストリームは START_SIGNATURE を送り直さないので、この上限のまま（互換実装の仕様書 4.5節）
pub const OPENING_RATE: u64 = 5;
/// 双方が START_SIGNATURE を送った後の、1秒あたりのフレームの数（バーストは2倍）
pub const GAME_RATE: u64 = 20;

/// 接続の両方向で共有する状態
#[derive(Default)]
pub struct ConnShared {
    /// 自分が START_SIGNATURE を送った
    pub sent_start_signature: AtomicBool,
    /// 相手から START_SIGNATURE を受けた
    pub received_start_signature: AtomicBool,
    /// まだ対局・UI側へ書いていない受信の量
    pub pending: Arc<AtomicUsize>,
}

impl ConnShared {
    /// 送るフレームを見る（START_SIGNATURE を送ったら覚える）
    pub fn observe_sent(&self, header: &FrameHeader) {
        if header.frame_type == TYPE_START_SIGNATURE {
            self.sent_start_signature.store(true, Ordering::SeqCst);
        }
    }

    /// 受けたフレームを見る（START_SIGNATURE を受けたら覚える）
    pub fn observe_received(&self, header: &FrameHeader) {
        if header.frame_type == TYPE_START_SIGNATURE {
            self.received_start_signature.store(true, Ordering::SeqCst);
        }
    }

    /// 今の受信の頻度の上限（1秒あたりの件数, バーストの件数）
    pub fn rate(&self) -> (u64, u64) {
        let in_game =
            self.sent_start_signature.load(Ordering::SeqCst) && self.received_start_signature.load(Ordering::SeqCst);
        let rate = if in_game { GAME_RATE } else { OPENING_RATE };
        (rate, rate.saturating_mul(2))
    }
}

/// 接続のタスクから主ループへの知らせ
pub enum ConnEvent {
    Frame {
        conn: u64,
        frame: Vec<u8>,
        shared: Arc<ConnShared>,
    },
    Closed {
        conn: u64,
        reason: ipc::CloseReason,
    },
}

/// 受信のタスク
pub async fn reader_task(
    conn: u64,
    mut reader: ReadHalf<Stream>,
    shared: Arc<ConnShared>,
    events: mpsc::UnboundedSender<ConnEvent>,
) {
    // フレームの頻度の上限（1秒あたりの件数とバーストは、双方が START_SIGNATURE を送ると上がる）
    let mut limiter = TokenBucket::full(OPENING_RATE.saturating_mul(2), Instant::now());
    let reason = loop {
        let (bytes, header) = match read_frame(&mut reader).await {
            Ok(v) => v,
            Err(ReadError::Closed) => break ipc::CloseReason::Closed,
            Err(ReadError::Io) => break ipc::CloseReason::Reset,
            Err(ReadError::Invalid) => break ipc::CloseReason::InvalidFrame,
        };
        let (rate, burst) = shared.rate();
        if !limiter.take(Instant::now(), rate, burst) {
            break ipc::CloseReason::RateLimited;
        }
        if header.frame_type == TYPE_INVITE_TOKEN {
            // トークンは接続の最初にだけ送る。後から来たものは外枠の違反として閉じる
            break ipc::CloseReason::InvalidFrame;
        }
        shared.observe_received(&header);
        let len = bytes.len();
        let pending = shared.pending.fetch_add(len, Ordering::SeqCst).saturating_add(len);
        if pending > MAX_PENDING_BYTES {
            shared.pending.fetch_sub(len, Ordering::SeqCst);
            break ipc::CloseReason::Limit;
        }
        if events
            .send(ConnEvent::Frame {
                conn,
                frame: bytes,
                shared: shared.clone(),
            })
            .is_err()
        {
            return;
        }
    };
    let _ = events.send(ConnEvent::Closed { conn, reason });
}

/// 送信のタスク。送る列が閉じたらストリームを閉じる
pub async fn writer_task(
    conn: u64,
    mut writer: WriteHalf<Stream>,
    mut frames: mpsc::Receiver<Vec<u8>>,
    events: mpsc::UnboundedSender<ConnEvent>,
) {
    while let Some(frame) = frames.recv().await {
        if write_frame(&mut writer, &frame).await.is_err() {
            let _ = events.send(ConnEvent::Closed {
                conn,
                reason: ipc::CloseReason::Reset,
            });
            return;
        }
    }
    let _ = writer.close().await;
}

#[cfg(test)]
#[allow(clippy::unwrap_used)]
mod tests {
    use super::*;

    /// 双方が START_SIGNATURE を送った後だけ、頻度の上限が上がる（protocol/ipc.md 5章）
    #[test]
    fn rate_rises_after_both_start_signatures() {
        let shared = ConnShared::default();
        let start = frame::check(&frame::build(TYPE_START_SIGNATURE, 0, &[0u8; 32], vec![])).unwrap();
        let other = frame::check(&frame::build(1, 0, &[0u8; 32], vec![])).unwrap();
        assert_eq!(shared.rate(), (OPENING_RATE, OPENING_RATE * 2));
        shared.observe_received(&other);
        shared.observe_received(&start);
        assert_eq!(shared.rate(), (OPENING_RATE, OPENING_RATE * 2));
        shared.observe_sent(&start);
        assert_eq!(shared.rate(), (GAME_RATE, GAME_RATE * 2));
    }

    #[tokio::test]
    async fn frame_roundtrip_and_limits() {
        let frame = frame::build(1, 0, &[0u8; 32], vec![]);
        let mut buf = Vec::new();
        write_frame(&mut buf, &frame).await.unwrap();
        let mut reader = futures::io::Cursor::new(buf);
        let (bytes, header) = read_frame(&mut reader).await.unwrap();
        assert_eq!(bytes, frame);
        assert_eq!(header.frame_type, 1);

        let mut too_long = futures::io::Cursor::new(vec![0u8, 1, 0, 1]);
        assert!(matches!(read_frame(&mut too_long).await, Err(ReadError::Invalid)));
        let mut not_frame = futures::io::Cursor::new(vec![0u8, 0, 0, 1, 0]);
        assert!(matches!(read_frame(&mut not_frame).await, Err(ReadError::Invalid)));
    }
}
