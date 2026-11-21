//! 対局ストリームの接続の番号と段階（protocol/ipc.md 3章・4章）
//!
//! 対局・UI側に返す接続の番号1つに、段階を1つ持つ。つなぐ側は、協力ノードから招待を引き取る（協力ノードを使う
//! 合言葉）・合言葉の宛先へ相手の PeerId を決めずにダイヤルする（同じ LAN の合言葉）・相手とのどの接続の上で
//! 開くかを待つ・ストリームを開く、と進み、開いたら届いた側と同じ「開いた」になる。番号の発行・数・あるか・
//! 閉じる・connectionClosed を出すかは、ここだけが決める。
//!
//! Swarm にも IPC の出力にも触れない（ダイヤル・ストリームを開くタスク・知らせは Node が受け持つ）。閉じる接続は
//! 段階を「閉じる」にし、Node が今の出来事（要求・Swarm の出来事・タスクの知らせ）を処理し終えたところで
//! take_closed で取り出して connectionClosed を知らせる。つなぎ始められなかった接続も同じなので、その知らせは
//! 番号を返す応答の後に届く。closeConnection で閉じた接続（cancel）は知らせない。

use std::collections::{BTreeMap, HashSet};
use std::sync::Arc;
use std::time::{Duration, Instant};

use libp2p::swarm::ConnectionId;
use libp2p::{Multiaddr, PeerId};
use tokio::sync::mpsc;
use tokio::task::AbortHandle;

use crate::game::ConnShared;
use crate::ipc::{CloseReason, Direction, Origin, Route};

/// 対局ストリーム（つなぐ途中・開始前・対局中を合わせて）の上限
pub const MAX_GAME_CONNECTIONS: usize = 8;
/// 対局ストリームを開くまでの時間（開く相手が決まってから。合言葉の宛先へつながるまでの時間は別に数える）
pub const CONNECT_TIMEOUT: Duration = Duration::from_secs(30);
/// 合言葉の宛先へつながるのを待つ時間（対局ストリームを開く時間は、この後に別に数える）
pub const CODE_CONNECT_TIMEOUT: Duration = Duration::from_secs(30);

/// 開いた対局ストリームの読み書き（送る列・両方向で共有する状態・受信のタスク）
pub struct ConnIo {
    pub writer: mpsc::Sender<Vec<u8>>,
    pub shared: Arc<ConnShared>,
    reader: AbortHandle,
}

impl ConnIo {
    pub fn new(writer: mpsc::Sender<Vec<u8>>, shared: Arc<ConnShared>, reader: AbortHandle) -> ConnIo {
        ConnIo { writer, shared, reader }
    }
}

impl Drop for ConnIo {
    fn drop(&mut self) {
        // 送信のタスクは送る列が閉じれば終わるが、受信のタスクは止める
        self.reader.abort();
    }
}

/// 対局ストリームを開く相手（つなぐ側）
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Outbound {
    pub peer: PeerId,
    pub origin: Origin,
    /// 開いた直後に送る招待のトークン（対局の相手へのつなぎ直しは None）
    pub token: Option<[u8; 32]>,
}

/// ストリームを開くタスクに渡すもの（release）
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Release {
    pub peer: PeerId,
    pub token: Option<[u8; 32]>,
    /// 開くのをあきらめる時刻（開く相手が決まってから CONNECT_TIMEOUT）
    pub deadline: Instant,
}

/// connectionOpened の中身
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Opened {
    pub conn: u64,
    pub peer: PeerId,
    pub route: Route,
    pub direction: Direction,
    pub origin: Origin,
    /// 友だちの申し込みで使われた在席の知らせの番号（ほかは 0）
    pub presence: u64,
}

/// connectionClosed の中身
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Closed {
    pub conn: u64,
    pub reason: CloseReason,
}

/// 合言葉の相手と鍵がそろった（Node がトークンを作り、相手へ対局ストリームを開き始める。proceed）
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CodeReady {
    pub conn: u64,
    pub peer: PeerId,
    pub key: [u8; 32],
    /// 合言葉の宛先（自分の待受を除いたもの）
    pub addrs: Vec<Multiaddr>,
}

/// 合言葉の宛先へのダイヤルの出来事の後に、Node がすること
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum DialStep {
    /// 何もしない（まだそろわない・知らないダイヤル・閉じた）
    Wait,
    /// 2本目からの接続なので閉じる（対局ストリームは最初の接続の上で開く）
    CloseExtra,
    /// 相手と鍵がそろった
    Ready(CodeReady),
}

/// 開いていない接続（sendFrame の断り）
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NotOpen {
    /// 番号が無い
    Unknown,
    /// つなぐ途中・閉じる途中
    Pending,
}

/// 合言葉の宛先へ、相手の PeerId を決めずにダイヤルしている
struct Dialing {
    addrs: Vec<Multiaddr>,
    /// 結果を待っているダイヤル
    dials: HashSet<ConnectionId>,
    /// 最初につながった相手
    peer: Option<PeerId>,
    /// 合言葉から作った鍵（作り終えるまで None）
    key: Option<[u8; 32]>,
    deadline: Instant,
}

/// 対局ストリームを、相手とのどの接続の上で開くかを待っている
struct Waiting {
    outbound: Outbound,
    deadline: Instant,
    /// 直接の接続を待たない（穴あけをあきらめた）
    no_hole_punch: bool,
}

/// 開いた対局ストリーム
struct Open {
    peer: PeerId,
    route: Route,
    /// 経路が、ストリームを開いた接続で決まっている（つないだ側）。届いた側は、相手との接続から決める
    fixed_route: bool,
    io: ConnIo,
}

enum Stage {
    /// 協力ノードから招待を引き取っている（協力ノードを使う合言葉の connectInvite）
    Fetching,
    Dialing(Dialing),
    Waiting(Waiting),
    /// 対局ストリームを開いている（開くタスクの結果を待つ）
    Opening {
        peer: PeerId,
        origin: Origin,
        route: Route,
    },
    Open(Open),
    /// 閉じて connectionClosed を知らせる（take_closed で取り出す）
    Closing(CloseReason),
}

/// 対局ストリームの接続
#[derive(Default)]
pub struct Conns {
    stages: BTreeMap<u64, Stage>,
    /// 最後に出した番号（番号は 1 から、使い回さない）
    last: u64,
}

fn deadline(now: Instant, after: Duration) -> Instant {
    now.checked_add(after).unwrap_or(now)
}

/// 開く相手が決まった接続の段階（始められなければ、閉じる）
fn connect_stage(start: Result<Outbound, CloseReason>, now: Instant) -> Stage {
    match start {
        Ok(outbound) => Stage::Waiting(Waiting {
            outbound,
            deadline: deadline(now, CONNECT_TIMEOUT),
            no_hole_punch: false,
        }),
        Err(reason) => Stage::Closing(reason),
    }
}

impl Conns {
    fn add_with(&mut self, stage: impl FnOnce(u64) -> Stage) -> u64 {
        self.last = self.last.saturating_add(1);
        let conn = self.last;
        self.stages.insert(conn, stage(conn));
        conn
    }

    /// もう1つ接続を受け付けられるか（つなぐ途中・閉じる途中の接続も数える）
    pub fn has_room(&self) -> bool {
        self.stages.len() < MAX_GAME_CONNECTIONS
    }

    // ---------------------------------------------------------------- 始める

    /// 協力ノードから招待を引き取ってからつなぐ接続
    pub fn fetch(&mut self) -> u64 {
        self.add_with(|_| Stage::Fetching)
    }

    /// 協力ノードから招待を引き取っている接続か（引き取りを待つ間に閉じられていなければ true）
    pub fn is_fetching(&self, conn: u64) -> bool {
        matches!(self.stages.get(&conn), Some(Stage::Fetching))
    }

    /// 合言葉の宛先へ始めたダイヤル（dials）の結果を待つ接続
    pub fn dial(&mut self, addrs: Vec<Multiaddr>, dials: Vec<ConnectionId>, now: Instant) -> u64 {
        self.add_with(|_| {
            Stage::Dialing(Dialing {
                addrs,
                dials: dials.into_iter().collect(),
                peer: None,
                key: None,
                deadline: deadline(now, CODE_CONNECT_TIMEOUT),
            })
        })
    }

    /// 開く相手が決まった接続。始められなかった（Err）なら、その理由で閉じる
    pub fn connect(&mut self, start: Result<Outbound, CloseReason>, now: Instant) -> u64 {
        self.add_with(|_| connect_stage(start, now))
    }

    /// 引き取り・合言葉の宛先へのダイヤルを終えた接続を、開く相手が決まった段階へ進める。その段階の接続でなければ
    /// （閉じられた）何もしないで false
    pub fn proceed(&mut self, conn: u64, start: Result<Outbound, CloseReason>, now: Instant) -> bool {
        match self.stages.get_mut(&conn) {
            Some(stage) if matches!(stage, Stage::Fetching | Stage::Dialing(_)) => {
                *stage = connect_stage(start, now);
                true
            }
            _ => false,
        }
    }

    // ---------------------------------------------------------------- 合言葉の宛先へのダイヤル

    fn dialing_with(&mut self, id: ConnectionId) -> Option<(u64, &mut Dialing)> {
        self.stages.iter_mut().find_map(|(conn, stage)| match stage {
            Stage::Dialing(dialing) if dialing.dials.contains(&id) => Some((*conn, dialing)),
            _ => None,
        })
    }

    /// PeerId を決めずに始めたダイヤルがつながった。開く段階へ移った後のダイヤル・閉じた接続のダイヤルは知らない
    /// （つながった接続はそのままにする）
    pub fn dial_established(&mut self, id: ConnectionId, peer: PeerId) -> DialStep {
        let Some((conn, dialing)) = self.dialing_with(id) else {
            return DialStep::Wait;
        };
        dialing.dials.remove(&id);
        if dialing.peer.is_some() {
            return DialStep::CloseExtra;
        }
        dialing.peer = Some(peer);
        self.check_dialing(conn)
    }

    /// PeerId を決めずに始めたダイヤルが失敗した
    pub fn dial_failed(&mut self, id: ConnectionId) -> DialStep {
        let Some((conn, dialing)) = self.dialing_with(id) else {
            return DialStep::Wait;
        };
        dialing.dials.remove(&id);
        self.check_dialing(conn)
    }

    /// 合言葉の鍵を作り終えた（作れなければ None で、unreachable で閉じる）
    pub fn code_key(&mut self, conn: u64, key: Option<[u8; 32]>) -> DialStep {
        let Some(Stage::Dialing(dialing)) = self.stages.get_mut(&conn) else {
            return DialStep::Wait;
        };
        match key {
            Some(key) => {
                dialing.key = Some(key);
                self.check_dialing(conn)
            }
            None => {
                self.fail(conn, CloseReason::Unreachable);
                DialStep::Wait
            }
        }
    }

    /// 相手と鍵がそろったか。どのダイヤルもつながらずに終わったら unreachable で閉じる
    fn check_dialing(&mut self, conn: u64) -> DialStep {
        let Some(Stage::Dialing(dialing)) = self.stages.get(&conn) else {
            return DialStep::Wait;
        };
        match (dialing.peer, dialing.key) {
            (Some(peer), Some(key)) => DialStep::Ready(CodeReady {
                conn,
                peer,
                key,
                addrs: dialing.addrs.clone(),
            }),
            (None, _) if dialing.dials.is_empty() => {
                self.fail(conn, CloseReason::Unreachable);
                DialStep::Wait
            }
            _ => DialStep::Wait,
        }
    }

    // ---------------------------------------------------------------- 開く接続を待つ・開く

    /// 開く接続を待っている相手（重複なし）
    pub fn waiting_peers(&self) -> Vec<PeerId> {
        let mut peers: Vec<PeerId> = self
            .stages
            .values()
            .filter_map(|stage| match stage {
                Stage::Waiting(w) => Some(w.outbound.peer),
                _ => None,
            })
            .collect();
        peers.sort_unstable();
        peers.dedup();
        peers
    }

    /// 相手へ開く接続を待っている接続と、直接の接続を待たないか
    pub fn waiting_for(&self, peer: &PeerId) -> Vec<(u64, bool)> {
        self.stages
            .iter()
            .filter_map(|(conn, stage)| match stage {
                Stage::Waiting(w) if &w.outbound.peer == peer => Some((*conn, w.no_hole_punch)),
                _ => None,
            })
            .collect()
    }

    /// 相手との穴あけをあきらめた（直接の接続を待たない）
    pub fn give_up_hole_punching(&mut self, peer: &PeerId) {
        for stage in self.stages.values_mut() {
            if let Stage::Waiting(w) = stage {
                if &w.outbound.peer == peer {
                    w.no_hole_punch = true;
                }
            }
        }
    }

    /// 相手へ開く接続を待っている接続を閉じる（相手への接続に失敗した・相手との接続がなくなった）
    pub fn fail_waiting(&mut self, peer: &PeerId, reason: CloseReason) {
        for stage in self.stages.values_mut() {
            if matches!(stage, Stage::Waiting(w) if &w.outbound.peer == peer) {
                *stage = Stage::Closing(reason);
            }
        }
    }

    /// 開く接続が決まった（route はその経路）。開くタスクに渡すものを返す
    pub fn release(&mut self, conn: u64, route: Route) -> Option<Release> {
        let stage = self.stages.get_mut(&conn)?;
        let Stage::Waiting(w) = stage else {
            return None;
        };
        let release = Release {
            peer: w.outbound.peer,
            token: w.outbound.token,
            deadline: w.deadline,
        };
        let origin = w.outbound.origin;
        *stage = Stage::Opening {
            peer: release.peer,
            origin,
            route,
        };
        Some(release)
    }

    /// 中継経由の接続の上で対局ストリームを開いた（開いている途中の）相手か。その後にできた直接の接続は使えない
    pub fn has_relayed_stream_to(&self, peer: &PeerId) -> bool {
        let waiting = self
            .stages
            .values()
            .any(|stage| matches!(stage, Stage::Waiting(w) if &w.outbound.peer == peer));
        !waiting
            && self.stages.values().any(|stage| match stage {
                Stage::Opening { peer: p, route, .. } => p == peer && *route == Route::Relay,
                Stage::Open(o) => &o.peer == peer && o.fixed_route && o.route == Route::Relay,
                _ => false,
            })
    }

    /// ストリームを開いている途中か（開いたストリームを受け取る。閉じられた接続のストリームは捨てる）
    pub fn is_opening(&self, conn: u64) -> bool {
        matches!(self.stages.get(&conn), Some(Stage::Opening { .. }))
    }

    /// 開いたストリームの読み書きを始めた（つないだ側）
    pub fn open(&mut self, conn: u64, io: ConnIo) -> Option<Opened> {
        let stage = self.stages.get_mut(&conn)?;
        let Stage::Opening { peer, origin, route } = *stage else {
            return None;
        };
        *stage = Stage::Open(Open {
            peer,
            route,
            fixed_route: true,
            io,
        });
        Some(Opened {
            conn,
            peer,
            route,
            direction: Direction::Outbound,
            origin,
            presence: 0,
        })
    }

    /// 届いたストリームを受け付けた（新しい番号。io は番号から読み書きを始める）。経路は相手との接続から決め、
    /// 接続が変わると reroute で変わる
    pub fn accept(
        &mut self,
        peer: PeerId,
        origin: Origin,
        presence: u64,
        route: Route,
        io: impl FnOnce(u64) -> ConnIo,
    ) -> Opened {
        let conn = self.add_with(|conn| {
            Stage::Open(Open {
                peer,
                route,
                fixed_route: false,
                io: io(conn),
            })
        });
        Opened {
            conn,
            peer,
            route,
            direction: Direction::Inbound,
            origin,
            presence,
        }
    }

    pub fn is_open(&self, conn: u64) -> bool {
        matches!(self.stages.get(&conn), Some(Stage::Open(_)))
    }

    /// 開いた接続の読み書き
    pub fn io(&self, conn: u64) -> Result<&ConnIo, NotOpen> {
        match self.stages.get(&conn) {
            Some(Stage::Open(o)) => Ok(&o.io),
            Some(_) => Err(NotOpen::Pending),
            None => Err(NotOpen::Unknown),
        }
    }

    /// 相手との接続から見た経路が route になった。経路の変わった接続（届いた側の開いた接続）を返す
    pub fn reroute(&mut self, peer: &PeerId, route: Route) -> Vec<u64> {
        let mut changed = Vec::new();
        for (conn, stage) in self.stages.iter_mut() {
            if let Stage::Open(o) = stage {
                if &o.peer == peer && !o.fixed_route && o.route != route {
                    o.route = route;
                    changed.push(*conn);
                }
            }
        }
        changed
    }

    // ---------------------------------------------------------------- 閉じる

    /// 閉じて、connectionClosed を知らせる（閉じる途中なら、先の理由のまま）
    pub fn fail(&mut self, conn: u64, reason: CloseReason) {
        if let Some(stage) = self.stages.get_mut(&conn) {
            if !matches!(stage, Stage::Closing(_)) {
                *stage = Stage::Closing(reason);
            }
        }
    }

    /// 期限を過ぎた接続（合言葉の宛先へのダイヤル・開く接続を待つ間）を timeout で閉じる。ストリームを開いている
    /// 途中の期限は、開くタスクが数える
    pub fn expire(&mut self, now: Instant) {
        let expired: Vec<u64> = self
            .stages
            .iter()
            .filter(|(_, stage)| match stage {
                Stage::Dialing(d) => now >= d.deadline,
                Stage::Waiting(w) => now >= w.deadline,
                _ => false,
            })
            .map(|(conn, _)| *conn)
            .collect();
        for conn in expired {
            self.fail(conn, CloseReason::Timeout);
        }
    }

    /// closeConnection: 知らせずに閉じる（あれば true）
    pub fn cancel(&mut self, conn: u64) -> bool {
        self.stages.remove(&conn).is_some()
    }

    /// 閉じた接続を取り出す（番号の順。Node が connectionClosed を知らせる）
    pub fn take_closed(&mut self) -> Vec<Closed> {
        let closed: Vec<Closed> = self
            .stages
            .iter()
            .filter_map(|(conn, stage)| match stage {
                Stage::Closing(reason) => Some(Closed {
                    conn: *conn,
                    reason: *reason,
                }),
                _ => None,
            })
            .collect();
        for c in &closed {
            self.stages.remove(&c.conn);
        }
        closed
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn later(now: Instant, secs: u64) -> Instant {
        deadline(now, Duration::from_secs(secs))
    }

    fn outbound(peer: PeerId) -> Outbound {
        Outbound {
            peer,
            origin: Origin::Invite,
            token: Some([7; 32]),
        }
    }

    fn io(conn: u64) -> ConnIo {
        let (writer, _) = mpsc::channel(1);
        let reader = tokio::spawn(async move { conn }).abort_handle();
        ConnIo::new(writer, Arc::default(), reader)
    }

    fn closed(conn: u64, reason: CloseReason) -> Closed {
        Closed { conn, reason }
    }

    /// 番号があるか（どの段階でも）
    fn known(conns: &Conns, conn: u64) -> bool {
        conns.io(conn).err() != Some(NotOpen::Unknown)
    }

    /// 番号は 1 から順に出し、使い回さない。どの段階も数に入り、上限を超えて受け付けない
    #[test]
    fn numbers_and_counts_cover_every_stage() {
        let now = Instant::now();
        let peer = PeerId::random();
        let mut conns = Conns::default();
        let numbers = [
            conns.fetch(),
            conns.dial(vec![], vec![ConnectionId::new_unchecked(1)], now),
            conns.connect(Ok(outbound(peer)), now),
            conns.connect(Err(CloseReason::Unreachable), now),
        ];
        assert_eq!(numbers, [1, 2, 3, 4]);
        assert!(numbers.iter().all(|n| known(&conns, *n)));
        let mut added: usize = 0;
        while conns.has_room() {
            conns.fetch();
            added = added.saturating_add(1);
        }
        assert_eq!(added, MAX_GAME_CONNECTIONS.saturating_sub(numbers.len()));
        // 閉じた番号は、知らせるまで数に入る。知らせた後は空き、番号は使い回さない
        assert_eq!(conns.take_closed(), vec![closed(4, CloseReason::Unreachable)]);
        assert!(conns.has_room());
        assert!(!known(&conns, 4));
        assert_eq!(conns.fetch(), 9);
    }

    /// closeConnection で閉じた接続は知らせない。ほかの閉じ方は1回だけ、最初の理由で知らせる
    #[test]
    fn only_failures_are_reported_once() {
        let now = Instant::now();
        let mut conns = Conns::default();
        let fetching = conns.fetch();
        let dialing = conns.dial(vec![], vec![ConnectionId::new_unchecked(1)], now);
        assert!(conns.cancel(fetching));
        assert!(!conns.cancel(fetching));
        conns.fail(dialing, CloseReason::Timeout);
        conns.fail(dialing, CloseReason::Unreachable);
        // 無い番号を閉じても何も起きない
        conns.fail(fetching, CloseReason::Unreachable);
        assert_eq!(conns.take_closed(), vec![closed(dialing, CloseReason::Timeout)]);
        assert_eq!(conns.take_closed(), vec![]);
        assert!(!known(&conns, dialing));
    }

    /// 協力ノードからの引き取りを待つ接続は、引き取った後に開く相手が決まる。待つ間に閉じられた接続は進めない
    #[test]
    fn fetching_proceeds_unless_cancelled() {
        let now = Instant::now();
        let peer = PeerId::random();
        let mut conns = Conns::default();
        let first = conns.fetch();
        let second = conns.fetch();
        let third = conns.fetch();
        assert!(conns.is_fetching(first));
        assert!(conns.proceed(first, Ok(outbound(peer)), now));
        assert!(!conns.is_fetching(first));
        assert_eq!(conns.waiting_for(&peer), vec![(first, false)]);
        // 開く相手が決まった後は、もう進めない
        assert!(!conns.proceed(first, Err(CloseReason::Unreachable), now));
        assert!(conns.cancel(second));
        assert!(!conns.proceed(second, Ok(outbound(peer)), now));
        assert!(conns.proceed(third, Err(CloseReason::NoRelayRoute), now));
        assert_eq!(conns.take_closed(), vec![closed(third, CloseReason::NoRelayRoute)]);
    }

    /// 合言葉: 最初につながった相手と鍵がそろったら開く段階へ進める。2本目からの接続は閉じる
    #[test]
    fn code_dial_waits_for_the_peer_and_the_key() {
        let now = Instant::now();
        let peer = PeerId::random();
        let addrs: Vec<Multiaddr> = vec![Multiaddr::empty()];
        let (a, b, c) = (
            ConnectionId::new_unchecked(1),
            ConnectionId::new_unchecked(2),
            ConnectionId::new_unchecked(3),
        );
        let mut conns = Conns::default();
        let conn = conns.dial(addrs.clone(), vec![a, b, c], now);
        assert_eq!(conns.dial_failed(a), DialStep::Wait);
        // 知らないダイヤル
        assert_eq!(conns.dial_failed(a), DialStep::Wait);
        assert_eq!(
            conns.dial_established(ConnectionId::new_unchecked(9), peer),
            DialStep::Wait
        );
        // 相手が分かっても、鍵がまだ
        assert_eq!(conns.dial_established(b, peer), DialStep::Wait);
        assert_eq!(conns.dial_established(c, PeerId::random()), DialStep::CloseExtra);
        let ready = CodeReady {
            conn,
            peer,
            key: [5; 32],
            addrs,
        };
        assert_eq!(conns.code_key(conn, Some([5; 32])), DialStep::Ready(ready.clone()));
        assert!(conns.proceed(conn, Ok(outbound(peer)), now));
        assert_eq!(conns.waiting_for(&peer), vec![(conn, false)]);
        // 開く段階へ移った後の鍵は知らない
        assert_eq!(conns.code_key(conn, Some([5; 32])), DialStep::Wait);

        // 鍵が先に届いても、相手が分かったときにそろう
        let d = ConnectionId::new_unchecked(4);
        let second = conns.dial(ready.addrs.clone(), vec![d], now);
        assert_eq!(conns.code_key(second, Some([5; 32])), DialStep::Wait);
        assert_eq!(
            conns.dial_established(d, peer),
            DialStep::Ready(CodeReady { conn: second, ..ready })
        );
        assert_eq!(conns.take_closed(), vec![]);
    }

    /// 合言葉: どのダイヤルもつながらない・鍵を作れない・時間が過ぎたら閉じる
    #[test]
    fn code_dial_fails() {
        let now = Instant::now();
        let mut conns = Conns::default();
        let (a, b) = (ConnectionId::new_unchecked(1), ConnectionId::new_unchecked(2));
        let nowhere = conns.dial(vec![], vec![a, b], now);
        assert_eq!(conns.dial_failed(a), DialStep::Wait);
        assert_eq!(conns.dial_failed(b), DialStep::Wait);
        let no_key = conns.dial(vec![], vec![ConnectionId::new_unchecked(3)], now);
        assert_eq!(conns.code_key(no_key, None), DialStep::Wait);
        let slow = conns.dial(vec![], vec![ConnectionId::new_unchecked(4)], now);
        conns.expire(later(now, 29));
        assert!(known(&conns, slow));
        conns.expire(later(now, 30));
        assert_eq!(
            conns.take_closed(),
            vec![
                closed(nowhere, CloseReason::Unreachable),
                closed(no_key, CloseReason::Unreachable),
                closed(slow, CloseReason::Timeout)
            ]
        );
    }

    /// 開く接続が決まったら開く段階へ進め、開いたら知らせる中身を返す。経路はストリームを開いた接続で決まる
    #[tokio::test]
    async fn waiting_is_released_and_opened() {
        let now = Instant::now();
        let peer = PeerId::random();
        let mut conns = Conns::default();
        let conn = conns.connect(Ok(outbound(peer)), now);
        let other = conns.connect(Ok(outbound(PeerId::random())), now);
        assert_eq!(conns.waiting_peers().len(), 2);
        conns.give_up_hole_punching(&peer);
        assert_eq!(conns.waiting_for(&peer), vec![(conn, true)]);
        assert_eq!(conns.waiting_for(&PeerId::random()), vec![]);
        // 待っている間は、中継の上で開いた相手として扱わない
        assert!(!conns.is_opening(conn));
        assert_eq!(conns.io(conn).err(), Some(NotOpen::Pending));
        assert_eq!(
            conns.release(conn, Route::Relay),
            Some(Release {
                peer,
                token: Some([7; 32]),
                deadline: later(now, 30)
            })
        );
        assert_eq!(conns.release(conn, Route::Relay), None);
        assert!(conns.is_opening(conn));
        assert!(conns.has_relayed_stream_to(&peer));
        assert_eq!(
            conns.open(conn, io(conn)),
            Some(Opened {
                conn,
                peer,
                route: Route::Relay,
                direction: Direction::Outbound,
                origin: Origin::Invite,
                presence: 0
            })
        );
        assert!(conns.is_open(conn));
        assert!(conns.io(conn).is_ok());
        assert!(conns.has_relayed_stream_to(&peer));
        // つないだ側の経路は、相手との接続が変わっても変わらない
        assert_eq!(conns.reroute(&peer, Route::Direct), Vec::<u64>::new());
        // 同じ相手へ開く接続を待っている間は、中継の上で開いた相手として扱わない（直接の接続を使う）
        let again = conns.connect(Ok(outbound(peer)), now);
        assert!(!conns.has_relayed_stream_to(&peer));
        // 直接の接続の上で開いた相手は、中継の上で開いた相手ではない
        assert!(conns.cancel(again));
        assert!(conns.cancel(conn));
        let direct = conns.connect(Ok(outbound(peer)), now);
        conns.release(direct, Route::Direct);
        assert!(!conns.has_relayed_stream_to(&peer));
        // 開いている途中でなければ、開いたストリームは受け取らない
        assert_eq!(conns.open(other, io(other)), None);
        assert_eq!(conns.io(99).err(), Some(NotOpen::Unknown));
    }

    /// 開く接続を待つ間に、相手への接続に失敗した・時間が過ぎたら閉じる。開いている途中のものは開くタスクが数える
    #[test]
    fn waiting_fails_or_expires() {
        let now = Instant::now();
        let peer = PeerId::random();
        let mut conns = Conns::default();
        let first = conns.connect(Ok(outbound(peer)), now);
        let opening = conns.connect(Ok(outbound(peer)), now);
        conns.release(opening, Route::Direct);
        let elsewhere = conns.connect(Ok(outbound(PeerId::random())), now);
        conns.fail_waiting(&peer, CloseReason::Unreachable);
        assert_eq!(conns.take_closed(), vec![closed(first, CloseReason::Unreachable)]);
        conns.expire(later(now, 29));
        assert_eq!(conns.take_closed(), vec![]);
        conns.expire(later(now, 30));
        assert_eq!(conns.take_closed(), vec![closed(elsewhere, CloseReason::Timeout)]);
        assert!(conns.is_opening(opening));
    }

    /// 届いた側の経路は、相手との接続から決め、変わったら知らせる接続を返す
    #[tokio::test]
    async fn accepted_streams_follow_the_peer_route() {
        let peer = PeerId::random();
        let mut conns = Conns::default();
        let opened = conns.accept(peer, Origin::Friend, 3, Route::Direct, io);
        assert_eq!(
            opened,
            Opened {
                conn: 1,
                peer,
                route: Route::Direct,
                direction: Direction::Inbound,
                origin: Origin::Friend,
                presence: 3
            }
        );
        assert!(conns.is_open(opened.conn));
        assert!(!conns.has_relayed_stream_to(&peer));
        assert_eq!(conns.reroute(&PeerId::random(), Route::Relay), Vec::<u64>::new());
        assert_eq!(conns.reroute(&peer, Route::Relay), vec![opened.conn]);
        assert_eq!(conns.reroute(&peer, Route::Relay), Vec::<u64>::new());
        // 届いた側の接続は、中継の上で開いた相手として扱わない（直接の接続を閉じない）
        assert!(!conns.has_relayed_stream_to(&peer));
        // 開いた接続が閉じたら知らせる
        conns.fail(opened.conn, CloseReason::Reset);
        assert_eq!(conns.io(opened.conn).err(), Some(NotOpen::Pending));
        assert_eq!(conns.take_closed(), vec![closed(opened.conn, CloseReason::Reset)]);
    }
}
