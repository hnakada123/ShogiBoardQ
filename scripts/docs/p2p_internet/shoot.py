#!/usr/bin/env python3
"""インターネット越し（ネットワーク「インターネット」）の P2P対局を、コンテナの中の仮想のネットワークで指して撮る。

コンテナの中で実行する（lab.sh up の後）:
    lab.sh exec python3 /repo/scripts/docs/p2p_internet/shoot.py <ja_JP|en|zh_CN|zh_TW>

/work/shots/<言語>/ に次の画像を書く（P2P対局のウィンドウ）:
    internet-no-node.png   協力ノードを入れずに「インターネット」で接続したところ（招待に宛先が入らない）
    internet-settings.png  「設定」タブの「接続候補」に協力ノードを入れたところ（アドレスは見本。IP の前半は XXX.XXX）
    internet-connected.png 協力ノードにつながり、招待の経路が中継になったところ
    internet-invite.png    受け取った招待文字列を確かめたところ（招待を受けた側）
    internet-relay.png     中継を通って指している「対局中」タブ（ルーターの NAT がホールパンチングを通さない）
    internet-direct.png    中継でつながった後、直接の接続に切り替わった「対局中」タブ
    internet-relay-only.png 「直接接続を使わない」で接続したところ
"""
import os
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

sys.path.insert(0, "/repo/scripts/docs")
from rpc import rpc as raw_rpc  # noqa: E402

INSIDE = "/repo/scripts/docs/p2p_internet/inside.sh"
WINDOW = "p2pWindow"
# 「設定」タブに写す協力ノードのアドレス（見本。IP アドレスの前半は伏せる。実際の接続には /work/node.addr を使う）
SAMPLE_NODE = "/ip4/XXX.XXX.113.10/tcp/4001/p2p/12D3KooWExampleCooperatingNodePeerIdForTheGuide"
TAB_CONNECTION, TAB_INVITE, TAB_GAME, TAB_SETTINGS = 0, 1, 2, 5
NAMES = {"ja_JP": ("鈴木", "山田"), "en": ("Suzuki", "Yamada"), "zh_CN": ("铃木", "山田"), "zh_TW": ("鈴木", "山田")}


# 協力ノードの中継の予約が取れていないときの「招待の経路」
NO_ROUTE = "外から届く接続先がまだありません。協力ノードを「設定」の「接続候補」に入れてください。"


def translated(language, source, context="P2pWindow"):
    """画面の文（日本語の原文）の訳を翻訳ファイルから引く"""
    if language == "ja_JP":
        return source
    tree = ET.parse(f"/repo/resources/translations/ShogiBoardQ_{language}.ts")
    for ctx in tree.iter("context"):
        if ctx.findtext("name") != context:
            continue
        for message in ctx.iter("message"):
            if message.findtext("source") == source:
                return message.findtext("translation") or source
    raise KeyError(source)


def route_text(language, direct):
    """「対局中」タブの経路の行"""
    return translated(language, "経路: %1").replace("%1", translated(language, "直接" if direct else "中継"))


def sock(name):
    return f"/work/run/{name}.sock"


def call(name, method, params=None):
    reply = raw_rpc(sock(name), method, params or {})
    if "error" in reply:
        raise RuntimeError(f"{name} {method} {params}: {reply['error']}")
    result = reply["result"]
    # UI の操作は応答の後に行われる。終わるまで待つ
    if isinstance(result, dict) and result.get("queued"):
        number = result["interaction"]
        for _ in range(200):
            state = raw_rpc(sock(name), "ui.interaction", {"id": number})["result"]
            if state.get("state") in ("done", "failed", "cancelled"):
                if state["state"] != "done":
                    raise RuntimeError(f"{name} {method} {params}: {state}")
                break
            time.sleep(0.05)
    return result


def text_of(name, widget, target=WINDOW):
    widgets = call(name, "widget.text", {"target": target, "widget": widget})["widgets"]
    return widgets[0].get("text", "") if widgets else ""


def wait_until(what, predicate, timeout=60):
    end = time.time() + timeout
    while time.time() < end:
        try:
            if predicate():
                return
        except RuntimeError:
            pass
        time.sleep(0.5)
    raise RuntimeError(f"timed out: {what}")


def window_open(name):
    return any(w["object_name"] == WINDOW for w in call(name, "dialog.list")["windows"])


def dialog_open(name, object_name):
    return any(w["object_name"] == object_name for w in call(name, "dialog.list")["windows"])


def set_value(name, widget, value, target=WINDOW):
    call(name, "widget.setValue", {"target": target, "widget": widget, "value": value})


def click(name, widget, target=WINDOW):
    call(name, "widget.click", {"target": target, "widget": widget})


def tab(name, index):
    set_value(name, "p2pTabs", index)


def shot(name, out_dir, file_name, target=WINDOW):
    path = call(name, "screenshot.capture", {"target": target, "output_dir": out_dir})["path"]
    os.replace(path, os.path.join(out_dir, file_name))
    print("  ", file_name)


def launch(name, language, side):
    subprocess.run([INSIDE, "player", name, language, side], check=True, stdout=subprocess.DEVNULL)
    call(name, "action.trigger", {"name": "actionP2pGame"})
    wait_until("P2P window", lambda: window_open(name))


def quit_app(name):
    if not os.path.exists(sock(name)):
        return
    try:
        raw_rpc(sock(name), "app.quit", {})
    except OSError:
        return
    wait_until("app quit", lambda: not os.path.exists(sock(name)) or not _alive(name), 20)


def _alive(name):
    try:
        raw_rpc(sock(name), "app.ping", {}, timeout=2)
        return True
    except OSError:
        return False


def connect(name, display_name, bootstrap, relay_only=False):
    """ネットワーク「インターネット」（sbq-limited-1）に接続する（初めてなら暗号化しないファイルでプレイヤー鍵を作る）"""
    tab(name, TAB_SETTINGS)
    set_value(name, "p2pBootstrap", bootstrap)
    set_value(name, "p2pRelayOnly", relay_only)
    tab(name, TAB_CONNECTION)
    set_value(name, "p2pNetwork", 1)
    set_value(name, "p2pDisplayName", display_name)
    click(name, "p2pConnect")
    time.sleep(1)
    if dialog_open(name, "p2pIdentityDialog"):
        set_value(name, "p2pIdentityPlainFile", True, target="p2pIdentityDialog")
        click(name, "p2pIdentityOk", target="p2pIdentityDialog")
    wait_until("connected", lambda: text_of(name, "p2pConnect") != call_text_connect(name), 30)


def call_text_connect(name):
    # 接続ボタンの文字（接続すると「切断」になる）は言語で違うので、最初に読んだものと比べる
    return CONNECT_TEXT[name]


CONNECT_TEXT = {}


def disconnect(name):
    click(name, "p2pConnect")
    wait_until("disconnected", lambda: text_of(name, "p2pConnect") == CONNECT_TEXT[name], 20)


def route(name):
    return text_of(name, "p2pRouteInfo")


def invite(host, guest, out_dir=None, invite_shot=None):
    """host が招待文字列を作り、guest が確かめて接続し、双方が開始を押す"""
    tab(host, TAB_INVITE)
    click(host, "p2pCreateInvite")
    wait_until("invite created", lambda: text_of(host, "p2pInviteOut").strip() != "")
    code = text_of(host, "p2pInviteOut").strip()
    tab(guest, TAB_INVITE)
    set_value(guest, "p2pInviteIn", code)
    click(guest, "p2pInspectInvite")
    wait_until("invite inspected", lambda: text_of(guest, "p2pInviteInfo").strip() != "")
    if invite_shot:
        shot(guest, out_dir, invite_shot)
    click(guest, "p2pConnectInvite")
    for name in (host, guest):
        wait_until("confirm dialog", lambda n=name: dialog_open(n, "p2pConfirmDialog"), 60)
    # 相手のプレイヤー鍵による確定がそろうまで待ってから開始を押す
    time.sleep(2)
    for name in (host, guest):
        click(name, "p2pConfirmStart", target="p2pConfirmDialog")
    for name in (host, guest):
        wait_until("game started", lambda n=name: call(n, "app.state")["play_mode"] != "not_started", 60)


def black_and_white(a, b):
    return (a, b) if call(a, "app.state").get("board_flipped") is False else (b, a)


# 先手・後手の順に指す手（筋・段）。▲7六歩 △3四歩 ▲2六歩 △8四歩
MOVES = [((7, 7), (7, 6)), ((3, 3), (3, 4)), ((2, 7), (2, 6)), ((8, 3), (8, 4))]


def play(black, white, count, first=0):
    for i in range(first, first + count):
        src, dst = MOVES[i]
        player = black if i % 2 == 0 else white
        # 対局が始まった直後は、盤の手を受け付けるまで少しかかる。指せるまで指し直す
        for _ in range(10):
            call(player, "board.click", {"file": src[0], "rank": src[1]})
            call(player, "board.click", {"file": dst[0], "rank": dst[1]})
            try:
                wait_until("move", lambda: call(player, "app.state")["current_ply"] > i, 3)
                break
            except RuntimeError:
                continue
        for name in (black, white):
            wait_until("move agreed", lambda n=name, ply=i + 1: call(n, "app.state")["current_ply"] >= ply, 30)


def route_line(name):
    """「対局中」タブの経路の行（3行目。相手・先後・経路の順）"""
    lines = text_of(name, "p2pGameStatus").splitlines()
    return lines[2] if len(lines) > 2 else ""


def restart(pairs, language):
    """アプリを終えて起動し直す（対局は通信中断で終わる。記録は作業フォルダの中だけ）"""
    for name, _ in pairs:
        quit_app(name)
    for name, side in pairs:
        launch(name, language, side)


class Lab:
    """1つの言語の撮影（プレイヤー A・B と、協力ノードの宛先）"""

    def __init__(self, language):
        self.language = language
        self.out_dir = f"/work/shots/{language}"
        os.makedirs(self.out_dir, exist_ok=True)
        with open("/work/node.addr", encoding="utf-8") as f:
            self.node = f.read().strip()
        tag = language.replace("_", "").lower()
        self.a, self.b = f"{tag}A", f"{tag}B"
        self.name_a, self.name_b = NAMES[language]
        self.no_node_route = translated(language, NO_ROUTE)

    def start(self, nat):
        for side in ("A", "B"):
            subprocess.run([INSIDE, "nat", side, nat], check=True)
        for name, side in ((self.a, "A"), (self.b, "B")):
            quit_app(name)
            launch(name, self.language, side)
            CONNECT_TEXT[name] = text_of(name, "p2pConnect")

    def connect_both(self, relay_only=False):
        connect(self.a, self.name_a, self.node, relay_only)
        connect(self.b, self.name_b, self.node, relay_only)
        for name in (self.a, self.b):
            wait_until("relay reserved", lambda n=name: route(n) not in ("", self.no_node_route), 90)

    def basic(self):
        """協力ノードなし・協力ノードあり・設定・招待・中継のまま指す（NAT がホールパンチングを通さない）"""
        a, b, out = self.a, self.b, self.out_dir
        self.start("strict")
        connect(a, self.name_a, "")
        wait_until("no route", lambda: route(a) != "", 30)
        time.sleep(3)
        shot(a, out, "internet-no-node.png")
        disconnect(a)
        self.connect_both()
        shot(a, out, "internet-connected.png")
        self.settings_shot()
        invite(a, b, out, "internet-invite.png")
        black, white = black_and_white(a, b)
        play(black, white, 4)
        tab(b, TAB_GAME)
        wait_until("relay route", lambda: route_line(b) == route_text(self.language, False), 30)
        time.sleep(2)
        shot(b, out, "internet-relay.png")

    def settings_shot(self):
        """接続した後の「設定」タブ（接続候補は見本のアドレスに書き換えて写し、元に戻す）"""
        a = self.a
        tab(a, TAB_SETTINGS)
        set_value(a, "p2pBootstrap", SAMPLE_NODE)
        shot(a, self.out_dir, "internet-settings.png")
        set_value(a, "p2pBootstrap", self.node)

    def connection(self):
        """対局をせずに撮れる画像（協力ノードなし・協力ノードあり・「設定」タブ）だけを撮り直す"""
        a, out = self.a, self.out_dir
        self.start("strict")
        connect(a, self.name_a, "")
        wait_until("no route", lambda: route(a) != "", 30)
        time.sleep(3)
        shot(a, out, "internet-no-node.png")
        disconnect(a)
        self.connect_both()
        shot(a, out, "internet-connected.png")
        self.settings_shot()

    def settings(self):
        """「設定」タブの画像だけを撮り直す"""
        self.start("easy")
        connect(self.a, self.name_a, self.node)
        wait_until("relay reserved", lambda: route(self.a) not in ("", self.no_node_route), 90)
        self.settings_shot()

    def direct(self):
        """ルーターの NAT がホールパンチングを通すと、中継でつながった後に直接の接続へ切り替わる"""
        a, b = self.a, self.b
        self.start("easy")
        self.connect_both()
        invite(a, b)
        black, white = black_and_white(a, b)
        play(black, white, 2)
        tab(b, TAB_GAME)
        # ホールパンチングは招待の接続の直後に済むので、対局が始まったときには直接になっていることが多い
        wait_until("direct route", lambda: route_line(b) == route_text(self.language, True), 120)
        time.sleep(1)
        shot(b, self.out_dir, "internet-direct.png")

    def relay_only(self):
        """「直接接続を使わない」"""
        a = self.a
        self.start("easy")
        connect(a, self.name_a, self.node, relay_only=True)
        wait_until("relay only route", lambda: route(a) not in ("", self.no_node_route), 90)
        time.sleep(1)
        shot(a, self.out_dir, "internet-relay-only.png")

    def finish(self):
        for name in (self.a, self.b):
            quit_app(name)


def main():
    lab = Lab(sys.argv[1])
    steps = sys.argv[2:] or ["basic", "direct", "relay_only"]
    for step in steps:
        print(step)
        getattr(lab, step)()
    lab.finish()


if __name__ == "__main__":
    main()
