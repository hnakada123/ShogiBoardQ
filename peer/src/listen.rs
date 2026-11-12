//! 待受（候補のアドレスと、決まったポートの持ち主を1つに決めて開く手順。protocol/ipc.md 5章）

use libp2p::{Multiaddr, Swarm};
use sbq_protocol::network::{Profile, LAN_PORT};

use crate::swarm::Behaviour;

/// 決まったポートの TCP の待受を、ほかのプロセスが使っていないか（ポートを決めていないアドレスと TCP 以外は true）。
/// libp2p の TCP の待受は、同じポートを複数のプロセスで共有できる設定（SO_REUSEPORT）で開くので、使用中でも開けて
/// しまい、届いた接続が2つのプロセスに振り分けられる。先に、共有しない待受を試して確かめる。
/// 確かめてから開くまでの間に、同時に起動した通信プロセスが同じことをすると両方が開けてしまうので、決まったポートの
/// TCP は、同じ番号の UDP を開けたプロセスだけが試す（fixed_port_allowed）
fn tcp_port_available(addr: &Multiaddr) -> bool {
    let mut ip: Option<std::net::IpAddr> = None;
    for protocol in addr.iter() {
        match protocol {
            libp2p::multiaddr::Protocol::Ip4(v4) => ip = Some(v4.into()),
            libp2p::multiaddr::Protocol::Ip6(v6) => ip = Some(v6.into()),
            libp2p::multiaddr::Protocol::Tcp(port) if port != 0 => {
                return ip.is_some_and(|ip| std::net::TcpListener::bind((ip, port)).is_ok());
            }
            _ => {}
        }
    }
    true
}

/// 決まったポート（LAN_PORT）のアドレスか（UDP か TCP か）
fn fixed_port_of(addr: &Multiaddr) -> Option<&'static str> {
    addr.iter().find_map(|p| match p {
        libp2p::multiaddr::Protocol::Udp(port) if port == LAN_PORT => Some("udp"),
        libp2p::multiaddr::Protocol::Tcp(port) if port == LAN_PORT => Some("tcp"),
        _ => None,
    })
}

/// この待受の候補を試してよいか。決まったポートの TCP は、同じ番号の UDP（QUIC）を開けたプロセスだけが試す。
/// UDP の待受は共有できない（ほかのプロセスが開けていれば失敗する）ので、決まったポートの持ち主が1つに決まる
fn fixed_port_allowed(addr: &Multiaddr, own_fixed_udp: bool) -> bool {
    fixed_port_of(addr) != Some("tcp") || own_fixed_udp
}

/// 待受のアドレスの候補。外側の1件ごとに、内側を先頭から試して最初に開けたものを使う
pub fn candidates(profile: Profile) -> Vec<Vec<Multiaddr>> {
    let groups: Vec<Vec<String>> = match profile {
        // 同じ PC・同じ LAN: LAN 側でも待ち受ける（受け付ける接続元は loopback と私設アドレスだけ。AddrPolicy）。
        // IPv6 では待ち受けない（家庭の LAN の IPv6 はインターネットから届くアドレスのことが多い）。
        // UDP を先に開ける（決まったポートの TCP は、同じ番号の UDP を開けたプロセスだけが使う。fixed_port_allowed）
        Profile::Local => vec![
            vec![
                format!("/ip4/0.0.0.0/udp/{LAN_PORT}/quic-v1"),
                "/ip4/0.0.0.0/udp/0/quic-v1".to_owned(),
            ],
            vec![format!("/ip4/0.0.0.0/tcp/{LAN_PORT}"), "/ip4/0.0.0.0/tcp/0".to_owned()],
        ],
        Profile::Internet => vec![
            vec!["/ip4/0.0.0.0/tcp/0".to_owned()],
            vec!["/ip4/0.0.0.0/udp/0/quic-v1".to_owned()],
            vec!["/ip6/::/tcp/0".to_owned()],
            vec!["/ip6/::/udp/0/quic-v1".to_owned()],
        ],
    };
    groups
        .into_iter()
        .map(|group| group.iter().filter_map(|a| a.parse().ok()).collect())
        .collect()
}

/// 候補の組ごとに、先頭から試して最初に開けたもので待ち受ける。開けた組の数を返す
pub fn open(swarm: &mut Swarm<Behaviour>, candidates: Vec<Vec<Multiaddr>>) -> usize {
    let mut listening = 0usize;
    let mut own_fixed_udp = false;
    for group in candidates {
        // 候補の先頭（決まったポート）が使用中なら、次の候補（空いているポート）で待ち受ける
        if group.into_iter().any(|addr| {
            if !fixed_port_allowed(&addr, own_fixed_udp) {
                return false;
            }
            let fixed_udp = fixed_port_of(&addr) == Some("udp");
            let opened = tcp_port_available(&addr) && swarm.listen_on(addr).is_ok();
            own_fixed_udp |= opened && fixed_udp;
            opened
        }) {
            listening = listening.saturating_add(1);
        }
    }
    listening
}

#[cfg(test)]
#[allow(clippy::unwrap_used)]
mod tests {
    use super::*;

    /// ローカルのネットワークは、TCP と UDP のそれぞれで、決まったポートを先に試し、使用中なら空いているポートを使う。
    /// IPv6 では待ち受けない
    #[test]
    fn local_listens_on_the_fixed_port_first() {
        let groups = candidates(Profile::Local);
        let text: Vec<Vec<String>> = groups
            .iter()
            .map(|g| g.iter().map(ToString::to_string).collect())
            .collect();
        assert_eq!(
            text,
            vec![
                vec![
                    "/ip4/0.0.0.0/udp/48210/quic-v1".to_owned(),
                    "/ip4/0.0.0.0/udp/0/quic-v1".to_owned()
                ],
                vec!["/ip4/0.0.0.0/tcp/48210".to_owned(), "/ip4/0.0.0.0/tcp/0".to_owned()],
            ]
        );
        // インターネットの profile は、空いているポートで IPv4 と IPv6 の両方
        let internet = candidates(Profile::Internet);
        assert_eq!(internet.len(), 4);
        assert!(internet.iter().all(|g| g.len() == 1));
    }

    /// 決まったポートの TCP は、同じ番号の UDP を開けたプロセスだけが試す（同時に起動した通信プロセスどうしが、
    /// TCP のポートを確かめ合って両方開けてしまわないように）。ほかの待受は、いつでも試す
    #[test]
    fn fixed_tcp_port_needs_the_fixed_udp_port() {
        let fixed_tcp: Multiaddr = "/ip4/0.0.0.0/tcp/48210".parse().unwrap();
        let any_tcp: Multiaddr = "/ip4/0.0.0.0/tcp/0".parse().unwrap();
        let fixed_udp: Multiaddr = "/ip4/0.0.0.0/udp/48210/quic-v1".parse().unwrap();
        assert!(!fixed_port_allowed(&fixed_tcp, false));
        assert!(fixed_port_allowed(&fixed_tcp, true));
        assert!(fixed_port_allowed(&any_tcp, false));
        assert!(fixed_port_allowed(&fixed_udp, false));
        assert_eq!(fixed_port_of(&fixed_udp), Some("udp"));
        assert_eq!(fixed_port_of(&fixed_tcp), Some("tcp"));
        assert_eq!(fixed_port_of(&any_tcp), None);
    }

    /// ほかのプロセスが待ち受けている TCP のポートは、使えないと判定する（同じポートを共有しない）
    #[test]
    fn busy_tcp_port_is_not_shared() {
        let holder = std::net::TcpListener::bind(("127.0.0.1", 0)).unwrap();
        let port = holder.local_addr().unwrap().port();
        let busy: Multiaddr = format!("/ip4/127.0.0.1/tcp/{port}").parse().unwrap();
        assert!(!tcp_port_available(&busy));
        drop(holder);
        assert!(tcp_port_available(&busy));
        // ポートを決めていないアドレスと、TCP でないアドレスは確かめない
        assert!(tcp_port_available(&"/ip4/0.0.0.0/tcp/0".parse().unwrap()));
        assert!(tcp_port_available(
            &format!("/ip4/127.0.0.1/udp/{port}/quic-v1").parse().unwrap()
        ));
    }
}
