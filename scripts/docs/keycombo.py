#!/usr/bin/env python3
"""Press a key combination on the X display in $DISPLAY (modifiers first, released in reverse).

usage: DISPLAY=:99 keycombo.py Alt_L s
Single keys and clicks are in ../x11ctl.py (key / click / list / move / focus).
"""
import ctypes
import sys
import time

X = ctypes.CDLL("libX11.so.6")
T = ctypes.CDLL("libXtst.so.6")
X.XOpenDisplay.restype = ctypes.c_void_p
display = ctypes.c_void_p(X.XOpenDisplay(None))
codes = [X.XKeysymToKeycode(display, X.XStringToKeysym(k.encode())) for k in sys.argv[1:]]
for code in codes:
    T.XTestFakeKeyEvent(display, code, 1, 0)
    X.XFlush(display)
    time.sleep(0.05)
for code in reversed(codes):
    T.XTestFakeKeyEvent(display, code, 0, 0)
    X.XFlush(display)
    time.sleep(0.05)
X.XSync(display, 0)
