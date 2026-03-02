# Task 08: `Q_SIGNALS:` / `Q_SLOTS:` → `signals:` / `slots:` 統一（P2 / §3.4）

## 概要

`signals:` / `public slots:` が多数派だが、11ファイルで `Q_SIGNALS:` / `Q_SLOTS:` が使われている。多数派に統一する。

## 対象ファイル

1. `src/ui/coordinators/dialogcoordinator.h`
2. `src/ui/coordinators/positioneditcoordinator.h`
3. `src/ui/controllers/playerinfocontroller.h`
4. `src/ui/controllers/evaluationgraphcontroller.h`
5. `src/ui/controllers/boardsetupcontroller.h`
6. `src/ui/controllers/pvclickcontroller.h`
7. `src/ui/controllers/timecontrolcontroller.h`
8. `src/game/gamestatecontroller.h`
9. `src/analysis/analysisflowcontroller.h`
10. `src/kifu/kifuexportclipboard.h`
11. `src/kifu/kifuexportcontroller.h`

## 手順

### Step 1: 置換

1. 上記各ファイルで以下を置換:
   - `Q_SIGNALS:` → `signals:`
   - `public Q_SLOTS:` → `public slots:`
   - `private Q_SLOTS:` → `private slots:`（存在する場合）
   - `protected Q_SLOTS:` → `protected slots:`（存在する場合）

### Step 2: 検証

1. 置換漏れがないか `grep -r "Q_SIGNALS\|Q_SLOTS" src/` で確認
2. `cmake --build build` でビルドが通ることを確認
3. テストを実行して全パスを確認

## 注意事項

- `Q_SIGNALS` / `Q_SLOTS` と `signals` / `slots` は MOC 的には同義。動作変更なし
- `Q_SIGNAL` / `Q_SLOT`（単数形、`emit` の代替）は別物なので触れない
- Windows のマクロ汚染回避のために `Q_SIGNALS` を使うケースがあるが、本プロジェクトでは `Fusion` スタイル固定で問題なし
