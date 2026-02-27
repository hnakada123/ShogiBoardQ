# Task 16: 所有権モデルの統一

## 目的
raw pointer 依存を低減し、所有権モデルを統一する。生成箇所を Factory/CompositionRoot に限定する。

## 背景
現在の所有権パターン:
- `QObject parent` 管理: Qt ウィジェット系
- `std::unique_ptr` 管理: 非QObject系コントローラ
- `raw pointer (非所有)` 注釈: 多数存在するが構造的保証が弱い

## 作業内容

### Phase A: 所有権ルールの明文化
`CLAUDE.md` に以下のルールを追記する:

```
## Ownership Rules
1. QObject 派生は parent ownership を基本とする
2. 非QObject は std::unique_ptr を基本とする
3. 非所有参照は T& を優先（nullptr 許容が必要な場合のみ T*）
4. 生成は *factory* / *composition* / *wiring* クラスに限定する
5. MainWindow / MatchCoordinator 等のハブクラスでの直接 new を禁止する
```

### Phase B: new 発生箇所の監査
`src/` 配下で `new` を使用している箇所を全て洗い出す:
```bash
grep -rn '\bnew\b' src/ --include='*.cpp' | grep -v '// ' | grep -v 'factory' | grep -v 'composition'
```

以下に分類する:
- **OK**: Factory/CompositionRoot/Wiring 内での生成
- **NG**: コントローラ/サービス内での直接生成 → 移動先を特定する

### Phase C: 直接 new の Factory/CompositionRoot への移動
NG と判定された箇所を段階的に移動する:
- 優先度: `MainWindow` 内 → `MatchCoordinator` 内 → その他

### Phase D: 非所有ポインタの注釈統一
`///< 非所有` 注釈が付いているメンバを確認し:
- 本当に非所有か再確認する
- 可能なら `T&` に変更する
- `T*` のままの場合は `// non-owning` コメントのスタイルを統一する

### テスト実行
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] 所有権ルールが CLAUDE.md に明文化されている
- [ ] Factory/CompositionRoot/Wiring 以外での `new` が原則排除されている
- [ ] 「どこが所有するか不明」のコメントが存在しない
- [ ] 全テストが PASS する

## 注意事項
- Qt ウィジェットの `new` は parent 引数付きが正常パターン。これは移動不要
- `std::make_unique` への置換が安全な箇所から先に対応する
- 所有権の変更は破壊的なので、1箇所ずつ変更→テストのサイクルで進める
