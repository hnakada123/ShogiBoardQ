#!/usr/bin/env python3
"""P2P対局のウィンドウの「接続」タブを、自動化 API から操作する補助（shoot_p2p.py と p2p_internet/shoot.py で共通）。

どの関数も、最初の引数に撮影スクリプトの call(name, method, params=None, wait=True) を受け取る。

- 相手のいる場所とつなぎ方（ラジオボタン）: choose
- 協力ノードの一覧: set_nodes（「追加…」の入力のダイアログに書いて足す。文字を直接入れられる欄は無い）
- 実際の協力ノードのアドレスを画像に残さないための差し替え: node_sample と paste_node_sample
  （接続している間は一覧を変えられないので、接続する前に見本のアドレスで撮った一覧の画像を、
  接続した後の画像の同じ場所に貼る）
"""
import os
import time

from PIL import Image

WINDOW = "p2pWindow"
TAB_CONNECTION = 0


def _widget(call, name, widget):
    widgets = call(name, "widget.text", {"target": WINDOW, "widget": widget})["widgets"]
    return widgets[0] if widgets else {}


def _visible(call, name, widget):
    return _widget(call, name, widget).get("visible", True)


def show_tab(call, name):
    call(name, "widget.setValue", {"target": WINDOW, "widget": "p2pTabs", "value": TAB_CONNECTION})


def choose(call, name, lan, relay_only=False):
    """相手のいる場所（同じPC・同じLAN か インターネット）と、つなぎ方（直接か、協力ノードの中継だけか）を選ぶ"""
    show_tab(call, name)
    call(name, "widget.click", {"target": WINDOW, "widget": "p2pPlaceLan" if lan else "p2pPlaceInternet"})
    call(name, "widget.click", {"target": WINDOW, "widget": "p2pRouteRelay" if relay_only else "p2pRouteDirect"})


def nodes(call, name):
    """登録してある協力ノードのアドレス"""
    return list(_widget(call, name, "p2pBootstrap").get("items", []))


def _open_section(call, name):
    # 協力ノードの要らない設定（同じPC・同じLAN で直接つなぐ）では、欄は「登録する…」で開く
    show_tab(call, name)
    if not _visible(call, name, "p2pNodeAdd"):
        call(name, "widget.click", {"target": WINDOW, "widget": "p2pNodeShow"})


def _input_dialog(call, name):
    for window in call(name, "dialog.list")["windows"]:
        if window.get("class") == "QInputDialog":
            return window["selector"]
    return None


def add_node(call, name, address):
    _open_section(call, name)
    before = len(nodes(call, name))
    # 「追加…」は入力のダイアログを開いて、閉じるまで戻らない。操作の完了を待たずに進める
    call(name, "widget.click", {"target": WINDOW, "widget": "p2pNodeAdd"}, wait=False)
    dialog = None
    for _ in range(100):
        dialog = _input_dialog(call, name)
        if dialog:
            break
        time.sleep(0.1)
    if not dialog:
        raise RuntimeError(f"{name}: the dialog for a cooperating node did not open")
    widgets = call(name, "widget.text", {"target": dialog})["widgets"]
    edit = next(w["selector"] for w in widgets if w.get("class") == "QLineEdit")
    call(name, "widget.setValue", {"target": dialog, "widget": edit, "value": address, "submit": True})
    for _ in range(100):
        if not _input_dialog(call, name) and len(nodes(call, name)) > before:
            return
        time.sleep(0.1)
    raise RuntimeError(f"{name}: the cooperating node was not added: {address}")


def clear_nodes(call, name):
    if not nodes(call, name):
        return
    _open_section(call, name)
    while nodes(call, name):
        call(name, "widget.setValue", {"target": WINDOW, "widget": "p2pBootstrap", "value": 0})
        call(name, "widget.click", {"target": WINDOW, "widget": "p2pNodeRemove"})


def set_nodes(call, name, addresses):
    """協力ノードの一覧を addresses にする（接続していない間だけ変えられる）"""
    wanted = [a for a in addresses if a]
    if nodes(call, name) == wanted:
        return
    clear_nodes(call, name)
    for address in wanted:
        add_node(call, name, address)


def _list_box(call, name):
    g = _widget(call, name, "p2pBootstrap")["geometry"]
    return (g["x"], g["y"], g["x"] + g["width"], g["y"] + g["height"])


def node_sample(call, name, out_dir, sample_address):
    """見本のアドレスだけを入れた協力ノードの一覧の画像を返す（接続していない間に呼ぶ。一覧は空に戻す）"""
    set_nodes(call, name, [sample_address])
    path = call(name, "screenshot.capture", {"target": WINDOW, "output_dir": out_dir})["path"]
    with Image.open(path) as shot:
        sample = shot.crop(_list_box(call, name)).copy()
    os.remove(path)
    clear_nodes(call, name)
    return sample


def paste_node_sample(call, name, image_path, sample):
    """撮った画像の協力ノードの一覧を、見本の一覧の画像に差し替える（実際のアドレスを画像に残さない）"""
    box = _list_box(call, name)
    size = (box[2] - box[0], box[3] - box[1])
    if sample.size != size:
        raise RuntimeError(f"{name}: the node list changed its size: {sample.size} -> {size}")
    with Image.open(image_path) as shot:
        image = shot.copy()
    image.paste(sample, (box[0], box[1]))
    image.save(image_path)
