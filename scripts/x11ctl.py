#!/usr/bin/env python3
"""X11 操作ヘルパー（Xvfb 上で ShogiBoardQ を自動操作するための最小ツール）

ctypes で libX11 / libXtst を直接呼ぶため追加パッケージは不要（xdotool 不要）。
使い方は docs/dev/live-game-verification.md を参照。

  x11ctl.py list                 マップ済みトップレベルを "id  name  x y w h  or=" で一覧
  x11ctl.py wait <substr> [sec]  名前に substr を含むトップレベルが現れるまで待ち id を出力
  x11ctl.py geom <id>            ルート座標での x y w h
  x11ctl.py move <id> x y w h    XMoveResizeWindow
  x11ctl.py focus <id>           XRaiseWindow + XSetInputFocus
  x11ctl.py click x y [button]   XTest によるクリック（ルート座標）
  x11ctl.py key <keysym>...      XTest によるキー入力（例: Return Down Escape）

Qt は日本語タイトルを _NET_WM_NAME(UTF8_STRING) に置くので、XFetchName が空のときは
そちらを読む。DISPLAY 環境変数で対象ディスプレイを指定する。
"""
import ctypes, ctypes.util, sys, time, os

X = ctypes.CDLL(ctypes.util.find_library("X11"))
T = ctypes.CDLL(ctypes.util.find_library("Xtst"))
X.XOpenDisplay.restype = ctypes.c_void_p
X.XOpenDisplay.argtypes = [ctypes.c_char_p]
X.XDefaultRootWindow.restype = ctypes.c_ulong
X.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
X.XQueryTree.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)), ctypes.POINTER(ctypes.c_uint)]
X.XFetchName.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_char_p)]
X.XInternAtom.restype = ctypes.c_ulong
X.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
X.XGetWindowProperty.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_long, ctypes.c_long, ctypes.c_int, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte))]
def net_wm_name(w):
    a_name = X.XInternAtom(d, b"_NET_WM_NAME", 0); a_utf8 = X.XInternAtom(d, b"UTF8_STRING", 0)
    t = ctypes.c_ulong(); f = ctypes.c_int(); n = ctypes.c_ulong(); b = ctypes.c_ulong(); p = ctypes.POINTER(ctypes.c_ubyte)()
    if X.XGetWindowProperty(d, w, a_name, 0, 1024, 0, a_utf8, ctypes.byref(t), ctypes.byref(f), ctypes.byref(n), ctypes.byref(b), ctypes.byref(p)) != 0 or not n.value:
        return ""
    s = bytes(p[i] for i in range(n.value)).decode("utf-8", "replace"); X.XFree(p); return s
X.XFree.argtypes = [ctypes.c_void_p]
X.XGetGeometry.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint)]
X.XTranslateCoordinates.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_ulong)]
X.XMoveResizeWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int, ctypes.c_uint, ctypes.c_uint]
X.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
X.XRaiseWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
X.XFlush.argtypes = [ctypes.c_void_p]
X.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
X.XStringToKeysym.restype = ctypes.c_ulong
X.XStringToKeysym.argtypes = [ctypes.c_char_p]
X.XKeysymToKeycode.restype = ctypes.c_ubyte
X.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
T.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ulong]
T.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
T.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]

class XWA(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("width", ctypes.c_int), ("height", ctypes.c_int),
                ("border_width", ctypes.c_int), ("depth", ctypes.c_int), ("visual", ctypes.c_void_p),
                ("root", ctypes.c_ulong), ("c_class", ctypes.c_int), ("bit_gravity", ctypes.c_int),
                ("win_gravity", ctypes.c_int), ("backing_store", ctypes.c_int), ("backing_planes", ctypes.c_ulong),
                ("backing_pixel", ctypes.c_ulong), ("save_under", ctypes.c_int), ("colormap", ctypes.c_ulong),
                ("map_installed", ctypes.c_int), ("map_state", ctypes.c_int), ("all_event_masks", ctypes.c_long),
                ("your_event_mask", ctypes.c_long), ("do_not_propagate_mask", ctypes.c_long),
                ("override_redirect", ctypes.c_int), ("screen", ctypes.c_void_p)]
X.XGetWindowAttributes.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(XWA)]

d = X.XOpenDisplay(None)
if not d:
    sys.exit("cannot open display")
root = X.XDefaultRootWindow(d)

def geom(w):
    r = ctypes.c_ulong(); x = ctypes.c_int(); y = ctypes.c_int()
    wd = ctypes.c_uint(); ht = ctypes.c_uint(); bw = ctypes.c_uint(); dp = ctypes.c_uint()
    X.XGetGeometry(d, w, ctypes.byref(r), ctypes.byref(x), ctypes.byref(y), ctypes.byref(wd), ctypes.byref(ht), ctypes.byref(bw), ctypes.byref(dp))
    ax = ctypes.c_int(); ay = ctypes.c_int(); child = ctypes.c_ulong()
    X.XTranslateCoordinates(d, w, root, 0, 0, ctypes.byref(ax), ctypes.byref(ay), ctypes.byref(child))
    return ax.value, ay.value, wd.value, ht.value

def toplevels():
    r = ctypes.c_ulong(); p = ctypes.c_ulong(); kids = ctypes.POINTER(ctypes.c_ulong)(); n = ctypes.c_uint()
    X.XQueryTree(d, root, ctypes.byref(r), ctypes.byref(p), ctypes.byref(kids), ctypes.byref(n))
    out = []
    for i in range(n.value):
        w = kids[i]
        a = XWA()
        X.XGetWindowAttributes(d, w, ctypes.byref(a))
        if a.map_state != 2:  # IsViewable
            continue
        name = ctypes.c_char_p()
        X.XFetchName(d, w, ctypes.byref(name))
        nm = name.value.decode("utf-8", "replace") if name.value else ""
        if name.value: X.XFree(name)
        if not nm: nm = net_wm_name(w)
        x, y, wd, ht = geom(w)
        out.append((w, nm, x, y, wd, ht, a.override_redirect))
    if n.value: X.XFree(kids)
    return out

cmd = sys.argv[1]
if cmd == "list":
    for w, nm, x, y, wd, ht, ov in toplevels():
        print(f"0x{w:x}\t{nm!r}\t{x} {y} {wd} {ht}\tor={ov}")
elif cmd == "wait":
    sub = sys.argv[2]; limit = float(sys.argv[3]) if len(sys.argv) > 3 else 30
    t0 = time.time()
    while time.time() - t0 < limit:
        for w, nm, *_ in toplevels():
            if sub in nm:
                print(f"0x{w:x}"); sys.exit(0)
        time.sleep(0.3)
    sys.exit("timeout waiting for " + sub)
elif cmd == "geom":
    print(*geom(int(sys.argv[2], 16)))
elif cmd == "move":
    w = int(sys.argv[2], 16); x, y, wd, ht = map(int, sys.argv[3:7])
    X.XMoveResizeWindow(d, w, x, y, wd, ht); X.XFlush(d)
elif cmd == "focus":
    w = int(sys.argv[2], 16)
    X.XRaiseWindow(d, w); X.XSetInputFocus(d, w, 1, 0); X.XFlush(d)
elif cmd == "click":
    x, y = int(sys.argv[2]), int(sys.argv[3]); b = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    T.XTestFakeMotionEvent(d, 0, x, y, 0); X.XFlush(d); time.sleep(0.05)
    T.XTestFakeButtonEvent(d, b, 1, 0); X.XFlush(d); time.sleep(0.08)
    T.XTestFakeButtonEvent(d, b, 0, 0); X.XFlush(d); time.sleep(0.05)
elif cmd == "key":
    for ks in sys.argv[2:]:
        kc = X.XKeysymToKeycode(d, X.XStringToKeysym(ks.encode()))
        T.XTestFakeKeyEvent(d, kc, 1, 0); X.XFlush(d); time.sleep(0.05)
        T.XTestFakeKeyEvent(d, kc, 0, 0); X.XFlush(d); time.sleep(0.08)
X.XSync(d, 0)
