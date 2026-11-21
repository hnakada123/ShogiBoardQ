//! 合言葉（同じ LAN の招待の短い形。protocol/README.md 3.1節・ipc.md 3章）
//!
//! 招待する側: 招待に付ける合言葉を、待受のアドレスから作る（code_for_invite）。
//! つなぐ側: 合言葉には相手の PeerId が入っていないので、合言葉の宛先へ PeerId を決めずにつなぎ、つながった相手の
//! PeerId と合言葉の鍵からトークンを作って、招待文字列のときと同じ手順で対局ストリームを開く。相手が返す確認の値
//! （同じ鍵から作る）を確かめてから、開いたものとする（node/streams）。つなぐ途中の段階（ダイヤルの結果・相手・鍵・
//! 期限）は node/conns が持つ。
//! 招待文字列（ローカル）に入れる自分のアドレスも、合言葉と同じ選び方にする（lan_invite_addrs）。

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

/// 同じ LAN の相手に渡す自分のアドレスを、待受の IP から選ぶ（合言葉と招待文字列で同じ選び方。監査 2026-11-21 I-1）。
/// 同じ LAN の相手が使う私設アドレスを入れ、私設アドレスが1つもないときだけ loopback（127.0.0.1）を入れる（同じ PC の
/// 相手は、私設アドレスでもつなげる）。idle（回線のつながっていないインターフェースのアドレス）は入れない。ただし、
/// 私設アドレスがすべて idle なら、外さずに入れる。順番は待受の順
fn choose_lan_ips(ips: impl IntoIterator<Item = Ipv4Addr>, idle: &[Ipv4Addr]) -> Vec<Ipv4Addr> {
    let mut private: Vec<Ipv4Addr> = Vec::new();
    let mut loopback = false;
    for ip in ips {
        match addr::classify(ip.into()) {
            IpClass::Private if !private.contains(&ip) => private.push(ip),
            IpClass::Loopback => loopback |= ip == Ipv4Addr::LOCALHOST,
            _ => {}
        }
    }
    if private.iter().any(|ip| !idle.contains(ip)) {
        private.retain(|ip| !idle.contains(ip));
    }
    if !private.is_empty() {
        private
    } else if loopback {
        vec![Ipv4Addr::LOCALHOST]
    } else {
        Vec::new()
    }
}

/// 待受のアドレスから、合言葉に入れるアドレスとポートを決める（アドレスの選び方は choose_lan_ips。4件まで）。
/// ポートは、QUIC も TCP も決まったポートなら None、そうでなければ QUIC のポート。QUIC の待受がなければ作れない
pub fn code_target(listen_addrs: &[Multiaddr], idle: &[Ipv4Addr]) -> Option<(Vec<Ipv4Addr>, Option<u16>)> {
    let mut tcp_fixed = false;
    let mut quic_port: Option<u16> = None;
    let mut quic_ips: Vec<Ipv4Addr> = Vec::new();
    for a in listen_addrs {
        let Some((ip, port, quic)) = listen_parts(a) else { continue };
        if !quic {
            tcp_fixed |= port == LAN_PORT;
            continue;
        }
        if quic_port.is_none() {
            quic_port = Some(port);
        }
        if quic_port == Some(port) {
            quic_ips.push(ip);
        }
    }
    let quic_port = quic_port?;
    let port = if tcp_fixed && quic_port == LAN_PORT {
        None
    } else {
        Some(quic_port)
    };
    let mut ips = choose_lan_ips(quic_ips, idle);
    ips.truncate(code::MAX_ADDRS);
    if ips.is_empty() {
        None
    } else {
        Some((ips, port))
    }
}

/// 長い招待文字列（と在席の知らせ）に入れる、ローカル（同じ PC・同じ LAN）の待受のアドレス。合言葉と同じ選び方の IP
/// （choose_lan_ips）の待受を、TCP と QUIC の両方とも入れる。中継経由の待受（協力ノードを指定したとき）はそのまま
pub fn lan_invite_addrs(listen_addrs: &[Multiaddr], idle: &[Ipv4Addr]) -> Vec<Multiaddr> {
    let chosen = choose_lan_ips(
        listen_addrs.iter().filter_map(|a| listen_parts(a).map(|(ip, _, _)| ip)),
        idle,
    );
    listen_addrs
        .iter()
        .filter(|a| addr::is_circuit(a) || listen_parts(a).is_some_and(|(ip, _, _)| chosen.contains(&ip)))
        .cloned()
        .collect()
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

    /// 相手の PeerId と鍵がそろった。トークンと、相手が返すはずの確認の値を作って、対局ストリームを開く段階へ移る
    /// （確認の値が合わなければ、相手は合言葉を知らない。protocol/README.md 3.1節）
    fn connect_code_peer(&mut self, ready: CodeReady) {
        let inviter = ready.peer.to_bytes();
        let own = self.local_peer.to_bytes();
        let token = code::token(&ready.key, &inviter, &own);
        let confirm = code::confirmation(&ready.key, &inviter, &own);
        let start = self.begin_outbound(ready.peer, ready.addrs, Some(token), Some(confirm), ipc::Origin::Invite);
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

    /// 長い招待文字列のアドレスも、合言葉と同じ選び方: 私設アドレス（idle を除く）の TCP と QUIC。loopback は私設
    /// アドレスが無いときだけ（監査 2026-11-21 I-1）
    #[test]
    fn invite_addrs_follow_the_code_choice() {
        let relay = "12D3KooWDpJ7As7BWAwRMfu1VU2WCqNjvq387JEYKDBj4kx6nXTN";
        let circuit = ma(&format!("/ip4/8.8.8.8/tcp/4001/p2p/{relay}/p2p-circuit"));
        let listen = vec![
            ma("/ip4/127.0.0.1/udp/48210/quic-v1"),
            ma("/ip4/192.168.1.5/udp/48210/quic-v1"),
            ma("/ip4/172.17.0.1/udp/48210/quic-v1"),
            ma("/ip4/127.0.0.1/tcp/48210"),
            ma("/ip4/192.168.1.5/tcp/48210"),
            ma("/ip4/172.17.0.1/tcp/48210"),
            circuit.clone(),
        ];
        assert_eq!(
            lan_invite_addrs(&listen, &[ip("172.17.0.1")]),
            vec![
                ma("/ip4/192.168.1.5/udp/48210/quic-v1"),
                ma("/ip4/192.168.1.5/tcp/48210"),
                circuit.clone()
            ]
        );
        // 私設アドレスがすべて idle なら、外さない（loopback は入れない）
        assert_eq!(
            lan_invite_addrs(&listen, &[ip("172.17.0.1"), ip("192.168.1.5")]),
            vec![
                ma("/ip4/192.168.1.5/udp/48210/quic-v1"),
                ma("/ip4/172.17.0.1/udp/48210/quic-v1"),
                ma("/ip4/192.168.1.5/tcp/48210"),
                ma("/ip4/172.17.0.1/tcp/48210"),
                circuit
            ]
        );
        // 私設アドレスが無ければ loopback（TCP と QUIC の両方）
        let only_loopback = vec![ma("/ip4/127.0.0.1/udp/40001/quic-v1"), ma("/ip4/127.0.0.1/tcp/40002")];
        assert_eq!(lan_invite_addrs(&only_loopback, &[]), only_loopback);
        // 招待にも合言葉にも、同じ IP が入る
        let (code_ips, _) = code_target(&listen, &[ip("172.17.0.1")]).unwrap();
        assert_eq!(code_ips, vec![ip("192.168.1.5")]);
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
