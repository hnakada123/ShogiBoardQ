# Task 25: lambda connect の解消

## 目的

CLAUDE.md の規約「`connect()` にラムダを使わない」に合わせ、残存する lambda connect を名前付きスロットに置換する。

## 背景

CLAUDE.md で lambda connect は禁止されているが、7箇所が残存している。

## 対象ファイルと箇所

### 1. `src/ui/coordinators/docklayoutmanager.cpp`（3箇所）

```cpp
// 270行目
connect(restoreAction, &QAction::triggered, this, [this, name]() { restoreLayout(name); });
// 276行目
connect(setStartupAction, &QAction::triggered, this, [this, name]() { setAsStartupLayout(name); });
// 284行目
connect(deleteAction, &QAction::triggered, this, [this, name]() { deleteLayout(name); });
```

改善方針: `QAction::setData(name)` + 単一スロットで `sender()` から名前を取得する。

### 2. `src/engine/usi.cpp`（2箇所）

```cpp
// 1023行目
connect(m_analysisStopTimer, &QTimer::timeout, this, [this]() {
    m_protocolHandler->sendStop(); m_analysisStopTimer = nullptr;
});
// 1072行目
connect(m_analysisStopTimer, &QTimer::timeout, this, [this]() {
    if (m_processManager && m_processManager->isRunning()) { ... }
});
```

改善方針: 名前付き private slot を作成する。

### 3. `src/widgets/collapsiblegroupbox.cpp`（1箇所）

```cpp
// 117行目
connect(m_animationGroup, &QParallelAnimationGroup::finished, this, [this]() {
    if (!m_expanded) { ... }
});
```

改善方針: 名前付き private slot を作成する。

### 4. `src/widgets/menubuttonwidget.cpp`（1箇所）

```cpp
// 126行目
connect(m_action, &QAction::changed, this, [this]() {
    m_mainButton->setEnabled(m_action->isEnabled());
});
```

改善方針: 名前付き private slot を作成する。

## 作業

1. 各ファイルの lambda connect を確認し、名前付きスロットに置換する。
2. ラムダが不可避な箇所があれば、理由を短いコメントで明示する。
3. `docklayoutmanager.cpp` は `QAction::data` + 単一スロットパターンを優先検討する。

## 受け入れ条件

- [ ] lambda connect の件数が 0 になる（または不可避な箇所のみコメント付きで残る）。
- [ ] ビルド成功、既存テストが通る。
- [ ] 可読性・デバッグ容易性が下がらない。
