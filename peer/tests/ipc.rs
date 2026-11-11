//! 通信プロセスを起動し、IPC（protocol/ipc.md）で動かす結合試験（ローカル検証用のネットワーク sbq-local-1）

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
            "protocolVersion": 1,
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
            assert_eq!(v["protocolVersion"], 1);
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
        let hello = self.ok("hello", json!({ "uiVersion": "test" }));
        assert_eq!(hello["ipcVersion"], 1);
        assert_eq!(hello["sandbox"]["applied"], true, "{hello}");
        let rules = hex(&network::rules_hash_v1());
        let start = self.ok(
            "start",
            json!({ "networkName": "sbq-local-1", "rulesHash": rules, "bootstrap": bootstrap, "knownPeers": [], "relayOnly": false }),
        );
        assert_eq!(start["profile"], "local");
        assert_eq!(start["networkId"], hex(&network::network_id("sbq-local-1")));
        let bytes = B64.decode(start["peerIdBytes"].as_str().unwrap()).unwrap();
        assert!(sbq_protocol::peerid::is_ed25519(&bytes));
        self.peer_id = start["peerId"].as_str().unwrap().to_owned();
        self.wait_status(|p| p["listenAddrs"].as_array().is_some_and(|a| !a.is_empty()))
            .expect("listening");
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

    // 使った招待はもう使えない（招待した側に届かない）
    let reused = b.ok("connectInvite", json!({ "invite": text, "allowPrivate": false }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    if b.wait("connectionOpened", |p| p["connection"] == reused.as_str(), TIMEOUT)
        .is_some()
    {
        b.ok("sendFrame", json!({ "connection": reused, "frame": hello_frame(3) }));
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
        &json!({ "protocolVersion": 1, "processGeneration": "7", "requestId": "2", "method": "evil", "params": {} }),
    );
    assert_eq!(p.child.wait().unwrap().code(), Some(2));

    // 世代の違う要求
    let mut p = Peer::spawn();
    p.ok("hello", json!({ "uiVersion": "test" }));
    p.send_raw(&json!({ "protocolVersion": 1, "processGeneration": "8", "requestId": "2", "method": "listOffers", "params": {} }));
    assert_eq!(p.child.wait().unwrap().code(), Some(2));

    // 上限を超える長さ
    let mut p = Peer::spawn();
    let stdin = p.stdin.as_mut().unwrap();
    stdin.write_all(&(200_000u32).to_be_bytes()).unwrap();
    stdin.flush().unwrap();
    assert_eq!(p.child.wait().unwrap().code(), Some(2));

    // 最初の要求が hello でない
    let mut p = Peer::spawn();
    p.send_raw(&json!({ "protocolVersion": 1, "processGeneration": "7", "requestId": "1", "method": "listOffers", "params": {} }));
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
        json!({ "networkName": "sbq-local-1", "rulesHash": "00", "bootstrap": [], "knownPeers": [], "relayOnly": false, "x": 1 }),
    );
    assert_eq!(r["error"]["code"], "invalid_request");
    let r = p.call(
        "start",
        json!({ "networkName": "public-1", "rulesHash": hex(&[0u8; 32]), "bootstrap": [], "knownPeers": [], "relayOnly": false }),
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
    for name in [
        "read_file",
        "write_file",
        "exec",
        "fork",
        "unix_socket",
        "unix_connect_abstract",
        "signal_parent",
    ] {
        assert_eq!(v["results"][name], "denied", "{name}: {v}");
    }
}

/// 協力ノード（--service）を起動し、PeerId と待受のアドレスを返す
struct Service {
    child: Child,
    addr: String,
    dir: std::path::PathBuf,
}

impl Drop for Service {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
        let _ = std::fs::remove_dir_all(&self.dir);
    }
}

fn spawn_service() -> Service {
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
        .args(["--listen", &listen, "--dht-server", "--relay"])
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
    std::thread::spawn(move || {
        let mut sink = String::new();
        let _ = stderr.read_to_string(&mut sink);
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
    }
}

#[test]
fn offers_through_service_node() {
    let service = spawn_service();
    let mut a = Peer::spawn();
    let mut b = Peer::spawn();
    a.start(vec![service.addr.clone()]);
    b.start(vec![service.addr.clone()]);
    a.wait_status(|p| p["connectedPeers"].as_u64().unwrap_or(0) >= 1)
        .expect("A connected");
    b.wait_status(|p| p["connectedPeers"].as_u64().unwrap_or(0) >= 1)
        .expect("B connected");

    // 規則に合わない募集は出せない
    let bad = a.call(
        "publishOffer",
        json!({ "displayName": " x", "mainMs": "600000", "byoyomiMs": "30000", "sidePreference": "none" }),
    );
    assert_eq!(bad["error"]["code"], "invalid_request");

    let offer = a.ok(
        "publishOffer",
        json!({ "displayName": "将棋太郎", "mainMs": "600000", "byoyomiMs": "30000", "sidePreference": "black" }),
    );
    let offer_id = offer["offerId"].as_str().unwrap().to_owned();

    let mut found = None;
    let deadline = Instant::now() + Duration::from_secs(40);
    while found.is_none() && Instant::now() < deadline {
        found = b.wait(
            "offersChanged",
            |p| {
                p["added"]
                    .as_array()
                    .is_some_and(|a| a.iter().any(|o| o["offerId"] == offer_id.as_str()))
            },
            Duration::from_secs(3),
        );
        if found.is_none() {
            b.ok("listOffers", json!({}));
        }
    }
    let changed = found.expect("B did not see A's offer");
    let item = changed["added"]
        .as_array()
        .unwrap()
        .iter()
        .find(|o| o["offerId"] == offer_id.as_str())
        .unwrap()
        .clone();
    assert_eq!(item["displayName"], "将棋太郎");
    assert_eq!(item["sidePreference"], "black");
    assert_eq!(item["peerId"], a.peer_id.as_str());
    assert_eq!(item["mainMs"], "600000");

    // 募集へ申し込む（最初のフレームが届いてから募集側へ知らせる）
    let conn_b = b.ok("connectOffer", json!({ "offerId": offer_id }))["connection"]
        .as_str()
        .unwrap()
        .to_owned();
    b.wait("connectionOpened", |p| p["connection"] == conn_b.as_str(), TIMEOUT)
        .expect("B opened");
    b.ok("sendFrame", json!({ "connection": conn_b, "frame": hello_frame(1) }));
    let opened = a
        .wait("connectionOpened", |p| p["origin"] == "offer", TIMEOUT)
        .expect("A got the applicant");
    assert_eq!(opened["peerId"], b.peer_id.as_str());

    // 取り下げると相手の一覧から消える
    a.ok("withdrawOffer", json!({}));
    b.wait(
        "offersChanged",
        |p| {
            p["removed"]
                .as_array()
                .is_some_and(|r| r.iter().any(|o| o == offer_id.as_str()))
        },
        Duration::from_secs(30),
    )
    .expect("withdrawal not seen");
}
