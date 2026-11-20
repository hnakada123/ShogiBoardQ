//! 合言葉（同じ LAN の招待の短い形。protocol/README.md 3.1節・ipc.md 3章）
//!
//! 招待する側: 招待に付ける合言葉を、待受のアドレスから作る（code_for_invite）。
//! つなぐ側: 合言葉には相手の PeerId が入っていないので、合言葉の宛先へ PeerId を決めずにつなぎ、つながった相手の
//! PeerId と合言葉の鍵からトークンを作って、招待文字列のときと同じ手順で対局ストリームを開く。

use std::net::Ipv4Addr;
use std::time::{Duration, Instant};

use libp2p::multiaddr::Protocol;
use libp2p::swarm::dial_opts::DialOpts;
use libp2p::swarm::ConnectionId;
use libp2p::{Multiaddr, PeerId};
use rand::RngCore;
use sbq_protocol::code::{self, Code};
use sbq_protocol::network::{Profile, LAN_PORT};

use super::streams::Internal;
use super::Node;
use crate::addr::{self, IpClass};
use crate::ipc;

/// 合言葉の宛先へつながるのを待つ時間（対局ストリームを開く時間は、この後に別に数える）
const CODE_CONNECT_TIMEOUT: Duration = Duration::from_secs(30);

/// 合言葉でつなぐ途中の接続（相手の PeerId が分かるまで。分かったら streams の接続に移す）
pub struct Pending {
    /// 合言葉の宛先（自分の待受を除いたもの）
    pub addrs: Vec<Multiaddr>,
    /// 結果を待っているダイヤルの数
    pub dials_left: usize,
    /// 最初につながった相手
    pub peer: Option<PeerId>,
    /// 合言葉から作った鍵（作り終えるまで None）
    pub key: Option<[u8; 32]>,
    pub started: Instant,
}

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

    /// 合言葉の宛先へつなぎ始める（結果は ConnectionEstablished・OutgoingConnectionError と Internal::CodeKey で届く）
    pub(super) fn start_code_connect(&mut self, conn: u64, code: Code, addrs: Vec<Multiaddr>) {
        let mut pending = Pending {
            addrs: addrs.clone(),
            dials_left: 0,
            peer: None,
            key: None,
            started: Instant::now(),
        };
        for a in addrs {
            let opts = DialOpts::unknown_peer_id().address(a).build();
            let id = opts.connection_id();
            if self.swarm.dial(opts).is_ok() {
                self.code_dials.insert(id, conn);
                pending.dials_left = pending.dials_left.saturating_add(1);
            }
        }
        let reachable = pending.dials_left > 0;
        self.code_connects.insert(conn, pending);
        if !reachable {
            // 応答（connection の番号）を返した後で知らせる
            let _ = self.internal.send(Internal::CodeFailed {
                conn,
                reason: ipc::CloseReason::Unreachable,
            });
            return;
        }
        // 鍵の導出は時間とメモリを使うので、主ループの外で行う
        let internal = self.internal.clone();
        let network_id = self.network_id;
        tokio::task::spawn_blocking(move || {
            let key = code.derive_key(&network_id);
            let _ = internal.send(Internal::CodeKey { conn, key });
        });
    }

    /// PeerId を決めずに始めたダイヤルがつながった
    pub(super) fn on_code_dial_established(&mut self, connection: ConnectionId, peer: PeerId) {
        let Some(conn) = self.code_dials.remove(&connection) else {
            return;
        };
        let Some(pending) = self.code_connects.get_mut(&conn) else {
            // 対局ストリームを開く段階へ移った後につながった接続は、そのままにする（開く途中の接続を閉じない）
            return;
        };
        pending.dials_left = pending.dials_left.saturating_sub(1);
        if pending.peer.is_some() {
            // 2本目からの接続は使わない（対局ストリームは最初の接続の上で開く）
            self.swarm.close_connection(connection);
            return;
        }
        pending.peer = Some(peer);
        self.advance_code_connect(conn);
    }

    /// PeerId を決めずに始めたダイヤルが失敗した
    pub(super) fn on_code_dial_failed(&mut self, connection: ConnectionId) {
        let Some(conn) = self.code_dials.remove(&connection) else {
            return;
        };
        if let Some(pending) = self.code_connects.get_mut(&conn) {
            pending.dials_left = pending.dials_left.saturating_sub(1);
            self.advance_code_connect(conn);
        }
    }

    /// 合言葉の鍵を作り終えた（作れなければ None）
    pub(super) fn on_code_key(&mut self, conn: u64, key: Option<[u8; 32]>) {
        let Some(pending) = self.code_connects.get_mut(&conn) else {
            return;
        };
        match key {
            Some(key) => {
                pending.key = Some(key);
                self.advance_code_connect(conn);
            }
            None => self.fail_code_connect(conn, ipc::CloseReason::Unreachable),
        }
    }

    /// 相手の PeerId と鍵がそろったら、トークンを作って対局ストリームを開く段階へ移る
    fn advance_code_connect(&mut self, conn: u64) {
        let Some(pending) = self.code_connects.get(&conn) else {
            return;
        };
        match (pending.peer, pending.key) {
            (Some(peer), Some(key)) => {
                let Some(pending) = self.code_connects.remove(&conn) else {
                    return;
                };
                let token = code::token(&key, &peer.to_bytes(), &self.local_peer.to_bytes());
                self.start_outbound(conn, peer, pending.addrs, Some(token), ipc::Origin::Invite);
            }
            (None, _) if pending.dials_left == 0 => self.fail_code_connect(conn, ipc::CloseReason::Unreachable),
            _ => {}
        }
    }

    /// 合言葉での接続をやめて connectionClosed を知らせる
    pub(super) fn fail_code_connect(&mut self, conn: u64, reason: ipc::CloseReason) {
        if self.drop_code_connect(conn) {
            self.out.notify(ipc::Notification::ConnectionClosed {
                connection: conn,
                reason,
            });
        }
    }

    /// 合言葉での接続を、知らせずにやめる（あれば true）
    pub(super) fn drop_code_connect(&mut self, conn: u64) -> bool {
        self.code_dials.retain(|_, c| *c != conn);
        self.code_connects.remove(&conn).is_some()
    }

    /// 時間が過ぎた合言葉での接続をやめる
    pub(super) fn expire_code_connects(&mut self) {
        let now = Instant::now();
        let expired: Vec<u64> = self
            .code_connects
            .iter()
            .filter(|(_, p)| now.saturating_duration_since(p.started) >= CODE_CONNECT_TIMEOUT)
            .map(|(conn, _)| *conn)
            .collect();
        for conn in expired {
            self.fail_code_connect(conn, ipc::CloseReason::Timeout);
        }
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
