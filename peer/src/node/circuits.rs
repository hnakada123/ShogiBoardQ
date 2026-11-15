//! 協力ノードが中継している接続（Circuit Relay v2 の回路）の数
//!
//! 協力ノードのログに出す数だけを数える（ピアの ID も接続元の IP も外へ出さない）。回路の終わりは、中継の先との
//! 接続が切れたときに2回知らされることがある（接続が閉じた知らせと、回路を運んでいた側の知らせ）。回路は、
//! 頼んだ側と先の組ごとに数え、数えていない組の終わりは捨てる（ほかの組の数を減らさない）。

use std::collections::HashMap;

use libp2p::PeerId;

/// 開いている回路（数は中継の上限 max_circuits まで。中継が受け付けた回路だけを数える）
#[derive(Default)]
pub struct Circuits {
    /// 頼んだ側と先の組ごとの、開いている回路の数
    open: HashMap<(PeerId, PeerId), usize>,
}

impl Circuits {
    /// 回路が開いた
    pub fn opened(&mut self, src: PeerId, dst: PeerId) {
        let count = self.open.entry((src, dst)).or_insert(0);
        *count = count.saturating_add(1);
    }

    /// 回路が閉じた
    pub fn closed(&mut self, src: PeerId, dst: PeerId) {
        let key = (src, dst);
        let Some(count) = self.open.get_mut(&key) else {
            return;
        };
        *count = count.saturating_sub(1);
        if *count == 0 {
            self.open.remove(&key);
        }
    }

    /// 開いている回路の数
    pub fn count(&self) -> usize {
        self.open.values().fold(0usize, |sum, n| sum.saturating_add(*n))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn counts_open_circuits() {
        let (a, b, c) = (PeerId::random(), PeerId::random(), PeerId::random());
        let mut circuits = Circuits::default();
        assert_eq!(circuits.count(), 0);
        circuits.opened(a, b);
        circuits.opened(c, b);
        assert_eq!(circuits.count(), 2);
        // 同じ組の2本目（つなぎ直しで、前の回路が閉じる前に次の回路が開く）
        circuits.opened(a, b);
        assert_eq!(circuits.count(), 3);
        circuits.closed(a, b);
        assert_eq!(circuits.count(), 2);
        circuits.closed(a, b);
        circuits.closed(c, b);
        assert_eq!(circuits.count(), 0);
    }

    #[test]
    fn repeated_close_does_not_reduce_other_circuits() {
        let (a, b, c) = (PeerId::random(), PeerId::random(), PeerId::random());
        let mut circuits = Circuits::default();
        circuits.opened(a, b);
        circuits.opened(c, b);
        // 中継の先との接続が切れると、同じ回路の終わりが2回知らされることがある
        circuits.closed(a, b);
        circuits.closed(a, b);
        assert_eq!(circuits.count(), 1);
        // 開いた知らせの無い組（受け付ける前に失敗した回路）の終わりも数えない
        circuits.closed(b, a);
        assert_eq!(circuits.count(), 1);
        // 向きが逆の組は別の回路
        circuits.opened(b, c);
        circuits.closed(c, b);
        assert_eq!(circuits.count(), 1);
    }
}
