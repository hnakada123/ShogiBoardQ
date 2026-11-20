//! 在席の知らせ（友だちの申し込みを受けるトークン。protocol/README.md 3.3節・ipc.md 3章）
//!
//! 対局・UI側は、createPresence で作った知らせを、友だちごとの鍵で暗号化して協力ノードに預ける。友だちは、それを
//! 引き取って復号し、中のトークンで対局ストリームを開く。招待と違い、トークンは期限まで何度でも使える
//! （だれの知らせかは番号で対局・UI側へ知らせ、対局・UI側が、名乗ったプレイヤーID をその友だちと照らす）。

use super::invites::constant_time_eq;

/// 同時に持つ在席の知らせの上限（友だち 64 人に、作り直す前のものが残る分）
pub const MAX_PRESENCES: usize = 256;
/// 在席の知らせの有効期間（秒。対局・UI側は、これより短い間隔で作り直して預ける）
pub const PRESENCE_LIFETIME_SECS: u64 = 300;

struct Entry {
    id: u64,
    token: [u8; 32],
    expires_at: u64,
}

/// 作った在席の知らせ
#[derive(Default)]
pub struct Presences {
    list: Vec<Entry>,
    last_id: u64,
}

impl Presences {
    /// 知らせを足して番号を返す（上限を超えたら古いものから無効にする）
    pub fn add(&mut self, token: [u8; 32], expires_at: u64) -> u64 {
        while self.list.len() >= MAX_PRESENCES {
            self.list.remove(0);
        }
        self.last_id = self.last_id.saturating_add(1);
        self.list.push(Entry {
            id: self.last_id,
            token,
            expires_at,
        });
        self.last_id
    }

    /// 届いたトークンが期限内の知らせのものなら、その番号（使い終えない）。すべての知らせと、処理時間が中身に
    /// よらない比較で照合する
    pub fn find(&self, token: &[u8; 32], now: u64) -> Option<u64> {
        let mut found = None;
        for entry in &self.list {
            if constant_time_eq(&entry.token, token) && entry.expires_at > now {
                found = Some(entry.id);
            }
        }
        found
    }

    /// 期限を過ぎた知らせを消す
    pub fn expire(&mut self, now: u64) {
        self.list.retain(|e| e.expires_at > now);
    }

    /// すべての知らせを無効にする
    pub fn clear(&mut self) {
        self.list.clear();
    }
}

#[cfg(test)]
#[allow(clippy::arithmetic_side_effects, clippy::indexing_slicing)]
mod tests {
    use super::*;

    #[test]
    fn token_works_until_it_expires_and_is_not_used_up() {
        let mut presences = Presences::default();
        let first = presences.add([1; 32], 100);
        let second = presences.add([2; 32], 200);
        assert_ne!(first, second);
        assert_ne!(first, 0);
        // 何度でも使える
        assert_eq!(presences.find(&[1; 32], 50), Some(first));
        assert_eq!(presences.find(&[1; 32], 99), Some(first));
        assert_eq!(presences.find(&[2; 32], 99), Some(second));
        assert_eq!(presences.find(&[3; 32], 50), None);
        // 期限を過ぎたら使えない
        assert_eq!(presences.find(&[1; 32], 100), None);
        presences.expire(100);
        assert_eq!(presences.find(&[2; 32], 150), Some(second));
        presences.clear();
        assert_eq!(presences.find(&[2; 32], 150), None);
    }

    #[test]
    fn oldest_is_dropped_over_the_limit() {
        let mut presences = Presences::default();
        let mut tokens = Vec::new();
        for i in 0..=MAX_PRESENCES {
            let mut token = [0u8; 32];
            token[..8].copy_from_slice(&(i as u64).to_be_bytes());
            presences.add(token, 100);
            tokens.push(token);
        }
        assert_eq!(presences.find(&tokens[0], 1), None);
        assert!(presences.find(&tokens[1], 1).is_some());
        assert!(presences.find(&tokens[MAX_PRESENCES], 1).is_some());
    }
}
