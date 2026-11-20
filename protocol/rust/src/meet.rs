//! 協力ノードを使う合言葉と、協力ノードの待ち合わせ（protocol/README.md 3.2節・2.2節）
//!
//! 招待する側は、招待の本体を合言葉から作った鍵で暗号化し、合言葉から作った番号（slot）で協力ノードに預ける。
//! 招待された側は、同じ合言葉から同じ番号と鍵を作り、協力ノードから引き取って復号する。合言葉には接続先が
//! 入らない。協力ノードには、番号の意味も預かったものの中身も分からない。

use chacha20poly1305::aead::{Aead, KeyInit, Payload};
use chacha20poly1305::{Key, XChaCha20Poly1305, XNonce};

use crate::cbor::{decode, encode, labeled, Value};
use crate::code::{labeled_check_char, read_symbols, symbols_text, ALPHABET, KDF_MEMORY_KIB, KDF_PASSES};
use crate::hash::h;

/// 待ち合わせのプロトコル（協力ノードが受け付けるストリーム）
pub const MEET_PROTOCOL: &str = "/shogiboardq/meet/1.0.0";
/// 合言葉の文字数（区切りを除く。秘密の9文字と検査の1文字）
pub const CODE_CHARS: usize = 10;
/// 秘密のビット数
pub const SECRET_BITS: u32 = 45;
const SECRET_MASK: u64 = (1 << SECRET_BITS) - 1;
const CHECK_LABEL: &str = "SBQ-MEET-CHECK";
/// 預けるもの（nonce と暗号文）の上限（バイト）
pub const MAX_BLOB_BYTES: usize = 1024;
/// 預けておく時間の上限（秒）
pub const MAX_TTL_SECS: u64 = 600;
/// 要求・応答1つの上限（バイト。長さの4バイトを除く）
pub const MAX_MESSAGE_BYTES: usize = 1200;
/// nonce の長さ（XChaCha20-Poly1305）
pub const NONCE_BYTES: usize = 24;
/// 認証の値の長さ（Poly1305）
pub const TAG_BYTES: usize = 16;

/// 協力ノードを使う合言葉
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MeetCode {
    /// 秘密（45 ビット）
    pub secret: u64,
}

/// 合言葉から作る番号と鍵
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MeetKeys {
    /// Argon2id の出力（番号と鍵のもと）
    pub master: [u8; 32],
    /// 協力ノードに預ける番号
    pub slot: [u8; 32],
    /// 預けるものを暗号化する鍵
    pub key: [u8; 32],
}

impl MeetCode {
    /// 乱数から合言葉を作る（乱数は秘密のビット数に切り詰める）
    pub fn new(random: u64) -> MeetCode {
        MeetCode {
            secret: random & SECRET_MASK,
        }
    }

    /// 文字列にする（5文字ごとに "-" で区切る）
    pub fn encode(&self) -> Option<String> {
        if self.secret > SECRET_MASK {
            return None;
        }
        let mut body = String::new();
        for i in (0..SECRET_BITS / 5).rev() {
            let shift = i.checked_mul(5)?;
            let index = usize::try_from(self.secret.checked_shr(shift)? & 31).ok()?;
            body.push(char::from(*ALPHABET.get(index)?));
        }
        let check = labeled_check_char(CHECK_LABEL, &body)?;
        body.push(char::from(check));
        let mut out = String::new();
        for (i, c) in body.chars().enumerate() {
            if i > 0 && i % 5 == 0 {
                out.push('-');
            }
            out.push(c);
        }
        Some(out)
    }

    /// 文字列を読む（文字の読み方は同じ LAN の合言葉と同じ）。10文字でない・検査の文字が合わなければ None
    pub fn decode(input: &str) -> Option<MeetCode> {
        let symbols = read_symbols(input)?;
        if symbols.len() != CODE_CHARS {
            return None;
        }
        let (check, body_symbols) = symbols.split_last()?;
        let body = symbols_text(body_symbols)?;
        if labeled_check_char(CHECK_LABEL, &body)? != *ALPHABET.get(usize::from(*check))? {
            return None;
        }
        let mut secret = 0u64;
        for s in body_symbols {
            secret = secret.checked_shl(5)? | u64::from(*s);
        }
        Some(MeetCode { secret })
    }

    /// 鍵の導出のソルト（16バイト。ネットワークで決まる）
    pub fn salt(network_id: &[u8; 32]) -> Option<[u8; 16]> {
        let digest = h(&Value::Array(vec![
            Value::text("SBQ-MEET-SALT"),
            Value::Uint(1),
            Value::bytes(network_id),
        ]));
        <[u8; 16]>::try_from(digest.get(..16)?).ok()
    }

    /// 合言葉から番号と鍵を作る（Argon2id。パスワードは秘密の8バイトのビッグエンディアン。時間とメモリを使うので、
    /// 主ループの外で呼ぶ）。協力ノードに見える番号も、時間のかかる導出の後の値から作る（番号から合言葉を
    /// 総当たりするのにも、1回ごとに同じ時間がかかる）
    pub fn derive(&self, network_id: &[u8; 32]) -> Option<MeetKeys> {
        let salt = MeetCode::salt(network_id)?;
        let params = argon2::Params::new(KDF_MEMORY_KIB, KDF_PASSES, 1, Some(32)).ok()?;
        let kdf = argon2::Argon2::new(argon2::Algorithm::Argon2id, argon2::Version::V0x13, params);
        let mut master = [0u8; 32];
        kdf.hash_password_into(&self.secret.to_be_bytes(), &salt, &mut master)
            .ok()?;
        Some(keys_from(&master))
    }
}

/// Argon2id の出力から、番号と鍵を作る
pub fn keys_from(master: &[u8; 32]) -> MeetKeys {
    let derive = |label: &str| {
        h(&Value::Array(vec![
            Value::text(label),
            Value::Uint(1),
            Value::bytes(master),
        ]))
    };
    MeetKeys {
        master: *master,
        slot: derive("SBQ-MEET-SLOT"),
        key: derive("SBQ-MEET-KEY"),
    }
}

/// 暗号化で一緒に認証する値（ネットワークで決まる。別のネットワークの預かりものを読まない）
fn associated_data(network_id: &[u8; 32]) -> Vec<u8> {
    encode(&Value::Array(vec![
        Value::text("SBQ-MEET-BLOB"),
        Value::Uint(1),
        Value::bytes(network_id),
    ]))
}

/// 預けるものを作る: nonce（24バイト）と、XChaCha20-Poly1305 の暗号文（認証の値を含む）を並べる。
/// nonce は預けるたびに乱数で作る。上限を超えれば None
pub fn seal(key: &[u8; 32], network_id: &[u8; 32], nonce: &[u8; NONCE_BYTES], plaintext: &[u8]) -> Option<Vec<u8>> {
    let cipher = XChaCha20Poly1305::new(&Key::from(*key));
    let aad = associated_data(network_id);
    let sealed = cipher
        .encrypt(
            &XNonce::from(*nonce),
            Payload {
                msg: plaintext,
                aad: &aad,
            },
        )
        .ok()?;
    let mut blob = nonce.to_vec();
    blob.extend_from_slice(&sealed);
    if blob.len() > MAX_BLOB_BYTES {
        return None;
    }
    Some(blob)
}

/// 預かったものを読む。鍵が違う・書き換えられている・形が合わなければ None（中身を読む前に捨てる）
pub fn open(key: &[u8; 32], network_id: &[u8; 32], blob: &[u8]) -> Option<Vec<u8>> {
    if blob.len() > MAX_BLOB_BYTES {
        return None;
    }
    let nonce = <[u8; NONCE_BYTES]>::try_from(blob.get(..NONCE_BYTES)?).ok()?;
    let sealed = blob.get(NONCE_BYTES..)?;
    if sealed.len() < TAG_BYTES {
        return None;
    }
    let cipher = XChaCha20Poly1305::new(&Key::from(*key));
    let aad = associated_data(network_id);
    cipher
        .decrypt(&XNonce::from(nonce), Payload { msg: sealed, aad: &aad })
        .ok()
}

/// 協力ノードへの要求
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Request {
    /// 預ける。同じ番号に預けたものがあれば、預けたピアだけが置き換えられる
    Put {
        slot: [u8; 32],
        blob: Vec<u8>,
        /// 預けておく秒数（1〜600）
        ttl_secs: u64,
        /// 1回引き取られたら消す
        one_time: bool,
    },
    /// 引き取る
    Get { slot: [u8; 32] },
    /// 消す（預けたピアだけ）
    Delete { slot: [u8; 32] },
}

const OP_PUT: u64 = 1;
const OP_GET: u64 = 2;
const OP_DELETE: u64 = 3;

impl Request {
    /// 決定的CBOR にする: `["SBQ-MEET-REQ", 1, 操作, 番号, 預けるもの, 秒数, 1回限りか]`（預ける以外は、
    /// 預けるものが空・秒数と1回限りかが 0）
    pub fn encode(&self) -> Vec<u8> {
        let (op, slot, blob, ttl, one_time) = match self {
            Request::Put {
                slot,
                blob,
                ttl_secs,
                one_time,
            } => (OP_PUT, slot, blob.as_slice(), *ttl_secs, u64::from(*one_time)),
            Request::Get { slot } => (OP_GET, slot, &[][..], 0, 0),
            Request::Delete { slot } => (OP_DELETE, slot, &[][..], 0, 0),
        };
        encode(&Value::Array(vec![
            Value::text("SBQ-MEET-REQ"),
            Value::Uint(1),
            Value::Uint(op),
            Value::bytes(slot),
            Value::bytes(blob),
            Value::Uint(ttl),
            Value::Uint(one_time),
        ]))
    }

    /// 読む。形・値の範囲が合わなければ None
    pub fn decode(bytes: &[u8]) -> Option<Request> {
        if bytes.len() > MAX_MESSAGE_BYTES {
            return None;
        }
        let value = decode(bytes).ok()?;
        let items = labeled(&value, "SBQ-MEET-REQ", 1, 7)?;
        let op = items.first()?.as_uint()?;
        let slot = items.get(1)?.as_fixed_bytes::<32>()?;
        let blob = items.get(2)?.as_bytes()?;
        let ttl_secs = items.get(3)?.as_uint()?;
        let one_time = items.get(4)?.as_uint()?;
        match op {
            OP_PUT => {
                if blob.len() < NONCE_BYTES.checked_add(TAG_BYTES)? || blob.len() > MAX_BLOB_BYTES {
                    return None;
                }
                if ttl_secs == 0 || ttl_secs > MAX_TTL_SECS || one_time > 1 {
                    return None;
                }
                Some(Request::Put {
                    slot,
                    blob: blob.to_vec(),
                    ttl_secs,
                    one_time: one_time == 1,
                })
            }
            OP_GET | OP_DELETE => {
                if !blob.is_empty() || ttl_secs != 0 || one_time != 0 {
                    return None;
                }
                Some(if op == OP_GET {
                    Request::Get { slot }
                } else {
                    Request::Delete { slot }
                })
            }
            _ => None,
        }
    }
}

/// 協力ノードの応答の結果
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Status {
    /// できた（引き取りでは、預かったものを付ける）
    Ok,
    /// 引き取るものが無い（預けられていない・期限が過ぎた・もう引き取られた）
    None,
    /// 上限（件数・頻度）を超えた
    Limit,
    /// 別のピアが預けた番号なので、置き換え・消去ができない
    Refused,
    /// 要求を読めない
    Invalid,
}

impl Status {
    fn number(self) -> u64 {
        match self {
            Status::Ok => 0,
            Status::None => 1,
            Status::Limit => 2,
            Status::Refused => 3,
            Status::Invalid => 4,
        }
    }

    fn from_number(n: u64) -> Option<Status> {
        match n {
            0 => Some(Status::Ok),
            1 => Some(Status::None),
            2 => Some(Status::Limit),
            3 => Some(Status::Refused),
            4 => Some(Status::Invalid),
            _ => None,
        }
    }
}

/// 協力ノードの応答
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Response {
    pub status: Status,
    /// 引き取ったもの（引き取りができたときだけ。ほかは空）
    pub blob: Vec<u8>,
}

impl Response {
    pub fn status(status: Status) -> Response {
        Response {
            status,
            blob: Vec::new(),
        }
    }

    /// 決定的CBOR にする: `["SBQ-MEET-RES", 1, 結果, 預かったもの]`
    pub fn encode(&self) -> Vec<u8> {
        encode(&Value::Array(vec![
            Value::text("SBQ-MEET-RES"),
            Value::Uint(1),
            Value::Uint(self.status.number()),
            Value::bytes(&self.blob),
        ]))
    }

    /// 読む。形・値の範囲が合わなければ None
    pub fn decode(bytes: &[u8]) -> Option<Response> {
        if bytes.len() > MAX_MESSAGE_BYTES {
            return None;
        }
        let value = decode(bytes).ok()?;
        let items = labeled(&value, "SBQ-MEET-RES", 1, 4)?;
        let status = Status::from_number(items.first()?.as_uint()?)?;
        let blob = items.get(1)?.as_bytes()?;
        if blob.len() > MAX_BLOB_BYTES || (status != Status::Ok && !blob.is_empty()) {
            return None;
        }
        Some(Response {
            status,
            blob: blob.to_vec(),
        })
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::indexing_slicing, clippy::arithmetic_side_effects)]
mod tests {
    use super::*;
    use crate::code::Code;
    use crate::network;

    #[test]
    fn code_is_ten_characters_in_two_groups() {
        let code = MeetCode::new(u64::MAX);
        assert_eq!(code.secret, SECRET_MASK);
        let text = code.encode().unwrap();
        assert_eq!(text.len(), 11);
        assert_eq!(text.find('-'), Some(5));
        assert_eq!(MeetCode::decode(&text).unwrap(), code);
        let zero = MeetCode::new(0).encode().unwrap();
        assert!(zero.starts_with("00000-0000"));
        assert_eq!(MeetCode::decode(&zero).unwrap().secret, 0);
    }

    #[test]
    fn reading_ignores_case_separators_and_look_alikes() {
        let code = MeetCode::new(0x0123_4567_89ab);
        let text = code.encode().unwrap();
        assert_eq!(MeetCode::decode(&text.to_lowercase()).unwrap(), code);
        assert_eq!(MeetCode::decode(&text.replace('-', "")).unwrap(), code);
        assert_eq!(
            MeetCode::decode(&format!(" {} ", text.replace('-', " "))).unwrap(),
            code
        );
        // O は 0、I と L は 1
        let one = MeetCode::new(0b00001_00000).encode().unwrap();
        assert!(one.starts_with("00000-0010"), "{one}");
        assert_eq!(
            MeetCode::decode(&one.replace('1', "I").replace('0', "O"))
                .unwrap()
                .secret,
            32
        );
        assert_eq!(MeetCode::decode(&one.replace('1', "l")).unwrap().secret, 32);
    }

    #[test]
    fn typos_and_other_lengths_are_rejected() {
        let text = MeetCode::new(0x1234_5678_9abc).encode().unwrap();
        // 1文字違うと、検査の文字が合わない（32 通りに 1 つは通るので、この値で確かめる）
        let mut wrong = text.clone().into_bytes();
        wrong[0] = if wrong[0] == b'0' { b'1' } else { b'0' };
        assert!(MeetCode::decode(std::str::from_utf8(&wrong).unwrap()).is_none());
        assert!(MeetCode::decode(&text[..text.len() - 1]).is_none());
        assert!(MeetCode::decode(&format!("{text}0")).is_none());
        assert!(MeetCode::decode("").is_none());
        assert!(MeetCode::decode("UUUUU-UUUUU").is_none());
        assert!(MeetCode::decode(&"0".repeat(200)).is_none());
    }

    /// 同じ LAN の合言葉と、協力ノードを使う合言葉は、互いに読めない（長さと検査の文字の作り方が違う）
    #[test]
    fn lan_codes_and_meet_codes_do_not_read_as_each_other() {
        let lan = Code::new(vec!["192.168.1.23".parse().unwrap()], None, 123).unwrap();
        assert!(MeetCode::decode(&lan.encode().unwrap()).is_none());
        // 同じ LAN の合言葉に 10 文字のものは無い（127.0.0.1 だけの 9 文字の次は 12 文字）
        let loopback = Code::new(vec!["127.0.0.1".parse().unwrap()], None, 5).unwrap();
        assert_eq!(loopback.encode().unwrap().replace('-', "").len(), 9);
        for secret in [0u64, 1, 0x1fff_ffff_ffff, 0x0123_4567_89ab] {
            let text = MeetCode::new(secret).encode().unwrap();
            assert!(Code::decode(&text).is_none(), "{text}");
        }
    }

    #[test]
    fn keys_depend_on_the_code_and_the_network() {
        let a = network::network_id("sbq-local-1");
        let b = network::network_id("sbq-limited-1");
        let code = MeetCode::new(42);
        let keys = code.derive(&a).unwrap();
        assert_eq!(keys, code.derive(&a).unwrap());
        assert_ne!(keys.slot, keys.key);
        assert_ne!(keys.slot, keys.master);
        assert_ne!(keys.slot, code.derive(&b).unwrap().slot);
        assert_ne!(keys.slot, MeetCode::new(43).derive(&a).unwrap().slot);
        assert_eq!(keys, keys_from(&keys.master));
    }

    #[test]
    fn sealed_blob_opens_only_with_the_same_key_and_network() {
        let net = network::network_id("sbq-local-1");
        let key = [7u8; 32];
        let nonce = [9u8; NONCE_BYTES];
        let blob = seal(&key, &net, &nonce, b"invite body").unwrap();
        assert_eq!(blob.len(), NONCE_BYTES + 11 + TAG_BYTES);
        assert_eq!(&blob[..NONCE_BYTES], &nonce);
        assert_eq!(open(&key, &net, &blob).unwrap(), b"invite body");
        // 鍵・ネットワークが違う、1ビットでも書き換えた、短すぎるものは読まない
        assert!(open(&[8u8; 32], &net, &blob).is_none());
        assert!(open(&key, &network::network_id("sbq-limited-1"), &blob).is_none());
        for i in [0, NONCE_BYTES, blob.len() - 1] {
            let mut changed = blob.clone();
            changed[i] ^= 1;
            assert!(open(&key, &net, &changed).is_none(), "byte {i}");
        }
        assert!(open(&key, &net, &blob[..NONCE_BYTES + TAG_BYTES - 1]).is_none());
        assert!(open(&key, &net, &[]).is_none());
        // 上限を超える平文は預けられない
        let large = vec![0u8; MAX_BLOB_BYTES - NONCE_BYTES - TAG_BYTES];
        assert_eq!(seal(&key, &net, &nonce, &large).unwrap().len(), MAX_BLOB_BYTES);
        assert!(seal(&key, &net, &nonce, &[large, vec![0]].concat()).is_none());
    }

    #[test]
    fn requests_roundtrip_and_reject_values_out_of_range() {
        let slot = [3u8; 32];
        let blob = vec![5u8; NONCE_BYTES + TAG_BYTES + 10];
        let put = Request::Put {
            slot,
            blob: blob.clone(),
            ttl_secs: 600,
            one_time: true,
        };
        for request in [put.clone(), Request::Get { slot }, Request::Delete { slot }] {
            assert_eq!(Request::decode(&request.encode()).unwrap(), request);
        }
        let bad = |request: Request| assert!(Request::decode(&request.encode()).is_none(), "{request:?}");
        bad(Request::Put {
            slot,
            blob: blob.clone(),
            ttl_secs: 0,
            one_time: false,
        });
        bad(Request::Put {
            slot,
            blob: blob.clone(),
            ttl_secs: 601,
            one_time: false,
        });
        bad(Request::Put {
            slot,
            blob: vec![0u8; NONCE_BYTES + TAG_BYTES - 1],
            ttl_secs: 60,
            one_time: false,
        });
        bad(Request::Put {
            slot,
            blob: vec![0u8; MAX_BLOB_BYTES + 1],
            ttl_secs: 60,
            one_time: false,
        });
        // 形の違うもの
        assert!(Request::decode(&[]).is_none());
        assert!(Request::decode(&Response::status(Status::Ok).encode()).is_none());
        let unknown_op = encode(&Value::Array(vec![
            Value::text("SBQ-MEET-REQ"),
            Value::Uint(1),
            Value::Uint(4),
            Value::bytes(&slot),
            Value::bytes(&[]),
            Value::Uint(0),
            Value::Uint(0),
        ]));
        assert!(Request::decode(&unknown_op).is_none());
        let get_with_blob = encode(&Value::Array(vec![
            Value::text("SBQ-MEET-REQ"),
            Value::Uint(1),
            Value::Uint(2),
            Value::bytes(&slot),
            Value::bytes(&blob),
            Value::Uint(0),
            Value::Uint(0),
        ]));
        assert!(Request::decode(&get_with_blob).is_none());
        // 最も大きい要求も、1つの上限に収まる
        let largest = Request::Put {
            slot,
            blob: vec![0u8; MAX_BLOB_BYTES],
            ttl_secs: 600,
            one_time: true,
        };
        assert!(largest.encode().len() <= MAX_MESSAGE_BYTES);
    }

    #[test]
    fn responses_roundtrip() {
        for status in [
            Status::Ok,
            Status::None,
            Status::Limit,
            Status::Refused,
            Status::Invalid,
        ] {
            let response = Response::status(status);
            assert_eq!(Response::decode(&response.encode()).unwrap(), response);
        }
        let with_blob = Response {
            status: Status::Ok,
            blob: vec![1u8; MAX_BLOB_BYTES],
        };
        assert!(with_blob.encode().len() <= MAX_MESSAGE_BYTES);
        assert_eq!(Response::decode(&with_blob.encode()).unwrap(), with_blob);
        // 引き取れなかった応答に、預かったものは付かない
        let odd = Response {
            status: Status::None,
            blob: vec![1],
        };
        assert!(Response::decode(&odd.encode()).is_none());
        assert!(Response::decode(&[]).is_none());
    }
}
