//! protocol/testvectors/ の入力で、C++ 側と同じ結果になることを確かめる

#![allow(clippy::unwrap_used, clippy::expect_used, clippy::panic, clippy::indexing_slicing)]

use sbq_protocol::{cbor, frame, hash, network};

fn read_vectors(name: &str) -> Vec<(String, String)> {
    let path = format!("{}/../testvectors/{}", env!("CARGO_MANIFEST_DIR"), name);
    let text = std::fs::read_to_string(&path).unwrap_or_else(|e| panic!("{path}: {e}"));
    text.lines()
        .map(str::trim)
        .filter(|l| !l.is_empty() && !l.starts_with('#'))
        .map(|l| {
            let (kind, value) = l.split_once(' ').unwrap_or((l, ""));
            (kind.to_owned(), value.trim().to_owned())
        })
        .collect()
}

#[test]
fn cbor_vectors() {
    let vectors = read_vectors("cbor.txt");
    assert!(vectors.len() > 40);
    for (kind, hex) in vectors {
        let bytes = hash::from_hex(&hex).expect("hex");
        let result = cbor::decode(&bytes);
        match kind.as_str() {
            "valid" => {
                let value = result.unwrap_or_else(|e| panic!("{hex} should be valid: {e}"));
                assert_eq!(cbor::encode(&value), bytes, "{hex} re-encodes differently");
            }
            "invalid" => assert!(result.is_err(), "{hex} should be invalid"),
            other => panic!("unknown kind {other}"),
        }
    }
}

#[test]
fn frame_vectors() {
    let vectors = read_vectors("frames.txt");
    assert!(vectors.len() > 20);
    for (kind, hex) in vectors {
        let bytes = hash::from_hex(&hex).expect("hex");
        let result = frame::check(&bytes);
        match kind.as_str() {
            "valid" => assert!(result.is_ok(), "{hex} should be valid: {result:?}"),
            "invalid" => assert!(result.is_err(), "{hex} should be invalid"),
            other => panic!("unknown kind {other}"),
        }
    }
}

#[test]
fn network_vectors() {
    let path = format!("{}/../testvectors/network.txt", env!("CARGO_MANIFEST_DIR"));
    let text = std::fs::read_to_string(path).unwrap();
    let mut count = 0;
    for line in text
        .lines()
        .map(str::trim)
        .filter(|l| !l.is_empty() && !l.starts_with('#'))
    {
        let fields: Vec<&str> = line.split(' ').collect();
        // 3つ目の rulesHash は対局・UI側（C++）だけが確かめる
        assert_eq!(fields.len(), 3, "{line}");
        let name = fields[0];
        assert!(network::find(name).is_some(), "{name} is not in the allowed list");
        let nid = network::network_id(name);
        assert_eq!(hash::hex(&nid), fields[1]);
        count += 1;
    }
    assert_eq!(count, network::NETWORKS.len());
}
