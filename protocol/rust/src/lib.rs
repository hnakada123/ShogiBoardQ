//! ShogiBoardQ の P2P対局で、通信プロセス（Rust）と対局・UI側（C++）が同じ結果を出すべき取り決め
//! （protocol/README.md）の Rust 実装。
//!
//! - [`cbor`] — 決定的CBOR（README 1章）
//! - [`hash`] — H（SHA-256）と16進
//! - [`network`] — networkId（README 1.1節）
//! - [`frame`] — 対局フレームの外枠と INVITE_TOKEN（README 2章）
//! - [`invite`] — 招待文字列（README 3章）
//! - [`code`] — 合言葉（同じ LAN の招待の短い形。README 3.1節）
//! - [`meet`] — 協力ノードを使う合言葉と、協力ノードの待ち合わせ（README 3.2節・2.2節）
//! - [`peerid`] — PeerId のバイナリ表現（Ed25519 の identity multihash）
//!
//! 表示名の判定と対局の規則（Rules）は対局・UI側だけが扱う（通信プロセスは使わない）。

pub mod cbor;
pub mod code;
pub mod frame;
pub mod hash;
pub mod invite;
pub mod meet;
pub mod network;
pub mod peerid;
