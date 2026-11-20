//! protocol/testvectors/ の入力で、C++ 側と同じ結果になることを確かめる

#![allow(clippy::unwrap_used, clippy::expect_used, clippy::panic, clippy::indexing_slicing)]

use sbq_protocol::code::{self, Code};
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

/// 合言葉（README 3.1節）: 文字列と中身の対応、読めない文字列、鍵の導出、トークン
#[test]
fn code_vectors() {
    let mut counts = [0usize; 4];
    for (kind, value) in read_vectors("code.txt") {
        let fields: Vec<&str> = value.split(' ').collect();
        match kind.as_str() {
            "code" => {
                assert_eq!(fields.len(), 4, "{value}");
                let port = match fields[1] {
                    "-" => None,
                    p => Some(p.parse::<u16>().unwrap()),
                };
                let addrs: Vec<std::net::Ipv4Addr> = fields[2].split(',').map(|a| a.parse().unwrap()).collect();
                let secret = u64::from_str_radix(fields[3], 16).unwrap();
                let decoded = Code::decode(fields[0]).unwrap_or_else(|| panic!("{} should be readable", fields[0]));
                assert_eq!(decoded, Code { addrs, port, secret }, "{value}");
                assert_eq!(decoded.encode().as_deref(), Some(fields[0]));
                counts[0] += 1;
            }
            "invalid" => {
                assert!(Code::decode(&value).is_none(), "{value} should be unreadable");
                counts[1] += 1;
            }
            "key" => {
                assert_eq!(fields.len(), 5, "{value}");
                let network_id = network::network_id(fields[0]);
                let decoded = Code::decode(fields[1]).unwrap();
                assert_eq!(hash::hex(&decoded.secret.to_be_bytes()), fields[2]);
                assert_eq!(hash::hex(&decoded.salt(&network_id).unwrap()), fields[3]);
                assert_eq!(hash::hex(&decoded.derive_key(&network_id).unwrap()), fields[4]);
                counts[2] += 1;
            }
            "token" => {
                assert_eq!(fields.len(), 4, "{value}");
                let key: [u8; 32] = hash::from_hex(fields[0]).unwrap().try_into().unwrap();
                let inviter = hash::from_hex(fields[1]).unwrap();
                let invitee = hash::from_hex(fields[2]).unwrap();
                assert_eq!(hash::hex(&code::token(&key, &inviter, &invitee)), fields[3]);
                counts[3] += 1;
            }
            other => panic!("unknown kind {other}"),
        }
    }
    assert!(counts.iter().all(|c| *c >= 2), "{counts:?}");
}
