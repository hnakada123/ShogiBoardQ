//! 作った招待（トークンと期限）。発行・照合・期限切れ・取り消し（protocol/README.md 2.1節・3.1節・ipc.md 3章）
//!
//! トークンと合言葉の鍵はこの中だけに置き、外には照合の結果（合言葉で使われたときは、相手に返す確認の値）だけを
//! 返す。

use std::collections::HashMap;

use sbq_protocol::code;

/// 同時に持つ招待の上限（超えたら古いものから無効にする）
pub const MAX_INVITES: usize = 4;

/// 合言葉のトークンの照合に続けて失敗してよい回数（1つの接続元）と、1回分が戻るまでの秒数。合言葉の秘密は短いので、
/// 総当たりの回数を絞る。LAN の機器が誤ったトークンを送り続けても、ほかの接続元の照合は止まらない（監査 2026-11-21 S4）
const SOURCE_GUESSES: GuessLimit = GuessLimit {
    burst: 8,
    gain: 1,
    period_secs: 4,
};
/// 合言葉のトークンの照合に続けて失敗してよい回数（すべての接続元を合わせて）と、1秒あたりに戻る回数。
/// 10 分の招待に、合わせて 32 + 2 × 600 = 1232 回まで試せる。秘密は 34 ビット以上なので、1つの招待が当てられる
/// 確率は 1232 / 2^34 ≒ 7.2 × 10^-8 以下（接続元をいくつ使っても変わらない）。この枠を空にし続けるには、
/// 接続元（アドレス）を 8 つ以上使って、4 秒に 1 回ずつ誤ったトークンを送り続ける必要がある
const TOTAL_GUESSES: GuessLimit = GuessLimit {
    burst: 32,
    gain: 2,
    period_secs: 1,
};
/// 失敗の回数を数える接続元の上限（超えたら、満杯まで戻った記録を消し、それでも多ければ1つにまとめて数える）
const MAX_GUESS_SOURCES: usize = 256;

/// 照合に失敗してよい回数の決まり（burst 回まで続けて、period_secs 秒ごとに gain 回分戻る）
#[derive(Clone, Copy)]
struct GuessLimit {
    burst: u64,
    gain: u64,
    period_secs: u64,
}

/// 照合に失敗してよい残りの回数（UNIX 秒で数える）
#[derive(Clone, Copy)]
struct Guesses {
    left: u64,
    /// 最後に戻した時刻（端数の秒は次に持ち越す）
    at: u64,
}

impl Guesses {
    fn full(limit: GuessLimit, now: u64) -> Guesses {
        Guesses {
            left: limit.burst,
            at: now,
        }
    }

    fn refill(&mut self, limit: GuessLimit, now: u64) {
        let periods = now.saturating_sub(self.at).checked_div(limit.period_secs).unwrap_or(0);
        if periods == 0 {
            return;
        }
        self.left = self
            .left
            .saturating_add(periods.saturating_mul(limit.gain))
            .min(limit.burst);
        self.at = if self.left >= limit.burst {
            now
        } else {
            self.at.saturating_add(periods.saturating_mul(limit.period_secs))
        };
    }

    fn is_full(&self, limit: GuessLimit) -> bool {
        self.left >= limit.burst
    }
}

struct Outstanding {
    /// 招待文字列のトークン
    token: [u8; 32],
    /// 同じ LAN の合言葉から作った鍵（その合言葉を出さなかった招待は None）
    code_key: Option<[u8; 32]>,
    /// 協力ノードに預けた番号（協力ノードを使う合言葉を出さなかった招待は None）
    meet_slot: Option<[u8; 32]>,
    expires_at: u64,
}

/// 照合できた招待
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Redeemed {
    /// 合言葉のトークンで使われたときに、相手へ返す確認の値（招待文字列のトークンなら None。README 3.1節）
    pub confirm: Option<[u8; 32]>,
}

/// 作った招待の一覧
pub struct Invites {
    list: Vec<Outstanding>,
    /// 合言葉のトークンの照合に、あと何回失敗してよいか（すべての接続元を合わせて）
    total_guesses: Guesses,
    /// 合言葉のトークンの照合に、あと何回失敗してよいか（接続元ごと。満杯の記録は消す）
    source_guesses: HashMap<Vec<u8>, Guesses>,
    /// 使われた・取り消された招待が協力ノードに預けていた番号（協力ノードから消す。take_released）
    released: Vec<[u8; 32]>,
}

impl Default for Invites {
    fn default() -> Self {
        Invites {
            list: Vec::new(),
            total_guesses: Guesses::full(TOTAL_GUESSES, 0),
            source_guesses: HashMap::new(),
            released: Vec::new(),
        }
    }
}

impl Invites {
    /// 招待を足す（上限を超えたら古いものから無効にする）。code_key は同じ LAN の合言葉の鍵、meet_slot は
    /// 協力ノードを使う合言葉で預けた番号（その合言葉を出した招待だけ）
    pub fn add(&mut self, token: [u8; 32], code_key: Option<[u8; 32]>, meet_slot: Option<[u8; 32]>, expires_at: u64) {
        while self.list.len() >= MAX_INVITES {
            self.remove_at(0);
        }
        self.list.push(Outstanding {
            token,
            code_key,
            meet_slot,
            expires_at,
        });
    }

    /// 招待を無効にする（協力ノードに預けた番号があれば、消すものに足す）
    fn remove_at(&mut self, index: usize) {
        if index >= self.list.len() {
            return;
        }
        let invite = self.list.remove(index);
        if let Some(slot) = invite.meet_slot {
            self.released.push(slot);
        }
    }

    /// 有効な招待が協力ノードに預けている番号（自分の合言葉を、協力ノードから引き取らないために見る）
    pub fn meet_slots(&self) -> Vec<[u8; 32]> {
        self.list.iter().filter_map(|i| i.meet_slot).collect()
    }

    /// 使われた・取り消された招待が協力ノードに預けていた番号を取り出す（期限で無効になった招待の分は、
    /// 協力ノードの側でも期限で消えるので、入らない）
    pub fn take_released(&mut self) -> Vec<[u8; 32]> {
        std::mem::take(&mut self.released)
    }

    /// 届いたトークンが期限内の招待と一致すれば、その招待を使い終えて返す（1つの招待で1回だけ接続を受ける）。
    /// 招待文字列のトークンか、合言葉の鍵と双方の PeerId から作ったトークン（own は自分、remote は送ってきた相手）
    /// のどちらでもよい。合言葉のトークンなら、相手へ返す確認の値も返す。source は送ってきた相手の接続元
    /// （SourceKey の strict。分からなければ空）で、合言葉の照合に失敗してよい回数を接続元ごとに数える。
    /// すべての招待と、処理時間が中身によらない比較で照合する
    pub fn redeem(&mut self, token: &[u8; 32], own: &[u8], remote: &[u8], source: &[u8], now: u64) -> Option<Redeemed> {
        let mut matched: Option<usize> = None;
        for (i, invite) in self.list.iter().enumerate() {
            if constant_time_eq(&invite.token, token) && invite.expires_at > now {
                matched = Some(i);
            }
        }
        let redeemed = match matched {
            Some(i) => (i, Redeemed { confirm: None }),
            None => self.redeem_code(token, own, remote, source, now)?,
        };
        self.remove_at(redeemed.0);
        Some(redeemed.1)
    }

    /// 合言葉のトークンとして照合する。その接続元か全体の、失敗してよい回数が残っていなければ、照合しない
    fn redeem_code(
        &mut self,
        token: &[u8; 32],
        own: &[u8],
        remote: &[u8],
        source: &[u8],
        now: u64,
    ) -> Option<(usize, Redeemed)> {
        if !self.list.iter().any(|i| i.code_key.is_some() && i.expires_at > now) {
            return None;
        }
        self.total_guesses.refill(TOTAL_GUESSES, now);
        let source_key = self.guess_source(source, now);
        let source_left = self
            .source_guesses
            .get(&source_key)
            .map_or(SOURCE_GUESSES.burst, |g| g.left);
        if self.total_guesses.left == 0 || source_left == 0 {
            return None;
        }
        let mut matched: Option<(usize, [u8; 32])> = None;
        for (i, invite) in self.list.iter().enumerate() {
            let Some(key) = &invite.code_key else { continue };
            let expected = code::token(key, own, remote);
            if constant_time_eq(&expected, token) && invite.expires_at > now {
                matched = Some((i, code::confirmation(key, own, remote)));
            }
        }
        match matched {
            Some((i, confirm)) => Some((i, Redeemed { confirm: Some(confirm) })),
            None => {
                self.total_guesses.left = self.total_guesses.left.saturating_sub(1);
                let entry = self
                    .source_guesses
                    .entry(source_key)
                    .or_insert_with(|| Guesses::full(SOURCE_GUESSES, now));
                entry.left = entry.left.saturating_sub(1);
                None
            }
        }
    }

    /// 失敗の回数を数える接続元（記録を戻し、満杯の記録を消してから決める。数えきれないほど多ければ、まとめて
    /// 1つ（空の値。接続元の分からない相手と同じ）にする）
    fn guess_source(&mut self, source: &[u8], now: u64) -> Vec<u8> {
        for guesses in self.source_guesses.values_mut() {
            guesses.refill(SOURCE_GUESSES, now);
        }
        self.source_guesses.retain(|_, g| !g.is_full(SOURCE_GUESSES));
        if self.source_guesses.contains_key(source) || self.source_guesses.len() < MAX_GUESS_SOURCES {
            source.to_vec()
        } else {
            Vec::new()
        }
    }

    /// 期限を過ぎた招待を消す
    pub fn expire(&mut self, now: u64) {
        self.list.retain(|i| i.expires_at > now);
    }

    /// すべての招待を無効にする
    pub fn clear(&mut self) {
        while !self.list.is_empty() {
            self.remove_at(0);
        }
    }
}

/// 長さが一致した値どうしで、処理時間が中身によらない比較
pub(super) fn constant_time_eq(a: &[u8; 32], b: &[u8; 32]) -> bool {
    let mut diff = 0u8;
    for (x, y) in a.iter().zip(b.iter()) {
        diff |= x ^ y;
    }
    diff == 0
}

#[cfg(test)]
#[allow(clippy::arithmetic_side_effects)]
mod tests {
    use super::*;

    const OWN: &[u8] = b"inviter";
    const FRIEND: &[u8] = b"friend";
    /// 相手の接続元（SourceKey の strict）
    const LAN_PEER: &[u8] = &[192, 168, 1, 20];
    const LAN_SPAMMER: &[u8] = &[192, 168, 1, 66];

    /// 照合できたか（確認の値は見ない）
    fn redeem(invites: &mut Invites, token: &[u8; 32], remote: &[u8], source: &[u8], now: u64) -> bool {
        invites.redeem(token, OWN, remote, source, now).is_some()
    }

    #[test]
    fn invite_is_redeemed_once_within_its_lifetime() {
        let mut invites = Invites::default();
        invites.add([1; 32], None, None, 100);
        // 別のトークン・期限を過ぎたトークンは受けない
        assert!(!redeem(&mut invites, &[2; 32], FRIEND, LAN_PEER, 50));
        assert!(!redeem(&mut invites, &[1; 32], FRIEND, LAN_PEER, 100));
        // 期限内なら1回だけ。招待文字列のトークンには、確認の値を返さない
        assert_eq!(
            invites.redeem(&[1; 32], OWN, FRIEND, LAN_PEER, 99),
            Some(Redeemed { confirm: None })
        );
        assert!(!redeem(&mut invites, &[1; 32], FRIEND, LAN_PEER, 99));
    }

    #[test]
    fn oldest_invite_is_dropped_over_the_limit() {
        let mut invites = Invites::default();
        for i in 0..=MAX_INVITES {
            invites.add([u8::try_from(i).unwrap_or(0); 32], None, None, 100);
        }
        // 最初の招待は無効になり、残りは使える
        assert!(!redeem(&mut invites, &[0; 32], FRIEND, LAN_PEER, 1));
        for i in 1..=MAX_INVITES {
            assert!(redeem(
                &mut invites,
                &[u8::try_from(i).unwrap_or(0); 32],
                FRIEND,
                LAN_PEER,
                1
            ));
        }
    }

    #[test]
    fn expired_and_cancelled_invites_are_removed() {
        let mut invites = Invites::default();
        invites.add([1; 32], None, None, 10);
        invites.add([2; 32], None, None, 20);
        invites.expire(15);
        assert!(!redeem(&mut invites, &[1; 32], FRIEND, LAN_PEER, 5));
        assert!(redeem(&mut invites, &[2; 32], FRIEND, LAN_PEER, 5));
        invites.add([3; 32], None, None, 30);
        invites.clear();
        assert!(!redeem(&mut invites, &[3; 32], FRIEND, LAN_PEER, 5));
    }

    /// 合言葉のトークンは、鍵と双方の PeerId で決まる。招待文字列のトークンとどちらか一方で、1回だけ使える。
    /// 合言葉で使われたときは、相手へ返す確認の値（鍵と双方の PeerId から作る）を返す
    #[test]
    fn code_token_is_bound_to_both_peers_and_shares_the_invite() {
        let key = [7u8; 32];
        let mut invites = Invites::default();
        invites.add([1; 32], Some(key), None, 1000);
        let good = code::token(&key, OWN, FRIEND);
        // 別の相手へ送ったトークン・向きを逆にしたトークンは使えない
        assert!(!redeem(
            &mut invites,
            &code::token(&key, b"other", FRIEND),
            FRIEND,
            LAN_PEER,
            100
        ));
        assert!(!redeem(
            &mut invites,
            &code::token(&key, FRIEND, OWN),
            FRIEND,
            LAN_PEER,
            100
        ));
        assert!(!redeem(&mut invites, &good, b"stranger", LAN_PEER, 100));
        assert_eq!(
            invites.redeem(&good, OWN, FRIEND, LAN_PEER, 100),
            Some(Redeemed {
                confirm: Some(code::confirmation(&key, OWN, FRIEND))
            })
        );
        // 合言葉で使った招待は、招待文字列のトークンでも使えない
        assert!(!redeem(&mut invites, &[1; 32], FRIEND, LAN_PEER, 100));
        assert!(!redeem(&mut invites, &good, FRIEND, LAN_PEER, 100));

        // 招待文字列のトークンで使った招待は、合言葉でも使えない
        invites.add([2; 32], Some(key), None, 1000);
        assert!(redeem(&mut invites, &[2; 32], FRIEND, LAN_PEER, 100));
        assert!(!redeem(&mut invites, &good, FRIEND, LAN_PEER, 100));
    }

    /// 合言葉のトークンの照合は、1つの接続元が続けて失敗できる回数を絞る（総当たり）。招待文字列のトークンは絞らない
    #[test]
    fn code_guesses_are_limited_per_source() {
        let key = [7u8; 32];
        let mut invites = Invites::default();
        invites.add([1; 32], Some(key), None, 100_000);
        let good = code::token(&key, OWN, FRIEND);
        let now = 1000;
        for _ in 0..SOURCE_GUESSES.burst {
            assert!(!redeem(&mut invites, &[9; 32], FRIEND, LAN_PEER, now));
        }
        // その接続元は、回数を使い切ると、正しいトークンも照合しない
        assert!(!redeem(&mut invites, &good, FRIEND, LAN_PEER, now));
        // 招待文字列のトークンは、回数によらず使える
        invites.add([2; 32], None, None, 100_000);
        assert!(redeem(&mut invites, &[2; 32], FRIEND, LAN_PEER, now));
        // 時間がたつと、1回分ずつ戻る
        let refill = SOURCE_GUESSES.period_secs;
        assert!(!redeem(&mut invites, &good, FRIEND, LAN_PEER, now + refill - 1));
        assert!(redeem(&mut invites, &good, FRIEND, LAN_PEER, now + refill));
    }

    /// LAN の機器が誤ったトークンを送り続けても、ほかの接続元からの正しいトークンは照合する（監査 2026-11-21 S4）
    #[test]
    fn a_spamming_source_does_not_block_the_right_code() {
        let key = [7u8; 32];
        let mut invites = Invites::default();
        invites.add([1; 32], Some(key), None, 100_000);
        let good = code::token(&key, OWN, FRIEND);
        let mut now = 1000;
        // 4 秒に 1 回ずつ、PeerId を替えながら誤ったトークンを送り続ける（1つの接続元の回数を空に保つ）
        for i in 0..200u32 {
            let spammer = i.to_be_bytes();
            assert!(!redeem(&mut invites, &[9; 32], &spammer, LAN_SPAMMER, now));
            now += 1;
        }
        assert_eq!(invites.source_guesses.get(LAN_SPAMMER).map(|g| g.left), Some(0));
        assert!(invites.total_guesses.left > 0);
        assert!(redeem(&mut invites, &good, FRIEND, LAN_PEER, now));
    }

    /// すべての接続元を合わせた回数でも絞る（接続元を替えても、10 分で試せる回数は決まっている）
    #[test]
    fn code_guesses_are_limited_in_total() {
        let key = [7u8; 32];
        let mut invites = Invites::default();
        invites.add([1; 32], Some(key), None, 100_000);
        let good = code::token(&key, OWN, FRIEND);
        let now = 1000;
        let mut failed: u64 = 0;
        for host in 0..=255u8 {
            if invites.total_guesses.left == 0 {
                break;
            }
            for _ in 0..SOURCE_GUESSES.burst {
                assert!(!redeem(&mut invites, &[9; 32], FRIEND, &[10, 0, 0, host], now));
                failed += 1;
                if invites.total_guesses.left == 0 {
                    break;
                }
            }
        }
        assert_eq!(failed, TOTAL_GUESSES.burst);
        // 全体の回数が残っていない間は、まだ失敗していない接続元の正しいトークンも照合しない
        assert!(!redeem(&mut invites, &good, FRIEND, LAN_PEER, now));
        // 1秒で gain 回分戻る
        assert!(redeem(&mut invites, &good, FRIEND, LAN_PEER, now + 1));
        // 10 分の招待に試せる回数の上限から、1つの招待が当てられる確率は 1e-7 未満（秘密は 34 ビット以上）
        let lifetime = sbq_protocol::invite::MAX_LIFETIME_SECS;
        let most = TOTAL_GUESSES.burst + TOTAL_GUESSES.gain * lifetime / TOTAL_GUESSES.period_secs;
        assert!((most as f64) / 2f64.powi(i32::try_from(code::SECRET_MIN_BITS).unwrap_or(34)) < 1e-7);
    }

    /// 使われた・取り消された・押し出された招待が協力ノードに預けていた番号は、消すものとして取り出せる。
    /// 期限で無効になった招待の分は入らない
    #[test]
    fn released_meet_slots_are_collected() {
        let mut invites = Invites::default();
        invites.add([1; 32], None, Some([11; 32]), 100);
        invites.add([2; 32], None, None, 100);
        invites.add([3; 32], None, Some([13; 32]), 100);
        assert!(invites.take_released().is_empty());
        assert_eq!(invites.meet_slots(), vec![[11; 32], [13; 32]]);
        // 使われた招待
        assert!(redeem(&mut invites, &[1; 32], FRIEND, LAN_PEER, 50));
        assert!(redeem(&mut invites, &[2; 32], FRIEND, LAN_PEER, 50));
        assert_eq!(invites.take_released(), vec![[11; 32]]);
        assert!(invites.take_released().is_empty());
        // 取り消された招待
        invites.clear();
        assert_eq!(invites.take_released(), vec![[13; 32]]);
        // 上限で押し出された招待
        for i in 0..=MAX_INVITES {
            let n = u8::try_from(i).unwrap_or(0);
            invites.add([n; 32], None, Some([n + 100; 32]), 100);
        }
        assert_eq!(invites.take_released(), vec![[100; 32]]);
        // 期限で無効になった招待
        invites.expire(100);
        assert!(invites.take_released().is_empty());
    }

    /// 合言葉を出した招待がなければ、合言葉の照合の回数を使わない
    #[test]
    fn wrong_tokens_do_not_use_guesses_without_a_code_invite() {
        let mut invites = Invites::default();
        invites.add([1; 32], None, None, 100_000);
        for _ in 0..(SOURCE_GUESSES.burst * 2) {
            assert!(!redeem(&mut invites, &[9; 32], FRIEND, LAN_PEER, 1000));
        }
        let key = [7u8; 32];
        invites.add([2; 32], Some(key), None, 100_000);
        assert!(redeem(
            &mut invites,
            &code::token(&key, OWN, FRIEND),
            FRIEND,
            LAN_PEER,
            1000
        ));
    }

    /// 失敗の記録は、満杯まで戻ったら消える。数えきれないほど多い接続元は、まとめて数える（全体の回数が先に尽きるので、
    /// ふつうは起きない）
    #[test]
    fn guess_records_are_bounded() {
        let mut invites = Invites::default();
        let now = 1000;
        for i in 0..MAX_GUESS_SOURCES {
            let host = u16::try_from(i).unwrap_or(0).to_be_bytes();
            invites
                .source_guesses
                .insert(vec![10, 0, host[0], host[1]], Guesses { left: 0, at: now });
        }
        // 数えている接続元はそのまま、新しい接続元はまとめて数える
        assert_eq!(invites.guess_source(&[10, 0, 0, 1], now), vec![10, 0, 0, 1]);
        assert_eq!(invites.guess_source(LAN_PEER, now), Vec::<u8>::new());
        // 満杯まで戻った記録は消える
        let later = now + SOURCE_GUESSES.period_secs * SOURCE_GUESSES.burst;
        assert_eq!(invites.guess_source(LAN_PEER, later), LAN_PEER.to_vec());
        assert!(invites.source_guesses.is_empty());
    }
}
