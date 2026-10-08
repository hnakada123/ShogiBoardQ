# 検証依頼：MCP サーバーのテストを Linux で実行し、macOS での失敗の原因を切り分ける

ShogiBoardQ の MCP サーバーのテスト（`mcp/tests`、CTest では `tst_mcp_python`）を Linux で実行し、
すべて通るかを確かめてください。macOS では一部が失敗しており、その多くは macOS 固有の原因と見ていますが、
Linux では確認していません。Linux での結果をもとに、残る失敗が今回の変更によるものか、環境やテストの作りによるものかを
切り分けて報告してください。

この依頼は、過去の会話を参照できない状態でも実行できるようにまとめています。作業範囲は、テストの実行・
失敗の原因調査・修正方針の提示です。本体やテストの修正、コミット、push は今回の依頼には含みません。
説明・報告は日本語でお願いします。

## 作業環境

- ShogiBoardQ：`/home/nakada/GitHub/ShogiBoardQ`（Hayanagi はサブモジュール `Hayanagi/`）
- Qt 6、CMake、Ninja、Python 3.10 以上（MCP の SDK `mcp` 2.x が 3.10 以上を必要とする）
- 作業前にリポジトリの状態と適用される `AGENTS.md`・`CLAUDE.md` を確認し、既存の変更を保全してください。
- 一時ファイル・ビルド・仮想環境は作業用のフォルダに置き、リポジトリに生成物を残さないでください
  （`mcp/debug.log`、`__pycache__`、`*.egg-info` など。デバッグビルドのアプリは起動したフォルダに `debug.log` を書きます）。

## 前提：テスト対象の変更が入っていること

検証対象は、次の2つの変更が入った main です。

1. 分岐ツリーの不具合修正と機能追加（コミット `051544c9`「分岐ツリーの不具合を直し、分岐の編集・盤上での変化追加・見やすい表示を加える」）
2. 設定・データ・キャッシュの隔離（環境変数 `SHOGIBOARDQ_CONFIG_HOME`・`SHOGIBOARDQ_DATA_HOME`・`SHOGIBOARDQ_CACHE_HOME`）と、
   それに合わせたテストの修正、定跡ウィンドウの「着手」の判定の修正

`git pull` の後、`src/services/apppaths.h` と `tests/tst_app_paths.cpp` があり、`mcp/tests/conftest.py` が
`SHOGIBOARDQ_CONFIG_HOME` を設定していることを確かめてください。無い場合は 2. がまだコミット・push されていないので、
作業を止めて報告してください。

## 背景：macOS での結果

macOS（Apple Silicon、Qt 6.11.2、Python 3.14.8）で `mcp/tests` を実行した結果は、成功 97、失敗 14、スキップ 26 でした。

- 以前は MCP テストもアプリの C++ テストも `XDG_CONFIG_HOME` だけで設定を隔離していました。macOS・Windows の Qt は
  XDG を見ないため、テストが利用者の本来の設定ファイル（macOS では `~/Library/Preferences/ShogiBoardQ/ShogiBoardQ.ini`）を
  書き換えていました。これは上記 2. で直しました（Linux では XDG でも隔離されるので、もともと問題は起きていなかったはずです）。
- 残る14件の失敗と、macOS で見立てた原因は次のとおりです。

| テスト | macOS での原因の見立て |
|---|---|
| `test_consideration.py::test_unavailable_engine_recovers[False]`・`[True]` | 名前もタイトルもないメッセージボックスを閉じようとして、`close_dialog {'dialog': ''}` が `"dialog" must be a non-empty string` で失敗 |
| `test_csa_game.py::test_single_chudan_and_individual_time_defaults[False]`・`[True]`、`test_connection_refused_recovers` | 同上（`dismiss_end` などで `close_dialog` に空の名前を渡す） |
| `test_game_info.py::test_reset_cancel_preserves_pending_edits`、`test_game_end_preserves_active_metadata` | メッセージボックスをウィンドウのタイトルで待つが、macOS ではメッセージボックスのタイトルが空になるため見つからない |
| `test_kifu_management.py::test_paste_dialog_controls_and_cancel`、`test_invalid_paste_preserves_record_and_editor`、`test_unsaved_save_cancel_then_discard`、`test_save_filter_warning_and_explicit_extension`、`test_game_end_auto_save` | 同上（`wait` でタイトルを探してタイムアウト） |
| `test_phase2_app.py::test_tsume_board_clicks` | 「成りの選択」ダイアログをタイトルで探すが見つからない（`No open window matches "成りの選択"`） |
| `test_feature_coverage.py::test_joseki_add_edit_save_delete_and_merge` | 直前のメッセージボックスが閉じる前にマージを開こうとする、タイミング依存の失敗（0.5 秒待つと通った） |

- C++ のテスト（CTest）は 120 件中 117 件が成功し、`tst_menu_window`（ショートカット表記が `⌘U` になる）、
  `tst_fontsettingsdialog`（フォント名の違い）、`tst_csalogpanel`（スクロール位置）が失敗しました。変更前の main でも同じでした。
- 定跡の「着手」のテストは今回の仕様変更に合わせて書き換えました（`test_joseki_play_outside_games_and_on_human_turn`。
  対局していないときも定跡手を指せて棋譜に記録される）。このテストを含め、書き換えたテストは Linux では未確認です。

## 手順

### 1. 普段の設定の記録

テストが利用者の設定に書き込まないことを確かめるため、実行前に次の場所の一覧とハッシュを記録してください
（存在しないものは「無し」と記録）。

- `~/.config/ShogiBoardQ/`（`ShogiBoardQ.ini`、`automation-endpoint.json`）
- `~/.local/share/ShogiBoardQ/`（`tsume_progress.sqlite` など）
- `~/.cache/ShogiBoardQ/`

### 2. ビルド

既存の `build/` を壊さないよう、作業用のフォルダにテスト付きでビルドしてください。

```bash
W=<作業用のフォルダ>
cmake -B "$W/build" -S /home/nakada/GitHub/ShogiBoardQ -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$W/build"
```

### 3. Python の環境

```bash
python3 --version   # 3.10 以上であること
python3 -m venv "$W/venv"
"$W/venv/bin/pip" install "mcp>=2,<3" "pytest>=8" "anyio>=4" pytest-timeout
```

リポジトリに `*.egg-info` を作らないよう、`pip install -e` は使わず、実行時に `PYTHONPATH` で `mcp/` を指定します。

### 4. MCP テストの実行

```bash
cd /home/nakada/GitHub/ShogiBoardQ
B="$W/build"
PYTHONPATH="$PWD/mcp" PYTHONDONTWRITEBYTECODE=1 QT_QPA_PLATFORM=offscreen \
SHOGIBOARDQ_CLI="$B/shogiboardq-cli" \
SHOGIBOARDQ_EXECUTABLE="$B/ShogiBoardQ" \
SHOGIBOARDQ_TEST_USI_ENGINE="$B/Hayanagi/hayanagi" \
SHOGIBOARDQ_TEST_MATE_ENGINE="$B/tests/mock_mate_engine" \
"$W/venv/bin/python" -m pytest -q -rfs -p no:cacheprovider --basetemp=/tmp/sbqmcp --timeout=300 mcp/tests \
  2>&1 | tee "$W/mcp-pytest.log"
```

- `--basetemp` は短いパスにしてください。テストはこの下にアプリの自動化ソケットを作り、Unix ドメインソケットのパスには
  上限（アプリ側で 100 バイト）があります。長いと、アプリが待ち受けできずにテストが止まります。
- `hayanagi` と `mock_mate_engine` の場所がこれと違う場合は、`find "$B" -name hayanagi -o -name mock_mate_engine` で確かめてください。
- 終了後に `/tmp/sbqmcp` を削除してください。
- CSA サーバー（`SHOGIBOARDQ_TEST_CSA_SERVER`）や ShogiHome（`SHOGIBOARDQ_TEST_SHOGIHOME_CDP`）が必要なテストは、
  準備が無ければスキップされます。用意できる場合は `mcp/tests/test_csa_game.py` の冒頭の説明に従って指定してください。

### 5. C++ テストの実行

```bash
ctest --test-dir "$W/build" -j8 --timeout 600 --output-on-failure -E tst_mcp_python 2>&1 | tee "$W/ctest.log"
```

### 6. 失敗があったときの切り分け

Linux では XDG による隔離が効くので、変更前の main とも安全に比較できます。失敗したテストについて、
変更前のコミット `d09c7c77`（`051544c9` の1つ前）でも失敗するかを確かめてください。

```bash
git -C /home/nakada/GitHub/ShogiBoardQ worktree add --detach "$W/head-wt" d09c7c77
# サブモジュールは worktree に入らないので、Hayanagi をリンクするか submodule update で用意する
cmake -B "$W/head-build" -S "$W/head-wt" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$W/head-build" --target ShogiBoardQ shogiboardq-cli hayanagi mock_mate_engine
```

変更前の版でも同じテスト（`$W/head-wt/mcp/tests`）を同じ環境変数で実行し、失敗したテストだけを比べてください。
タイミング依存が疑われるものは、単独で3回ほど実行し直してください。
終わったら `git worktree remove` で worktree を片付けてください。

### 7. 普段の設定の再確認

1. で記録した場所の一覧とハッシュが変わっていないことを確かめてください。変わっていたら、どのテストが書き込んだかを調べてください。

## 報告していただきたいこと

- 実行環境（ディストリビューション、Qt・Python・mcp のバージョン、デスクトップ環境の有無）
- MCP テストと C++ テストの件数（成功・失敗・スキップ）
- 失敗したテストごとに、次のどれにあたるか
  - 今回の変更（分岐ツリー・設定の隔離・定跡の着手）による退行（変更前は通り、変更後に失敗）
  - 変更前から失敗している（環境やテストの作りによる）
  - タイミング依存（単独で実行し直すと通る）
- 上の表にある macOS の失敗が Linux で通ったか（タイトルの空のメッセージボックスが原因という見立てが正しかったか）
- スキップされたテストと、その理由
- 普段の設定に書き込みが無かったか
- 退行や不具合があれば、原因の箇所と修正方針（修正そのものは行わない）
