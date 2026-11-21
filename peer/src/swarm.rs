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

use sbq_protocol::network::{Profile, IDENTIFY_PROTOCOL};

use crate::addr::{AddrPolicy, FilteredTransport};
use crate::behaviours::{MeetSources, PendingPerSource, PublicAutonatClient};
use crate::limits::TierLimits;
use crate::meet::MeetService;

/// 接続の確立（暗号の手続きと多重化の取り決め）の期限。何も送らない接続で確立前の枠を使い続けさせない
const CONNECTION_SETUP_TIMEOUT: Duration = Duration::from_secs(20);
/// 1つの接続で同時に開ける yamux のストリームの上限（対局・Identify・ping・中継の予約に足りる数）
const MAX_YAMUX_STREAMS: usize = 64;

/// 接続の数の上限（役割ごと。protocol/ipc.md 5章・7章）
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ConnLimits {
    /// 確立した接続（受けと掛けの合計）
    pub established: u32,
    /// 確立前の接続（受け・掛け）
    pub pending_incoming: u32,
    pub pending_outgoing: u32,
    /// 1ピアあたりの確立した接続
    pub per_peer: u32,
    /// 確立前の受けの、接続元ごと・アドレス帯ごとの上限（behaviours::PendingPerSource）
    pub pending_per_source: TierLimits,
    /// 受け付けた（確立した直接の）接続の、接続元ごと・アドレス帯ごとの上限（node/peerlinks）
    pub inbound_per_source: TierLimits,
}

/// 中継の hop のプロトコル
pub const RELAY_HOP_PROTOCOL: &str = "/libp2p/circuit/relay/0.2.0/hop";

/// 通信プロセスの振る舞い
#[derive(NetworkBehaviour)]
pub struct Behaviour {
    pub limits: connection_limits::Behaviour,
    pub pending_per_source: PendingPerSource,
    /// 待ち合わせの要求の接続元（待ち合わせを受け持つ協力ノードだけ）
    pub meet_sources: Toggle<MeetSources>,
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

/// 通信プロセスの役割
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Role {
    /// 対局・UI側から使うプレイヤー。relay_only は「直接接続を使わない」（協力ノードの中継だけを使う）
    Player { relay_only: bool },
    /// 協力ノード（--service）。relay は中継（--relay）を、meet は合言葉の待ち合わせ（--meet）を受け持つ
    Service { relay: bool, meet: bool },
}

impl Role {
    /// 協力ノードか
    pub fn is_service(self) -> bool {
        matches!(self, Role::Service { .. })
    }

    /// 直接接続を使わないプレイヤーか
    pub fn relay_only(self) -> bool {
        matches!(self, Role::Player { relay_only: true })
    }

    /// 中継を受け持つ協力ノードか
    pub fn relays(self) -> bool {
        matches!(self, Role::Service { relay: true, .. })
    }

    /// 合言葉の待ち合わせを受け持つ協力ノードか
    pub fn meets(self) -> bool {
        matches!(self, Role::Service { meet: true, .. })
    }

    /// 直接の接続（穴あけ・DCUtR）と到達性の確認の依頼（AutoNAT）を使うか。直接接続を使うプレイヤーだけ
    /// （「直接接続を使わない」では、どちらも自分のアドレスの候補を相手に渡すので使わない。協力ノードは使わない）
    pub fn uses_direct(self) -> bool {
        matches!(self, Role::Player { relay_only: false })
    }

    /// 接続の数の上限。プレイヤーは、協力ノードと招待・対局の相手だけとつながるので小さく（全体 32）、IPv4 の /24・
    /// IPv6 の /48 からは 8 本まで。協力ノードは、利用者のアプリ（在席を知らせる利用者・中継・待ち合わせ）を数百
    /// 受けるので大きくし、帯ごとの上限を2段にする: 1つの接続元（IPv4 のアドレス・IPv6 の /64）から 16 本、同じ
    /// /24・/48 から 64 本（CGNAT・IPoE の利用者どうしが分け合っても足り、数個のアドレス帯では埋められない。
    /// 監査 2026-11-21 S2）。協力ノードの掛ける接続は、ほかの協力ノードと、到達性の確認の折り返し（AutoNAT）
    pub fn limits(self) -> ConnLimits {
        match self {
            Role::Player { .. } => ConnLimits {
                established: 32,
                pending_incoming: 8,
                pending_outgoing: 8,
                per_peer: 4,
                pending_per_source: TierLimits {
                    per_source: 3,
                    per_prefix: 8,
                },
                inbound_per_source: TierLimits {
                    per_source: 8,
                    per_prefix: 8,
                },
            },
            Role::Service { .. } => ConnLimits {
                established: 512,
                pending_incoming: 64,
                pending_outgoing: 32,
                per_peer: 4,
                pending_per_source: TierLimits {
                    per_source: 4,
                    per_prefix: 16,
                },
                inbound_per_source: TierLimits {
                    per_source: 16,
                    per_prefix: 64,
                },
            },
        }
    }
}

/// Swarm の組み立ての条件
pub struct SwarmOptions {
    pub keypair: Keypair,
    pub policy: Arc<AddrPolicy>,
    pub role: Role,
    /// ネットワークの profile（インターネットでは到達性の確認を使う。接続元の数え方も profile で決まる）
    pub profile: Profile,
    /// 待ち合わせ（協力ノードの --meet）。受け付けた接続の接続元を、その接続の要求が届く前に覚えさせる
    pub meet: Option<Arc<MeetService>>,
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

    let role = options.role;
    let role_limits = role.limits();
    let limits = connection_limits::ConnectionLimits::default()
        .with_max_established(Some(role_limits.established))
        .with_max_pending_incoming(Some(role_limits.pending_incoming))
        .with_max_pending_outgoing(Some(role_limits.pending_outgoing))
        .with_max_established_per_peer(Some(role_limits.per_peer));
    let internet = options.profile == Profile::Internet;

    let identify_config = identify::Config::new(IDENTIFY_PROTOCOL.to_owned(), keypair.public())
        .with_agent_version(format!("shogiboardq-peer/{}", env!("CARGO_PKG_VERSION")))
        // 待受のアドレスをそのままは配らない。配るのは、自分のアドレスとして確かめた・決めたものだけ
        // （local は loopback と私設アドレス、internet は AutoNAT で確かめたアドレス、どちらも中継経由のアドレス）
        .with_hide_listen_addrs(true)
        .with_push_listen_addr_updates(false);

    let relay_server = if role.relays() {
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
        pending_per_source: PendingPerSource::new(role_limits.pending_per_source, options.profile),
        // 接続の上限（limits）が断った接続は覚えない（上の振る舞いが先に断る）
        meet_sources: Toggle::from(options.meet.map(|service| MeetSources::new(service, options.profile))),
        identify: identify::Behaviour::new(identify_config),
        ping: ping::Behaviour::new(
            ping::Config::new()
                .with_interval(Duration::from_secs(10))
                .with_timeout(Duration::from_secs(30)),
        ),
        relay_client: Toggle::from(if role.is_service() { None } else { Some(relay_client) }),
        relay_server: Toggle::from(relay_server),
        dcutr: Toggle::from(role.uses_direct().then(|| dcutr::Behaviour::new(local_peer))),
        // 到達性の確認（AutoNAT）を頼むのは、インターネットで直接接続を使うプレイヤーだけ。確認に応えるのは協力ノード
        // だけ（AutoNAT v2 のクライアントは、応える側のピアにだけ頼むので、確認の依頼は協力ノードにだけ届く）
        autonat_client: Toggle::from((internet && role.uses_direct()).then(PublicAutonatClient::new)),
        autonat_server: Toggle::from((internet && role.is_service()).then(autonat::v2::server::Behaviour::default)),
        game: libp2p_stream::Behaviour::new(),
    };

    let config = libp2p::swarm::Config::with_tokio_executor().with_idle_connection_timeout(Duration::from_secs(120));
    Ok(Swarm::new(transport, behaviour, local_peer, config))
}

/// 中継の hop を持つピアか（Identify の protocols）
pub fn supports_relay_hop(info: &identify::Info) -> bool {
    info.protocols.iter().any(|p| p.as_ref() == RELAY_HOP_PROTOCOL)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// 役割から決まること（以前の bool の組 service・relay_only・relay_server・meet_server と同じ）
    #[test]
    fn role_decides_the_behaviours() {
        let row = |role: Role| {
            (
                role.is_service(),
                role.relay_only(),
                role.relays(),
                role.meets(),
                role.uses_direct(),
            )
        };
        assert_eq!(
            row(Role::Player { relay_only: false }),
            (false, false, false, false, true)
        );
        assert_eq!(
            row(Role::Player { relay_only: true }),
            (false, true, false, false, false)
        );
        assert_eq!(
            row(Role::Service {
                relay: true,
                meet: false
            }),
            (true, false, true, false, false)
        );
        assert_eq!(
            row(Role::Service {
                relay: false,
                meet: true
            }),
            (true, false, false, true, false)
        );
    }

    /// 協力ノードは数百の接続を受け、帯ごとの上限は2段（1つの接続元より同じアドレス帯を緩く）。数個のアドレス帯では
    /// 確立した接続を埋められない（監査 2026-11-21 S2）。プレイヤーの上限は前のまま
    #[test]
    fn service_limits_are_larger_and_two_tiered() {
        let player = Role::Player { relay_only: false }.limits();
        assert_eq!(
            (
                player.established,
                player.pending_incoming,
                player.pending_outgoing,
                player.per_peer
            ),
            (32, 8, 8, 4)
        );
        assert_eq!(player.inbound_per_source.per_prefix, 8);
        assert_eq!(player.pending_per_source.per_source, 3);
        let service = Role::Service {
            relay: true,
            meet: true,
        }
        .limits();
        assert!(service.established >= 256);
        assert!(service.pending_incoming > player.pending_incoming);
        for tiers in [service.inbound_per_source, service.pending_per_source] {
            assert!(tiers.per_source < tiers.per_prefix);
        }
        // 4つのアドレス帯では、確立した接続を埋められない
        let per_prefix = u32::try_from(service.inbound_per_source.per_prefix).unwrap_or(u32::MAX);
        assert!(per_prefix.saturating_mul(4) < service.established);
        // 2つのアドレス帯では、確立前の受けを埋められない
        let pending_per_prefix = u32::try_from(service.pending_per_source.per_prefix).unwrap_or(u32::MAX);
        assert!(pending_per_prefix.saturating_mul(2) < service.pending_incoming);
    }
}
