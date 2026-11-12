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
            "protocolVersion": 2,
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
            assert_eq!(v["protocolVersion"], 2);
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
        assert_eq!(hello["ipcVersion"], 2);
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
        &json!({ "protocolVersion": 2, "processGeneration": "7", "requestId": "2", "method": "evil", "params": {} }),
    );
    assert_eq!(p.child.wait().unwrap().code(), Some(2));

    // 世代の違う要求
    let mut p = Peer::spawn();
    p.ok("hello", json!({ "uiVersion": "test" }));
    p.send_raw(&json!({ "protocolVersion": 2, "processGeneration": "8", "requestId": "2", "method": "createInvite", "params": {} }));
    assert_eq!(p.child.wait().unwrap().code(), Some(2));

    // 上限を超える長さ
    let mut p = Peer::spawn();
    let stdin = p.stdin.as_mut().unwrap();
    stdin.write_all(&(200_000u32).to_be_bytes()).unwrap();
    stdin.flush().unwrap();
    assert_eq!(p.child.wait().unwrap().code(), Some(2));

    // 最初の要求が hello でない
    let mut p = Peer::spawn();
    p.send_raw(&json!({ "protocolVersion": 2, "processGeneration": "7", "requestId": "1", "method": "createInvite", "params": {} }));
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
        .args(["--listen", &listen, "--relay"])
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
