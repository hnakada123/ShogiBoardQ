//! libp2p の Swarm（使う機能だけ）

use std::sync::Arc;
use std::time::Duration;

use futures::future::Either;
use libp2p::core::muxing::StreamMuxerBox;
use libp2p::core::transport::Transport as _;
use libp2p::core::upgrade::Version;
use libp2p::identity::Keypair;
use libp2p::swarm::behaviour::toggle::Toggle;
use libp2p::swarm::NetworkBehaviour;
use libp2p::{autonat, connection_limits, dcutr, identify, noise, ping, quic, relay, tcp, yamux, PeerId, Swarm};

use crate::addr::{AddrPolicy, FilteredTransport};

/// Identify で示すプロトコルの版
pub const IDENTIFY_PROTOCOL: &str = "/shogiboardq/1.0.0";
/// 中継の hop のプロトコル
pub const RELAY_HOP_PROTOCOL: &str = "/libp2p/circuit/relay/0.2.0/hop";

/// 通信プロセスの振る舞い
#[derive(NetworkBehaviour)]
pub struct Behaviour {
    pub limits: connection_limits::Behaviour,
    pub identify: identify::Behaviour,
    pub ping: ping::Behaviour,
    pub relay_client: Toggle<relay::client::Behaviour>,
    pub relay_server: Toggle<relay::Behaviour>,
    pub dcutr: Toggle<dcutr::Behaviour>,
    pub autonat: Toggle<autonat::Behaviour>,
    pub game: libp2p_stream::Behaviour,
}

/// Swarm の組み立ての条件
pub struct SwarmOptions {
    pub keypair: Keypair,
    pub policy: Arc<AddrPolicy>,
    /// 協力ノード（中継の server）
    pub service: bool,
    pub relay_server: bool,
    /// 直接接続を使わない（DCUtR をしない）
    pub relay_only: bool,
    /// AutoNAT を使う（internet で、直接接続を使うとき）
    pub autonat: bool,
}

fn io_err<E: std::fmt::Display>(e: E) -> String {
    e.to_string()
}

/// Swarm を作る
pub fn build(options: SwarmOptions) -> Result<Swarm<Behaviour>, String> {
    let keypair = options.keypair.clone();
    let local_peer = keypair.public().to_peer_id();

    let (relay_transport, relay_client) = relay::client::new(local_peer);
    let tcp_and_relay = relay_transport
        .or_transport(tcp::tokio::Transport::new(tcp::Config::default().nodelay(true)))
        .upgrade(Version::V1Lazy)
        .authenticate(noise::Config::new(&keypair).map_err(io_err)?)
        .multiplex(yamux::Config::default());
    let quic_transport = quic::tokio::Transport::new(quic::Config::new(&keypair));
    let combined = quic_transport
        .or_transport(tcp_and_relay)
        .map(|either, _| match either {
            Either::Left((peer, conn)) => (peer, StreamMuxerBox::new(conn)),
            Either::Right((peer, muxer)) => (peer, StreamMuxerBox::new(muxer)),
        })
        .boxed();
    let transport = FilteredTransport::new(combined, options.policy.clone()).boxed();

    let limits = connection_limits::ConnectionLimits::default()
        .with_max_established(Some(32))
        .with_max_pending_incoming(Some(8))
        .with_max_pending_outgoing(Some(8))
        .with_max_established_per_peer(Some(4));

    let identify_config = identify::Config::new(IDENTIFY_PROTOCOL.to_owned(), keypair.public())
        .with_agent_version(format!("shogiboardq-peer/{}", env!("CARGO_PKG_VERSION")))
        // 待受のアドレスをそのままは配らない。配るのは、自分のアドレスとして確かめた・決めたものだけ
        // （local は loopback と私設アドレス、internet は AutoNAT で確かめたアドレス、どちらも中継経由のアドレス）
        .with_hide_listen_addrs(true)
        .with_push_listen_addr_updates(false);

    let relay_server = if options.relay_server {
        let config = relay::Config {
            max_reservations: 128,
            max_reservations_per_peer: 1,
            max_circuits: 256,
            max_circuits_per_peer: 4,
            max_circuit_duration: Duration::from_secs(30 * 60),
            max_circuit_bytes: 8 * 1024 * 1024,
            ..relay::Config::default()
        };
        Some(relay::Behaviour::new(local_peer, config))
    } else {
        None
    };

    let behaviour = Behaviour {
        limits: connection_limits::Behaviour::new(limits),
        identify: identify::Behaviour::new(identify_config),
        ping: ping::Behaviour::new(
            ping::Config::new()
                .with_interval(Duration::from_secs(10))
                .with_timeout(Duration::from_secs(30)),
        ),
        relay_client: Toggle::from(if options.service { None } else { Some(relay_client) }),
        relay_server: Toggle::from(relay_server),
        dcutr: Toggle::from(if options.relay_only || options.service {
            None
        } else {
            Some(dcutr::Behaviour::new(local_peer))
        }),
        autonat: Toggle::from(if options.autonat {
            Some(autonat::Behaviour::new(
                local_peer,
                autonat::Config {
                    only_global_ips: true,
                    ..Default::default()
                },
            ))
        } else {
            None
        }),
        game: libp2p_stream::Behaviour::new(),
    };

    let config = libp2p::swarm::Config::with_tokio_executor().with_idle_connection_timeout(Duration::from_secs(120));
    Ok(Swarm::new(transport, behaviour, local_peer, config))
}

/// 中継の hop を持つピアか（Identify の protocols）
pub fn supports_relay_hop(info: &identify::Info) -> bool {
    info.protocols.iter().any(|p| p.as_ref() == RELAY_HOP_PROTOCOL)
}

/// ピアの PeerId を表記から読む（Ed25519 の identity multihash に限る）
pub fn parse_peer_id(text: &str) -> Option<PeerId> {
    let peer: PeerId = text.parse().ok()?;
    if sbq_protocol::peerid::is_ed25519(&peer.to_bytes()) {
        Some(peer)
    } else {
        None
    }
}
