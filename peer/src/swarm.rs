//! libp2p の Swarm（使う機能だけ）

// derive(NetworkBehaviour) が作るコードには、イベントを出さない振る舞い（イベントの型が値を持たない）の分岐があり、
// 新しい rustc はそこを「届かないコード」と警告する。生成されたコードなので、このファイルでは警告にしない
#![allow(unreachable_code)]

use std::sync::Arc;
use std::time::Duration;

use futures::future::Either;
use libp2p::core::muxing::StreamMuxerBox;
use libp2p::core::transport::timeout::TransportTimeout;
use libp2p::core::transport::Transport as _;
use libp2p::core::upgrade::Version;
use libp2p::identity::Keypair;
use libp2p::swarm::behaviour::toggle::Toggle;
use libp2p::swarm::NetworkBehaviour;
use libp2p::{autonat, connection_limits, dcutr, identify, noise, ping, quic, relay, tcp, yamux, Swarm};

use sbq_protocol::network::IDENTIFY_PROTOCOL;

use crate::addr::{AddrPolicy, FilteredTransport};
use crate::behaviours::{PendingPerSource, PublicAutonatClient};

/// 接続の確立（暗号の手続きと多重化の取り決め）の期限。何も送らない接続で確立前の枠を使い続けさせない
const CONNECTION_SETUP_TIMEOUT: Duration = Duration::from_secs(20);
/// 1つの接続元から受け付ける確立前の接続の上限（全体の上限 8 より小さくし、1つの接続元で使い切れないようにする）
const MAX_PENDING_PER_SOURCE: usize = 3;
/// 1つの接続で同時に開ける yamux のストリームの上限（対局・Identify・ping・中継の予約に足りる数）
const MAX_YAMUX_STREAMS: usize = 64;

/// 中継の hop のプロトコル
pub const RELAY_HOP_PROTOCOL: &str = "/libp2p/circuit/relay/0.2.0/hop";

/// 通信プロセスの振る舞い
#[derive(NetworkBehaviour)]
pub struct Behaviour {
    pub limits: connection_limits::Behaviour,
    pub pending_per_source: PendingPerSource,
    pub identify: identify::Behaviour,
    pub ping: ping::Behaviour,
    pub relay_client: Toggle<relay::client::Behaviour>,
    pub relay_server: Toggle<relay::Behaviour>,
    pub dcutr: Toggle<dcutr::Behaviour>,
    /// 到達性の確認のクライアント（AutoNAT v2。確かめてもらう候補はインターネット上のアドレスだけ）
    pub autonat_client: Toggle<PublicAutonatClient>,
    /// 到達性の確認に応える側（協力ノードだけ。ふつうのプレイヤーは、ほかのピアの頼みでそのピアへ接続を始めない）
    pub autonat_server: Toggle<autonat::v2::server::Behaviour>,
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
    /// 到達性の確認を頼む（internet のふつうのプレイヤーで、直接接続を使うとき）
    pub autonat_client: bool,
    /// 到達性の確認に応える（internet の協力ノード）
    pub autonat_server: bool,
}

fn io_err<E: std::fmt::Display>(e: E) -> String {
    e.to_string()
}

fn io_err_owned<E: std::error::Error + Send + Sync + 'static>(e: E) -> std::io::Error {
    std::io::Error::other(e)
}

fn yamux_config() -> yamux::Config {
    let mut config = yamux::Config::default();
    config.set_max_num_streams(MAX_YAMUX_STREAMS);
    config
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
        .multiplex(yamux_config());
    let quic_transport = quic::tokio::Transport::new(quic::Config::new(&keypair));
    let combined = quic_transport
        .or_transport(tcp_and_relay)
        .map(|either, _| match either {
            Either::Left((peer, conn)) => (peer, StreamMuxerBox::new(conn)),
            Either::Right((peer, muxer)) => (peer, StreamMuxerBox::new(muxer)),
        })
        .boxed();
    // 確立の期限は、届いた接続（待受）とつなぐ接続のどちらにも掛かる
    let combined = TransportTimeout::new(combined, CONNECTION_SETUP_TIMEOUT)
        .map_err(io_err_owned)
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
        pending_per_source: PendingPerSource::new(MAX_PENDING_PER_SOURCE),
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
        autonat_client: Toggle::from(options.autonat_client.then(PublicAutonatClient::new)),
        autonat_server: Toggle::from(options.autonat_server.then(autonat::v2::server::Behaviour::default)),
        game: libp2p_stream::Behaviour::new(),
    };

    let config = libp2p::swarm::Config::with_tokio_executor().with_idle_connection_timeout(Duration::from_secs(120));
    Ok(Swarm::new(transport, behaviour, local_peer, config))
}

/// 中継の hop を持つピアか（Identify の protocols）
pub fn supports_relay_hop(info: &identify::Info) -> bool {
    info.protocols.iter().any(|p| p.as_ref() == RELAY_HOP_PROTOCOL)
}
