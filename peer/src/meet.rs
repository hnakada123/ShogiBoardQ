//! 待ち合わせのストリーム（/shogiboardq/meet/1.0.0。protocol/README.md 2.2節）
//!
//! 1本のストリームで、要求を1つ送り、応答を1つ受ける（どちらも、4バイトの長さ＋決定的CBOR）。
//! 協力ノード（--meet）は受け付ける側、利用者の通信プロセスは頼む側になる。

use std::collections::HashMap;
use std::sync::{Mutex, MutexGuard};
use std::time::{Duration, Instant};

use futures::{AsyncWriteExt, StreamExt};
use libp2p::swarm::ConnectionId;
use libp2p::{PeerId, Stream, StreamProtocol};
use sbq_protocol::meet::{Request, Response, Status, MAX_MESSAGE_BYTES, MEET_PROTOCOL};

use crate::addr::SourceKey;
use crate::limits::{SourceSlots, TierLimits};
use crate::meetstore::MeetStore;
use crate::wire;

/// 要求1つを終えるまでの時間（頼む側・受け付ける側とも）
const EXCHANGE_TIMEOUT: Duration = Duration::from_secs(5);
/// 受け付ける側が、要求を読み終えるまで待つ時間（何も送らないストリームで、処理の枠を使い続けさせない）
const READ_TIMEOUT: Duration = Duration::from_secs(2);
/// 同時に処理する要求の上限（全体）
const MAX_PENDING: usize = 256;
/// 同時に処理する要求の上限（1つの接続元・同じアドレス帯。addr::SourceKey）。PeerId はいくらでも作れるので、
/// ピアごとには数えない（監査 2026-11-21 S2）
const PENDING_PER_SOURCE: TierLimits = TierLimits {
    per_source: 8,
    per_prefix: 32,
};
/// 1ピアについて覚える接続元の数（接続は1ピアあたり4本まで）
const MAX_SOURCES_PER_PEER: usize = 8;

async fn read_message(stream: &mut Stream) -> Option<Vec<u8>> {
    wire::read(stream, MAX_MESSAGE_BYTES).await.ok()
}

async fn write_message(stream: &mut Stream, body: &[u8]) -> Option<()> {
    wire::write(stream, body).await.ok()
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
    /// 受け付けた接続の接続元（ピアごと・接続ごと）。接続を受け付けたときに振る舞い（behaviours::MeetSources）が
    /// 書くので、その接続の要求が届くときには、もう分かっている
    sources: HashMap<PeerId, Vec<(ConnectionId, SourceKey)>>,
    /// 処理している要求
    pending: SourceSlots,
}

/// 協力ノードの待ち合わせ（受け付けのタスクと主ループ・振る舞いで共有する）
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

    /// 受け付けた接続の接続元を覚える（件数と頻度を、接続元ごとに数える）
    pub fn add_source(&self, peer: PeerId, connection: ConnectionId, source: SourceKey) {
        let mut state = self.lock();
        let list = state.sources.entry(peer).or_default();
        if list.len() < MAX_SOURCES_PER_PEER && !list.iter().any(|(id, _)| *id == connection) {
            list.push((connection, source));
        }
    }

    /// 閉じた接続の接続元を忘れる（預かったものは、期限まで持つ）
    pub fn remove_source(&self, peer: &PeerId, connection: ConnectionId) {
        let mut state = self.lock();
        let Some(list) = state.sources.get_mut(peer) else {
            return;
        };
        list.retain(|(id, _)| *id != connection);
        if list.is_empty() {
            state.sources.remove(peer);
        }
    }

    /// ピアの接続元（受け付けた接続のもの。分からなければ None）
    fn source_of(&self, peer: &PeerId) -> Option<SourceKey> {
        self.lock()
            .sources
            .get(peer)
            .and_then(|list| list.first())
            .map(|(_, source)| source.clone())
    }

    /// 期限の過ぎたものを消し、預かっている件数を返す
    pub fn expire(&self, now: Instant) -> usize {
        let mut state = self.lock();
        state.store.expire(now);
        state.store.len()
    }

    /// 処理の枠を1つ使う（接続元の分からない要求は、まとめて1つの接続元として数える）
    fn enter(&self, source: &SourceKey) -> bool {
        self.lock().pending.enter(source, MAX_PENDING, PENDING_PER_SOURCE)
    }

    fn leave(&self, source: &SourceKey) {
        self.lock().pending.leave(source);
    }

    fn handle(&self, peer: &PeerId, source: &SourceKey, request: Request) -> Response {
        self.lock().store.handle(peer, source, request, Instant::now())
    }

    /// 要求（body。接続元が分からなければ読まない）への応答。接続元の分からない要求（中継経由・こちらから掛けた
    /// 接続の上の要求）には、上限（status 2）を返し、預かりものにも頻度の記録にも触れない（ピアごとに数えると、
    /// PeerId を替えて接続元ごとの上限を逃れられる。監査 2026-11-21 S5）
    fn answer(&self, peer: &PeerId, source: Option<&SourceKey>, body: Option<&[u8]>) -> Response {
        let Some(source) = source else {
            return Response::status(Status::Limit);
        };
        match body.and_then(Request::decode) {
            Some(request) => self.handle(peer, source, request),
            None => Response::status(Status::Invalid),
        }
    }
}

/// 要求を1つ読み、応答を返す。接続元の分からない要求は読まない（MeetService::answer）
async fn serve(peer: PeerId, source: Option<SourceKey>, mut stream: Stream, service: &MeetService) {
    let body = if source.is_some() {
        let Ok(Some(body)) = tokio::time::timeout(READ_TIMEOUT, read_message(&mut stream)).await else {
            return;
        };
        Some(body)
    } else {
        None
    };
    let response = service.answer(&peer, source.as_ref(), body.as_deref());
    let _ = write_message(&mut stream, &response.encode()).await;
    let _ = stream.close().await;
}

/// 届いた待ち合わせのストリームを受け付ける（協力ノード）
pub async fn accept_task(mut incoming: libp2p_stream::IncomingStreams, service: std::sync::Arc<MeetService>) {
    while let Some((peer, stream)) = incoming.next().await {
        let source = service.source_of(&peer);
        let slot = source.clone().unwrap_or_else(SourceKey::unknown);
        // 処理の枠がなければ、読まずに閉じる（捨てると閉じる）
        if !service.enter(&slot) {
            continue;
        }
        let service = service.clone();
        tokio::spawn(async move {
            let _ = tokio::time::timeout(EXCHANGE_TIMEOUT, serve(peer, source, stream, &service)).await;
            service.leave(&slot);
        });
    }
}

#[cfg(test)]
#[allow(clippy::arithmetic_side_effects)]
mod tests {
    use super::*;
    use sbq_protocol::network::Profile;

    fn source(ip: [u8; 4]) -> SourceKey {
        SourceKey::of_ip(ip.into(), Profile::Internet)
    }

    /// 処理の枠は接続元ごとに数える。PeerId を替えても同じ接続元なら同じ枠で、ほかの接続元の要求は止められない
    #[test]
    fn pending_requests_are_limited_per_source() {
        let service = MeetService::default();
        let home = source([203, 0, 114, 7]);
        let neighbour = source([203, 0, 114, 8]);
        let other = source([198, 51, 101, 1]);
        for _ in 0..PENDING_PER_SOURCE.per_source {
            assert!(service.enter(&home));
        }
        assert!(!service.enter(&home));
        assert!(service.enter(&neighbour));
        assert!(service.enter(&other));
        service.leave(&home);
        assert!(service.enter(&home));
        // 接続元の分からない要求は、まとめて1つの接続元として数える
        let unknown = SourceKey::unknown();
        for _ in 0..PENDING_PER_SOURCE.per_source {
            assert!(service.enter(&unknown));
        }
        assert!(!service.enter(&unknown));
    }

    /// 接続元は、受け付けた接続ごとに覚え、接続が閉じたら忘れる
    #[test]
    fn sources_follow_the_connections() {
        let service = MeetService::default();
        let peer = PeerId::random();
        let (c1, c2) = (ConnectionId::new_unchecked(1), ConnectionId::new_unchecked(2));
        assert_eq!(service.source_of(&peer), None);
        service.add_source(peer, c1, source([203, 0, 114, 7]));
        service.add_source(peer, c2, source([203, 0, 114, 7]));
        assert_eq!(service.source_of(&peer), Some(source([203, 0, 114, 7])));
        service.remove_source(&peer, c1);
        assert!(service.source_of(&peer).is_some());
        service.remove_source(&peer, c2);
        assert_eq!(service.source_of(&peer), None);
        // 知らない接続を忘れても何も起きない
        service.remove_source(&peer, c1);
    }

    /// 件数と頻度は、覚えた接続元ごとに数える（PeerId を替えても同じ枠）
    #[test]
    fn requests_are_counted_by_source() {
        let service = MeetService::default();
        let a = PeerId::random();
        let b = PeerId::random();
        let home = source([203, 0, 114, 7]);
        let get = Request::Get { slot: [1; 32] };
        let mut limited = 0;
        for _ in 0..crate::meetstore::REQUEST_LIMITS.source_burst {
            if service.handle(&a, &home, get.clone()).status == Status::Limit {
                limited += 1;
            }
        }
        assert_eq!(limited, 0);
        // 同じ接続元の別のピアも、同じ枠を使う
        assert_eq!(service.handle(&b, &home, get.clone()).status, Status::Limit);
        // 別の接続元は別の枠
        assert_eq!(service.handle(&b, &source([198, 51, 101, 1]), get).status, Status::None);
        assert_eq!(service.expire(Instant::now()), 0);
    }

    /// 接続元の分からない要求には、読まずに上限を返す。ピアごとに数えないので、PeerId を替えても預けられない
    /// （監査 2026-11-21 S5）
    #[test]
    fn requests_from_unknown_sources_get_limit() {
        let service = MeetService::default();
        let put = Request::Put {
            slot: [2; 32],
            blob: vec![0u8; 48],
            ttl_secs: 60,
            one_time: false,
        }
        .encode();
        for _ in 0..3 {
            let response = service.answer(&PeerId::random(), None, Some(&put));
            assert_eq!(response.status, Status::Limit);
        }
        assert_eq!(service.expire(Instant::now()), 0);
        // 接続元が分かれば、同じ要求を受け付ける。読めない要求は status 4
        let home = source([203, 0, 114, 7]);
        assert_eq!(
            service.answer(&PeerId::random(), Some(&home), Some(&put)).status,
            Status::Ok
        );
        assert_eq!(
            service.answer(&PeerId::random(), Some(&home), Some(&[0xff])).status,
            Status::Invalid
        );
        assert_eq!(service.expire(Instant::now()), 1);
    }
}
