# ガイド（docs/）作成用のスクリプト

公開ガイド（`docs/`）の4言語ページ（日本語・英語・簡体字・繁体字）と、そのスクリーンショットを作るための補助スクリプトです。
作業手順は Claude Code の Skill [.claude/skills/guide-page/SKILL.md](../../.claude/skills/guide-page/SKILL.md)、
依頼の例は [docs/dev/guide-update-prompts.md](../../docs/dev/guide-update-prompts.md) にあります。

## ファイル

| ファイル | 内容 |
|---|---|
| `guide_common.py` | 4言語の共通処理。言語メニュー（`lang_menu`）、hreflang（`alternates`）、言語の表（`LANGS`/`meta`）、既存の ja/en ページに4言語メニューを付ける `update_existing` |
| `gen_home.py` | ホームページ（`docs/index.html` と en / zh-cn / zh-tw）を生成する。機能カードは `FEATURE_GROUPS` と各言語の文言。カードのリンク先は、その言語のガイドがあればそれ、無ければ英語版になる |
| `sitemap_add_zh.py` | `docs/sitemap.xml` のページに簡体字・繁体字版を追加する（`guide/<page>.html` を渡す） |
| `check_links.py` | ページ内のリンク・画像の参照先があるか、`<section>` の開閉が対応しているかを4言語分確かめる（`--all` で全ページ） |
| `build_guide/` | Linux ビルド手順のページ（4言語の `guide/linux-build-and-release.html`）と、開発者向けの `docs/dev/linux-build-and-release.md` を同じ本文（`texts_linux_build.py`）から書き出す。ページも md も直接編集しない |
| `examples/piece_sound/` | ガイドページの生成スクリプトの見本（`texts.py` に言語ごとの本文、`gen_piece_sound.py` が4言語の HTML を書く） |
| `launch.sh` | Xvfb 上で ShogiBoardQ を `--automation` 付きで起動する。設定・キャッシュ・データ・状態を作業フォルダに隔離する |
| `rpc.py` | 自動化 API（JSON-RPC）を1回呼ぶ。`rpc.py <名前> <メソッド> [JSON]` |
| `keycombo.py` | X のキーの組み合わせを押す（例: `Alt_L s`）。単独のキー・クリック・ウィンドウ一覧は `../x11ctl.py` |
| `humanplay.py` | 対局中に人間（先手）側の手を Hayanagi に考えさせ、盤のクリックで指す |
| `kwin_capture.sh` | ウィンドウ枠（KDE の装飾と影）付きで撮るための入れ子の KWin を、普段の環境から隔離して実行する |

## 典型的な使い方

```bash
# 日本語で起動（ディスプレイ :99、ウィンドウ 1300x980、Hayanagi を登録）
SBQ_FRESH=1 SBQ_ENGINES=1 scripts/docs/launch.sh ja99 ja_JP 99 1300 980
python3 scripts/docs/rpc.py ja99 app.state
python3 scripts/docs/rpc.py ja99 action.trigger '{"name":"actionPieceSoundSettings"}'
python3 scripts/docs/rpc.py ja99 dialog.list
python3 scripts/docs/rpc.py ja99 screenshot.capture '{"target":"@widget-2","output_dir":"/abs/path"}'
python3 scripts/docs/rpc.py ja99 app.quit

# 英語・簡体字・繁体字も同じ（en / zh_CN / zh_TW）。中国語のロケールは作業フォルダに自動で作る
SBQ_FRESH=1 scripts/docs/launch.sh zh97 zh_CN 97 1300 980

# ウィンドウ枠付き（スクリプトに SBQ_SOCK が渡る）
scripts/docs/kwin_capture.sh k1 en bash -c 'python3 scripts/docs/rpc.py "$SBQ_SOCK" app.state; spectacle -b -n -a -o /abs/out.png'
```

自動化 API の主なメソッド: `action.trigger`、`dialog.list`/`close`/`clickButton`、`widget.text`/`click`/`setValue`/`showDock`、
`widget.clickCell`、`screenshot.capture`、`dock.configure`/`list`、`menu.items`/`select`、`kifu.load`/`goto`/`get`、
`position.set`/`get`、`board.click`、`app.state`/`quit`。一覧は `src/automation/` と `docs/dev/mcp-server.md`。
許可されている QAction は `rpc.py <名前> action.list` で確認できる（許可されていない操作はメニューをキーボードで開いて操作する）。

## 撮影の注意

- 普段の設定・履歴に書き込まないよう、必ず `launch.sh` / `kwin_capture.sh` で起動する（`XDG_CONFIG_HOME`・`XDG_CACHE_HOME`・`XDG_DATA_HOME`・`XDG_STATE_HOME` を隔離する）。
- `screenshot.capture` はクライアント領域だけを撮り、幅1920を超える画像は縮小する。くっきりした盤面は `SBQ_SCREEN=2560x1800x24`、`SBQ_EXTRA_INI=$'[SizeRelated]\nsquareSize=100'`、ウィンドウ幅1900以下で撮る。
- メニューのポップアップは `screenshot.capture` に写らない。`../x11ctl.py click <x> 12` でメニュー名をクリックし、`../x11ctl.py list` の `or=1`（ポップアップ）の範囲を `import -window root -crop` で切り抜く。メニューの位置は Alt+キー（`keycombo.py Alt_L s`）で開いて `list` から調べられる。
- 中国語（zh_CN/zh_TW）は `LOCPATH` を付けて起動する（`launch.sh` が行う）。`LOCPATH` を付けると ja/en のロケールは読めなくなるので、言語ごとに別のインスタンスにする。
- 止めるときは `rpc.py <名前> app.quit`。Xvfb は `pgrep -x Xvfb` で探し、`/proc/<pid>/cmdline` でディスプレイ番号を確かめてから止める（`pkill -f` は自分のシェルにも一致する）。
- 入れ子の KWin は `kwin_capture.sh` 以外の方法で起動しない。普段のディスプレイの変数を残したまま使い捨ての D-Bus を起動すると、ポータルや fcitx5 が普段の画面につながり、終了後も残る。

## ページを作るときの流れ

1. 画面・操作の説明をコードで確かめる（ボタン名は翻訳ファイル `resources/translations/*.ts` の訳と一致させる）。
2. 4言語で撮影し、`docs/images/<ページ>/`（日本語）と `docs/images/{en,zh-cn,zh-tw}/<ページ>/` に置く。
3. 生成スクリプト（`examples/piece_sound/` を写して作る）で4言語の HTML を書く。既存の ja/en ページを部分的に直すだけなら、`guide_common.update_existing` で4言語メニューを付け、本文はその場で編集する。
4. `python3 scripts/docs/gen_home.py` でホームのリンクを更新し、`scripts/docs/sitemap_add_zh.py guide/<ページ>.html` でサイトマップに追加する。
5. `chromium --headless=new --screenshot=out.png --window-size=1280,4000 file://…` で表示を確かめ、
   `python3 scripts/docs/check_links.py guide/<ページ>.html` でリンク切れを確認する。
