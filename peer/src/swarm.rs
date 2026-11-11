//! libp2p の Swarm（使う機能だけ）と一覧取得の符号器

use std::io;
use std::sync::Arc;
use std::time::Duration;

use async_trait::async_trait;
use futures::future::Either;
use futures::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};
use libp2p::core::muxing::StreamMuxerBox;
use libp2p::core::transport::Transport as _;
use libp2p::core::upgrade::Version;
use libp2p::identity::Keypair;
use libp2p::kad::store::{MemoryStore, MemoryStoreConfig};
use libp2p::swarm::behaviour::toggle::Toggle;
use libp2p::swarm::NetworkBehaviour;
use libp2p::{
    autonat, connection_limits, dcutr, gossipsub, identify, kad, noise, ping, quic, relay, request_response, tcp,
    yamux, PeerId, StreamProtocol, Swarm,
};
use sbq_protocol::offer::{LOBBY_PROTOCOL, MAX_LIST_MESSAGE_BYTES};

use crate::addr::{AddrPolicy, FilteredTransport};

/// Kademlia のプロトコル（専用の DHT）
pub const KAD_PROTOCOL: &str = "/shogiboardq/kad/1.0.0";
/// Identify で示すプロトコルの版
pub const IDENTIFY_PROTOCOL: &str = "/shogiboardq/1.0.0";
/// 中継の hop のプロトコル
pub const RELAY_HOP_PROTOCOL: &str = "/libp2p/circuit/relay/0.2.0/hop";

/// 一覧取得の要求と応答は決定的CBORのバイト列のまま運ぶ（読むのは受けた側）
#[derive(Clone, Default)]
pub struct ListCodec;

async fn read_lp<T: AsyncRead + Unpin + Send>(io: &mut T) -> io::Result<Vec<u8>> {
    let mut len_buf = [0u8; 4];
    io.read_exact(&mut len_buf).await?;
    let len = usize::try_from(u32::from_be_bytes(len_buf)).map_err(|_| io::Error::other("length"))?;
    if len == 0 || len > MAX_LIST_MESSAGE_BYTES {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "list message too large"));
    }
    let mut buf = vec![0u8; len];
    io.read_exact(&mut buf).await?;
    Ok(buf)
}

async fn write_lp<T: AsyncWrite + Unpin + Send>(io: &mut T, data: &[u8]) -> io::Result<()> {
    if data.is_empty() || data.len() > MAX_LIST_MESSAGE_BYTES {
        return Err(io::Error::new(io::ErrorKind::InvalidInput, "list message too large"));
    }
    let len = u32::try_from(data.len()).map_err(|_| io::Error::other("length"))?;
    io.write_all(&len.to_be_bytes()).await?;
    io.write_all(data).await?;
    io.close().await
}

#[async_trait]
impl request_response::Codec for ListCodec {
    type Protocol = StreamProtocol;
    type Request = Vec<u8>;
    type Response = Vec<u8>;

    async fn read_request<T>(&mut self, _: &StreamProtocol, io: &mut T) -> io::Result<Vec<u8>>
    where
        T: AsyncRead + Unpin + Send,
    {
        read_lp(io).await
    }

    async fn read_response<T>(&mut self, _: &StreamProtocol, io: &mut T) -> io::Result<Vec<u8>>
    where
        T: AsyncRead + Unpin + Send,
    {
        read_lp(io).await
    }

    async fn write_request<T>(&mut self, _: &StreamProtocol, io: &mut T, req: Vec<u8>) -> io::Result<()>
    where
        T: AsyncWrite + Unpin + Send,
    {
        write_lp(io, &req).await
    }

    async fn write_response<T>(&mut self, _: &StreamProtocol, io: &mut T, res: Vec<u8>) -> io::Result<()>
    where
        T: AsyncWrite + Unpin + Send,
    {
        write_lp(io, &res).await
    }
}

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
    pub kad: kad::Behaviour<MemoryStore>,
    pub gossipsub: gossipsub::Behaviour,
    pub lobby: request_response::Behaviour<ListCodec>,
    pub game: libp2p_stream::Behaviour,
}

/// Swarm の組み立ての条件
pub struct SwarmOptions {
    pub keypair: Keypair,
    pub policy: Arc<AddrPolicy>,
    /// 協力ノード（中継の server、DHT の server）
    pub service: bool,
    pub relay_server: bool,
    pub dht_server: bool,
    /// 直接接続を使わない（DCUtR をしない）
    pub relay_only: bool,
    /// AutoNAT を使う（internet）
    pub autonat: bool,
    /// Identify で待受のアドレスを配らない（internet）
    pub hide_listen_addrs: bool,
}

fn io_err<E: std::fmt::Display>(e: E) -> String {
    e.to_string()
}

/// gossipsub のメッセージ ID（データの SHA-256。同じ募集の再受信を同じ ID にする）
pub(crate) fn gossip_message_id(message: &gossipsub::Message) -> gossipsub::MessageId {
    gossipsub::MessageId::from(sbq_protocol::hash::sha256(&message.data).to_vec())
}

/// ピアの採点の閾値（GossipSub v1.1 の既定値。点が graylist を下回ったピアからは何も受け取らない）
pub(crate) fn peer_score_thresholds() -> gossipsub::PeerScoreThresholds {
    gossipsub::PeerScoreThresholds::default()
}

/// ピアの採点の全体の設定
fn peer_score_params() -> gossipsub::PeerScoreParams {
    gossipsub::PeerScoreParams {
        // topic で稼いだ点の上限。長くつながっていても、不正なメッセージの減点を打ち消せないようにする
        topic_score_cap: 10.0,
        // アプリが決める点は使わない
        app_specific_weight: 0.0,
        ..gossipsub::PeerScoreParams::default()
    }
}

/// 募集の topic の採点（実装仕様 3.2節）
///
/// gossipsub は、不正なメッセージの減点（P4）を、topic ごとの設定がある topic でしか数えない。
/// 募集の topic に必ずこの設定を渡す（`Behaviour::set_topic_params`）。
/// - P4: 署名のないメッセージと、検査で拒否した募集（Reject）は1件で -40、2件で -160（graylist の -80 を下回る）。
///   減点は1秒ごとに 0.997 倍に減り、2件の後はおよそ2分で graylist を上回る。
/// - P1・P2: つながっている時間と最初の配送の加点は小さくし（合わせて上限 10）、減点を打ち消せないようにする。
/// - P3: 転送を怠るピアの減点は弱くする（最大 -0.5）。募集は流量が少なく、正しいピアにも届けるものが無い時間が長いので、
///   強い減点は正しいピアを締め出すため。
pub(crate) fn lobby_topic_score_params() -> gossipsub::TopicScoreParams {
    gossipsub::TopicScoreParams {
        topic_weight: 1.0,
        // P1
        time_in_mesh_weight: 0.1,
        time_in_mesh_quantum: Duration::from_secs(1),
        time_in_mesh_cap: 10.0,
        // P2
        first_message_deliveries_weight: 0.5,
        first_message_deliveries_decay: 0.9,
        first_message_deliveries_cap: 10.0,
        // P3
        mesh_message_deliveries_weight: -0.5,
        mesh_message_deliveries_decay: 0.9,
        mesh_message_deliveries_cap: 1.0,
        mesh_message_deliveries_threshold: 1.0,
        mesh_message_deliveries_window: Duration::from_secs(2),
        mesh_message_deliveries_activation: Duration::from_secs(60),
        // P3b
        mesh_failure_penalty_weight: -0.5,
        mesh_failure_penalty_decay: 0.9,
        // P4
        invalid_message_deliveries_weight: -40.0,
        invalid_message_deliveries_decay: 0.997,
    }
}

/// ピアの点が graylist を下回ったか（受け取らない・切断する対象）
pub(crate) fn below_graylist(behaviour: &gossipsub::Behaviour, peer: &PeerId) -> bool {
    behaviour
        .peer_score(peer)
        .is_some_and(|score| score < peer_score_thresholds().graylist_threshold)
}

/// gossipsub の設定（署名のないメッセージを拒否し、検査するまで転送しない。ピアを採点する）
pub(crate) fn gossipsub_behaviour(keypair: &Keypair) -> Result<gossipsub::Behaviour, String> {
    let config = gossipsub::ConfigBuilder::default()
        .validation_mode(gossipsub::ValidationMode::Strict)
        .validate_messages()
        .max_transmit_size(16 * 1024)
        .heartbeat_interval(Duration::from_secs(1))
        .message_id_fn(gossip_message_id)
        .build()
        .map_err(io_err)?;
    let mut behaviour =
        gossipsub::Behaviour::new(gossipsub::MessageAuthenticity::Signed(keypair.clone()), config).map_err(io_err)?;
    // ピアの採点（不正なメッセージ・転送を怠るピアを減点し、点の低いピアから受け取らない）
    behaviour
        .with_peer_score(peer_score_params(), peer_score_thresholds())
        .map_err(io_err)?;
    Ok(behaviour)
}

/// 募集の topic を購読し、採点の設定を渡す
pub(crate) fn subscribe_lobby(
    behaviour: &mut gossipsub::Behaviour,
    topic: &gossipsub::IdentTopic,
) -> Result<(), String> {
    behaviour.subscribe(topic).map_err(io_err)?;
    behaviour
        .set_topic_params(topic.clone(), lobby_topic_score_params())
        .map_err(io_err)
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
        .with_hide_listen_addrs(options.hide_listen_addrs)
        .with_push_listen_addr_updates(false);

    let mut kad_config = kad::Config::new(StreamProtocol::new(KAD_PROTOCOL));
    kad_config
        .set_record_filtering(kad::StoreInserts::FilterBoth)
        .disjoint_query_paths(true)
        .set_query_timeout(Duration::from_secs(30))
        .set_provider_publication_interval(Some(Duration::from_secs(60 * 10)))
        .set_provider_record_ttl(Some(Duration::from_secs(60 * 30)));
    let store = MemoryStore::with_config(
        local_peer,
        MemoryStoreConfig {
            max_records: 0,
            max_value_bytes: 0,
            max_providers_per_key: 256,
            max_provided_keys: 4,
        },
    );
    let mut kad = kad::Behaviour::with_config(local_peer, store, kad_config);
    if options.dht_server {
        kad.set_mode(Some(kad::Mode::Server));
    }

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

    let lobby = request_response::Behaviour::with_codec(
        ListCodec,
        [(
            StreamProtocol::new(LOBBY_PROTOCOL),
            request_response::ProtocolSupport::Full,
        )],
        request_response::Config::default().with_request_timeout(Duration::from_secs(15)),
    );

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
        kad,
        gossipsub: gossipsub_behaviour(&keypair)?,
        lobby,
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
