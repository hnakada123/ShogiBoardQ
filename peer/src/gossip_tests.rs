//! 募集の GossipSub の振る舞いの試験（実装仕様 2.2節の表の項目を、採用した版で動かして確かめる）
//!
//! 通信プロセスと同じ設定（`swarm::gossipsub_behaviour`・`swarm::subscribe_lobby`）の検証役 B に、
//! 攻撃役（署名する S・署名しない U）がメッセージを送り、B の先の観測役 C に何が届くかと、B が付ける点を見る。
//! つなぎ方は S — B — C、U — B（攻撃役と C は直接つながない）。
//!
//! 1. 署名のないメッセージを拒否する（B のアプリに届かず、C に転送されず、U が減点される）
//! 2. アプリが検証するまで転送しない（B が Accept を返すまで C に届かない）
//! 3. 検証で拒否したメッセージを転送せず、送ったピアを減点する。点が graylist を下回ったピアからは受け取らない

#![allow(
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::indexing_slicing,
    clippy::arithmetic_side_effects
)]

use std::collections::HashMap;
use std::time::{Duration, Instant};

use futures::StreamExt;
use libp2p::core::transport::{MemoryTransport, Transport};
use libp2p::core::upgrade::Version;
use libp2p::identity::Keypair;
use libp2p::swarm::{Swarm, SwarmEvent};
use libp2p::{gossipsub, noise, yamux, Multiaddr, PeerId};

use crate::swarm;

const TOPIC: &str = "/shogiboardq/lobby/1/test/test";

fn memory_swarm(keypair: &Keypair, behaviour: gossipsub::Behaviour) -> Swarm<gossipsub::Behaviour> {
    let transport = MemoryTransport::default()
        .upgrade(Version::V1)
        .authenticate(noise::Config::new(keypair).unwrap())
        .multiplex(yamux::Config::default())
        .boxed();
    Swarm::new(
        transport,
        behaviour,
        keypair.public().to_peer_id(),
        libp2p::swarm::Config::with_tokio_executor().with_idle_connection_timeout(Duration::from_secs(120)),
    )
}

/// 攻撃役の gossipsub（自分では検査しない。署名するかを選べる）
fn attacker_behaviour(keypair: &Keypair, signed: bool) -> gossipsub::Behaviour {
    let config = gossipsub::ConfigBuilder::default()
        .validation_mode(if signed {
            gossipsub::ValidationMode::Strict
        } else {
            gossipsub::ValidationMode::Anonymous
        })
        .heartbeat_interval(Duration::from_secs(1))
        .message_id_fn(swarm::gossip_message_id)
        .build()
        .unwrap();
    let authenticity = if signed {
        gossipsub::MessageAuthenticity::Signed(keypair.clone())
    } else {
        gossipsub::MessageAuthenticity::Anonymous
    };
    gossipsub::Behaviour::new(authenticity, config).unwrap()
}

/// 4つのノード
struct Net {
    b: Swarm<gossipsub::Behaviour>,
    c: Swarm<gossipsub::Behaviour>,
    s: Swarm<gossipsub::Behaviour>,
    u: Swarm<gossipsub::Behaviour>,
    /// B のアプリに届いた、検証待ちのメッセージ（データ → (ID, 送ってきたピア)）
    b_pending: HashMap<Vec<u8>, (gossipsub::MessageId, PeerId)>,
    /// B のアプリに届いたメッセージのデータ（届いた順）
    b_received: Vec<Vec<u8>>,
    /// C に届いたメッセージのデータ
    c_received: Vec<Vec<u8>>,
}

impl Net {
    async fn new() -> Net {
        let topic = gossipsub::IdentTopic::new(TOPIC);
        let kb = Keypair::generate_ed25519();
        let kc = Keypair::generate_ed25519();
        let ks = Keypair::generate_ed25519();
        let ku = Keypair::generate_ed25519();
        // B と C は通信プロセスと同じ設定（署名の必須・検証するまで転送しない・採点）
        let mut b = memory_swarm(&kb, swarm::gossipsub_behaviour(&kb).unwrap());
        let mut c = memory_swarm(&kc, swarm::gossipsub_behaviour(&kc).unwrap());
        swarm::subscribe_lobby(b.behaviour_mut(), &topic).unwrap();
        swarm::subscribe_lobby(c.behaviour_mut(), &topic).unwrap();
        let mut s = memory_swarm(&ks, attacker_behaviour(&ks, true));
        let mut u = memory_swarm(&ku, attacker_behaviour(&ku, false));
        s.behaviour_mut().subscribe(&topic).unwrap();
        u.behaviour_mut().subscribe(&topic).unwrap();

        let port: u64 = rand::random::<u64>() | 1;
        let addr: Multiaddr = format!("/memory/{port}").parse().unwrap();
        b.listen_on(addr.clone()).unwrap();
        let mut net = Net {
            b,
            c,
            s,
            u,
            b_pending: HashMap::new(),
            b_received: Vec::new(),
            c_received: Vec::new(),
        };
        // 待受が始まってからつなぐ
        net.run(Duration::from_millis(200)).await;
        for dialer in [&mut net.c, &mut net.s, &mut net.u] {
            dialer.dial(addr.clone()).unwrap();
        }
        // 購読の交換とメッシュの形成（heartbeat が 1秒）
        net.run(Duration::from_secs(4)).await;
        net
    }

    fn peer(swarm: &Swarm<gossipsub::Behaviour>) -> PeerId {
        *swarm.local_peer_id()
    }

    /// 4つの Swarm を duration だけ動かし、届いたメッセージを覚える。
    /// C は届いたものを受け入れる（C の先にはだれもいない）。B は検証待ちのまま覚え、結果は試験が返す
    async fn run(&mut self, duration: Duration) {
        let deadline = Instant::now() + duration;
        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            if remaining.is_zero() {
                return;
            }
            tokio::select! {
                event = self.b.select_next_some() => {
                    if let SwarmEvent::Behaviour(gossipsub::Event::Message { propagation_source, message_id, message }) = event {
                        self.b_received.push(message.data.clone());
                        self.b_pending.insert(message.data, (message_id, propagation_source));
                    }
                }
                event = self.c.select_next_some() => {
                    if let SwarmEvent::Behaviour(gossipsub::Event::Message { propagation_source, message_id, message }) = event {
                        self.c_received.push(message.data.clone());
                        let _ = self.c.behaviour_mut().report_message_validation_result(
                            &message_id, &propagation_source, gossipsub::MessageAcceptance::Accept);
                    }
                }
                _ = self.s.select_next_some() => {}
                _ = self.u.select_next_some() => {}
                _ = tokio::time::sleep(remaining) => return,
            }
        }
    }

    /// B のアプリとしての検証の結果を返す
    fn report(&mut self, data: &[u8], acceptance: gossipsub::MessageAcceptance) {
        let (id, source) = self.b_pending.remove(data).expect("B did not receive the message");
        let _ = self
            .b
            .behaviour_mut()
            .report_message_validation_result(&id, &source, acceptance);
    }

    fn publish(swarm: &mut Swarm<gossipsub::Behaviour>, data: &[u8]) {
        swarm
            .behaviour_mut()
            .publish(gossipsub::IdentTopic::new(TOPIC), data.to_vec())
            .unwrap();
    }

    fn score_of(&self, peer: &PeerId) -> f64 {
        self.b.behaviour().peer_score(peer).unwrap_or(0.0)
    }
}

#[tokio::test(flavor = "multi_thread", worker_threads = 2)]
async fn lobby_gossip_rejects_unverified_and_scores_peers() {
    let mut net = Net::new().await;
    let s_peer = Net::peer(&net.s);
    let u_peer = Net::peer(&net.u);
    // つながって、どちらも減点されていない
    assert!(net.b.behaviour().all_peers().count() >= 3, "peers did not connect");
    assert!(net.score_of(&s_peer) >= 0.0);
    assert!(net.score_of(&u_peer) >= 0.0);

    // 1. 署名のないメッセージは B のアプリに届かず、C に転送されず、送ったピアは減点される
    Net::publish(&mut net.u, b"unsigned offer");
    net.run(Duration::from_secs(3)).await;
    assert!(
        !net.b_received.iter().any(|d| d == b"unsigned offer"),
        "unsigned message reached the application"
    );
    assert!(
        !net.c_received.iter().any(|d| d == b"unsigned offer"),
        "unsigned message was forwarded"
    );
    assert!(
        net.score_of(&u_peer) < 0.0,
        "unsigned message did not lower the score: {}",
        net.score_of(&u_peer)
    );

    // 2. アプリが検証するまで転送しない。Accept を返した後に初めて C に届く
    Net::publish(&mut net.s, b"valid offer");
    net.run(Duration::from_secs(3)).await;
    assert!(
        net.b_received.iter().any(|d| d == b"valid offer"),
        "B did not receive the signed message"
    );
    assert!(
        !net.c_received.iter().any(|d| d == b"valid offer"),
        "message was forwarded before validation"
    );
    net.report(b"valid offer", gossipsub::MessageAcceptance::Accept);
    net.run(Duration::from_secs(3)).await;
    assert!(
        net.c_received.iter().any(|d| d == b"valid offer"),
        "accepted message was not forwarded"
    );

    // 3. 検証で拒否したメッセージは転送せず、送ったピアを減点する
    let before = net.score_of(&s_peer);
    Net::publish(&mut net.s, b"invalid offer 1");
    net.run(Duration::from_secs(2)).await;
    net.report(b"invalid offer 1", gossipsub::MessageAcceptance::Reject);
    net.run(Duration::from_secs(2)).await;
    assert!(
        !net.c_received.iter().any(|d| d == b"invalid offer 1"),
        "rejected message was forwarded"
    );
    let after_one = net.score_of(&s_peer);
    assert!(
        after_one < before && after_one < 0.0,
        "rejection did not lower the score: {before} -> {after_one}"
    );
    assert!(
        !swarm::below_graylist(net.b.behaviour(), &s_peer),
        "one invalid message should not graylist"
    );

    // 2件目で graylist を下回り、その後の S のメッセージは B のアプリに届かない
    Net::publish(&mut net.s, b"invalid offer 2");
    net.run(Duration::from_secs(2)).await;
    net.report(b"invalid offer 2", gossipsub::MessageAcceptance::Reject);
    net.run(Duration::from_secs(2)).await;
    assert!(
        swarm::below_graylist(net.b.behaviour(), &s_peer),
        "two invalid messages should graylist: {}",
        net.score_of(&s_peer)
    );
    Net::publish(&mut net.s, b"after graylist");
    net.run(Duration::from_secs(3)).await;
    assert!(
        !net.b_received.iter().any(|d| d == b"after graylist"),
        "graylisted peer's message was received"
    );
    assert!(!net.c_received.iter().any(|d| d == b"after graylist"));

    // C は正しく振る舞ったので減点されていない
    let c_peer = Net::peer(&net.c);
    assert!(
        net.score_of(&c_peer) >= 0.0,
        "honest peer was penalized: {}",
        net.score_of(&c_peer)
    );
}

#[test]
fn lobby_topic_score_params_are_valid() {
    assert!(swarm::lobby_topic_score_params().validate().is_ok());
    // 1件の不正で graylist を下回らず、2件で下回る
    let params = swarm::lobby_topic_score_params();
    let graylist = swarm::peer_score_thresholds().graylist_threshold;
    let cap = 10.0;
    assert!(params.topic_weight * params.invalid_message_deliveries_weight + cap > graylist);
    assert!(params.topic_weight * params.invalid_message_deliveries_weight * 4.0 + cap < graylist);
}
