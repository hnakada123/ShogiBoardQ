//! ShogiBoardQ の P2P対局で、通信プロセス（Rust）と対局・UI側（C++）が同じ結果を出すべき取り決め
//! （protocol/README.md）の Rust 実装。
//!
//! - [`cbor`] — 決定的CBOR（README 1章）
//! - [`hash`] — H（SHA-256）と16進
//! - [`network`] — networkId・lobbyKey・topic・v1 の rulesHash（README 1.1節）
//! - [`frame`] — 対局フレームの外枠と INVITE_TOKEN（README 2章）
//! - [`invite`] — 招待文字列（README 3章）
//! - [`offer`] — 募集と一覧取得（README 4章・5章）
//! - [`displayname`] — 表示名の判定（displayname-v1.txt）

pub mod cbor;
pub mod displayname;
pub mod frame;
pub mod hash;
pub mod invite;
pub mod network;
pub mod offer;
pub mod peerid;
