#!/usr/bin/env python3
"""同じ PC の2つのアプリで P2P対局（ネットワーク「ローカル」）を操作して、ガイドの画像を撮る。

usage:
    shoot_p2p.py guide <ja_JP|en|zh_CN|zh_TW> <出力フォルダ>
        p2p-identity.png  p2p-connection.png  p2p-settings.png  p2p-invite.png
        p2p-confirm.png   p2p-records.png     p2p-confirm-engine.png
    shoot_p2p.py lan <言語> <出力フォルダ> <協力ノードの multiaddr>
        p2p-lan-node-connected.png  p2p-internet-lan-address.png
        （協力ノードは先に起動しておく:
          env -i build-p2p-docs/shogiboardq-peer --service --relay --meet --network sbq-local-1 \\
              --identity <作業フォルダ>/node.key --listen /ip4/0.0.0.0/tcp/<空きポート>
          multiaddr は /ip4/<この PC の LAN の IP>/tcp/<ポート>/p2p/<起動時に出る PeerId>）

アプリは撮影用のビルド（cmake -DENABLE_P2P_DOCS_AUTOMATION=ON。既定 build-p2p-docs。SBQ_APP で変えられる）を
launch.sh で起動する（Xvfb のディスプレイは SBQ_DISPLAY、既定 97。作業フォルダは SBQ_WORK、既定 /tmp/sbq-shots-<uid>）。
登録エンジンは launch.sh の SBQ_ENGINES=1 の Hayanagi（build/Hayanagi/hayanagi）。インターネット越しの画像は
p2p_internet/shoot.py で撮る。終わったら、起動した Xvfb を PID で止める（README の「後片付け」）。
"""
import os
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rpc import rpc as raw_rpc  # noqa: E402

APP = os.environ.get("SBQ_APP", os.path.join(REPO, "build-p2p-docs", "ShogiBoardQ"))
WORK = os.environ.get("SBQ_WORK", f"/tmp/sbq-shots-{os.getuid()}")
DISPLAY = os.environ.get("SBQ_DISPLAY", "97")
WINDOW = "p2pWindow"
CONFIRM = "p2pConfirmDialog"
NAMES = {"ja_JP": ("鈴木", "山田"), "en": ("Suzuki", "Yamada"), "zh_CN": ("铃木", "山田"), "zh_TW": ("鈴木", "山田")}
TAB_CONNECTION, TAB_INVITE, TAB_GAME, TAB_RECORDS, TAB_SETTINGS = 0, 1, 2, 3, 5
# 合言葉の見本（protocol/testvectors/code.txt の1行目。192.168.1.23 の PC の合言葉）
SAMPLE_CODE = "008Q-4HMA-SW95"
PIDS = {}


def call(name, method, params=None, wait=True):
    reply = raw_rpc(name, method, params or {})
    if "error" in reply:
        raise RuntimeError(f"{name} {method} {params}: {reply['error']}")
    result = reply["result"]
    if wait and isinstance(result, dict) and result.get("queued"):
        number = result["interaction"]
        for _ in range(400):
            state = raw_rpc(name, "ui.interaction", {"id": number})["result"]
            if state.get("state") in ("done", "failed", "cancelled"):
                if state["state"] != "done":
                    raise RuntimeError(f"{name} {method} {params}: {state}")
                break
            time.sleep(0.05)
    return result


def launch(name, language):
    # P2P対局のウィンドウは、ガイドの画像と同じ 900×790 にする
    env = dict(os.environ, SBQ_APP=APP, SBQ_WORK=WORK, SBQ_FRESH="1", SBQ_ENGINES="1",
               SBQ_EXTRA_INI="[SizeRelated]\np2pWindowSize=@Size(900 790)")
    out = subprocess.run([os.path.join(REPO, "scripts", "docs", "launch.sh"), name, language, DISPLAY],
                         env=env, capture_output=True, text=True, check=True).stdout
    PIDS[name] = int(out.split("pid")[1].split()[0])
    call(name, "action.trigger", {"name": "actionP2pGame"})
    wait_until("P2P window", lambda: window_open(name, WINDOW))


def quit_all():
    for name in list(PIDS):
        try:
            raw_rpc(name, "app.quit", {})
        except OSError:
            pass
    time.sleep(2)
    for name, pid in PIDS.items():
        try:
            os.kill(pid, 15)
        except OSError:
            pass


def windows(name):
    return call(name, "dialog.list")["windows"]


def window_open(name, object_name):
    return any(w["object_name"] == object_name for w in windows(name))


def text_of(name, widget, target=WINDOW):
    widgets = call(name, "widget.text", {"target": target, "widget": widget})["widgets"]
    return widgets[0].get("text", "") if widgets else ""


def set_value(name, widget, value, target=WINDOW):
    call(name, "widget.setValue", {"target": target, "widget": widget, "value": value})


def click(name, widget, target=WINDOW):
    call(name, "widget.click", {"target": target, "widget": widget})


def wait_until(what, predicate, timeout=60, step=0.3):
    end = time.time() + timeout
    while time.time() < end:
        try:
            if predicate():
                return
        except RuntimeError:
            pass
        time.sleep(step)
    raise RuntimeError(f"timed out: {what}")


def close_boxes(name):
    """名前のない知らせの箱（QMessageBox）を閉じる"""
    for selector in [w.get("selector") for w in windows(name) if w.get("class") == "QMessageBox"]:
        try:
            call(name, "dialog.close", {"target": selector})
        except RuntimeError as error:
            print("   close:", error)


def translated(language, source, context):
    if language == "ja_JP":
        return source
    tree = ET.parse(os.path.join(REPO, "resources", "translations", f"ShogiBoardQ_{language}.ts"))
    for ctx in tree.iter("context"):
        if ctx.findtext("name") != context:
            continue
        for message in ctx.iter("message"):
            if message.findtext("source") == source:
                return message.findtext("translation") or source
    raise KeyError(source)


def shot(name, out_dir, file_name, target=WINDOW):
    path = call(name, "screenshot.capture", {"target": target, "output_dir": out_dir})["path"]
    os.replace(path, os.path.join(out_dir, file_name))
    print("  ", file_name)


def prepare(name, display_name, side):
    set_value(name, "p2pTabs", TAB_CONNECTION)
    set_value(name, "p2pNetwork", 0)
    set_value(name, "p2pDisplayName", display_name)
    set_value(name, "p2pMainMinutes", 10)
    set_value(name, "p2pByoyomi", 30)
    set_value(name, "p2pSide", side)
    set_value(name, "p2pPlayerKind", 0)


def connect_first_time(name, passphrase, out_dir=None):
    """初めて接続する（プレイヤー鍵を作る画面を撮ってから、保管の方法を選ぶ）"""
    before = text_of(name, "p2pConnect")
    click(name, "p2pConnect")
    wait_until("identity dialog", lambda: window_open(name, "p2pIdentityDialog"), 20)
    time.sleep(0.5)
    if out_dir:
        shot(name, out_dir, "p2p-identity.png", "p2pIdentityDialog")
    if passphrase:
        set_value(name, "p2pIdentityPassphraseFile", True, target="p2pIdentityDialog")
        set_value(name, "p2pPassphrase", "guide passphrase", target="p2pIdentityDialog")
        set_value(name, "p2pPassphraseConfirm", "guide passphrase", target="p2pIdentityDialog")
    else:
        set_value(name, "p2pIdentityPlainFile", True, target="p2pIdentityDialog")
    click(name, "p2pIdentityOk", target="p2pIdentityDialog")
    wait_until("connected", lambda: text_of(name, "p2pConnect") != before and text_of(name, "p2pRouteInfo") != "", 60)


def create_invite(name):
    """招待を作り、招待文字列を返す（作り直しのときは、前の招待文字列と違うものが出るまで待つ）"""
    set_value(name, "p2pTabs", TAB_INVITE)
    before = text_of(name, "p2pInviteOut").strip()
    click(name, "p2pCreateInvite")
    wait_until("invite created", lambda: text_of(name, "p2pInviteOut").strip() not in ("", before))
    return text_of(name, "p2pInviteOut").strip()


def invite_code(name):
    """作った招待の合言葉（ローカルのネットワークで、直接つなぐ招待にだけ付く）"""
    return text_of(name, "p2pInviteCode").strip()


def inspect(name, code, lines=3):
    """招待文字列（確かめた結果は3行）か合言葉（2行）を入れて確かめる"""
    set_value(name, "p2pTabs", TAB_INVITE)
    set_value(name, "p2pInviteIn", code)
    click(name, "p2pInspectInvite")
    wait_until("invite inspected", lambda: len(text_of(name, "p2pInviteInfo").splitlines()) >= lines)


def play(black, white, moves):
    for i, (src, dst) in enumerate(moves):
        player = black if i % 2 == 0 else white
        for _ in range(10):
            call(player, "board.click", {"file": src[0], "rank": src[1]})
            call(player, "board.click", {"file": dst[0], "rank": dst[1]})
            try:
                wait_until("move", lambda p=i: call(player, "app.state")["current_ply"] > p, 3)
                break
            except RuntimeError:
                continue
        for name in (black, white):
            wait_until("move agreed", lambda n=name, p=i + 1: call(n, "app.state")["current_ply"] >= p, 30)


def guide(language, out_dir):
    """ローカルのネットワークで、準備・接続・招待・条件確認・対局記録を撮る"""
    os.makedirs(out_dir, exist_ok=True)
    tag = language.replace("_", "").lower()
    a, b = f"{tag}A", f"{tag}B"
    name_a, name_b = NAMES[language]
    launch(a, language)
    launch(b, language)

    # 準備の画面・接続したところ・設定
    prepare(a, name_a, 0)
    connect_first_time(a, passphrase=True, out_dir=out_dir)
    time.sleep(1)
    shot(a, out_dir, "p2p-connection.png")
    set_value(a, "p2pTabs", TAB_SETTINGS)
    shot(a, out_dir, "p2p-settings.png")

    # 相手（後手を希望して、鈴木が先手になる）
    prepare(b, name_b, 2)
    connect_first_time(b, passphrase=False)

    # 招待: 自分の招待を作り（合言葉と招待文字列）、受け取った合言葉を確かめたところ。先に起動した a は決まった
    # ポートで待ち受けるので、合言葉は12文字になる。同じ PC の2つ目のアプリ（b）の合言葉はポートを含んで長くなるので、
    # 画像の下の欄には、別の PC の合言葉の見本（12文字。確かめるだけで、つながない）を入れる
    create_invite(a)
    create_invite(b)
    try:
        inspect(a, SAMPLE_CODE, lines=2)
    except RuntimeError:
        inspect(a, invite_code(b), lines=2)   # 見本のアドレスがこの PC のものだった
    time.sleep(0.5)
    shot(a, out_dir, "p2p-invite.png")

    # 条件確認画面（相手を確かめる前）。相手の合言葉を、小文字・空白区切りで入力して接続する
    inspect(a, invite_code(b).lower().replace("-", " "), lines=2)
    click(a, "p2pConnectInvite")
    for name in (a, b):
        wait_until("confirm dialog", lambda n=name: window_open(n, CONFIRM), 60)
    time.sleep(1.5)
    shot(a, out_dir, "p2p-confirm.png", CONFIRM)
    for name in (b, a):
        click(name, "p2pConfirmStart", target=CONFIRM)
    for name in (a, b):
        wait_until("game started", lambda n=name: call(n, "app.state")["play_mode"] != "not_started", 60)

    # 5手指して後手が投了する（先手の勝ち（投了））
    play(a, b, [((7, 7), (7, 6)), ((3, 3), (3, 4)), ((2, 7), (2, 6)), ((8, 3), (8, 4)), ((2, 6), (2, 5))])
    call(b, "action.trigger", {"name": "actionResign"}, wait=False)
    settled = translated(language, "結果が一致し、双方で確定しました。", "P2pWindowText")
    wait_until("finished", lambda: settled in text_of(a, "p2pGameStatus"), 60)
    time.sleep(2)
    for name in (a, b):
        close_boxes(name)
    set_value(a, "p2pTabs", TAB_RECORDS)
    time.sleep(0.5)
    call(a, "widget.clickCell", {"target": WINDOW, "widget": "p2pRecordTable", "row": 0, "column": 0})
    time.sleep(0.5)
    shot(a, out_dir, "p2p-records.png")

    # 相手がエンジンに指させるときの条件確認画面
    set_value(b, "p2pTabs", TAB_CONNECTION)
    set_value(b, "p2pPlayerKind", 1)
    code = create_invite(b)
    inspect(a, code)
    click(a, "p2pConnectInvite")
    for name in (a, b):
        wait_until("confirm dialog", lambda n=name: window_open(n, CONFIRM), 60)
    time.sleep(1.5)
    shot(a, out_dir, "p2p-confirm-engine.png", CONFIRM)
    click(a, "p2pConfirmCancel", target=CONFIRM)
    time.sleep(1.5)
    for name in (a, b):
        close_boxes(name)


def connect_with_node(name, display_name, network, bootstrap):
    """接続候補に協力ノードを入れて接続する（network は 0=ローカル、1=インターネット）。接続する前のボタンの文字を返す"""
    set_value(name, "p2pTabs", TAB_SETTINGS)
    set_value(name, "p2pBootstrap", bootstrap)
    prepare(name, display_name, 0)
    set_value(name, "p2pNetwork", network)
    before = text_of(name, "p2pConnect")
    click(name, "p2pConnect")
    end = time.time() + 40
    while time.time() < end and text_of(name, "p2pConnect") == before:
        if window_open(name, "p2pIdentityDialog"):
            set_value(name, "p2pIdentityPlainFile", True, target="p2pIdentityDialog")
            click(name, "p2pIdentityOk", target="p2pIdentityDialog")
            time.sleep(1)
        time.sleep(0.3)
    wait_until("connected", lambda: text_of(name, "p2pRouteInfo") != "", 30)
    return before


def disconnect(name, connect_text):
    set_value(name, "p2pTabs", TAB_CONNECTION)
    click(name, "p2pConnect")
    wait_until("disconnected", lambda: text_of(name, "p2pConnect") == connect_text, 20)


def lan(language, out_dir, node):
    """同じ PC の協力ノードを使う画像を撮る"""
    os.makedirs(out_dir, exist_ok=True)
    tag = language.replace("_", "").lower()
    a, b = f"{tag}A", f"{tag}B"
    name_a, name_b = NAMES[language]
    launch(a, language)
    launch(b, language)

    # 「ローカル」で、同じ LAN の協力ノードにつないだところ（中継の予約が取れる）
    relay = translated(language, "同じPC・同じLANの相手と直接つなぎます（協力ノードの中継も使えます）。", "P2pWindow")
    connect_text = connect_with_node(a, name_a, 0, node)
    wait_until("relay reserved", lambda: text_of(a, "p2pRouteInfo") == relay, 90)
    time.sleep(1)
    shot(a, out_dir, "p2p-lan-node-connected.png")
    disconnect(a, connect_text)

    # 「インターネット」では、LAN のアドレスの協力ノードは使われず、招待文字列に接続先が入らない
    connect_with_node(a, name_a, 1, node)
    connect_with_node(b, name_b, 1, node)
    time.sleep(3)
    code = create_invite(b)
    inspect(a, code)
    time.sleep(0.5)
    shot(a, out_dir, "p2p-internet-lan-address.png")


if __name__ == "__main__":
    if len(sys.argv) < 4 or sys.argv[1] not in ("guide", "lan") or sys.argv[2] not in NAMES:
        sys.exit(__doc__)
    try:
        if sys.argv[1] == "guide":
            guide(sys.argv[2], os.path.abspath(sys.argv[3]))
        elif len(sys.argv) < 5:
            sys.exit(__doc__)
        else:
            lan(sys.argv[2], os.path.abspath(sys.argv[3]), sys.argv[4])
    finally:
        quit_all()
