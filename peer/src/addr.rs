//! 宛先の規則（protocol/ipc.md 5章）と、すべてのダイヤルに規則を掛ける Transport の包み

use std::collections::{HashMap, HashSet};
use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};
use std::pin::Pin;
use std::sync::{Arc, Mutex, MutexGuard};
use std::task::{Context, Poll};
use std::time::{Duration, Instant};

use libp2p::core::muxing::StreamMuxerBox;
use libp2p::core::transport::{Boxed, DialOpts, ListenerId, Transport, TransportError, TransportEvent};
use libp2p::multiaddr::Protocol;
use libp2p::{Multiaddr, PeerId};
use sbq_protocol::network::Profile;

/// 1ピアあたりの宛先の上限
pub const MAX_ADDRS_PER_PEER: usize = 8;
/// 失敗した宛先を再試行しない時間
pub const FAILED_ADDR_BACKOFF: Duration = Duration::from_secs(60);
const MAX_FAILED_ENTRIES: usize = 1024;

/// IP アドレスの区分
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum IpClass {
    Loopback,
    /// 私設（10/8・172.16/12・192.168/16・ULA fc00::/7）
    Private,
    Public,
    /// どの profile でも使わない（未指定・マルチキャスト・link-local・共有アドレス空間・予約済み・文書用・射影）
    Forbidden,
}

fn classify_v4(ip: Ipv4Addr) -> IpClass {
    let [a, b, c, _] = ip.octets();
    if ip.is_loopback() {
        return IpClass::Loopback;
    }
    if a == 10 || (a == 172 && (16..=31).contains(&b)) || (a == 192 && b == 168) {
        return IpClass::Private;
    }
    let forbidden = a == 0
        || ip.is_link_local()
        || (a == 100 && (64..=127).contains(&b))
        || ip.is_multicast()
        || a >= 240
        || ip.is_broadcast()
        || (a == 192 && b == 0 && c == 2)
        || (a == 198 && b == 51 && c == 100)
        || (a == 203 && b == 0 && c == 113)
        || (a == 198 && (18..=19).contains(&b))
        || (a == 192 && b == 0 && c == 0);
    if forbidden {
        return IpClass::Forbidden;
    }
    IpClass::Public
}

fn classify_v6(ip: Ipv6Addr) -> IpClass {
    if ip.is_loopback() {
        return IpClass::Loopback;
    }
    let seg = ip.segments();
    let first = seg.first().copied().unwrap_or(0);
    let second = seg.get(1).copied().unwrap_or(0);
    if (first & 0xfe00) == 0xfc00 {
        return IpClass::Private;
    }
    let ipv4_embedded = seg.get(..5).is_some_and(|s| s.iter().all(|&x| x == 0))
        && matches!(seg.get(5).copied(), Some(0) | Some(0xffff));
    let forbidden = ip.is_unspecified()
        || ipv4_embedded
        || (first & 0xffc0) == 0xfe80
        || (first & 0xff00) == 0xff00
        || (first == 0x2001 && second == 0x0db8)
        || (first == 0x0064 && second == 0xff9b);
    if forbidden {
        return IpClass::Forbidden;
    }
    IpClass::Public
}

pub fn classify(ip: IpAddr) -> IpClass {
    match ip {
        IpAddr::V4(v4) => classify_v4(v4),
        IpAddr::V6(v6) => classify_v6(v6),
    }
}

/// multiaddr の先頭の IP（なければ None）
pub fn first_ip(addr: &Multiaddr) -> Option<IpAddr> {
    match addr.iter().next()? {
        Protocol::Ip4(ip) => Some(IpAddr::V4(ip)),
        Protocol::Ip6(ip) => Some(IpAddr::V6(ip)),
        _ => None,
    }
}

/// 中継経由のアドレスか
pub fn is_circuit(addr: &Multiaddr) -> bool {
    addr.iter().any(|p| matches!(p, Protocol::P2pCircuit))
}

/// DNS 名を含むか
fn has_dns(addr: &Multiaddr) -> bool {
    addr.iter().any(|p| {
        matches!(
            p,
            Protocol::Dns(_) | Protocol::Dns4(_) | Protocol::Dns6(_) | Protocol::Dnsaddr(_)
        )
    })
}

/// 末尾の /p2p/<PeerId> を除く
pub fn without_peer_id(addr: &Multiaddr) -> Multiaddr {
    let mut out = addr.clone();
    if matches!(out.iter().last(), Some(Protocol::P2p(_))) {
        out.pop();
    }
    out
}

/// 末尾の /p2p/<PeerId>
pub fn peer_id_of(addr: &Multiaddr) -> Option<PeerId> {
    match addr.iter().last()? {
        Protocol::P2p(peer) => Some(peer),
        _ => None,
    }
}

/// 宛先の規則
pub struct AddrPolicy {
    profile: Profile,
    /// 利用者が許可した私設アドレス（internet の招待の allowPrivate）
    allowed_private: Mutex<HashSet<IpAddr>>,
    /// 失敗した宛先と時刻
    failed: Mutex<HashMap<Multiaddr, Instant>>,
}

fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    match m.lock() {
        Ok(g) => g,
        Err(poisoned) => poisoned.into_inner(),
    }
}

impl AddrPolicy {
    pub fn new(profile: Profile) -> AddrPolicy {
        AddrPolicy {
            profile,
            allowed_private: Mutex::new(HashSet::new()),
            failed: Mutex::new(HashMap::new()),
        }
    }

    /// IP の区分が、この profile の宛先として使えるか（利用者の許可を含む）
    fn ip_usable(&self, ip: IpAddr) -> bool {
        match (self.profile, classify(ip)) {
            (_, IpClass::Forbidden) => false,
            (Profile::Local, IpClass::Loopback | IpClass::Private) => true,
            (Profile::Local, IpClass::Public) => false,
            (Profile::Internet, IpClass::Public) => true,
            (Profile::Internet, IpClass::Private) => lock(&self.allowed_private).contains(&ip),
            (Profile::Internet, IpClass::Loopback) => false,
        }
    }

    /// 発見などで得た宛先を、この profile で使えるか（失敗の待ちは見ない）
    pub fn usable(&self, addr: &Multiaddr) -> bool {
        if has_dns(addr) {
            return false;
        }
        match addr.iter().next() {
            Some(Protocol::Ip4(_)) | Some(Protocol::Ip6(_)) => first_ip(addr).is_some_and(|ip| self.ip_usable(ip)),
            // /p2p/<relay>/p2p-circuit/... は中継のピアへの接続で規則を掛ける
            Some(Protocol::P2p(_)) => is_circuit(addr),
            _ => false,
        }
    }

    /// 私設アドレスか（internet の招待で利用者に見せるもの）
    pub fn is_private(addr: &Multiaddr) -> bool {
        first_ip(addr).is_some_and(|ip| classify(ip) == IpClass::Private)
    }

    /// 利用者が許可した私設アドレスを足す
    pub fn allow_private(&self, addr: &Multiaddr) {
        if let Some(ip) = first_ip(addr) {
            if classify(ip) == IpClass::Private {
                lock(&self.allowed_private).insert(ip);
            }
        }
    }

    /// 自分のアドレスとして広告してよいか
    pub fn advertisable(&self, addr: &Multiaddr) -> bool {
        if is_circuit(addr) {
            return !has_dns(addr);
        }
        matches!(
            (self.profile, first_ip(addr).map(classify)),
            (Profile::Local, Some(IpClass::Loopback | IpClass::Private)) | (Profile::Internet, Some(IpClass::Public))
        )
    }

    /// ダイヤルしてよいか（失敗の待ちも見る）
    pub fn allows_dial(&self, addr: &Multiaddr) -> bool {
        if !self.usable(addr) {
            return false;
        }
        let key = without_peer_id(addr);
        let failed = lock(&self.failed);
        match failed.get(&key) {
            Some(at) => at.elapsed() >= FAILED_ADDR_BACKOFF,
            None => true,
        }
    }

    /// 失敗した宛先を覚える
    pub fn record_failure(&self, addr: &Multiaddr) {
        let mut failed = lock(&self.failed);
        failed.retain(|_, at| at.elapsed() < FAILED_ADDR_BACKOFF);
        if failed.len() < MAX_FAILED_ENTRIES {
            failed.insert(without_peer_id(addr), Instant::now());
        }
    }

    /// 使える宛先だけを、上限まで残す
    pub fn filter_addrs(&self, addrs: impl IntoIterator<Item = Multiaddr>) -> Vec<Multiaddr> {
        let mut out: Vec<Multiaddr> = Vec::new();
        for a in addrs {
            let a = without_peer_id(&a);
            if self.usable(&a) && !out.contains(&a) {
                out.push(a);
                if out.len() >= MAX_ADDRS_PER_PEER {
                    break;
                }
            }
        }
        out
    }
}

/// すべてのダイヤルに宛先の規則を掛ける Transport
pub struct FilteredTransport {
    inner: Boxed<(PeerId, StreamMuxerBox)>,
    policy: Arc<AddrPolicy>,
}

impl FilteredTransport {
    pub fn new(inner: Boxed<(PeerId, StreamMuxerBox)>, policy: Arc<AddrPolicy>) -> FilteredTransport {
        FilteredTransport { inner, policy }
    }
}

impl Transport for FilteredTransport {
    type Output = (PeerId, StreamMuxerBox);
    type Error = std::io::Error;
    type ListenerUpgrade = <Boxed<(PeerId, StreamMuxerBox)> as Transport>::ListenerUpgrade;
    type Dial = <Boxed<(PeerId, StreamMuxerBox)> as Transport>::Dial;

    fn listen_on(&mut self, id: ListenerId, addr: Multiaddr) -> Result<(), TransportError<Self::Error>> {
        self.inner.listen_on(id, addr)
    }

    fn remove_listener(&mut self, id: ListenerId) -> bool {
        self.inner.remove_listener(id)
    }

    fn dial(&mut self, addr: Multiaddr, opts: DialOpts) -> Result<Self::Dial, TransportError<Self::Error>> {
        if !self.policy.allows_dial(&addr) {
            return Err(TransportError::MultiaddrNotSupported(addr));
        }
        self.inner.dial(addr, opts)
    }

    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<TransportEvent<Self::ListenerUpgrade, Self::Error>> {
        Pin::new(&mut self.get_mut().inner).poll(cx)
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used)]
mod tests {
    use super::*;

    fn ma(s: &str) -> Multiaddr {
        s.parse().unwrap()
    }

    #[test]
    fn internet_rules() {
        let p = AddrPolicy::new(Profile::Internet);
        assert!(p.usable(&ma("/ip4/8.8.8.8/tcp/4001")));
        assert!(p.usable(&ma("/ip6/2400:4050::1/udp/4001/quic-v1")));
        for bad in [
            "/ip4/127.0.0.1/tcp/1",
            "/ip4/10.0.0.1/tcp/1",
            "/ip4/172.16.0.1/tcp/1",
            "/ip4/192.168.1.1/tcp/1",
            "/ip4/169.254.1.1/tcp/1",
            "/ip4/100.64.0.1/tcp/1",
            "/ip4/224.0.0.1/tcp/1",
            "/ip4/0.0.0.0/tcp/1",
            "/ip4/240.0.0.1/tcp/1",
            "/ip4/255.255.255.255/tcp/1",
            "/ip4/192.0.2.1/tcp/1",
            "/ip4/198.51.100.1/tcp/1",
            "/ip4/203.0.113.1/tcp/1",
            "/ip4/198.18.0.1/tcp/1",
            "/ip6/::1/tcp/1",
            "/ip6/::/tcp/1",
            "/ip6/fd00::1/tcp/1",
            "/ip6/fe80::1/tcp/1",
            "/ip6/ff02::1/tcp/1",
            "/ip6/::ffff:127.0.0.1/tcp/1",
            "/ip6/2001:db8::1/tcp/1",
            "/dns4/example.com/tcp/1",
        ] {
            assert!(!p.usable(&ma(bad)), "{bad}");
        }
        let private = ma("/ip4/192.168.1.5/tcp/4001");
        p.allow_private(&private);
        assert!(p.usable(&private));
        assert!(!p.usable(&ma("/ip4/192.168.1.6/tcp/4001")));
    }

    #[test]
    fn local_rules() {
        let p = AddrPolicy::new(Profile::Local);
        assert!(p.usable(&ma("/ip4/127.0.0.1/tcp/4001")));
        assert!(p.usable(&ma("/ip4/192.168.1.1/tcp/4001")));
        assert!(!p.usable(&ma("/ip4/8.8.8.8/tcp/4001")));
        assert!(!p.usable(&ma("/ip4/169.254.1.1/tcp/4001")));
    }

    #[test]
    fn backoff() {
        let p = AddrPolicy::new(Profile::Local);
        let a = ma("/ip4/127.0.0.1/tcp/4001");
        assert!(p.allows_dial(&a));
        p.record_failure(&a);
        assert!(!p.allows_dial(&a));
    }
}
