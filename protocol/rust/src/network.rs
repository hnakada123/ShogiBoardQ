//! ネットワークの識別子（protocol/README.md 1.1節）

use crate::cbor::Value;
use crate::hash::h;

/// ネットワークの使い方
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Profile {
    /// ローカル（同じ PC・同じ LAN。loopback と私設アドレス。待受は IPv4 の全インターフェース）
    Local,
    /// インターネット（インターネット上のアドレスと中継）
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

/// ビルドの使用期限（ビルドした日からの日数。実装仕様 3.1節）。対局・UI側（C++）の日数も、CMake がこの行から読む
/// （行の形 `pub const BUILD_LIFETIME_DAYS: u64 = <数>;` を変えない）
pub const BUILD_LIFETIME_DAYS: u64 = 180;

/// Identify で示すプロトコルの版（README 1.1節。相手のこの値で接続を断らない）
pub const IDENTIFY_PROTOCOL: &str = "/shogiboardq/1.0.0";

/// ローカルのネットワーク（同じ PC・同じ LAN）の待受のポート（TCP と UDP。README 1.1節・ipc.md 5章）。起動のたびに
/// 変わると、招待する側の PC のファイアウォールで許可するポートを決められない。対局・UI側の案内の文も、CMake が
/// この行から読む（行の形 `pub const LAN_PORT: u16 = <数>;` を変えない）
pub const LAN_PORT: u16 = 48210;

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
