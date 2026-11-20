//! 作った招待（トークンと期限）。発行・照合・期限切れ・取り消し（protocol/README.md 2.1節・3.1節・ipc.md 3章）
//!
//! トークンと合言葉の鍵はこの中だけに置き、外には照合の結果だけを返す。

use sbq_protocol::code;

/// 同時に持つ招待の上限（超えたら古いものから無効にする）
pub const MAX_INVITES: usize = 4;
/// 合言葉のトークンの照合に失敗してよい回数（続けて）。合言葉の秘密は短いので、総当たりの回数を絞る
const CODE_GUESS_BURST: u32 = 8;
/// 失敗してよい回数が1回分戻るまでの秒数（10分の招待に、合わせて 150 回ほどしか試せない）
const CODE_GUESS_REFILL_SECS: u64 = 4;

struct Outstanding {
    /// 招待文字列のトークン
    token: [u8; 32],
    /// 同じ LAN の合言葉から作った鍵（その合言葉を出さなかった招待は None）
    code_key: Option<[u8; 32]>,
    /// 協力ノードに預けた番号（協力ノードを使う合言葉を出さなかった招待は None）
    meet_slot: Option<[u8; 32]>,
    expires_at: u64,
}

/// 作った招待の一覧
pub struct Invites {
    list: Vec<Outstanding>,
    /// 合言葉のトークンの照合に、あと何回失敗してよいか
    code_guesses: u32,
    /// 失敗してよい回数を最後に戻した時刻（UNIX 秒）
    code_refilled_at: u64,
    /// 使われた・取り消された招待が協力ノードに預けていた番号（協力ノードから消す。take_released）
    released: Vec<[u8; 32]>,
}

impl Default for Invites {
    fn default() -> Self {
        Invites {
            list: Vec::new(),
            code_guesses: CODE_GUESS_BURST,
            code_refilled_at: 0,
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

    /// 届いたトークンが期限内の招待と一致すれば、その招待を使い終えて true（1つの招待で1回だけ接続を受ける）。
    /// 招待文字列のトークンか、合言葉の鍵と双方の PeerId から作ったトークン（own は自分、remote は送ってきた相手）
    /// のどちらでもよい。すべての招待と、処理時間が中身によらない比較で照合する
    pub fn redeem(&mut self, token: &[u8; 32], own: &[u8], remote: &[u8], now: u64) -> bool {
        let mut matched: Option<usize> = None;
        for (i, invite) in self.list.iter().enumerate() {
            if constant_time_eq(&invite.token, token) && invite.expires_at > now {
                matched = Some(i);
            }
        }
        if matched.is_none() {
            matched = self.redeem_code(token, own, remote, now);
        }
        let Some(i) = matched else { return false };
        self.remove_at(i);
        true
    }

    /// 合言葉のトークンとして照合する。失敗してよい回数が残っていなければ、照合しない
    fn redeem_code(&mut self, token: &[u8; 32], own: &[u8], remote: &[u8], now: u64) -> Option<usize> {
        if !self.list.iter().any(|i| i.code_key.is_some() && i.expires_at > now) {
            return None;
        }
        self.refill_code_guesses(now);
        if self.code_guesses == 0 {
            return None;
        }
        let mut matched: Option<usize> = None;
        for (i, invite) in self.list.iter().enumerate() {
            let Some(key) = &invite.code_key else { continue };
            let expected = code::token(key, own, remote);
            if constant_time_eq(&expected, token) && invite.expires_at > now {
                matched = Some(i);
            }
        }
        if matched.is_none() {
            self.code_guesses = self.code_guesses.saturating_sub(1);
        }
        matched
    }

    fn refill_code_guesses(&mut self, now: u64) {
        let gained = now.saturating_sub(self.code_refilled_at) / CODE_GUESS_REFILL_SECS;
        if gained == 0 {
            return;
        }
        let gained = u32::try_from(gained).unwrap_or(u32::MAX);
        self.code_guesses = self.code_guesses.saturating_add(gained).min(CODE_GUESS_BURST);
        self.code_refilled_at = now;
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
fn constant_time_eq(a: &[u8; 32], b: &[u8; 32]) -> bool {
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

    #[test]
    fn invite_is_redeemed_once_within_its_lifetime() {
        let mut invites = Invites::default();
        invites.add([1; 32], None, None, 100);
        // 別のトークン・期限を過ぎたトークンは受けない
        assert!(!invites.redeem(&[2; 32], OWN, FRIEND, 50));
        assert!(!invites.redeem(&[1; 32], OWN, FRIEND, 100));
        // 期限内なら1回だけ
        assert!(invites.redeem(&[1; 32], OWN, FRIEND, 99));
        assert!(!invites.redeem(&[1; 32], OWN, FRIEND, 99));
    }

    #[test]
    fn oldest_invite_is_dropped_over_the_limit() {
        let mut invites = Invites::default();
        for i in 0..=MAX_INVITES {
            invites.add([u8::try_from(i).unwrap_or(0); 32], None, None, 100);
        }
        // 最初の招待は無効になり、残りは使える
        assert!(!invites.redeem(&[0; 32], OWN, FRIEND, 1));
        for i in 1..=MAX_INVITES {
            assert!(invites.redeem(&[u8::try_from(i).unwrap_or(0); 32], OWN, FRIEND, 1));
        }
    }

    #[test]
    fn expired_and_cancelled_invites_are_removed() {
        let mut invites = Invites::default();
        invites.add([1; 32], None, None, 10);
        invites.add([2; 32], None, None, 20);
        invites.expire(15);
        assert!(!invites.redeem(&[1; 32], OWN, FRIEND, 5));
        assert!(invites.redeem(&[2; 32], OWN, FRIEND, 5));
        invites.add([3; 32], None, None, 30);
        invites.clear();
        assert!(!invites.redeem(&[3; 32], OWN, FRIEND, 5));
    }

    /// 合言葉のトークンは、鍵と双方の PeerId で決まる。招待文字列のトークンとどちらか一方で、1回だけ使える
    #[test]
    fn code_token_is_bound_to_both_peers_and_shares_the_invite() {
        let key = [7u8; 32];
        let mut invites = Invites::default();
        invites.add([1; 32], Some(key), None, 1000);
        let good = code::token(&key, OWN, FRIEND);
        // 別の相手へ送ったトークン・向きを逆にしたトークンは使えない
        assert!(!invites.redeem(&code::token(&key, b"other", FRIEND), OWN, FRIEND, 100));
        assert!(!invites.redeem(&code::token(&key, FRIEND, OWN), OWN, FRIEND, 100));
        assert!(!invites.redeem(&good, OWN, b"stranger", 100));
        assert!(invites.redeem(&good, OWN, FRIEND, 100));
        // 合言葉で使った招待は、招待文字列のトークンでも使えない
        assert!(!invites.redeem(&[1; 32], OWN, FRIEND, 100));
        assert!(!invites.redeem(&good, OWN, FRIEND, 100));

        // 招待文字列のトークンで使った招待は、合言葉でも使えない
        invites.add([2; 32], Some(key), None, 1000);
        assert!(invites.redeem(&[2; 32], OWN, FRIEND, 100));
        assert!(!invites.redeem(&good, OWN, FRIEND, 100));
    }

    /// 合言葉のトークンの照合は、続けて失敗できる回数を絞る（総当たり）。招待文字列のトークンは絞らない
    #[test]
    fn code_guesses_are_limited() {
        let key = [7u8; 32];
        let mut invites = Invites::default();
        invites.add([1; 32], Some(key), None, 100_000);
        let good = code::token(&key, OWN, FRIEND);
        let now = 1000;
        for _ in 0..CODE_GUESS_BURST {
            assert!(!invites.redeem(&[9; 32], OWN, FRIEND, now));
        }
        // 回数を使い切ると、正しいトークンも照合しない
        assert!(!invites.redeem(&good, OWN, FRIEND, now));
        // 招待文字列のトークンは、回数によらず使える
        invites.add([2; 32], None, None, 100_000);
        assert!(invites.redeem(&[2; 32], OWN, FRIEND, now));
        // 時間がたつと、1回分ずつ戻る
        assert!(!invites.redeem(&good, OWN, FRIEND, now + CODE_GUESS_REFILL_SECS - 1));
        assert!(invites.redeem(&good, OWN, FRIEND, now + CODE_GUESS_REFILL_SECS));
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
        assert!(invites.redeem(&[1; 32], OWN, FRIEND, 50));
        assert!(invites.redeem(&[2; 32], OWN, FRIEND, 50));
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
        for _ in 0..(CODE_GUESS_BURST * 2) {
            assert!(!invites.redeem(&[9; 32], OWN, FRIEND, 1000));
        }
        let key = [7u8; 32];
        invites.add([2; 32], Some(key), None, 100_000);
        assert!(invites.redeem(&code::token(&key, OWN, FRIEND), OWN, FRIEND, 1000));
    }
}
