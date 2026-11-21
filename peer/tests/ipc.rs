//! 通信プロセスを起動し、IPC（protocol/ipc.md）で動かす結合試験（ローカルのネットワーク sbq-local-1）
//!
//! Linux と macOS で動かす（通信プロセスが自分で制限を掛ける OS）。Windows の通信プロセスは、起動する側（対局・UI側の PeerConfinement）が AppContainer とジョブを
//! 付けて起動しないと動かないので、Windows の試験は C++ の側（tst_p2p_sandbox・tst_p2p_endtoend）で行う。

#![cfg(any(target_os = "linux", target_os = "macos"))]
#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::indexing_slicing,
    clippy::arithmetic_side_effects
)]

use std::io::{BufRead, BufReader, Read, Write};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::mpsc;
use std::time::{Duration, Instant};

use base64::engine::general_purpose::STANDARD as B64;
use base64::Engine;
use sbq_protocol::code::Code;
use sbq_protocol::frame;
use sbq_protocol::hash::hex;
use sbq_protocol::invite::Invite;
use sbq_protocol::network;
use serde_json::{json, Value};

const BIN: &str = env!("CARGO_BIN_EXE_shogiboardq-peer");
const TIMEOUT: Duration = Duration::from_secs(20);

struct Peer {
    child: Child,
    stdin: Option<ChildStdin>,
    rx: mpsc::Receiver<Value>,
    pending: Vec<Value>,
    next_id: u64,
    generation: String,
    peer_id: String,
    /// 最後に見た networkStatus
    status: Value,
}

impl Drop for Peer {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

fn write_message(stdin: &mut ChildStdin, body: &[u8]) -> std::io::Result<()> {
    stdin.write_all(&(body.len() as u32).to_be_bytes())?;
    stdin.write_all(body)?;
    stdin.flush()
}

impl Peer {
    fn spawn() -> Peer {
        let mut child = Command::new(BIN)
            .env_clear()
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::null())
            .spawn()
            .expect("spawn peer");
        let mut stdout = child.stdout.take().unwrap();
        let stdin = child.stdin.take();
        let (tx, rx) = mpsc::channel();
        std::thread::spawn(move || loop {
            let mut len = [0u8; 4];
            if stdout.read_exact(&mut len).is_err() {
                return;
            }
            let len = u32::from_be_bytes(len) as usize;
            assert!(len <= 128 * 1024, "message too large");
            let mut body = vec![0u8; len];
            if stdout.read_exact(&mut body).is_err() {
                return;
            }
            let value: Value = serde_json::from_slice(&body).expect("json");
            if tx.send(value).is_err() {
                return;
            }
        });
        Peer {
            child,
            stdin,
            rx,
            pending: Vec::new(),
            next_id: 1,
            generation: "7".into(),
            peer_id: String::new(),
            status: Value::Null,
        }
    }

    fn send_raw(&mut self, value: &Value) {
        let body = serde_json::to_vec(value).unwrap();
        write_message(self.stdin.as_mut().unwrap(), &body).unwrap();
    }

    /// 要求を送り、応答（result か error）を返す
    fn call(&mut self, method: &str, params: Value) -> Value {
        let id = self.next_id.to_string();
        self.next_id += 1;
        let msg = json!({
            "protocolVersion": 6,
            "processGeneration": self.generation,
            "requestId": id,
            "method": method,
            "params": params,
        });
        self.send_raw(&msg);
        let deadline = Instant::now() + TIMEOUT;
        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            let v = self
                .rx
                .recv_timeout(remaining)
                .unwrap_or_else(|_| panic!("no response to {method}"));
            assert_eq!(v["protocolVersion"], 6);
            assert_eq!(v["processGeneration"], self.generation.as_str());
            if v.get("requestId").and_then(Value::as_str) == Some(id.as_str()) {
                return v;
            }
            self.pending.push(v);
        }
    }

    fn ok(&mut self, method: &str, params: Value) -> Value {
        let v = self.call(method, params);
        assert!(v.get("error").is_none(), "{method} failed: {v}");
        v["result"].clone()
    }

    /// 条件に合う通知を待つ
    fn wait(&mut self, name: &str, pred: impl Fn(&Value) -> bool, timeout: Duration) -> Option<Value> {
        if let Some(i) = self
            .pending
            .iter()
            .position(|v| v["notification"] == name && pred(&v["params"]))
        {
            return Some(self.pending.remove(i)["params"].clone());
        }
        let deadline = Instant::now() + timeout;
        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            if remaining.is_zero() {
                return None;
            }
            let Ok(v) = self.rx.recv_timeout(remaining) else {
                return None;
            };
            if v["notification"] == name && pred(&v["params"]) {
                return Some(v["params"].clone());
            }
            self.pending.push(v);
        }
    }

    fn start(&mut self, bootstrap: Vec<String>) {
        self.start_with(bootstrap, false);
    }

    /// relay_only: 「直接接続を使わない」
    fn start_with(&mut self, bootstrap: Vec<String>, relay_only: bool) {
        let hello = self.ok("hello", json!({ "uiVersion": "test" }));
        assert_eq!(hello["ipcVersion"], 6);
        assert_eq!(hello["sandbox"]["applied"], true, "{hello}");
        let wait_node = !bootstrap.is_empty();
        let start = self.ok(
            "start",
            json!({ "networkName": "sbq-local-1", "bootstrap": bootstrap, "relayOnly": relay_only }),
        );
        assert_eq!(start["profile"], "local");
        assert_eq!(start["networkId"], hex(&network::network_id("sbq-local-1")));
        let bytes = B64.decode(start["peerIdBytes"].as_str().unwrap()).unwrap();
        assert!(sbq_protocol::peerid::is_ed25519(&bytes));
        self.peer_id = start["peerId"].as_str().unwrap().to_owned();
        // 待受を始めた（協力ノードを指定したときは、つながった）
        self.wait_status(|p| p["state"] == "online").expect("online");
        if wait_node {
            self.wait_status(|p| p["connectedPeers"].as_u64().unwrap_or(0) >= 1)
                .expect("connected to the node");
        }
    }

    /// この接続の connectionOpened が、もう届いているか（待たない）
    fn was_opened(&mut self, connection: &str) -> bool {
        self.wait("connectionOpened", |p| p["connection"] == connection, Duration::ZERO)
            .is_some()
    }

    /// networkStatus は変わったときにだけ届くので、最後に見た値から確かめる
    fn wait_status(&mut self, pred: impl Fn(&Value) -> bool) -> Option<Value> {
        let deadline = Instant::now() + TIMEOUT;
        loop {
            if pred(&self.status) {
                return Some(self.status.clone());
            }
            let remaining = deadline.saturating_duration_since(Instant::now());
            let status = self.wait("networkStatus", |_| true, remaining)?;
            self.status = status;
        }
    }
}

fn hello_frame(request_id: u64) -> String {
    B64.encode(frame::build(
        frame::TYPE_HELLO,
        request_id,
        &[0u8; 32],
        vec![sbq_protocol::cbor::Value::text("hi")],
    ))
}

#[test]
fn invite_connection_and_frames() {
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start(vec![]);
    b.start(vec![]);

    let invite = a.ok("createInvite", json!({}));
    let text = invite["invite"].as_str().unwrap().to_owned();
    assert!(text.starts_with("sbqinv1."));
    assert!(invite["addressCount"].as_u64().unwrap() >= 1);

    let inspected = b.ok("inspectInvite", json!({ "invite": text }));
    assert_eq!(inspected["peerId"], a.peer_id.as_str());
    assert!(inspected["usableAddressCount"].as_u64().unwrap() >= 1);
    assert_eq!(inspected["privateAddresses"], json!([]));
    // 自分の招待は使えない
    assert_eq!(
        a.call("inspectInvite", json!({ "invite": text }))["error"]["code"],
        "invite_own"
    );

    let conn_b = b.ok("connectInvite", json!({ "invite": text, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let opened_b = b
        .wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    assert_eq!(opened_b["direction"], "outbound");
    assert_eq!(opened_b["origin"], "invite");
    assert_eq!(opened_b["peerId"], a.peer_id.as_str());
    let opened_a = a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");
    assert_eq!(opened_a["direction"], "inbound");
    assert_eq!(opened_a["origin"], "invite");
    assert_eq!(opened_a["peerId"], b.peer_id.as_str());
    let conn_a = opened_a["connection"].as_str().unwrap().to_owned();

    // フレームの往復
    let f1 = hello_frame(1);
    b.ok("sendFrame", json!({ "connection": conn_b, "frame": f1 }));
    let got = a
        .wait("frameReceived", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("A frame");
    assert_eq!(got["frame"], f1.as_str());
    let f2 = hello_frame(2);
    a.ok("sendFrame", json!({ "connection": conn_a, "frame": f2 }));
    let got = b
        .wait("frameReceived", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B frame");
    assert_eq!(got["frame"], f2.as_str());

    // 上限いっぱい（64 KiB）のフレームは届き、1バイト超えると送れない
    let max = frame::build(
        frame::TYPE_STATE_PROPOSE,
        3,
        &[7u8; 32],
        vec![sbq_protocol::cbor::Value::Bytes(vec![0xab; 65_494])],
    );
    assert_eq!(max.len(), frame::MAX_FRAME_BYTES);
    let max_b64 = B64.encode(&max);
    b.ok("sendFrame", json!({ "connection": conn_b, "frame": max_b64 }));
    let got = a
        .wait("frameReceived", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("A max frame");
    assert_eq!(got["frame"], max_b64.as_str());
    let over = frame::build(
        frame::TYPE_STATE_PROPOSE,
        4,
        &[7u8; 32],
        vec![sbq_protocol::cbor::Value::Bytes(vec![0xab; 65_495])],
    );
    assert_eq!(
        b.call("sendFrame", json!({ "connection": conn_b, "frame": B64.encode(&over) }))["error"]["code"],
        "invalid_frame"
    );

    // 外枠の正しくないフレーム・INVITE_TOKEN は送れない
    let bad = B64.encode([0x00u8]);
    assert_eq!(
        b.call("sendFrame", json!({ "connection": conn_b, "frame": bad }))["error"]["code"],
        "invalid_frame"
    );
    let token = B64.encode(frame::invite_token_frame(&[1u8; 32]));
    assert_eq!(
        b.call("sendFrame", json!({ "connection": conn_b, "frame": token }))["error"]["code"],
        "invalid_frame"
    );
    assert_eq!(
        b.call("sendFrame", json!({ "connection": "999", "frame": f1 }))["error"]["code"],
        "unknown_connection"
    );

    // 使った招待はもう使えない（招待した側に届かない）。招待した側は合言葉の合わないストリームをすぐ閉じるので、
    // B が送る前に接続が閉じていることがある（そのときの sendFrame は unknown_connection）
    let reused = b.ok("connectInvite", json!({ "invite": text, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    if b.wait("connectionOpened", |p| p["connection"] == reused.as_str(), TIMEOUT)
        .is_some()
    {
        let _ = b.call("sendFrame", json!({ "connection": reused, "frame": hello_frame(3) }));
    }
    assert!(
        a.wait("connectionOpened", |_| true, Duration::from_secs(3)).is_none(),
        "a used invite was accepted"
    );

    // 誤ったトークンの招待は、招待した側に届かない
    let fresh = a.ok("createInvite", json!({}))["invite"].as_str().unwrap().to_owned();
    let mut forged = Invite::decode(&fresh).unwrap();
    forged.token[0] ^= 0xff;
    let forged = forged.encode().unwrap();
    let wrong = b.ok("connectInvite", json!({ "invite": forged, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    if b.wait("connectionOpened", |p| p["connection"] == wrong.as_str(), TIMEOUT)
        .is_some()
    {
        let _ = b.call("sendFrame", json!({ "connection": wrong, "frame": hello_frame(4) }));
    }
    assert!(
        a.wait("connectionOpened", |_| true, Duration::from_secs(3)).is_none(),
        "a forged invite was accepted"
    );

    // 閉じる
    b.ok("closeConnection", json!({ "connection": conn_b }));
    a.wait("connectionClosed", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("A closed");
    a.ok("shutdown", json!({}));
    let status = a.child.wait().unwrap();
    assert_eq!(status.code(), Some(0));
}

/// 合言葉（同じ LAN の招待の短い形。protocol/README.md 3.1節）: 招待と一緒に作られ、相手の PeerId を知らずに
/// つなげる。招待文字列とどちらか一方で1回だけ使え、秘密の違う合言葉は招待した側に届かない
#[test]
fn code_connection_and_frames() {
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start(vec![]);
    b.start(vec![]);

    let invite = a.ok("createInvite", json!({}));
    let code_text = invite["code"].as_str().unwrap().to_owned();
    let code = Code::decode(&code_text).expect("a code comes with the invite");
    assert!(!code.addrs.is_empty());
    assert_eq!(code.encode().as_deref(), Some(code_text.as_str()));

    let inspected = b.ok("inspectInvite", json!({ "invite": code_text }));
    assert_eq!(inspected["kind"], "code");
    assert_eq!(inspected["peerId"], "");
    assert_eq!(inspected["expiresAt"], "0");
    assert!(inspected["usableAddressCount"].as_u64().unwrap() >= 1);
    assert_eq!(inspected["privateAddresses"], json!([]));
    // 招待文字列は kind が invite
    assert_eq!(
        b.ok("inspectInvite", json!({ "invite": invite["invite"] }))["kind"],
        "invite"
    );
    // 自分の合言葉は使えない。読めない文字列は invite_invalid
    assert_eq!(
        a.call("inspectInvite", json!({ "invite": code_text }))["error"]["code"],
        "invite_own"
    );
    for bad in ["", "!!!", "K7MP", "sbqinv2.AAAA"] {
        assert_eq!(
            b.call("inspectInvite", json!({ "invite": bad }))["error"]["code"],
            "invite_invalid",
            "{bad}"
        );
    }

    // 区切りなしの小文字で打っても読める
    let typed = code_text.replace('-', "").to_lowercase();
    let conn_b = b.ok("connectInvite", json!({ "invite": typed, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let opened_b = b
        .wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    assert_eq!(opened_b["direction"], "outbound");
    assert_eq!(opened_b["origin"], "invite");
    assert_eq!(opened_b["route"], "direct");
    assert_eq!(opened_b["peerId"], a.peer_id.as_str());
    let opened_a = a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");
    assert_eq!(opened_a["direction"], "inbound");
    assert_eq!(opened_a["origin"], "invite");
    assert_eq!(opened_a["peerId"], b.peer_id.as_str());
    let conn_a = opened_a["connection"].as_str().unwrap().to_owned();

    let f1 = hello_frame(1);
    b.ok("sendFrame", json!({ "connection": conn_b, "frame": f1 }));
    let got = a
        .wait("frameReceived", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("A frame");
    assert_eq!(got["frame"], f1.as_str());
    let f2 = hello_frame(2);
    a.ok("sendFrame", json!({ "connection": conn_a, "frame": f2 }));
    let got = b
        .wait("frameReceived", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B frame");
    assert_eq!(got["frame"], f2.as_str());
    b.ok("closeConnection", json!({ "connection": conn_b }));
    a.wait("connectionClosed", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("A closed");

    // 合言葉で使った招待は、合言葉でも、同じ招待の招待文字列でも、もう使えない（招待した側に届かない）。
    // 合言葉でつなぐ側には、招待した側の確認の値が届かないので、開かずに refused で閉じる
    let again = b.ok("connectInvite", json!({ "invite": code_text, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let refused = b
        .wait("connectionClosed", |p| p["connection"] == again.as_str(), TIMEOUT)
        .expect("a used code was not refused");
    assert_eq!(refused["reason"], "refused", "{refused}");
    assert!(!b.was_opened(&again));
    let invite_text = invite["invite"].as_str().unwrap().to_owned();
    let again = b.ok("connectInvite", json!({ "invite": invite_text, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    if b.wait("connectionOpened", |p| p["connection"] == again.as_str(), TIMEOUT)
        .is_some()
    {
        let _ = b.call("sendFrame", json!({ "connection": again, "frame": hello_frame(3) }));
    }
    assert!(
        a.wait("connectionOpened", |_| true, Duration::from_secs(3)).is_none(),
        "a used invite was accepted"
    );

    // 秘密の違う合言葉は、招待した側に届かず、つなぐ側は開かずに refused で閉じる（招待した側の確認の値が無い）
    let fresh = a.ok("createInvite", json!({}));
    let mut forged = Code::decode(fresh["code"].as_str().unwrap()).unwrap();
    forged.secret ^= 1;
    let wrong = b.ok(
        "connectInvite",
        json!({ "invite": forged.encode().unwrap(), "allowPrivate": false }),
    )["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let refused = b
        .wait("connectionClosed", |p| p["connection"] == wrong.as_str(), TIMEOUT)
        .expect("a forged code was not refused");
    assert_eq!(refused["reason"], "refused", "{refused}");
    assert!(!b.was_opened(&wrong));
    assert!(
        a.wait("connectionOpened", |_| true, Duration::from_secs(1)).is_none(),
        "a forged code was accepted"
    );
    // 同じ招待の招待文字列は、まだ使える（合言葉の照合に失敗しても、招待は残る）
    let conn_b = b.ok(
        "connectInvite",
        json!({ "invite": fresh["invite"], "allowPrivate": false }),
    )["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    b.wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened by the invite string");
    a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");
}

/// だれも待ち受けていない宛先の合言葉は、つながらずに閉じる。招待を取り消すと、合言葉も使えなくなる
#[test]
fn code_to_nowhere_and_cancelled_code() {
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start(vec![]);
    b.start(vec![]);

    // この PC のだれも待ち受けていないポート
    let free = std::net::UdpSocket::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port();
    let nowhere = Code::new(vec![std::net::Ipv4Addr::LOCALHOST], Some(free), 5)
        .unwrap()
        .encode()
        .unwrap();
    let conn = b.ok("connectInvite", json!({ "invite": nowhere, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let closed = b
        .wait("connectionClosed", |p| p["connection"] == conn.as_str(), Duration::from_secs(40))
        .expect("closed");
    assert!(closed["reason"] == "unreachable" || closed["reason"] == "timeout", "{closed}");

    // つなぐ途中の接続は、閉じられる（閉じた接続は知らせない）
    let free = std::net::UdpSocket::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port();
    let nowhere = Code::new(vec![std::net::Ipv4Addr::LOCALHOST], Some(free), 5)
        .unwrap()
        .encode()
        .unwrap();
    let conn = b.ok("connectInvite", json!({ "invite": nowhere, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    b.ok("closeConnection", json!({ "connection": conn }));
    assert_eq!(
        b.call("closeConnection", json!({ "connection": conn }))["error"]["code"],
        "unknown_connection"
    );

    let code = a.ok("createInvite", json!({}))["code"].as_str().unwrap().to_owned();
    a.ok("cancelInvites", json!({}));
    let conn = b.ok("connectInvite", json!({ "invite": code, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let refused = b
        .wait("connectionClosed", |p| p["connection"] == conn.as_str(), TIMEOUT)
        .expect("a cancelled code was not refused");
    assert_eq!(refused["reason"], "refused", "{refused}");
    assert!(
        a.wait("connectionOpened", |_| true, Duration::from_secs(1)).is_none(),
        "a cancelled code was accepted"
    );
}

/// 合言葉の宛先で応えた相手が、招待した本人でない（合言葉を知らない）なら、つなぐ側は開かずに refused で閉じる。
/// ここでは、合言葉の宛先を別の通信プロセス（招待していない C）の待受に書き換えて確かめる（監査 2026-11-21 S3）
#[test]
fn code_to_someone_else_is_refused() {
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    let mut c = Peer::spawn();
    a.start(vec![]);
    b.start(vec![]);
    c.start(vec![]);
    // C の待受（招待文字列の到達候補から、QUIC のポートを読む）
    let c_invite = Invite::decode(c.ok("createInvite", json!({}))["invite"].as_str().unwrap()).unwrap();
    let c_port = c_invite
        .addrs
        .iter()
        .find_map(|bytes| {
            let addr = libp2p::Multiaddr::try_from(bytes.clone()).unwrap();
            let mut port = None;
            let mut quic = false;
            for p in addr.iter() {
                match p {
                    libp2p::multiaddr::Protocol::Udp(n) => port = Some(n),
                    libp2p::multiaddr::Protocol::QuicV1 => quic = true,
                    _ => {}
                }
            }
            port.filter(|_| quic)
        })
        .expect("C listens on QUIC");
    c.ok("cancelInvites", json!({}));
    // A の招待の合言葉の秘密のまま、宛先だけを C にする（C は合言葉の鍵を知らない）
    let code = Code::decode(a.ok("createInvite", json!({}))["code"].as_str().unwrap()).unwrap();
    let redirected = Code::new(vec![std::net::Ipv4Addr::LOCALHOST], Some(c_port), code.secret)
        .unwrap()
        .encode()
        .unwrap();
    let conn = b.ok("connectInvite", json!({ "invite": redirected, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let closed = b
        .wait("connectionClosed", |p| p["connection"] == conn.as_str(), TIMEOUT)
        .expect("closed");
    assert_eq!(closed["reason"], "refused", "{closed}");
    assert!(!b.was_opened(&conn));
    assert!(c.wait("connectionOpened", |_| true, Duration::from_secs(1)).is_none());
    assert!(a.wait("connectionOpened", |_| true, Duration::from_secs(1)).is_none());
}

/// 起動した側の PID（--parent-pid）が、読んだ親と違えば、ほかのことをする前に終了コード 3 で終わる。
/// 合っていれば、ふつうに動く（監査 2026-11-21 I-15）
#[test]
fn parent_pid_must_match() {
    let own = std::process::id().to_string();
    let wrong = (std::process::id() + 1).to_string();
    for (pid, code) in [(wrong.as_str(), Some(3)), ("abc", Some(3))] {
        let output = Command::new(BIN)
            .args(["--parent-pid", pid, "--sandbox-self-test"])
            .env_clear()
            .output()
            .unwrap();
        assert_eq!(output.status.code(), code, "{pid}");
        assert!(output.stdout.is_empty(), "{pid}");
        let line = String::from_utf8_lossy(&output.stderr);
        assert!(line.starts_with("shogiboardq-peer: sandbox: "), "{line}");
    }
    let output = Command::new(BIN)
        .args(["--sandbox-self-test", "--parent-pid", own.as_str()])
        .env_clear()
        .output()
        .unwrap();
    assert_eq!(output.status.code(), Some(0));
    let v: Value = serde_json::from_slice(&output.stdout).expect("json");
    assert_eq!(v["applied"], true);
}

/// 権限の制限の下でも、回線のつながっていないインターフェース（コンテナのいない docker0 など）を調べられ、
/// そのアドレスを合言葉に入れない（この PC にそういうインターフェースがなければ、確かめることがない）
#[cfg(target_os = "linux")]
#[test]
fn code_skips_addresses_of_idle_interfaces() {
    use nix::net::if_::InterfaceFlags;
    let mut idle = Vec::new();
    let mut active = Vec::new();
    for interface in nix::ifaddrs::getifaddrs().unwrap() {
        let Some(ip) = interface.address.as_ref().and_then(|a| a.as_sockaddr_in()).map(|a| a.ip()) else {
            continue;
        };
        if !ip.is_private() {
            continue;
        }
        if interface
            .flags
            .contains(InterfaceFlags::IFF_UP | InterfaceFlags::IFF_RUNNING)
        {
            active.push(ip);
        } else {
            idle.push(ip);
        }
    }
    if idle.is_empty() || active.is_empty() {
        return;
    }
    let mut a = Peer::spawn();
    a.start(vec![]);
    let invite = a.ok("createInvite", json!({}));
    let code = Code::decode(invite["code"].as_str().unwrap()).unwrap();
    for ip in &code.addrs {
        assert!(!idle.contains(ip), "{ip} is on an idle interface");
        assert!(active.contains(ip), "{ip}");
    }
}

#[test]
fn game_peer_reconnect() {
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start(vec![]);
    b.start(vec![]);
    let text = a.ok("createInvite", json!({}))["invite"].as_str().unwrap().to_owned();
    let conn_b = b.ok("connectInvite", json!({ "invite": text, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    b.wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");

    // 対局の相手を決めると、その相手からは招待なしで受け付ける（再接続）
    a.ok("setGamePeer", json!({ "peerId": b.peer_id }));
    b.ok("setGamePeer", json!({ "peerId": a.peer_id }));
    let again = b.ok("connectPeer", json!({ "peerId": a.peer_id }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    b.wait("connectionOpened", |p| p["connection"] == again.as_str(), TIMEOUT)
        .expect("B reopened");
    b.ok("sendFrame", json!({ "connection": again, "frame": hello_frame(9) }));
    let opened = a
        .wait("connectionOpened", |p| p["origin"] == "game", TIMEOUT)
        .expect("A accepted the game peer");
    let conn_a = opened["connection"].as_str().unwrap().to_owned();
    a.wait("frameReceived", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("first frame forwarded");

    // 対局の相手でないピアには connectPeer を使えない
    assert_eq!(
        a.call("connectPeer", json!({ "peerId": a.peer_id }))["error"]["code"],
        "invalid_state"
    );
}

#[test]
fn ipc_violations_end_the_process() {
    // 知らないメソッド
    let mut p = Peer::spawn();
    p.ok("hello", json!({ "uiVersion": "test" }));
    p.send_raw(
        &json!({ "protocolVersion": 6, "processGeneration": "7", "requestId": "2", "method": "evil", "params": {} }),
    );
    assert_eq!(p.child.wait().unwrap().code(), Some(2));

    // 世代の違う要求
    let mut p = Peer::spawn();
    p.ok("hello", json!({ "uiVersion": "test" }));
    p.send_raw(&json!({ "protocolVersion": 6, "processGeneration": "8", "requestId": "2", "method": "createInvite", "params": {} }));
    assert_eq!(p.child.wait().unwrap().code(), Some(2));

    // 上限を超える長さ
    let mut p = Peer::spawn();
    let stdin = p.stdin.as_mut().unwrap();
    stdin.write_all(&(200_000u32).to_be_bytes()).unwrap();
    stdin.flush().unwrap();
    assert_eq!(p.child.wait().unwrap().code(), Some(2));

    // 最初の要求が hello でない
    let mut p = Peer::spawn();
    p.send_raw(&json!({ "protocolVersion": 6, "processGeneration": "7", "requestId": "1", "method": "createInvite", "params": {} }));
    assert_eq!(p.child.wait().unwrap().code(), Some(2));

    // 知らない引数はエラーの応答（終了しない）、start の前の要求は invalid_state
    let mut p = Peer::spawn();
    p.ok("hello", json!({ "uiVersion": "test" }));
    assert_eq!(
        p.call("createInvite", json!({ "x": 1 }))["error"]["code"],
        "invalid_state"
    );
    let r = p.call(
        "start",
        json!({ "networkName": "sbq-local-1", "bootstrap": [], "relayOnly": false, "x": 1 }),
    );
    assert_eq!(r["error"]["code"], "invalid_request");
    let r = p.call(
        "start",
        json!({ "networkName": "public-1", "bootstrap": [], "relayOnly": false }),
    );
    assert_eq!(r["error"]["code"], "network_not_allowed");

    // 標準入力の終端で終わる
    p.stdin = None;
    assert_eq!(p.child.wait().unwrap().code(), Some(0));
}

#[test]
fn sandbox_self_test_denies_forbidden_operations() {
    let output = Command::new(BIN)
        .arg("--sandbox-self-test")
        .env_clear()
        .output()
        .unwrap();
    assert_eq!(output.status.code(), Some(0));
    let v: Value = serde_json::from_slice(&output.stdout).expect("json");
    assert_eq!(v["applied"], true);
    #[cfg(target_os = "linux")]
    let denied = [
        "read_file",
        "write_file",
        "exec",
        "fork",
        "unix_socket",
        "unix_connect_abstract",
        "signal_parent",
    ];
    #[cfg(target_os = "macos")]
    let denied = ["read_file", "write_file", "exec", "fork", "signal_parent"];
    for name in denied {
        assert_eq!(v["results"][name], "denied", "{name}: {v}");
    }
}

/// 協力ノード（--service）を起動し、PeerId と待受のアドレスを返す
struct Service {
    child: Child,
    addr: String,
    dir: std::path::PathBuf,
    /// 標準エラーに書かれた行（起動の行の後）
    log: mpsc::Receiver<String>,
}

impl Service {
    /// 標準エラーに、この行が書かれるのを待つ
    fn wait_log(&self, expected: &str) -> bool {
        self.wait_log_where(|line| line == expected)
    }

    /// 標準エラーに、条件に合う行が書かれるのを待つ
    fn wait_log_where(&self, pred: impl Fn(&str) -> bool) -> bool {
        let deadline = Instant::now() + TIMEOUT;
        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            match self.log.recv_timeout(remaining) {
                Ok(line) if pred(&line) => return true,
                Ok(_) => {}
                Err(_) => return false,
            }
        }
    }
}

impl Drop for Service {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
        let _ = std::fs::remove_dir_all(&self.dir);
    }
}

fn spawn_service() -> Service {
    spawn_service_with(&["--relay"])
}

/// options: 協力ノードが受け持つもの（--relay・--meet）
fn spawn_service_with(options: &[&str]) -> Service {
    let port = std::net::TcpListener::bind("127.0.0.1:0")
        .unwrap()
        .local_addr()
        .unwrap()
        .port();
    let dir = std::env::temp_dir().join(format!("sbq-peer-test-{}-{}", std::process::id(), port));
    std::fs::create_dir_all(&dir).unwrap();
    let identity = dir.join("identity.key");
    let listen = format!("/ip4/127.0.0.1/tcp/{port}");
    let mut child = Command::new(BIN)
        .args(["--service", "--network", "sbq-local-1", "--identity"])
        .arg(&identity)
        .args(["--listen", &listen])
        .args(options)
        .env_clear()
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::piped())
        .spawn()
        .unwrap();
    let mut stderr = BufReader::new(child.stderr.take().unwrap());
    let mut line = String::new();
    stderr.read_line(&mut line).unwrap();
    let peer = line.trim().rsplit(' ').next().unwrap().to_owned();
    assert!(line.contains("service started"), "{line}");
    let (log_tx, log) = mpsc::channel();
    std::thread::spawn(move || {
        for line in stderr.lines().map_while(Result::ok) {
            // 受け手がいなくなっても読み続ける（協力ノードが書き込みで止まらないように）
            let _ = log_tx.send(line);
        }
    });
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        let mode = std::fs::metadata(&identity).unwrap().permissions().mode();
        assert_eq!(mode & 0o777, 0o600);
    }
    Service {
        child,
        addr: format!("{listen}/p2p/{peer}"),
        dir,
        log,
    }
}

/// 招待の到達候補を、中継経由かどうかに分けて数える
fn invite_routes(text: &str) -> (usize, usize) {
    let invite = Invite::decode(text).unwrap();
    let mut direct = 0;
    let mut relayed = 0;
    for bytes in &invite.addrs {
        let addr = libp2p::Multiaddr::try_from(bytes.clone()).unwrap();
        if addr.to_string().contains("/p2p-circuit") {
            relayed += 1;
        } else {
            direct += 1;
        }
    }
    (direct, relayed)
}

/// 協力ノードは、つながっている相手の数と中継している回路の数を、変わったときに標準エラーへ書く
/// （決まった間隔では書かない。接続元の IP もピアの ID も書かない）
#[test]
fn service_logs_the_counts_when_they_change() {
    let service = spawn_service();
    // 起動した直後の数
    assert!(service.wait_log("shogiboardq-peer: peers=0 relays=0"));
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start(vec![service.addr.clone()]);
    b.start_with(vec![service.addr.clone()], true);
    a.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("A reserved the relay");
    b.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("B reserved the relay");
    // 2人がつながっただけでは、中継の数は増えない
    assert!(service.wait_log("shogiboardq-peer: peers=2 relays=0"));

    // 中継を通る対局の接続が1本できると、中継の数は1になる（人数ではなく、中継している回路の数）
    let invite = a.ok("createInvite", json!({}))["invite"].as_str().unwrap().to_owned();
    let conn_b = b.ok("connectInvite", json!({ "invite": invite, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let opened_b = b
        .wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    assert_eq!(opened_b["route"], "relay");
    assert!(service.wait_log("shogiboardq-peer: peers=2 relays=1"));

    // 2人が終了すると、どちらの数も0に戻る
    drop(b);
    drop(a);
    assert!(service.wait_log("shogiboardq-peer: peers=0 relays=0"));
}

/// 「直接接続を使わない」: 相手の直接のアドレスへはつながず、指定した協力ノードの中継だけを通る。
/// 自分の招待には中継経由のアドレスだけを入れる
#[test]
fn relay_only_uses_only_the_node_relay() {
    let service = spawn_service();
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start(vec![service.addr.clone()]);
    b.start_with(vec![service.addr.clone()], true);
    // 中継の予約は、直接つなげる側も取る（直接接続を使わない相手がつなげるように）
    a.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("A reserved the relay");
    b.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("B reserved the relay");

    // A の招待には直接と中継経由の両方が入る。B が使えるのは中継経由だけ
    let invite = a.ok("createInvite", json!({}))["invite"].as_str().unwrap().to_owned();
    let (direct, relayed) = invite_routes(&invite);
    assert!(direct >= 1 && relayed >= 1, "direct={direct} relayed={relayed}");
    let inspected = b.ok("inspectInvite", json!({ "invite": invite }));
    assert_eq!(inspected["addressCount"].as_u64().unwrap(), (direct + relayed) as u64);
    assert_eq!(inspected["usableAddressCount"].as_u64().unwrap(), relayed as u64);

    // つなぐと、どちらから見ても経路は中継（直接の接続は作られない）
    let conn_b = b.ok("connectInvite", json!({ "invite": invite, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let opened_b = b
        .wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    assert_eq!(opened_b["route"], "relay");
    let opened_a = a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");
    assert_eq!(opened_a["route"], "relay");
    assert_eq!(opened_a["peerId"], b.peer_id.as_str());
    let conn_a = opened_a["connection"].as_str().unwrap().to_owned();
    let f1 = hello_frame(1);
    b.ok("sendFrame", json!({ "connection": conn_b, "frame": f1 }));
    let got = a
        .wait("frameReceived", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("A frame");
    assert_eq!(got["frame"], f1.as_str());
    // しばらく待っても、直接の経路に切り替わらない
    assert!(a
        .wait("connectionRoute", |p| p["route"] == "direct", Duration::from_secs(5))
        .is_none());
    assert!(b
        .wait("connectionRoute", |p| p["route"] == "direct", Duration::from_secs(1))
        .is_none());

    // B の招待には中継経由のアドレスだけが入る（自分の直接のアドレスを出さない）
    let invite_b = b.ok("createInvite", json!({}))["invite"].as_str().unwrap().to_owned();
    let (direct, relayed) = invite_routes(&invite_b);
    assert_eq!(direct, 0);
    assert!(relayed >= 1);
}

/// 招待の到達候補を、中継経由のものだけにする（最初の接続が中継経由になる。家庭の回線どうしのインターネット越しの
/// 対局と同じ形）
fn relayed_addrs_only(text: &str) -> String {
    let mut invite = Invite::decode(text).unwrap();
    invite.addrs.retain(|bytes| {
        libp2p::Multiaddr::try_from(bytes.clone())
            .unwrap()
            .to_string()
            .contains("/p2p-circuit")
    });
    assert!(!invite.addrs.is_empty(), "the invite has no relayed address");
    invite.encode().unwrap()
}

/// 中継でつながった後に直接の接続ができたら、対局ストリームは直接の接続の上で開く。
/// 中継の上で開いてしまうと、画面は「直接」と出るのに指し手は協力ノードを通り続け、使われない直接の接続は
/// しばらくして閉じる。直接の接続の上にあれば、協力ノードを止めても対局ストリームは切れない
#[test]
fn game_stream_opens_on_the_direct_connection_after_hole_punching() {
    let service = spawn_service();
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start(vec![service.addr.clone()]);
    b.start(vec![service.addr.clone()]);
    a.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("A reserved the relay");
    b.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("B reserved the relay");

    let text = a.ok("createInvite", json!({}))["invite"].as_str().unwrap().to_owned();
    let invite = relayed_addrs_only(&text);
    let conn_b = b.ok("connectInvite", json!({ "invite": invite, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let opened_b = b
        .wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    let opened_a = a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");
    assert_eq!(opened_b["route"], "direct");
    assert_eq!(opened_a["route"], "direct");
    let conn_a = opened_a["connection"].as_str().unwrap().to_owned();

    // 協力ノードを止めても、対局ストリームは切れず、フレームが届く
    drop(service);
    std::thread::sleep(Duration::from_secs(2));
    let frame = hello_frame(1);
    b.ok("sendFrame", json!({ "connection": conn_b, "frame": frame }));
    let got = a
        .wait("frameReceived", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("A frame");
    assert_eq!(got["frame"], frame.as_str());
    assert!(b
        .wait(
            "connectionClosed",
            |p| p["connection"] == conn_b.as_str(),
            Duration::from_secs(2)
        )
        .is_none());
    assert!(a
        .wait(
            "connectionClosed",
            |p| p["connection"] == conn_a.as_str(),
            Duration::from_secs(1)
        )
        .is_none());
    // 経路は直接のまま変わらない
    assert!(b.wait("connectionRoute", |_| true, Duration::from_secs(1)).is_none());
}

/// 直接の接続ができない相手（ここでは「直接接続を使わない」相手。穴あけを始めない）とは、少し待ってから、
/// 中継経由の接続の上で対局ストリームを開く。待ち続けて開かないままにならない
#[test]
fn game_stream_opens_on_the_relay_when_no_direct_connection_comes() {
    let service = spawn_service();
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start_with(vec![service.addr.clone()], true);
    b.start(vec![service.addr.clone()]);
    a.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("A reserved the relay");
    b.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("B reserved the relay");

    // A の招待には中継経由のアドレスだけが入る
    let invite = a.ok("createInvite", json!({}))["invite"].as_str().unwrap().to_owned();
    let (direct, relayed) = invite_routes(&invite);
    assert_eq!(direct, 0);
    assert!(relayed >= 1);
    let started = Instant::now();
    let conn_b = b.ok("connectInvite", json!({ "invite": invite, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let opened_b = b
        .wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    // 直接の接続を待つ時間（3秒）の後に開く
    let waited = started.elapsed();
    assert!(waited >= Duration::from_secs(2), "opened after {waited:?}");
    assert!(waited < Duration::from_secs(10), "opened after {waited:?}");
    assert_eq!(opened_b["route"], "relay");
    let opened_a = a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");
    assert_eq!(opened_a["route"], "relay");
    let conn_a = opened_a["connection"].as_str().unwrap().to_owned();
    let frame = hello_frame(1);
    b.ok("sendFrame", json!({ "connection": conn_b, "frame": frame }));
    let got = a
        .wait("frameReceived", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("A frame");
    assert_eq!(got["frame"], frame.as_str());
    // 経路は中継のまま変わらない
    assert!(b.wait("connectionRoute", |_| true, Duration::from_secs(2)).is_none());
    assert!(a.wait("connectionRoute", |_| true, Duration::from_secs(1)).is_none());
}

/// 「直接接続を使わない」側は、中継を通る宛先の無い招待にはつながない（相手の直接のアドレスへ自分の IP を見せない）
#[test]
fn relay_only_refuses_direct_only_invites() {
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start(vec![]);
    b.start_with(vec![], true);
    let invite = a.ok("createInvite", json!({}))["invite"].as_str().unwrap().to_owned();
    let (direct, relayed) = invite_routes(&invite);
    assert!(direct >= 1);
    assert_eq!(relayed, 0);
    let inspected = b.ok("inspectInvite", json!({ "invite": invite }));
    assert_eq!(inspected["usableAddressCount"], 0);
    let conn = b.ok("connectInvite", json!({ "invite": invite, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let closed = b
        .wait("connectionClosed", |p| p["connection"] == conn.as_str(), TIMEOUT)
        .expect("B closed");
    assert_eq!(closed["reason"], "no_relay_route");
    // A には何も届かない
    assert!(a.wait("connectionOpened", |_| true, Duration::from_secs(2)).is_none());
    // 自分の招待には到達候補が入らない（中継が無い）
    let own = b.ok("createInvite", json!({}));
    assert_eq!(own["addressCount"], 0);
}

/// 決まったポート（48210）の UDP をほかのプロセスが使っているときは、TCP も決まったポートを使わない。
/// libp2p の TCP の待受は共有できる設定（SO_REUSEPORT）で開くので、同時に起動した通信プロセスがどちらも TCP の
/// ポートが空いていると確かめてから開けると、同じポートを共有して届いた接続を取り違える。共有できない UDP を
/// 開けたプロセスだけが TCP でも決まったポートを使う
#[test]
fn fixed_tcp_port_follows_the_udp_port() {
    // 決まったポートの UDP を開けておく（ほかの試験の通信プロセスが使っていれば、空くまで待つ）
    let deadline = Instant::now() + TIMEOUT;
    let holder = loop {
        if let Ok(socket) = std::net::UdpSocket::bind(("0.0.0.0", 48210)) {
            break socket;
        }
        assert!(Instant::now() < deadline, "UDP 48210 stayed busy");
        std::thread::sleep(Duration::from_millis(100));
    };
    let mut p = Peer::spawn();
    p.start(vec![]);
    // 待受のアドレス（インターフェースごと）が出そろうのを待つ
    std::thread::sleep(Duration::from_secs(1));
    let text = p.ok("createInvite", json!({}))["invite"].as_str().unwrap().to_owned();
    let addrs: Vec<String> = Invite::decode(&text)
        .unwrap()
        .addrs
        .iter()
        .map(|bytes| libp2p::Multiaddr::try_from(bytes.clone()).unwrap().to_string())
        .collect();
    assert!(addrs.iter().any(|a| a.contains("/tcp/")), "{addrs:?}");
    for addr in &addrs {
        assert!(!addr.contains("/tcp/48210") && !addr.contains("/udp/48210"), "{addr}");
    }
    drop(holder);
}

/// 招待の到達候補の先頭の IPv4 アドレス
fn invite_ipv4(bytes: &[u8]) -> std::net::Ipv4Addr {
    let addr = libp2p::Multiaddr::try_from(bytes.to_vec()).unwrap();
    match addr.iter().next() {
        Some(libp2p::multiaddr::Protocol::Ip4(ip)) => ip,
        other => panic!("unexpected address {addr} ({other:?})"),
    }
}

/// 外向きの通信に使う IPv4 アドレス（経路が無ければ None。UDP の connect は何も送らない）
fn default_route_ipv4() -> Option<std::net::Ipv4Addr> {
    let socket = std::net::UdpSocket::bind("0.0.0.0:0").ok()?;
    socket.connect("192.0.2.1:9").ok()?;
    match socket.local_addr().ok()?.ip() {
        std::net::IpAddr::V4(ip) => Some(ip),
        std::net::IpAddr::V6(_) => None,
    }
}

/// 同じ LAN の PC どうし: ローカルのネットワークでは LAN 側でも待ち受け、招待には私設アドレスを先に入れる。
/// LAN のアドレスだけにした招待でもつながる（同じ PC の中で、loopback でないアドレスを通して確かめる）
#[test]
fn local_network_listens_on_the_lan() {
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start(vec![]);
    b.start(vec![]);
    // 待受のアドレス（インターフェースごと）が出そろうのを待つ
    std::thread::sleep(Duration::from_secs(1));
    let text = a.ok("createInvite", json!({}))["invite"].as_str().unwrap().to_owned();
    let invite = Invite::decode(&text).unwrap();
    assert!(!invite.addrs.is_empty());

    // 招待に入るのは loopback と私設アドレスだけ。同じ LAN の相手が使う私設アドレスが先
    let mut seen_loopback = false;
    for bytes in &invite.addrs {
        let ip = invite_ipv4(bytes);
        assert!(
            ip.is_loopback() || ip.is_private(),
            "{ip} must not be in a local invite"
        );
        if ip.is_loopback() {
            seen_loopback = true;
        } else {
            assert!(!seen_loopback, "LAN addresses must come before loopback");
        }
    }

    let lan: Vec<Vec<u8>> = invite
        .addrs
        .iter()
        .filter(|bytes| !invite_ipv4(bytes).is_loopback())
        .cloned()
        .collect();
    // この PC に LAN のアドレスがあるなら、招待に必ず入っている（待受が 127.0.0.1 だけなら入らない）
    let host_lan = default_route_ipv4().filter(std::net::Ipv4Addr::is_private);
    if let Some(ip) = host_lan {
        assert!(
            lan.iter().any(|bytes| invite_ipv4(bytes) == ip),
            "the host has the LAN address {ip}, but the invite does not contain it"
        );
    }
    if lan.is_empty() {
        eprintln!("this host has no private (LAN) address; the LAN connection is not exercised");
        return;
    }
    // 私設アドレスがあるときは、合言葉と同じく loopback を入れない（監査 2026-11-21 I-1）
    assert!(!seen_loopback, "{:?}", invite.addrs);
    // LAN のアドレスだけにした招待でつなぐ（相手は 127.0.0.1 を使えない別の PC、という場合に当たる）
    let lan_only = Invite { addrs: lan, ..invite }.encode().unwrap();
    let inspected = b.ok("inspectInvite", json!({ "invite": lan_only }));
    assert!(inspected["usableAddressCount"].as_u64().unwrap() >= 1);
    // ローカルのネットワークを選んだ利用者には、私設アドレスへつなぐ許可を改めて求めない
    assert_eq!(inspected["privateAddresses"], json!([]));
    let conn_b = b.ok("connectInvite", json!({ "invite": lan_only, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let opened_b = b
        .wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened over the LAN address");
    assert_eq!(opened_b["route"], "direct");
    let opened_a = a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");
    assert_eq!(opened_a["peerId"], b.peer_id.as_str());
    assert_eq!(opened_a["origin"], "invite");
}

/// 協力ノードを使う合言葉の、検査の文字まで正しい別の合言葉
fn other_meet_code(code: &str) -> String {
    let secret = sbq_protocol::meet::MeetCode::decode(code).unwrap().secret;
    sbq_protocol::meet::MeetCode::new(secret ^ 1).encode().unwrap()
}

/// 協力ノードを使う合言葉: 招待する側は招待を暗号化して協力ノードに預け、つなぐ側は合言葉で引き取ってつなぐ。
/// 協力ノードは、1回引き取られたら消す
#[test]
fn meet_code_connection_and_frames() {
    let service = spawn_service_with(&["--relay", "--meet"]);
    assert!(service.wait_log("shogiboardq-peer: peers=0 relays=0 meets=0"));
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    // 「直接接続を使わない」では、同じ LAN の合言葉を出さない（協力ノードを使う合言葉になる）
    a.start_with(vec![service.addr.clone()], true);
    b.start_with(vec![service.addr.clone()], true);
    a.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("A reserved the relay");
    b.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("B reserved the relay");

    let created = a.ok("createInvite", json!({}));
    let code = created["code"].as_str().unwrap().to_owned();
    // 10 文字（5文字ごとに区切る）。接続先は入っていない
    assert_eq!(code.len(), 11, "{code}");
    assert_eq!(code.find('-'), Some(5));
    assert!(sbq_protocol::meet::MeetCode::decode(&code).is_some());
    assert!(Invite::decode(created["invite"].as_str().unwrap()).is_ok());
    assert!(service.wait_log("shogiboardq-peer: peers=2 relays=0 meets=1"));

    // 自分の合言葉は、協力ノードから引き取らない（相手が引き取れなくなる）
    let own = a.call("inspectInvite", json!({ "invite": code }));
    assert_eq!(own["error"]["code"], "invite_own", "{own}");

    // 検査の文字まで正しい別の合言葉では、何も引き取れない
    let wrong = b.call("inspectInvite", json!({ "invite": other_meet_code(&code) }));
    assert_eq!(wrong["error"]["code"], "invite_not_found", "{wrong}");

    // 小文字・区切りなしでも読める。確かめると、相手の PeerId・期限・到達候補が分かる
    let typed = code.replace('-', "").to_lowercase();
    let inspected = b.ok("inspectInvite", json!({ "invite": typed }));
    assert_eq!(inspected["kind"], "meetCode");
    assert_eq!(inspected["peerId"], a.peer_id.as_str());
    assert_eq!(inspected["expiresAt"], created["expiresAt"]);
    assert!(inspected["addressCount"].as_u64().unwrap() >= 1);
    assert!(inspected["usableAddressCount"].as_u64().unwrap() >= 1);
    // 協力ノードは、1回引き取られたら消す
    assert!(service.wait_log("shogiboardq-peer: peers=2 relays=0 meets=0"));
    // もう一度確かめても、引き取ってある招待から答える
    let again = b.ok("inspectInvite", json!({ "invite": code }));
    assert_eq!(again["peerId"], a.peer_id.as_str());

    let conn_b = b.ok("connectInvite", json!({ "invite": code, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let opened_b = b
        .wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    assert_eq!(opened_b["peerId"], a.peer_id.as_str());
    assert_eq!(opened_b["origin"], "invite");
    assert_eq!(opened_b["route"], "relay");
    let opened_a = a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");
    assert_eq!(opened_a["peerId"], b.peer_id.as_str());
    let conn_a = opened_a["connection"].as_str().unwrap().to_owned();
    let f1 = hello_frame(1);
    b.ok("sendFrame", json!({ "connection": conn_b, "frame": f1 }));
    let got = a
        .wait("frameReceived", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("A frame");
    assert_eq!(got["frame"], f1.as_str());

    // 使った合言葉は、もう使えない（引き取ってある招待は接続に使った。協力ノードにも残っていない）
    let used = b.call("inspectInvite", json!({ "invite": code }));
    assert_eq!(used["error"]["code"], "invite_not_found", "{used}");
}

/// 協力ノードを使う合言葉: 確かめずにつないでも引き取ってつなぐ。招待が使われた・取り消されたら、預けたものを消す。
/// 待ち合わせに対応した協力ノードにつながっていなければ、合言葉を出さず、読んだ合言葉も問い合わせられない
#[test]
fn meet_code_is_released_and_needs_a_meet_node() {
    let service = spawn_service_with(&["--relay", "--meet"]);
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start_with(vec![service.addr.clone()], true);
    b.start_with(vec![service.addr.clone()], true);
    a.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("A reserved the relay");
    b.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("B reserved the relay");

    // 確かめずに connectInvite: 協力ノードから引き取ってからつなぐ
    let code = a.ok("createInvite", json!({}))["code"].as_str().unwrap().to_owned();
    assert_eq!(code.len(), 11, "{code}");
    let conn_b = b.ok("connectInvite", json!({ "invite": code, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    b.wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    let opened_a = a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");
    b.ok("closeConnection", json!({ "connection": conn_b }));
    let conn_a = opened_a["connection"].as_str().unwrap().to_owned();
    a.wait("connectionClosed", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
        .expect("A closed");

    // 違う合言葉で connectInvite: 引き取れず、閉じた知らせが届く
    let code = a.ok("createInvite", json!({}))["code"].as_str().unwrap().to_owned();
    let conn = b.ok(
        "connectInvite",
        json!({ "invite": other_meet_code(&code), "allowPrivate": false }),
    )["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    let closed = b
        .wait("connectionClosed", |p| p["connection"] == conn.as_str(), TIMEOUT)
        .expect("closed");
    assert_eq!(closed["reason"], "unreachable");

    // 招待文字列で使われた招待の合言葉は、協力ノードから消える（協力ノードが預かっている件数は、1秒ごとに
    // 変わっていればログに出る。中継の数は、閉じた対局の接続がしばらく残るので見ない）
    let created = a.ok("createInvite", json!({}));
    assert!(service.wait_log_where(|line| line.ends_with(" meets=2")));
    let invite = created["invite"].as_str().unwrap().to_owned();
    let conn_b = b.ok("connectInvite", json!({ "invite": invite, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    b.wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    assert!(service.wait_log_where(|line| line.ends_with(" meets=1")));
    b.ok("closeConnection", json!({ "connection": conn_b }));
    let used = b.call("inspectInvite", json!({ "invite": created["code"] }));
    assert_eq!(used["error"]["code"], "invite_not_found", "{used}");

    // 取り消した招待の合言葉も消える
    a.ok("cancelInvites", json!({}));
    assert!(service.wait_log_where(|line| line.ends_with(" meets=0")));
    let cancelled = b.call("inspectInvite", json!({ "invite": code }));
    assert_eq!(cancelled["error"]["code"], "invite_not_found", "{cancelled}");

    // 協力ノードにつながっていないピアは、合言葉を問い合わせられない
    let mut alone = Peer::spawn();
    alone.start(vec![]);
    let no_node = alone.call("inspectInvite", json!({ "invite": code }));
    assert_eq!(no_node["error"]["code"], "invite_no_node", "{no_node}");
    let no_node = alone.call("connectInvite", json!({ "invite": code, "allowPrivate": false }));
    assert_eq!(no_node["error"]["code"], "invite_no_node", "{no_node}");
    // 同じ LAN の合言葉は、協力ノードなしで出る（形が違う）
    let lan = alone.ok("createInvite", json!({}))["code"].as_str().unwrap().to_owned();
    assert!(lan.replace('-', "").len() >= 9 && lan.replace('-', "").len() != 10, "{lan}");
    assert!(sbq_protocol::meet::MeetCode::decode(&lan).is_none());
}

/// 待ち合わせを受け持たない協力ノード（--meet なし）では、協力ノードを使う合言葉を出さない
#[test]
fn node_without_meet_gives_no_meet_code() {
    let service = spawn_service();
    let mut a = Peer::spawn();
    a.start_with(vec![service.addr.clone()], true);
    a.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("A reserved the relay");
    let created = a.ok("createInvite", json!({}));
    assert_eq!(created["code"], "");
    assert!(created["addressCount"].as_u64().unwrap() >= 1);
    let other = sbq_protocol::meet::MeetCode::new(12345).encode().unwrap();
    let no_node = a.call("inspectInvite", json!({ "invite": other }));
    assert_eq!(no_node["error"]["code"], "invite_no_node", "{no_node}");
}

/// 友だち一覧のための要求: meetPut・meetGet は、対局・UI側が作った番号と預けるものを、中身を読まずに協力ノードと
/// やり取りする。createPresence の在席の知らせ（対局・UI側が暗号化して預け、友だちが引き取って復号する）で、
/// 友だちは申し込みの接続を開ける。トークンは期限まで何度でも使え、取り消すと使えない
#[test]
fn presence_connection_and_meet_requests() {
    let service = spawn_service_with(&["--relay", "--meet"]);
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start_with(vec![service.addr.clone()], true);
    b.start_with(vec![service.addr.clone()], true);
    a.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("A reserved the relay");
    b.wait_status(|p| p["relayReservations"].as_u64().unwrap_or(0) >= 1)
        .expect("B reserved the relay");
    // 待ち合わせに対応した協力ノードの数が分かる
    a.wait_status(|p| p["meetNodes"].as_u64().unwrap_or(0) == 1)
        .expect("A sees the meet node");

    // 預けたものは、期限まで何度でも引き取れる（1回限りにしない）
    let slot = "ab".repeat(32);
    let blob = B64.encode(vec![7u8; 64]);
    let put = a.ok("meetPut", json!({ "slot": slot, "blob": blob, "ttlSeconds": 180 }));
    assert_eq!(put["stored"], 1);
    for _ in 0..2 {
        let got = b.ok("meetGet", json!({ "slot": slot }));
        assert_eq!(got["blobs"], json!([blob]));
    }
    let none = b.ok("meetGet", json!({ "slot": "cd".repeat(32) }));
    assert_eq!(none["blobs"], json!([]));
    // 範囲の外の値は断る
    let short = B64.encode(vec![0u8; 39]);
    for params in [
        json!({ "slot": slot, "blob": short, "ttlSeconds": 180 }),
        json!({ "slot": slot, "blob": blob, "ttlSeconds": 601 }),
        json!({ "slot": slot, "blob": blob, "ttlSeconds": 0 }),
        json!({ "slot": "zz", "blob": blob, "ttlSeconds": 180 }),
    ] {
        let bad = a.call("meetPut", params.clone());
        assert_eq!(bad["error"]["code"], "invalid_request", "{params}");
    }
    let bad = a.call("meetGet", json!({ "slot": "ab".repeat(31) }));
    assert_eq!(bad["error"]["code"], "invalid_request");

    // 在席の知らせ
    let presence = a.ok("createPresence", json!({}));
    let id = presence["id"].as_str().unwrap().to_owned();
    assert_ne!(id, "0");
    assert!(presence["addressCount"].as_u64().unwrap() >= 1);
    let body = presence["presence"].as_str().unwrap().to_owned();
    // 自分の知らせにはつながない。読めない知らせは断る
    let own = a.call("connectPresence", json!({ "presence": body, "allowPrivate": false }));
    assert_eq!(own["error"]["code"], "invite_own", "{own}");
    let unreadable = b.call("connectPresence", json!({ "presence": "AAAA", "allowPrivate": false }));
    assert_eq!(unreadable["error"]["code"], "invite_invalid", "{unreadable}");

    // 友だちは、知らせのトークンで何度でも接続を開ける。受けた側には、どの知らせかが番号で分かる
    for _ in 0..2 {
        let conn_b = b.ok("connectPresence", json!({ "presence": body, "allowPrivate": false }))["connection"]
            .as_str()
            .unwrap()
            .to_owned();
        let opened_b = b
            .wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
            .expect("B opened");
        assert_eq!(opened_b["peerId"], a.peer_id.as_str());
        assert_eq!(opened_b["presence"], "0");
        let opened_a = a.wait("connectionOpened", |_| true, TIMEOUT).expect("A opened");
        assert_eq!(opened_a["origin"], "friend");
        assert_eq!(opened_a["presence"], id.as_str());
        assert_eq!(opened_a["peerId"], b.peer_id.as_str());
        let conn_a = opened_a["connection"].as_str().unwrap().to_owned();
        let f1 = hello_frame(1);
        b.ok("sendFrame", json!({ "connection": conn_b, "frame": f1 }));
        let got = a
            .wait("frameReceived", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
            .expect("A frame");
        assert_eq!(got["frame"], f1.as_str());
        b.ok("closeConnection", json!({ "connection": conn_b }));
        a.wait("connectionClosed", |p| p["connection"] == conn_a.as_str(), TIMEOUT)
            .expect("A closed");
    }

    // 取り消した知らせのトークンは受け付けない（何も返さずに閉じる）
    a.ok("cancelPresence", json!({}));
    let conn_b = b.ok("connectPresence", json!({ "presence": body, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    b.wait("connectionClosed", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B closed");
    assert!(a.wait("connectionOpened", |_| true, Duration::from_secs(1)).is_none());
}
