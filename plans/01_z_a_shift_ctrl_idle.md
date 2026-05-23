# 01. タイピングエリア内のMod-Tap誤爆防止（require-prior-idle-ms）の設定

高速タイピング中に修飾キー付きタップ（Mod-Tap）のキーを押した際、意図せず修飾キー（ShiftやCtrl）がホールドされたと判定される「誤爆」を防ぐため、キー入力がない状態（idle状態）のときのみホールドを有効化する設定を導入しました。

---

## 課題
ZMKのMod-Tap (`&mt`) はタップと長押しを判別しますが、タイピング速度が非常に速いと、通常のアルファベットを打つ際にキー同士の重なりが「長押し（Hold）」と判定されてしまい、不要な大文字が入力されたりショートカットが暴発したりする現象が発生していました。

* 対象キー: `z` (左Shift), `a` (右Ctrl), `;` (右Ctrl), `/` (右Shift) など

---

## 解決策

### 1. カスタムMod-Tapビヘイビア `mt_idle` の定義
親指側のMod-Tap（`&mt LEFT_GUI LANGUAGE_2` など）は、文字入力の直後に操作することが多いため、誤爆防止のディレイを入れると快適性が損なわれます。
そのため、親指には影響を与えず、タイピングエリア内のキーにだけ個別に設定できるよう、カスタムの `hold-tap` ビヘイビア `mt_idle` を定義しました。

`require-prior-idle-ms = <125>;` を設定することで、「前にキーが押されてから125ミリ秒以上の空き（アイドル状態）がある時だけ、長押しを有効にする（125ms未満の高速打鍵時は長押ししても強制的にタップ判定にする）」という動作を実現しました。

```dts
    behaviors {
        mt_idle: mt_idle {
            compatible = "zmk,behavior-hold-tap";
            #binding-cells = <2>;
            flavor = "balanced";
            tapping-term-ms = <200>;
            quick-tap-ms = <0>;
            require-prior-idle-ms = <125>;
            bindings = <&kp>, <&kp>;
        };
    };
```

### 2. キーマップの書き換え
タイピングエリアにあるMod-Tapキーを `&mt` から `&mt_idle` に置き換えました。

* **Mac レイヤー**: `a` (RIGHT_CONTROL), `z` (LEFT_SHIFT), `;` (RIGHT_CONTROL), `/` (RIGHT_SHIFT)
* **Number レイヤー**: `1` (RIGHT_CONTROL), `6` (LEFT_SHIFT)
* **Function レイヤー**: `F6` (RIGHT_CONTROL), `F11` (LEFT_SHIFT)

---

## 効果と検証
* 高速打鍵時におけるShift/Ctrlの意図しないホールド誤判定がほぼゼロになりました。
* 修飾キーとして長押し入力したい場合は、少し意識して一瞬間（125ms以上）空けてから押すことで、変わらず正常に入力可能です。
* 125msというディレイ値は、タイピング速度に合わせて好みに応じて変更可能です（推奨：100ms〜150msの範囲）。
