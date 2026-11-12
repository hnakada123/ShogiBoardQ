//! 作った招待（合言葉と期限）。発行・照合・期限切れ・取り消し（protocol/README.md 2.1節・ipc.md 3章）
//!
//! 合言葉の値はこの中だけに置き、外には照合の結果だけを返す。

/// 同時に持つ招待の上限（超えたら古いものから無効にする）
pub const MAX_INVITES: usize = 4;

struct Outstanding {
    token: [u8; 32],
    expires_at: u64,
}

/// 作った招待の一覧
#[derive(Default)]
pub struct Invites {
    list: Vec<Outstanding>,
}

impl Invites {
    /// 招待を足す（上限を超えたら古いものから無効にする）
    pub fn add(&mut self, token: [u8; 32], expires_at: u64) {
        while self.list.len() >= MAX_INVITES {
            self.list.remove(0);
        }
        self.list.push(Outstanding { token, expires_at });
    }

    /// 届いた合言葉が期限内の招待と一致すれば、その招待を使い終えて true（1つの招待で1回だけ接続を受ける）。
    /// すべての招待と、処理時間が中身によらない比較で照合する
    pub fn redeem(&mut self, token: &[u8; 32], now: u64) -> bool {
        let mut matched: Option<usize> = None;
        for (i, invite) in self.list.iter().enumerate() {
            if constant_time_eq(&invite.token, token) && invite.expires_at > now {
                matched = Some(i);
            }
        }
        let Some(i) = matched else { return false };
        if i < self.list.len() {
            self.list.remove(i);
        }
        true
    }

    /// 期限を過ぎた招待を消す
    pub fn expire(&mut self, now: u64) {
        self.list.retain(|i| i.expires_at > now);
    }

    /// すべての招待を無効にする
    pub fn clear(&mut self) {
        self.list.clear();
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
mod tests {
    use super::*;

    #[test]
    fn invite_is_redeemed_once_within_its_lifetime() {
        let mut invites = Invites::default();
        invites.add([1; 32], 100);
        // 別の合言葉・期限を過ぎた合言葉は受けない
        assert!(!invites.redeem(&[2; 32], 50));
        assert!(!invites.redeem(&[1; 32], 100));
        // 期限内なら1回だけ
        assert!(invites.redeem(&[1; 32], 99));
        assert!(!invites.redeem(&[1; 32], 99));
    }

    #[test]
    fn oldest_invite_is_dropped_over_the_limit() {
        let mut invites = Invites::default();
        for i in 0..=MAX_INVITES {
            invites.add([u8::try_from(i).unwrap_or(0); 32], 100);
        }
        // 最初の招待は無効になり、残りは使える
        assert!(!invites.redeem(&[0; 32], 1));
        for i in 1..=MAX_INVITES {
            assert!(invites.redeem(&[u8::try_from(i).unwrap_or(0); 32], 1));
        }
    }

    #[test]
    fn expired_and_cancelled_invites_are_removed() {
        let mut invites = Invites::default();
        invites.add([1; 32], 10);
        invites.add([2; 32], 20);
        invites.expire(15);
        assert!(!invites.redeem(&[1; 32], 5));
        assert!(invites.redeem(&[2; 32], 5));
        invites.add([3; 32], 30);
        invites.clear();
        assert!(!invites.redeem(&[3; 32], 5));
    }
}
