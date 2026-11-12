//! PeerId のバイナリ表現（Ed25519 の identity multihash）

/// Ed25519 の PeerId の先頭（identity multihash 0x00、長さ 36、protobuf の KeyType=Ed25519 と Data の長さ 32）
pub const ED25519_PREFIX: [u8; 6] = [0x00, 0x24, 0x08, 0x01, 0x12, 0x20];
/// Ed25519 の PeerId の長さ
pub const ED25519_LEN: usize = 38;

/// Ed25519 の identity multihash の PeerId か
pub fn is_ed25519(peer_id: &[u8]) -> bool {
    peer_id.len() == ED25519_LEN && peer_id.starts_with(&ED25519_PREFIX)
}
