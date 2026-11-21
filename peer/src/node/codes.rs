//! 合言葉（同じ LAN の招待の短い形。protocol/README.md 3.1節・ipc.md 3章）
//!
//! 招待する側: 招待に付ける合言葉を、待受のアドレスから作る（code_for_invite）。
//! つなぐ側: 合言葉には相手の PeerId が入っていないので、合言葉の宛先へ PeerId を決めずにつなぎ、つながった相手の
//! PeerId と合言葉の鍵からトークンを作って、招待文字列のときと同じ手順で対局ストリームを開く。つなぐ途中の段階
//! （ダイヤルの結果・相手・鍵・期限）は node/conns が持つ。

use std::net::Ipv4Addr;
use std::time::Instant;

use libp2p::multiaddr::Protocol;
use libp2p::swarm::dial_opts::DialOpts;
use libp2p::swarm::ConnectionId;
use libp2p::{Multiaddr, PeerId};
use rand::RngCore;
use sbq_protocol::code::{self, Code};
use sbq_protocol::network::{Profile, LAN_PORT};

use super::conns::{CodeReady, DialStep};
use super::streams::Internal;
use super::Node;
use crate::addr::{self, IpClass};
use crate::ipc;

/// 待受のアドレスが ip4 の QUIC か TCP なら、その IP・ポート・QUIC か
fn listen_parts(addr: &Multiaddr) -> Option<(Ipv4Addr, u16, bool)> {
    let parts: Vec<Protocol<'_>> = addr.iter().collect();
    match parts.as_slice() {
        [Protocol::Ip4(ip), Protocol::Udp(port), Protocol::QuicV1] => Some((*ip, *port, true)),
        [Protocol::Ip4(ip), Protocol::Tcp(port)] => Some((*ip, *port, false)),
        _ => None,
    }
}

/// 回線のつながっていないインターフェース（コンテナのいない仮想のブリッジなど）の IPv4 アドレス。ほかの PC からは
/// 届かないので、合言葉に入れない（入れると、その分だけ合言葉が長くなる）。調べられなければ空
#[cfg(target_os = "linux")]
pub fn idle_addrs() -> Vec<Ipv4Addr> {
    use nix::net::if_::InterfaceFlags;
    let Ok(interfaces) = nix::ifaddrs::getifaddrs() else {
        return Vec::new();
    };
    let mut out = Vec::new();
    for interface in interfaces {
        if interface
            .flags
            .contains(InterfaceFlags::IFF_UP | InterfaceFlags::IFF_RUNNING)
        {
            continue;
        }
        let ip = interface
            .address
            .as_ref()
            .and_then(|a| a.as_sockaddr_in())
            .map(|a| a.ip());
        if let Some(ip) = ip {
            out.push(ip);
        }
    }
    out
}

/// インターフェースの状態を調べない OS では、どのアドレスも外さない
#[cfg(not(target_os = "linux"))]
pub fn idle_addrs() -> Vec<Ipv4Addr> {
    Vec::new()
}

/// 待受のアドレスから、合言葉に入れるアドレスとポートを決める。同じ LAN の相手が使う私設アドレスを入れ、
/// 私設アドレスが1つもないときだけ loopback を入れる（同じ PC の相手は、私設アドレスでもつなげる）。
/// idle（回線のつながっていないインターフェースのアドレス）は入れない。ただし、私設アドレスがすべて idle なら、
/// 外さずに入れる。
/// ポートは、QUIC も TCP も決まったポートなら None、そうでなければ QUIC のポート。QUIC の待受がなければ作れない
pub fn code_target(listen_addrs: &[Multiaddr], idle: &[Ipv4Addr]) -> Option<(Vec<Ipv4Addr>, Option<u16>)> {
    let mut tcp_fixed = false;
    let mut quic_port: Option<u16> = None;
    let mut private: Vec<Ipv4Addr> = Vec::new();
    let mut loopback = false;
    for a in listen_addrs {
        let Some((ip, port, quic)) = listen_parts(a) else { continue };
        if !quic {
            tcp_fixed |= port == LAN_PORT;
            continue;
        }
        if quic_port.is_none() {
            quic_port = Some(port);
        }
        if quic_port != Some(port) {
            continue;
        }
        match addr::classify(ip.into()) {
            IpClass::Private if !private.contains(&ip) => private.push(ip),
            IpClass::Loopback => loopback |= ip == Ipv4Addr::LOCALHOST,
            _ => {}
        }
    }
    let quic_port = quic_port?;
    let port = if tcp_fixed && quic_port == LAN_PORT {
        None
    } else {
        Some(quic_port)
    };
    if private.iter().any(|ip| !idle.contains(ip)) {
        private.retain(|ip| !idle.contains(ip));
    }
    private.truncate(code::MAX_ADDRS);
    if !private.is_empty() {
        Some((private, port))
    } else if loopback {
        Some((vec![Ipv4Addr::LOCALHOST], port))
    } else {
        None
    }
}

/// 合言葉の宛先。決まったポートなら QUIC と TCP、ポートを書いた合言葉は QUIC だけ
pub fn dial_addrs(code: &Code) -> Vec<Multiaddr> {
    let mut out = Vec::new();
    for ip in &code.addrs {
        let base = Multiaddr::empty().with(Protocol::Ip4(*ip));
        out.push(
            base.clone()
                .with(Protocol::Udp(code.port.unwrap_or(LAN_PORT)))
                .with(Protocol::QuicV1),
        );
        if code.port.is_none() {
            out.push(base.with(Protocol::Tcp(LAN_PORT)));
        }
    }
    out
}

impl Node {
    /// 招待に付ける合言葉（同じ LAN の相手と直接つなぐときだけ。出せなければ None）
    pub(super) fn code_for_invite(&self) -> Option<Code> {
        if self.spec.profile != Profile::Local || self.policy.relay_only() {
            return None;
        }
        let (addrs, port) = code_target(&self.listen_addrs, &idle_addrs())?;
        Code::new(addrs, port, rand::rngs::OsRng.next_u64())
    }

    /// 合言葉の宛先から、自分の待受を除く（同じ仮想の NIC のアドレスを持つ PC どうしで、自分へつなごうとしない）
    pub(super) fn code_addrs(&self, code: &Code) -> Vec<Multiaddr> {
        dial_addrs(code)
            .into_iter()
            .filter(|a| !self.listen_addrs.contains(a))
            .collect()
    }

    /// 合言葉の宛先へつなぎ始め、接続の番号を返す（結果は ConnectionEstablished・OutgoingConnectionError と
    /// Internal::CodeKey で届く）。addrs は、宛先の規則で使える宛先
    pub(super) fn start_code_connect(&mut self, code: Code, addrs: Vec<Multiaddr>) -> u64 {
        let mut dials = Vec::new();
        for a in &addrs {
            let opts = DialOpts::unknown_peer_id().address(a.clone()).build();
            let id = opts.connection_id();
            if self.swarm.dial(opts).is_ok() {
                dials.push(id);
            }
        }
        if dials.is_empty() {
            // 応答（connection の番号）を返した後で知らせる。「直接接続を使わない」で宛先が残らなかったときも
            // unreachable のまま（no_relay_route の画面の文は、招待文字列と協力ノードの登録を案内するため。
            // 監査 2026-11-21 U4）
            return self.conns.connect(Err(ipc::CloseReason::Unreachable), Instant::now());
        }
        let conn = self.conns.dial(addrs, dials, Instant::now());
        // 鍵の導出は時間とメモリを使うので、主ループの外で行う
        let internal = self.internal.clone();
        let network_id = self.network_id;
        tokio::task::spawn_blocking(move || {
            let key = code.derive_key(&network_id);
            let _ = internal.send(Internal::CodeKey { conn, key });
        });
        conn
    }

    /// PeerId を決めずに始めたダイヤルがつながった
    pub(super) fn on_code_dial_established(&mut self, connection: ConnectionId, peer: PeerId) {
        match self.conns.dial_established(connection, peer) {
            DialStep::Wait => {}
            DialStep::CloseExtra => {
                self.swarm.close_connection(connection);
            }
            DialStep::Ready(ready) => self.connect_code_peer(ready),
        }
    }

    /// PeerId を決めずに始めたダイヤルが失敗した
    pub(super) fn on_code_dial_failed(&mut self, connection: ConnectionId) {
        if let DialStep::Ready(ready) = self.conns.dial_failed(connection) {
            self.connect_code_peer(ready);
        }
    }

    /// 合言葉の鍵を作り終えた（作れなければ None）
    pub(super) fn on_code_key(&mut self, conn: u64, key: Option<[u8; 32]>) {
        if let DialStep::Ready(ready) = self.conns.code_key(conn, key) {
            self.connect_code_peer(ready);
        }
    }

    /// 相手の PeerId と鍵がそろった。トークンを作って、対局ストリームを開く段階へ移る
    fn connect_code_peer(&mut self, ready: CodeReady) {
        let token = code::token(&ready.key, &ready.peer.to_bytes(), &self.local_peer.to_bytes());
        let start = self.begin_outbound(ready.peer, ready.addrs, Some(token), ipc::Origin::Invite);
        self.proceed(ready.conn, start);
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used)]
mod tests {
    use super::*;

    fn ma(s: &str) -> Multiaddr {
        s.parse().unwrap()
    }

    fn ip(s: &str) -> Ipv4Addr {
        s.parse().unwrap()
    }

    /// 決まったポートで待ち受けていれば、私設アドレスだけを入れ、ポートは書かない
    #[test]
    fn target_on_the_fixed_port() {
        let listen = vec![
            ma("/ip4/127.0.0.1/udp/48210/quic-v1"),
            ma("/ip4/192.168.1.5/udp/48210/quic-v1"),
            ma("/ip4/172.17.0.1/udp/48210/quic-v1"),
            ma("/ip4/127.0.0.1/tcp/48210"),
            ma("/ip4/192.168.1.5/tcp/48210"),
            ma("/ip4/172.17.0.1/tcp/48210"),
        ];
        assert_eq!(
            code_target(&listen, &[]),
            Some((vec![ip("192.168.1.5"), ip("172.17.0.1")], None))
        );
    }

    /// 決まったポートを使えなかったときは、QUIC のポートを書く（TCP のポートは別なので使わない）
    #[test]
    fn target_on_another_port() {
        let listen = vec![
            ma("/ip4/192.168.1.5/udp/40001/quic-v1"),
            ma("/ip4/192.168.1.5/tcp/40002"),
        ];
        assert_eq!(code_target(&listen, &[]), Some((vec![ip("192.168.1.5")], Some(40001))));
        // QUIC は決まったポートでも、TCP が別のポートなら QUIC だけにする
        let listen = vec![
            ma("/ip4/192.168.1.5/udp/48210/quic-v1"),
            ma("/ip4/192.168.1.5/tcp/40002"),
        ];
        assert_eq!(code_target(&listen, &[]), Some((vec![ip("192.168.1.5")], Some(48210))));
    }

    /// 私設アドレスがなければ loopback（同じ PC の相手）。QUIC の待受がなければ作れない
    #[test]
    fn target_without_a_lan_address() {
        let listen = vec![ma("/ip4/127.0.0.1/udp/40001/quic-v1"), ma("/ip4/127.0.0.1/tcp/40002")];
        assert_eq!(code_target(&listen, &[]), Some((vec![Ipv4Addr::LOCALHOST], Some(40001))));
        assert_eq!(code_target(&[ma("/ip4/192.168.1.5/tcp/48210")], &[]), None);
        assert_eq!(code_target(&[ma("/ip4/8.8.8.8/udp/48210/quic-v1")], &[]), None);
        assert_eq!(code_target(&[], &[]), None);
    }

    /// 入れるアドレスは4件まで
    #[test]
    fn target_keeps_four_addresses() {
        let listen: Vec<Multiaddr> = (1..=6)
            .map(|i| ma(&format!("/ip4/10.0.0.{i}/udp/48210/quic-v1")))
            .collect();
        let (addrs, _) = code_target(&listen, &[]).unwrap();
        assert_eq!(addrs.len(), code::MAX_ADDRS);
        assert_eq!(addrs.first(), Some(&ip("10.0.0.1")));
    }

    /// 回線のつながっていないインターフェースのアドレスは入れない。私設アドレスがすべてそうなら、外さない
    #[test]
    fn target_skips_idle_interfaces() {
        let listen = vec![
            ma("/ip4/172.17.0.1/udp/48210/quic-v1"),
            ma("/ip4/192.168.1.5/udp/48210/quic-v1"),
            ma("/ip4/127.0.0.1/udp/48210/quic-v1"),
            ma("/ip4/192.168.1.5/tcp/48210"),
        ];
        assert_eq!(
            code_target(&listen, &[ip("172.17.0.1")]),
            Some((vec![ip("192.168.1.5")], None))
        );
        assert_eq!(
            code_target(&listen, &[ip("172.17.0.1"), ip("192.168.1.5")]),
            Some((vec![ip("172.17.0.1"), ip("192.168.1.5")], None))
        );
        // 5件目からの切り捨ては、外した後に行う
        let mut many: Vec<Multiaddr> = (1..=4)
            .map(|i| ma(&format!("/ip4/172.17.0.{i}/udp/48210/quic-v1")))
            .collect();
        many.push(ma("/ip4/192.168.1.5/udp/48210/quic-v1"));
        let idle: Vec<Ipv4Addr> = (1..=4).map(|i| ip(&format!("172.17.0.{i}"))).collect();
        assert_eq!(
            code_target(&many, &idle).map(|(addrs, _)| addrs),
            Some(vec![ip("192.168.1.5")])
        );
    }

    /// この PC のインターフェースを調べられる（権限の制限の下でも読めることは、結合試験の合言葉の長さで確かめる）
    #[test]
    fn idle_addrs_never_include_the_loopback() {
        assert!(!idle_addrs().contains(&Ipv4Addr::LOCALHOST));
    }

    #[test]
    fn dial_addrs_follow_the_port() {
        let fixed = Code::new(vec![ip("192.168.1.5")], None, 1).unwrap();
        assert_eq!(
            dial_addrs(&fixed),
            vec![ma("/ip4/192.168.1.5/udp/48210/quic-v1"), ma("/ip4/192.168.1.5/tcp/48210")]
        );
        let other = Code::new(vec![ip("192.168.1.5"), ip("10.0.0.2")], Some(40001), 1).unwrap();
        assert_eq!(
            dial_addrs(&other),
            vec![ma("/ip4/192.168.1.5/udp/40001/quic-v1"), ma("/ip4/10.0.0.2/udp/40001/quic-v1")]
        );
    }
}
