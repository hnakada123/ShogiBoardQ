//! ネットワークとロビーの識別子（protocol/README.md 1.1節）

use crate::cbor::{encode, Value};
use crate::hash::{h, hex};

/// ネットワークの使い方
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Profile {
    /// ローカル検証用（loopback と私設アドレス。待受は 127.0.0.1）
    Local,
    /// 限定試験用（インターネット上のアドレスと中継）
    Internet,
}

impl Profile {
    pub fn name(self) -> &'static str {
        match self {
            Profile::Local => "local",
            Profile::Internet => "internet",
        }
    }
}

/// このビルドが参加できるネットワーク
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct NetworkSpec {
    pub name: &'static str,
    pub profile: Profile,
    /// ビルドの使用期限の対象か
    pub expires_with_build: bool,
}

/// 参加できるネットワークの一覧（README 1.1節）
pub const NETWORKS: &[NetworkSpec] = &[
    NetworkSpec {
        name: "sbq-local-1",
        profile: Profile::Local,
        expires_with_build: false,
    },
    NetworkSpec {
        name: "sbq-limited-1",
        profile: Profile::Internet,
        expires_with_build: true,
    },
];

/// ビルドの使用期限（ビルドした日からの日数。実装仕様 3.1節）
pub const BUILD_LIFETIME_DAYS: u64 = 180;

/// 名前のネットワークを探す（一覧にない名前は None）
pub fn find(name: &str) -> Option<NetworkSpec> {
    NETWORKS.iter().copied().find(|n| n.name == name)
}

/// networkName として形が正しいか（ASCII の印字できる文字、1〜32バイト）
pub fn is_valid_name(name: &str) -> bool {
    !name.is_empty() && name.len() <= 32 && name.bytes().all(|b| (0x21..=0x7e).contains(&b))
}

/// networkId = H(CBOR(["SBQ-NETWORK", 1, networkName]))
pub fn network_id(name: &str) -> [u8; 32] {
    h(&Value::Array(vec![
        Value::text("SBQ-NETWORK"),
        Value::Uint(1),
        Value::text(name),
    ]))
}

/// lobbyKey = H(CBOR(["SBQ-LOBBY", 1, networkId, rulesHash]))
pub fn lobby_key(network_id: &[u8; 32], rules_hash: &[u8; 32]) -> [u8; 32] {
    h(&Value::Array(vec![
        Value::text("SBQ-LOBBY"),
        Value::Uint(1),
        Value::bytes(network_id),
        Value::bytes(rules_hash),
    ]))
}

/// GossipSub の topic
pub fn topic(network_id: &[u8; 32], rules_hash: &[u8; 32]) -> String {
    format!("/shogiboardq/lobby/1/{}/{}", hex(network_id), hex(rules_hash))
}

/// v1 の開始局面（平手）
pub const INITIAL_SFEN_V1: &str = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

/// v1 の Rules の値
pub fn rules_v1() -> Value {
    Value::Array(vec![
        Value::text("SBQ-RULES"),
        Value::Uint(1),
        Value::text(INITIAL_SFEN_V1),
        Value::Uint(512),
        Value::text("fourfold-draw"),
        Value::text("perpetual-check-loss"),
        Value::text("no-declaration"),
        Value::text("human"),
        Value::text("human"),
    ])
}

/// v1 の Rules の決定的CBOR
pub fn rules_v1_bytes() -> Vec<u8> {
    encode(&rules_v1())
}

/// v1 の rulesHash
pub fn rules_hash_v1() -> [u8; 32] {
    h(&rules_v1())
}
