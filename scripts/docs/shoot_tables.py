#!/usr/bin/env python3
"""Take the guide screenshots whose subject is the Thinking or Consideration table.

usage: shoot_tables.py <ja_JP|en|zh_CN|zh_TW> <scenario> <output dir>

scenarios (window ja / zh 1300x980, en 1400x1000 unless noted):
  main   consideration/consideration-01-initial.png, -02-running.png, -03-thinking-tab.png,
         -04-different-move.png and dock/dock-01-default.png, with the sample record (sample_kifu/sample-<lang>.kif)
  tsumi  tsumi-search/tsumi-search-thinking.png (the Tsume Initial Setup position, from a fresh start)
  wide   screenshot-main.png (all languages 1400x1000), with sample_kifu/main-<lang>.kif
  game   game-play/game-03-playing-thinking-tab.png (Human vs YaneuraOu; the human moves come from humanplay.py)
  info   game-play/game-06-game-info.png (Human vs YaneuraOu, 5 minutes + 10 seconds; the human resigns after
         20 moves and the Game Info tab is shown)
  lang   multilanguage/main-screen.png (every language shows the Japanese record sample_kifu/sample-ja.kif)

Images are written to <output dir>/<lang dir>/<path>, laid out like docs/images (ja at the top, the others
under en/, zh-cn/ and zh-tw/), so docs/images can be given directly. The app is started with launch.sh, so
SBQ_WORK and SBQ_SOCKET_DIR are honoured. Engines: YaneuraOu at $SBQ_YANEURAOU (default
~/shogi/YaneuraOu/YaneuraOu-by-gcc) registered as "YaneuraOu NNUE 9.10git 64ZEN2", and the bundled Hayanagi
($SBQ_HAYANAGI, default build/Hayanagi/hayanagi) registered as "Hayanagi 1.5.1", as in the screenshots.
"""
import json
import os
import shutil
import socket
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
KIFU = os.path.join(HERE, "sample_kifu")
LANG, SCENARIO, OUTPUT = sys.argv[1], sys.argv[2], os.path.abspath(sys.argv[3])
KIF = {"ja_JP": "ja", "en": "en", "zh_CN": "zh-cn", "zh_TW": "zh-tw"}[LANG]
LANG_DIR = "" if KIF == "ja" else KIF
DISPLAY = {"ja_JP": 95, "en": 94, "zh_CN": 93, "zh_TW": 92}[LANG]
NAME = "tbl" + KIF.replace("-", "")
WORK = os.environ.get("SBQ_WORK", f"/tmp/sbq-shots-{os.getuid()}")
SOCKDIR = os.environ.get("SBQ_SOCKET_DIR", f"/tmp/sbq-{os.getuid()}")
YANEURAOU = "YaneuraOu NNUE 9.10git 64ZEN2"
HAYANAGI = "Hayanagi 1.5.1"
YANEURAOU_PATH = os.environ.get("SBQ_YANEURAOU", os.path.expanduser("~/shogi/YaneuraOu/YaneuraOu-by-gcc"))
HAYANAGI_PATH = os.environ.get("SBQ_HAYANAGI", os.path.join(REPO, "build", "Hayanagi", "hayanagi"))
ENGINES_INI = ("[Engines]\n"
               f"1\\author=yaneurao\n1\\name={YANEURAOU}\n1\\path={YANEURAOU_PATH}\n"
               f"2\\author=hnakada123\n2\\name={HAYANAGI}\n2\\path={HAYANAGI_PATH}\n"
               "size=2")
SEARCH_START = {"ja_JP": "探索開始", "en": "Start search", "zh_CN": "开始搜索", "zh_TW": "開始搜尋"}[LANG]
GAME_START = {"ja_JP": "対局開始", "en": "Start Game", "zh_CN": "开始对局", "zh_TW": "開始對局"}[LANG]


def rpc(method, params=None, timeout=120):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(timeout)
    s.connect(os.path.join(SOCKDIR, NAME + ".sock"))
    s.sendall((json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}}) + "\n").encode())
    buf = b""
    while not buf.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        buf += chunk
    s.close()
    reply = json.loads(buf)
    if "error" in reply:
        raise RuntimeError(f"{method}: {reply['error']}")
    return reply["result"]


def launch(width, height):
    env = dict(os.environ, SBQ_WORK=WORK, SBQ_SOCKET_DIR=SOCKDIR, SBQ_FRESH="1", SBQ_EXTRA_INI=ENGINES_INI)
    env.pop("SBQ_ENGINES", None)
    subprocess.run(["bash", os.path.join(HERE, "launch.sh"), NAME, LANG, str(DISPLAY), str(width), str(height)],
                   env=env, check=True, stdout=subprocess.DEVNULL)
    time.sleep(3)
    rpc("app.state")


def quit_app():
    try:
        rpc("app.quit")
    except (OSError, RuntimeError):
        pass
    time.sleep(2)


def capture(rel):
    r = rpc("screenshot.capture", {"output_dir": os.path.join(WORK, "captures")})
    dest = os.path.join(OUTPUT, LANG_DIR, rel)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    shutil.move(r["path"], dest)
    print(f"{os.path.join(LANG_DIR, rel)}: {r['width']}x{r['height']}", flush=True)


def widget(name):
    return rpc("widget.text", {"widget": name})["widgets"][0]


def set_checked(name, checked):
    if widget(name).get("checked") != checked:
        rpc("widget.click", {"widget": name})
        time.sleep(0.3)


def wait_until(pred, timeout=60, step=0.05):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return
        time.sleep(step)
    raise TimeoutError("condition not met")


def elapsed():
    return widget("considerationElapsed")["text"]


def setup_consideration(engine, timed, seconds=20):
    rpc("widget.setValue", {"widget": "considerationEngine", "value": engine})
    time.sleep(0.3)
    rpc("widget.click", {"widget": "considerationTimed" if timed else "considerationUnlimited"})
    time.sleep(0.3)
    if timed:
        rpc("widget.setValue", {"widget": "considerationSeconds", "value": seconds})
    rpc("widget.setValue", {"widget": "considerationMultiPV", "value": 2})  # 候補手の数 3
    time.sleep(0.3)
    set_checked("considerationArrows", True)


def start_and_wait(target):
    """検討を始め、経過時間が target（例 '0:07'）になった直後に戻る。"""
    rpc("widget.click", {"widget": "considerationStartStop"})
    wait_until(lambda: elapsed().endswith(target), timeout=90)
    time.sleep(0.15)


def goto_and_wait(ply, target):
    """検討中に別の局面を選ぶ。経過時間が数え直されて target になった直後に戻る。"""
    rpc("kifu.goto", {"ply": ply})
    wait_until(lambda: elapsed().endswith(("0:00", "0:01")), timeout=30)
    wait_until(lambda: elapsed().endswith(target), timeout=90)
    time.sleep(0.15)


def stop_consideration():
    rpc("widget.click", {"widget": "considerationStartStop"})
    time.sleep(1.5)


def message_boxes():
    return [w for w in rpc("dialog.list").get("windows", []) if w.get("class") == "QMessageBox"]


def close_message_boxes():
    for box in message_boxes():
        rpc("dialog.close", {"target": box["selector"]})
        time.sleep(0.5)


def require_yaneuraou():
    if not os.access(YANEURAOU_PATH, os.X_OK):
        sys.exit(f"YaneuraOu not found at {YANEURAOU_PATH} (set SBQ_YANEURAOU)")


def scenario_main():
    require_yaneuraou()
    launch(*((1400, 1000) if LANG == "en" else (1300, 980)))
    rpc("kifu.load", {"path": os.path.join(KIFU, f"sample-{KIF}.kif")})
    time.sleep(1)
    # 検討ページ: 44手目の局面をやねうら王で検討する（検討時間20秒、候補手3、矢印表示）。
    # 棋譜欄は上から選ぶので、選んだ手が見える範囲の一番下に来る（画像と同じ並び）
    rpc("kifu.goto", {"ply": 44})
    rpc("widget.showDock", {"widget": "ConsiderationDock"})
    time.sleep(0.5)
    setup_consideration(YANEURAOU, timed=True)
    time.sleep(1)
    capture("consideration/consideration-01-initial.png")
    start_and_wait("0:07")
    capture("consideration/consideration-02-running.png")
    # 同じ検討の思考タブ（検討中の矢印表示は検討タブで設定するので、思考タブのチェックは無効になる）
    rpc("widget.showDock", {"widget": "ThinkingDock"})
    wait_until(lambda: elapsed().endswith("0:10"), timeout=30)
    time.sleep(0.15)
    capture("consideration/consideration-03-thinking-tab.png")
    rpc("widget.showDock", {"widget": "ConsiderationDock"})
    time.sleep(0.5)
    goto_and_wait(48, "0:07")
    capture("consideration/consideration-04-different-move.png")
    stop_consideration()
    # ドックの既定配置: 49手目の局面を Hayanagi で検討する（時間無制限）
    setup_consideration(HAYANAGI, timed=False)
    rpc("kifu.goto", {"ply": 49})
    time.sleep(0.5)
    start_and_wait("0:06")
    capture("dock/dock-01-default.png")
    stop_consideration()
    quit_app()


def scenario_tsumi():
    launch(*((1400, 1000) if LANG == "en" else (1300, 980)))
    # 「詰将棋初期配置」は局面編集中だけ使える
    for action in ("actionStartEditPosition", "actionSetTsumePosition", "actionEndEditPosition"):
        rpc("action.trigger", {"name": action})
        time.sleep(1.5)
        close_message_boxes()
    rpc("action.trigger", {"name": "actionTsumeShogiSearch"})
    time.sleep(1.5)
    dialog = next(w for w in rpc("dialog.list")["windows"] if w.get("object_name") == "TsumeShogiSearchDialog")
    rpc("widget.setValue", {"target": dialog["selector"], "widget": "comboBoxEngine1", "value": HAYANAGI})
    time.sleep(0.3)
    rpc("dialog.clickButton", {"target": dialog["selector"], "text": SEARCH_START})
    wait_until(lambda: bool(message_boxes()), timeout=60)  # 結果のメッセージ
    time.sleep(0.5)
    close_message_boxes()
    rpc("widget.showDock", {"widget": "ThinkingDock"})
    set_checked("matchArrows", False)
    time.sleep(1)
    capture("tsumi-search/tsumi-search-thinking.png")
    quit_app()


def scenario_wide():
    launch(1400, 1000)
    rpc("kifu.load", {"path": os.path.join(KIFU, f"main-{KIF}.kif")})
    time.sleep(1)
    rpc("kifu.goto", {"ply": 46})
    rpc("widget.showDock", {"widget": "ConsiderationDock"})
    time.sleep(0.5)
    setup_consideration(HAYANAGI, timed=False)
    time.sleep(0.5)
    start_and_wait("0:13")
    capture("screenshot-main.png")
    stop_consideration()
    quit_app()


def scenario_lang():
    launch(*((1400, 1000) if LANG == "en" else (1300, 980)))
    # 多言語対応のページ: どの言語の画面でも日本語の棋譜を読み込み、31手目を Hayanagi で検討する（時間無制限）
    rpc("kifu.load", {"path": os.path.join(KIFU, "sample-ja.kif")})
    time.sleep(1)
    rpc("kifu.goto", {"ply": 31})
    rpc("widget.showDock", {"widget": "ConsiderationDock"})
    time.sleep(0.5)
    setup_consideration(HAYANAGI, timed=False)
    time.sleep(0.5)
    start_and_wait("0:07")
    capture("multilanguage/main-screen.png")
    stop_consideration()
    quit_app()


_selectors = {}


def engine_thinking_deep(min_depth=18, min_rows=5):
    """後手のやねうら王が min_depth 以上まで読み、思考表が min_rows 行以上あるか。"""
    state = rpc("app.state")
    if state.get("ui_state") != "game" or state["sfen"].split()[1] != "w":
        return False
    if not _selectors:
        # エンジン情報の表は同じ名前のものが複数あるので、selector で見る
        widgets = rpc("widget.text")["widgets"]
        _selectors["info"] = [w["selector"] for w in widgets if w.get("object_name") == "engineInfoTable"]
        _selectors["view"] = next(w["selector"] for w in widgets if w.get("object_name") == "thinkingView1")
    for selector in _selectors["info"]:
        rows = widget(selector).get("rows") or []
        if len(rows) > 1 and YANEURAOU in str(rows[1][0]):
            depth = str(rows[1][3]).split("/")[0]
            if depth.isdigit() and int(depth) >= min_depth:
                return widget(_selectors["view"]).get("row_count", 0) >= min_rows
    return False


def scenario_game():
    require_yaneuraou()
    launch(*((1400, 1000) if LANG == "en" else (1300, 980)))
    rpc("widget.showDock", {"widget": "ThinkingDock"})
    time.sleep(0.5)
    set_checked("matchArrows", True)
    set_checked("ponderArrows", False)
    rpc("action.trigger", {"name": "actionStartGame"})
    time.sleep(1.5)
    # 先手が人間、後手がやねうら王、持ち時間5分（後手の時間の欄は同じ設定のとき隠れている）
    for key, value in [("comboBoxPlayer1", 0), ("comboBoxPlayer2", 1), ("basicTimeHour1", 0),
                       ("basicTimeMinutes1", 5), ("byoyomiSec1", 0)]:
        rpc("widget.setValue", {"target": "StartGameDialog", "widget": key, "value": value})
    rpc("dialog.clickButton", {"target": "StartGameDialog", "text": GAME_START})
    time.sleep(2)
    env = dict(os.environ, SBQ_HAYANAGI=HAYANAGI_PATH)
    for human_move in range(1, 30):
        subprocess.run(["python3", os.path.join(HERE, "humanplay.py"), os.path.join(SOCKDIR, NAME + ".sock"), "1",
                        "600"], env=env, check=True, stdout=subprocess.DEVNULL)
        if human_move < 7:
            continue
        # 定跡ですぐ指した手のときは、次の手で待つ
        try:
            wait_until(engine_thinking_deep, timeout=20, step=0.1)
        except TimeoutError:
            continue
        capture("game-play/game-03-playing-thinking-tab.png")
        break
    quit_app()


def scenario_info():
    require_yaneuraou()
    launch(*((1400, 1000) if LANG == "en" else (1300, 980)))
    rpc("action.trigger", {"name": "actionStartGame"})
    time.sleep(1.5)
    # 先手が人間、後手がやねうら王、持ち時間5分・1手ごとに10秒加算（後手は同じ設定）
    for key, value in [("comboBoxPlayer1", 0), ("comboBoxPlayer2", 1), ("basicTimeHour1", 0),
                       ("basicTimeMinutes1", 5), ("byoyomiSec1", 0), ("addEachMoveSec1", 10)]:
        rpc("widget.setValue", {"target": "StartGameDialog", "widget": key, "value": value})
    rpc("dialog.clickButton", {"target": "StartGameDialog", "text": GAME_START})
    time.sleep(2)
    env = dict(os.environ, SBQ_HAYANAGI=HAYANAGI_PATH)
    subprocess.run(["python3", os.path.join(HERE, "humanplay.py"), os.path.join(SOCKDIR, NAME + ".sock"), "20",
                    "600"], env=env, check=True, stdout=subprocess.DEVNULL)
    # 人間の手番で投了し、結果の知らせを閉じる
    wait_until(lambda: rpc("app.state")["sfen"].split()[1] == "b", timeout=60, step=0.2)
    rpc("action.trigger", {"name": "actionResign"})
    time.sleep(1.5)
    for window in rpc("dialog.list")["windows"]:
        if window.get("class") == "QMessageBox":
            rpc("dialog.close", {"target": window["selector"]})
    time.sleep(1)
    rpc("widget.showDock", {"widget": "GameInfoDock"})
    time.sleep(1)
    capture("game-play/game-06-game-info.png")
    quit_app()


SCENARIOS = {"main": scenario_main, "tsumi": scenario_tsumi, "wide": scenario_wide, "game": scenario_game,
             "info": scenario_info, "lang": scenario_lang}

if __name__ == "__main__":
    SCENARIOS[SCENARIO]()
