//! 対局・UI側との IPC（protocol/ipc.md）
//!
//! 4バイトのネットワークバイト順の長さ＋UTF-8 の JSON。外枠（版・世代・知らないメソッド・上限）の違反は
//! 終了コード 2 で終わる。引数の形の違いはエラーの応答（invalid_request）で返す。

use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

use base64::engine::general_purpose::STANDARD as B64;
use base64::Engine;
use serde::de::DeserializeOwned;
use serde::Deserialize;
use serde_json::{json, Value};
use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};
use tokio::sync::mpsc;

/// 1メッセージの上限（バイト）
pub const MAX_MESSAGE_BYTES: usize = 128 * 1024;
/// IPC の版
pub const IPC_VERSION: u64 = 2;

/// 読み取りの失敗
#[derive(Debug, PartialEq, Eq)]
pub enum ReadError {
    /// 標準入力の終端（親の終了）
    Eof,
    /// IPC の違反
    Violation(String),
}

/// 長さ付きのメッセージを1つ読む
pub async fn read_message<R: AsyncRead + Unpin>(reader: &mut R) -> Result<Vec<u8>, ReadError> {
    let mut len_buf = [0u8; 4];
    match reader.read_exact(&mut len_buf).await {
        Ok(_) => {}
        Err(e) if e.kind() == std::io::ErrorKind::UnexpectedEof => return Err(ReadError::Eof),
        Err(e) => return Err(ReadError::Violation(format!("read error: {e}"))),
    }
    let len = usize::try_from(u32::from_be_bytes(len_buf)).map_err(|_| ReadError::Violation("length".into()))?;
    if !(2..=MAX_MESSAGE_BYTES).contains(&len) {
        return Err(ReadError::Violation(format!("message length {len} out of range")));
    }
    let mut body = vec![0u8; len];
    match reader.read_exact(&mut body).await {
        Ok(_) => Ok(body),
        Err(e) if e.kind() == std::io::ErrorKind::UnexpectedEof => Err(ReadError::Eof),
        Err(e) => Err(ReadError::Violation(format!("read error: {e}"))),
    }
}

/// 長さを前に付けたバイト列（上限を超えれば None）
pub fn frame_message(value: &Value) -> Option<Vec<u8>> {
    let body = serde_json::to_vec(value).ok()?;
    if body.len() > MAX_MESSAGE_BYTES || body.len() < 2 {
        return None;
    }
    let len = u32::try_from(body.len()).ok()?;
    let mut out = Vec::with_capacity(body.len().saturating_add(4));
    out.extend_from_slice(&len.to_be_bytes());
    out.extend_from_slice(&body);
    Some(out)
}

/// 64ビットの整数の10進の文字列を読む
pub fn parse_u64(s: &str) -> Option<u64> {
    if s.is_empty() || s.len() > 20 || !s.bytes().all(|b| b.is_ascii_digit()) {
        return None;
    }
    if s.len() > 1 && s.starts_with('0') {
        return None;
    }
    s.parse().ok()
}

/// 受けた要求の外枠
#[derive(Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
struct RawRequest {
    protocol_version: u64,
    process_generation: String,
    request_id: String,
    method: String,
    params: Value,
}

/// 外枠を確かめた要求
#[derive(Debug, Clone)]
pub struct Request {
    pub generation: String,
    pub request_id: String,
    pub method: Method,
    pub params: Value,
}

/// 要求のメソッド
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Method {
    Hello,
    Start,
    CreateInvite,
    CancelInvites,
    InspectInvite,
    ConnectInvite,
    SetGamePeer,
    ConnectPeer,
    SendFrame,
    CloseConnection,
    Shutdown,
}

impl Method {
    fn parse(name: &str) -> Option<Method> {
        Some(match name {
            "hello" => Method::Hello,
            "start" => Method::Start,
            "createInvite" => Method::CreateInvite,
            "cancelInvites" => Method::CancelInvites,
            "inspectInvite" => Method::InspectInvite,
            "connectInvite" => Method::ConnectInvite,
            "setGamePeer" => Method::SetGamePeer,
            "connectPeer" => Method::ConnectPeer,
            "sendFrame" => Method::SendFrame,
            "closeConnection" => Method::CloseConnection,
            "shutdown" => Method::Shutdown,
            _ => return None,
        })
    }
}

/// 要求の外枠を確かめる。違反なら理由（呼び出し側は終了コード 2 で終わる）
pub fn parse_request(body: &[u8], expected_generation: Option<&str>) -> Result<Request, String> {
    let raw: RawRequest = serde_json::from_slice(body).map_err(|e| format!("invalid request envelope: {e}"))?;
    if raw.protocol_version != IPC_VERSION {
        return Err(format!("unsupported protocolVersion {}", raw.protocol_version));
    }
    if parse_u64(&raw.process_generation).is_none() {
        return Err("invalid processGeneration".into());
    }
    if let Some(expected) = expected_generation {
        if raw.process_generation != expected {
            return Err("processGeneration mismatch".into());
        }
    }
    if parse_u64(&raw.request_id).is_none() {
        return Err("invalid requestId".into());
    }
    let method = Method::parse(&raw.method).ok_or_else(|| format!("unknown method {}", raw.method))?;
    if !raw.params.is_object() {
        return Err("params is not an object".into());
    }
    Ok(Request {
        generation: raw.process_generation,
        request_id: raw.request_id,
        method,
        params: raw.params,
    })
}

/// 引数を型に読む（知らないフィールドは拒否する型を使う）
pub fn params<T: DeserializeOwned>(request: &Request) -> Result<T, String> {
    serde_json::from_value(request.params.clone()).map_err(|e| format!("invalid params: {e}"))
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Empty {}

#[derive(Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct HelloParams {
    #[allow(dead_code)]
    pub ui_version: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct StartParams {
    pub network_name: String,
    pub bootstrap: Vec<String>,
    pub relay_only: bool,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct InviteParams {
    pub invite: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct ConnectInviteParams {
    pub invite: String,
    pub allow_private: bool,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct PeerIdParams {
    pub peer_id: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct SendFrameParams {
    pub connection: String,
    pub frame: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields, rename_all = "camelCase")]
pub struct ConnectionParams {
    pub connection: String,
}

// ---------------------------------------------------------------- 通知（ipc.md 4章）

/// 語の値（ipc.md の語はこの enum の as_str だけが決める）
macro_rules! words {
    ($(#[$doc:meta])* $name:ident { $($variant:ident => $word:literal),+ $(,)? }) => {
        $(#[$doc])*
        #[derive(Clone, Copy, Debug, PartialEq, Eq)]
        pub enum $name {
            $($variant),+
        }

        impl $name {
            pub fn as_str(self) -> &'static str {
                match self {
                    $($name::$variant => $word),+
                }
            }
        }
    };
}

words! {
    /// 対局ストリームの経路
    Route { Direct => "direct", Relay => "relay" }
}

words! {
    /// 接続を開いた向き
    Direction { Inbound => "inbound", Outbound => "outbound" }
}

words! {
    /// 接続の由来（招待か、対局の相手のつなぎ直しか）
    Origin { Invite => "invite", Game => "game" }
}

words! {
    /// 接続が閉じた理由
    CloseReason {
        Closed => "closed",
        Reset => "reset",
        Timeout => "timeout",
        Unreachable => "unreachable",
        NoRelayRoute => "no_relay_route",
        InvalidFrame => "invalid_frame",
        RateLimited => "rate_limited",
        Limit => "limit",
    }
}

words! {
    /// ネットワークの状態
    NetworkState { Connecting => "connecting", Online => "online", Offline => "offline" }
}

words! {
    /// インターネットから届くか（AutoNAT）
    Reachability { Unknown => "unknown", Public => "public", Private => "private" }
}

/// networkStatus の中身（変わったときにだけ知らせるので比べられる）
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct NetworkStatus {
    pub state: NetworkState,
    pub connected_peers: usize,
    pub reachability: Reachability,
    pub relay_reservations: usize,
}

/// 通知（名前とフィールドの名前はここだけ）
pub enum Notification<'a> {
    NetworkStatus(NetworkStatus),
    ConnectionOpened {
        connection: u64,
        peer_id: &'a str,
        peer_id_bytes: &'a [u8],
        route: Route,
        direction: Direction,
        origin: Origin,
    },
    ConnectionRoute {
        connection: u64,
        route: Route,
    },
    FrameReceived {
        connection: u64,
        frame: &'a [u8],
    },
    ConnectionClosed {
        connection: u64,
        reason: CloseReason,
    },
}

impl Notification<'_> {
    fn name(&self) -> &'static str {
        match self {
            Notification::NetworkStatus(_) => "networkStatus",
            Notification::ConnectionOpened { .. } => "connectionOpened",
            Notification::ConnectionRoute { .. } => "connectionRoute",
            Notification::FrameReceived { .. } => "frameReceived",
            Notification::ConnectionClosed { .. } => "connectionClosed",
        }
    }

    fn params(&self) -> Value {
        match self {
            Notification::NetworkStatus(s) => json!({
                "state": s.state.as_str(),
                "connectedPeers": s.connected_peers,
                "reachability": s.reachability.as_str(),
                "relayReservations": s.relay_reservations,
            }),
            Notification::ConnectionOpened {
                connection,
                peer_id,
                peer_id_bytes,
                route,
                direction,
                origin,
            } => json!({
                "connection": connection.to_string(),
                "peerId": peer_id,
                "peerIdBytes": B64.encode(peer_id_bytes),
                "route": route.as_str(),
                "direction": direction.as_str(),
                "origin": origin.as_str(),
            }),
            Notification::ConnectionRoute { connection, route } => {
                json!({ "connection": connection.to_string(), "route": route.as_str() })
            }
            Notification::FrameReceived { connection, frame } => {
                json!({ "connection": connection.to_string(), "frame": B64.encode(frame) })
            }
            Notification::ConnectionClosed { connection, reason } => {
                json!({ "connection": connection.to_string(), "reason": reason.as_str() })
            }
        }
    }
}

/// 書き出しの後に戻す受信キューの量（接続ごとの上限の計算に使う）
pub struct Release {
    pub counter: Arc<AtomicUsize>,
    pub bytes: usize,
}

/// 標準出力へ書くメッセージ
pub struct OutMessage {
    pub bytes: Vec<u8>,
    pub release: Option<Release>,
}

/// 標準出力への書き手
#[derive(Clone)]
pub struct Out {
    generation: Arc<std::sync::OnceLock<String>>,
    /// 書き出す列（協力ノードは対局・UI側を持たないので None。何も書かない）
    tx: Option<mpsc::UnboundedSender<OutMessage>>,
}

impl Out {
    pub fn new(tx: mpsc::UnboundedSender<OutMessage>) -> Out {
        Out {
            generation: Arc::new(std::sync::OnceLock::new()),
            tx: Some(tx),
        }
    }

    /// 何も書かない書き手（IPC を持たない協力ノード）
    pub fn discard() -> Out {
        Out {
            generation: Arc::new(std::sync::OnceLock::new()),
            tx: None,
        }
    }

    pub fn set_generation(&self, generation: &str) {
        let _ = self.generation.set(generation.to_owned());
    }

    fn generation(&self) -> String {
        self.generation.get().cloned().unwrap_or_else(|| "0".to_owned())
    }

    fn send(&self, value: &Value, release: Option<Release>) {
        let Some(tx) = &self.tx else {
            if let Some(r) = release {
                r.counter.fetch_sub(r.bytes, Ordering::SeqCst);
            }
            return;
        };
        match frame_message(value) {
            Some(bytes) => {
                let _ = tx.send(OutMessage { bytes, release });
            }
            None => {
                if let Some(r) = release {
                    r.counter.fetch_sub(r.bytes, Ordering::SeqCst);
                }
                eprintln!("shogiboardq-peer: dropped an oversized IPC message");
            }
        }
    }

    pub fn result(&self, request_id: &str, result: Value) {
        let v = json!({
            "protocolVersion": IPC_VERSION,
            "processGeneration": self.generation(),
            "requestId": request_id,
            "result": result,
        });
        self.send(&v, None);
    }

    pub fn error(&self, request_id: &str, code: &str, message: &str) {
        let v = json!({
            "protocolVersion": IPC_VERSION,
            "processGeneration": self.generation(),
            "requestId": request_id,
            "error": { "code": code, "message": message },
        });
        self.send(&v, None);
    }

    pub fn notify(&self, notification: Notification<'_>) {
        self.notify_with_release(notification, None);
    }

    /// 書き出した後に release の量を受信キューから戻す（frameReceived）
    pub fn notify_with_release(&self, notification: Notification<'_>, release: Option<Release>) {
        let v = json!({
            "protocolVersion": IPC_VERSION,
            "processGeneration": self.generation(),
            "notification": notification.name(),
            "params": notification.params(),
        });
        self.send(&v, release);
    }
}

/// 標準出力へ書き続けるタスク
pub async fn writer_task<W: AsyncWrite + Unpin>(mut writer: W, mut rx: mpsc::UnboundedReceiver<OutMessage>) {
    while let Some(msg) = rx.recv().await {
        let ok = writer.write_all(&msg.bytes).await.is_ok() && writer.flush().await.is_ok();
        if let Some(r) = msg.release {
            r.counter.fetch_sub(r.bytes, Ordering::SeqCst);
        }
        if !ok {
            // 対局・UI側が読まなくなった（終了した）
            std::process::exit(0);
        }
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::indexing_slicing)]
mod tests {
    use super::*;

    fn req(json: &str) -> Vec<u8> {
        json.as_bytes().to_vec()
    }

    /// 通知の名前とフィールドは ipc.md 4章のとおり（対局・UI側の PeerIpc::checkNotification が検査する形）
    #[test]
    fn notifications_have_the_documented_names_and_fields() {
        let closed = Notification::ConnectionClosed {
            connection: 7,
            reason: CloseReason::NoRelayRoute,
        };
        assert_eq!(closed.name(), "connectionClosed");
        assert_eq!(
            closed.params(),
            json!({ "connection": "7", "reason": "no_relay_route" })
        );
        let frame = Notification::FrameReceived {
            connection: 3,
            frame: b"ab",
        };
        assert_eq!(frame.name(), "frameReceived");
        assert_eq!(frame.params(), json!({ "connection": "3", "frame": "YWI=" }));
        let opened = Notification::ConnectionOpened {
            connection: 5,
            peer_id: "12D3KooWabc",
            peer_id_bytes: &[0, 1, 2],
            route: Route::Relay,
            direction: Direction::Outbound,
            origin: Origin::Game,
        };
        assert_eq!(
            opened.params(),
            json!({ "connection": "5", "peerId": "12D3KooWabc", "peerIdBytes": "AAEC", "route": "relay",
                    "direction": "outbound", "origin": "game" })
        );
        let status = Notification::NetworkStatus(NetworkStatus {
            state: NetworkState::Online,
            connected_peers: 2,
            reachability: Reachability::Private,
            relay_reservations: 1,
        });
        assert_eq!(status.name(), "networkStatus");
        assert_eq!(
            status.params(),
            json!({ "state": "online", "connectedPeers": 2, "reachability": "private", "relayReservations": 1 })
        );
        let reasons: Vec<&str> = [
            CloseReason::Closed,
            CloseReason::Reset,
            CloseReason::Timeout,
            CloseReason::Unreachable,
            CloseReason::NoRelayRoute,
            CloseReason::InvalidFrame,
            CloseReason::RateLimited,
            CloseReason::Limit,
        ]
        .iter()
        .map(|r| r.as_str())
        .collect();
        assert_eq!(
            reasons,
            [
                "closed",
                "reset",
                "timeout",
                "unreachable",
                "no_relay_route",
                "invalid_frame",
                "rate_limited",
                "limit"
            ]
        );
    }

    #[test]
    fn parses_valid_request() {
        let r = parse_request(
            &req(r#"{"protocolVersion":2,"processGeneration":"3","requestId":"1","method":"hello","params":{"uiVersion":"x"}}"#),
            None,
        )
        .unwrap();
        assert_eq!(r.method, Method::Hello);
        let p: HelloParams = params(&r).unwrap();
        assert_eq!(p.ui_version, "x");
    }

    #[test]
    fn rejects_envelope_violations() {
        for bad in [
            r#"{"protocolVersion":1,"processGeneration":"3","requestId":"1","method":"hello","params":{}}"#,
            r#"{"protocolVersion":2,"processGeneration":"3","requestId":"1","method":"evil","params":{}}"#,
            r#"{"protocolVersion":2,"processGeneration":"3","requestId":"1","method":"hello","params":{},"extra":1}"#,
            r#"{"protocolVersion":2,"processGeneration":"03","requestId":"1","method":"hello","params":{}}"#,
            r#"{"protocolVersion":2,"processGeneration":"3","requestId":"x","method":"hello","params":{}}"#,
            r#"{"protocolVersion":2,"processGeneration":"3","requestId":"1","method":"hello","params":[]}"#,
            r#"{"protocolVersion":2,"processGeneration":3,"requestId":"1","method":"hello","params":{}}"#,
            r#"[]"#,
            r#"not json"#,
        ] {
            assert!(parse_request(&req(bad), None).is_err(), "{bad}");
        }
        assert!(parse_request(
            &req(r#"{"protocolVersion":2,"processGeneration":"4","requestId":"1","method":"hello","params":{}}"#),
            Some("3")
        )
        .is_err());
    }

    #[test]
    fn rejects_unknown_params() {
        let r = parse_request(
            &req(r#"{"protocolVersion":2,"processGeneration":"3","requestId":"1","method":"hello","params":{"uiVersion":"x","x":1}}"#),
            None,
        )
        .unwrap();
        assert!(params::<HelloParams>(&r).is_err());
        let r = parse_request(
            &req(r#"{"protocolVersion":2,"processGeneration":"3","requestId":"1","method":"cancelInvites","params":{"a":1}}"#),
            None,
        )
        .unwrap();
        assert!(params::<Empty>(&r).is_err());
    }

    #[tokio::test]
    async fn rejects_oversized_messages() {
        let mut data: &[u8] = &[0, 2, 0, 1];
        assert!(matches!(read_message(&mut data).await, Err(ReadError::Violation(_))));
        let mut data: &[u8] = &[0, 0, 0, 1, b'x'];
        assert!(matches!(read_message(&mut data).await, Err(ReadError::Violation(_))));
        let mut data: &[u8] = &[];
        assert_eq!(read_message(&mut data).await, Err(ReadError::Eof));
        let mut data: &[u8] = &[0, 0, 0, 2, b'{', b'}'];
        assert_eq!(read_message(&mut data).await.unwrap(), b"{}".to_vec());
    }

    #[test]
    fn u64_strings() {
        assert_eq!(parse_u64("0"), Some(0));
        assert_eq!(parse_u64("18446744073709551615"), Some(u64::MAX));
        assert_eq!(parse_u64("18446744073709551616"), None);
        assert_eq!(parse_u64("01"), None);
        assert_eq!(parse_u64("-1"), None);
        assert_eq!(parse_u64(""), None);
    }
}
