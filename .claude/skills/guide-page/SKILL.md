---
name: guide-page
description: ShogiBoardQ の公開ガイド（docs/guide/*.html）やホームページ（docs/index.html）を、日本語・英語・簡体字・繁体字の4言語で更新・新規作成し、スクリーンショットを4言語で撮り直す手順。「menu.html をお願いします」「〇〇のガイドを作って」「ホームの機能一覧を更新して」「スクリーンショットを撮り直して」のような依頼で使う。
---

# ガイドページの更新・新規作成（4言語）

公開ガイドのページを、現在の GUI に合わせて4言語（ja / en / zh-cn / zh-tw）で作る。
補助スクリプトは `scripts/docs/`（使い方は `scripts/docs/README.md`）にある。最初に README を読むこと。
撮影の細かい落とし穴はメモリの docs-screenshot-workflow、翻訳の運用は translation-baseline-workflow、
繁体字の用語は zh-tw-terminology にある。

## 進め方

1. **調べる**: 既存のページ（`docs/guide/<page>.html`、`docs/en/…`、あれば `docs/zh-cn/…`・`docs/zh-tw/…`）と画像を読み、
   書かれていることを**コードと実際の画面で**確かめる。画面の項目名は `resources/translations/*.ts` の訳と一致させる
   （日本語の原文、en・zh_CN・zh_TW の訳）。推測で書かない。
2. **不具合を直す**: 確認中に見つけた不具合や表示崩れは、その場で直す（依頼の範囲内）。直したらテストを足し、
   **修正を外すと失敗する**ことを確かめる。`tr()` を変えたら `cmake --build build --target update_translations` の後、
   4言語の訳を入れて未翻訳0を保つ（ja_JP は原文と同じ訳。`%n` の複数形は使わない）。
3. **撮影**: 4言語すべてで撮る。`scripts/docs/launch.sh <名前> <ja_JP|en|zh_CN|zh_TW> <ディスプレイ> [W H]` で起動し、
   `scripts/docs/rpc.py` で操作・撮影する（ウィンドウ枠が要るときは `kwin_capture.sh`）。
   - メインウィンドウは ja・zh 1300×980、en 1400×1000。ダイアログは `screenshot.capture` にダイアログの selector を渡す。
   - メニューのポップアップは `scripts/x11ctl.py` でクリックして `import -window root -crop` で切り抜く。
   - 盤面をくっきり撮るときは README の高解像度の手順に従う。撮影用のスクリプトは作業フォルダ（scratchpad）に置く。
   - 置き場所: `docs/images/<page>/`（ja）と `docs/images/{en,zh-cn,zh-tw}/<page>/`。使わなくなった旧画像は `git rm` する。
   - Windows・macOS の画面はこの PC では撮れない。必要ならユーザーに画像を用意してもらう。
4. **ページを書く**: `scripts/docs/examples/piece_sound/` を写して作業フォルダに生成スクリプトを作り、4言語の HTML を出力する。
   本文は言語ごとに自然な文で書き、画面の項目名は「」（ja・zh-tw）／“”（zh-cn）で引用する。
   - 既存ページの一部だけを直す依頼（例: Linux の箇所だけ）では、ほかの箇所を変えない。ja・en は
     `guide_common.update_existing` で4言語メニューを付け、本文はその場で編集する。
   - 中国語版が無いページへのリンクは `../../en/guide/<page>.html` にし、文中に（英文）と添える。
   - 新しいページは利用ガイドの目次（4言語の `guide/index.html`）にも載せる。目次のカードはホームと共通なので、
     5. で `gen_home.py` にカードを足し、`python3 scripts/docs/gen_guide_index.py` で目次を生成する。
5. **ホーム・サイトマップ**: `python3 scripts/docs/gen_home.py` でホームの4言語を生成する（中国語版ができたページへの
   リンクは自動で切り替わる。カードの追加・説明文の変更は `gen_home.py` を編集する。HTML を直接直さない）。
   中国語版を新しく作ったページは `python3 scripts/docs/sitemap_add_zh.py guide/<page>.html` でサイトマップに追加する
   （ja・en の項目があるページ用。4言語とも新しいページは、4つの `<url>` を同じ hreflang の組で足す）。
6. **確認**:
   - `python3 scripts/docs/check_links.py guide/<page>.html`（リンク切れと `<section>` の対応）
   - headless chromium で4言語の表示を見る。普段の `~/.cache/chromium-headless` に一時フォルダが残らないよう、
     設定とプロファイルを作業フォルダに向ける（`W=<作業フォルダ>/chromium; XDG_CONFIG_HOME=$W/cfg XDG_CACHE_HOME=$W/cache
     chromium --headless=new --user-data-dir=$W/profile --screenshot=out.png --window-size=1280,6000 file://…`）。
     スマートフォン幅（`--window-size=390,7000`）でも表や画像がはみ出さないか見る
   - コードを変えたときは `ctest` 全体と GUI 監査（`python3 tests/gui/prepare.py` の後に
     `xvfb-run -a -s '-screen 0 1600x1200x24' python3 tests/gui/run.py`）。詰将棋の GUI テストは
     `build/gui-audit/test-build/tst_tsume_*` を別に実行する
7. **後片付け**: 起動したアプリは `rpc.py <名前> app.quit` で止め、起動した Xvfb も止める
   （`pgrep -x Xvfb` と `/proc/<pid>/cmdline` で確かめてから。`pkill -f` は使わない）。普段の設定（`~/.config` など）に
   書き込みが無いことを確かめる。
8. **報告**: 日本語で、変えたこと・直した不具合・テスト結果・残した課題を短くまとめる。**コミットはしない**。

## コミットを頼まれたら

- 内容ごとにコミットを分ける（例: 不具合の修正とテスト ／ ガイドのページと画像）。同じファイルに別の変更が混ざるときは
  インデックスに部分的に載せる。`git mv` 済みの移動など、意図しない変更が入っていないか `git diff --cached --stat` で確かめる。
- メッセージは日本語で「件名 → 空行 → 本文 → 空行 → Co-Authored-By」。プッシュ前に `git log -N --format='[%s]'` で件名が
  1行になっていることを確かめる。main は保護ブランチで強制プッシュできない。
