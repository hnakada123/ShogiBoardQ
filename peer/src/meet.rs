//! 待ち合わせのストリーム（/shogiboardq/meet/1.0.0。protocol/README.md 2.2節）
//!
//! 1本のストリームで、要求を1つ送り、応答を1つ受ける（どちらも、4バイトの長さ＋決定的CBOR）。
//! 協力ノード（--meet）は受け付ける側、利用者の通信プロセスは頼む側になる。

use std::collections::HashMap;
use std::sync::{Mutex, MutexGuard};
use std::time::{Duration, Instant};

use futures::{AsyncReadExt, AsyncWriteExt, StreamExt};
use libp2p::{PeerId, Stream, StreamProtocol};
use sbq_protocol::meet::{Request, Response, Status, MAX_MESSAGE_BYTES, MEET_PROTOCOL};

use crate::meetstore::MeetStore;

/// 要求1つを終えるまでの時間（頼む側・受け付ける側とも）
const EXCHANGE_TIMEOUT: Duration = Duration::from_secs(5);
/// 同時に処理する要求の上限（全体）
const MAX_PENDING: usize = 64;
/// 同時に処理する要求の上限（ピアごと）
const MAX_PENDING_PER_PEER: usize = 4;

async fn read_message(stream: &mut Stream) -> Option<Vec<u8>> {
    let mut len_buf = [0u8; 4];
    stream.read_exact(&mut len_buf).await.ok()?;
    let len = usize::try_from(u32::from_be_bytes(len_buf)).ok()?;
    if len == 0 || len > MAX_MESSAGE_BYTES {
        return None;
    }
    let mut body = vec![0u8; len];
    stream.read_exact(&mut body).await.ok()?;
    Some(body)
}

async fn write_message(stream: &mut Stream, body: &[u8]) -> Option<()> {
    let len = u32::try_from(body.len()).ok()?;
    stream.write_all(&len.to_be_bytes()).await.ok()?;
    stream.write_all(body).await.ok()?;
    stream.flush().await.ok()
}

/// 協力ノードに要求を1つ送り、応答を受ける。つながらない・待ち合わせに対応していない・時間切れ・読めない応答は None
pub async fn exchange(control: &mut libp2p_stream::Control, node: PeerId, request: &Request) -> Option<Response> {
    let body = request.encode();
    tokio::time::timeout(EXCHANGE_TIMEOUT, async {
        let mut stream = control
            .open_stream(node, StreamProtocol::new(MEET_PROTOCOL))
            .await
            .ok()?;
        write_message(&mut stream, &body).await?;
        let response = read_message(&mut stream).await?;
        let _ = stream.close().await;
        Response::decode(&response)
    })
    .await
    .ok()
    .flatten()
}

/// 要求を、協力ノードに順に送る。応答を返した協力ノードごとに `on_response` を呼び、真を返したらそこでやめる
pub async fn ask_nodes(
    control: &mut libp2p_stream::Control,
    nodes: &[PeerId],
    request: &Request,
    mut on_response: impl FnMut(Response) -> bool,
) {
    for node in nodes {
        if let Some(response) = exchange(control, *node, request).await {
            if on_response(response) {
                break;
            }
        }
    }
}

#[derive(Default)]
struct ServiceState {
    store: MeetStore,
    /// つながっているピアの接続元（アドレス帯）。主ループが、接続ができた・なくなったときに書く
    sources: HashMap<PeerId, Vec<u8>>,
    pending: usize,
    pending_per_peer: HashMap<PeerId, usize>,
}

/// 協力ノードの待ち合わせ（受け付けのタスクと主ループで共有する）
#[derive(Default)]
pub struct MeetService {
    state: Mutex<ServiceState>,
}

impl MeetService {
    fn lock(&self) -> MutexGuard<'_, ServiceState> {
        match self.state.lock() {
            Ok(g) => g,
            Err(poisoned) => poisoned.into_inner(),
        }
    }

    /// ピアの接続元を覚える（件数と頻度を、接続元ごとに数える）
    pub fn set_source(&self, peer: PeerId, source: Vec<u8>) {
        self.lock().sources.insert(peer, source);
    }

    /// つながらなくなったピアの接続元を忘れる（預かったものは、期限まで持つ）
    pub fn forget_source(&self, peer: &PeerId) {
        self.lock().sources.remove(peer);
    }

    /// 期限の過ぎたものを消し、預かっている件数を返す
    pub fn expire(&self, now: Instant) -> usize {
        let mut state = self.lock();
        state.store.expire(now);
        state.store.len()
    }

    fn enter(&self, peer: &PeerId) -> bool {
        let mut state = self.lock();
        let mine = state.pending_per_peer.get(peer).copied().unwrap_or(0);
        if state.pending >= MAX_PENDING || mine >= MAX_PENDING_PER_PEER {
            return false;
        }
        state.pending = state.pending.saturating_add(1);
        state.pending_per_peer.insert(*peer, mine.saturating_add(1));
        true
    }

    fn leave(&self, peer: &PeerId) {
        let mut state = self.lock();
        state.pending = state.pending.saturating_sub(1);
        let left = state.pending_per_peer.get(peer).copied().unwrap_or(0).saturating_sub(1);
        if left == 0 {
            state.pending_per_peer.remove(peer);
        } else {
            state.pending_per_peer.insert(*peer, left);
        }
    }

    fn handle(&self, peer: &PeerId, request: Request) -> Response {
        let mut state = self.lock();
        // 接続元の分からないピア（中継経由など）は、ピアごとに数える
        let source = state.sources.get(peer).cloned().unwrap_or_else(|| peer.to_bytes());
        state.store.handle(peer, &source, request, Instant::now())
    }
}

async fn serve(peer: PeerId, mut stream: Stream, service: &MeetService) {
    let Some(body) = read_message(&mut stream).await else {
        return;
    };
    let response = match Request::decode(&body) {
        Some(request) => service.handle(&peer, request),
        None => Response::status(Status::Invalid),
    };
    let _ = write_message(&mut stream, &response.encode()).await;
    let _ = stream.close().await;
}

/// 届いた待ち合わせのストリームを受け付ける（協力ノード）
pub async fn accept_task(mut incoming: libp2p_stream::IncomingStreams, service: std::sync::Arc<MeetService>) {
    while let Some((peer, stream)) = incoming.next().await {
        // 処理の枠がなければ、読まずに閉じる（捨てると閉じる）
        if !service.enter(&peer) {
            continue;
        }
        let service = service.clone();
        tokio::spawn(async move {
            let _ = tokio::time::timeout(EXCHANGE_TIMEOUT, serve(peer, stream, &service)).await;
            service.leave(&peer);
        });
    }
}

#[cfg(test)]
#[allow(clippy::arithmetic_side_effects)]
mod tests {
    use super::*;

    /// 処理の枠はピアごとに数える（1つのピアが、ほかのピアの要求を止められない）
    #[test]
    fn pending_requests_are_limited_per_peer() {
        let service = MeetService::default();
        let a = PeerId::random();
        let b = PeerId::random();
        for _ in 0..MAX_PENDING_PER_PEER {
            assert!(service.enter(&a));
        }
        assert!(!service.enter(&a));
        assert!(service.enter(&b));
        service.leave(&a);
        assert!(service.enter(&a));
    }

    /// 件数と頻度は、覚えた接続元ごとに数える。接続元の分からないピアは、ピアごと
    #[test]
    fn requests_are_counted_by_source() {
        let service = MeetService::default();
        let a = PeerId::random();
        let b = PeerId::random();
        service.set_source(a, vec![203, 0, 113]);
        service.set_source(b, vec![203, 0, 113]);
        let get = Request::Get { slot: [1; 32] };
        let mut limited = 0;
        for _ in 0..crate::meetstore::REQUEST_BURST {
            if service.handle(&a, get.clone()).status == Status::Limit {
                limited += 1;
            }
        }
        assert_eq!(limited, 0);
        // 同じ接続元の別のピアも、同じ枠を使う
        assert_eq!(service.handle(&b, get.clone()).status, Status::Limit);
        // 接続元を忘れたピアは、ピアごとに数える
        service.forget_source(&b);
        assert_eq!(service.handle(&b, get).status, Status::None);
        assert_eq!(service.expire(Instant::now()), 0);
    }
}
