# 06. mt_idle と &lt の flavor を hold-preferred に変更（balanced/tap-preferred の判定不安定を解消）

Zキー（`&mt_idle LEFT_SHIFT Z`）でShift+文字が意図通り出ない問題（「zs」になる、「ー」のつもりがNumberレイヤー経由で`j`になる等）について、`flavor="balanced"` が割り込みキーとの相対タイミング（どちらが先に離れるか）で判定するため、ほぼ同時押しだとジッターで結果が不安定になる可能性が高いと判断した。`mt_idle` の `flavor` を `hold-preferred` に変更し、同じ理屈が当てはまるプレーンな `&lt`（`lt_idle`とは別、こちらは変更しない）にも、新規のグローバルoverrideで `flavor="hold-preferred"` を適用した。

---

## 経緯

1. [[01_z_a_shift_ctrl_idle]] で `mt_idle`（`flavor=balanced`）を導入。高速タイピング中の誤爆防止として `require-prior-idle-ms` を追加した。
2. [[05_mt_idle_require_prior_idle_tuning]] で `require-prior-idle-ms` を150ms→80msへ調整し実機投入。末尾の「次の調整候補」として `flavor`（balanced→hold-preferred寄り）の調整を既に候補に挙げていた。
3. 80ms調整の投入後も、専用Shiftキーを持つ別キーボードでは同じ速度で打っても問題が出ないとの証言があり、技量の問題ではなく `flavor="balanced"` の判定方式そのものが原因である可能性が高いと判断した。balancedは「割り込みキーとホールドキーのどちらが先に離れるか」で判定するため、Shift+Zのようなほぼ同時押しでは、わずかなタイミングのブレで判定結果が入れ替わりうる。
4. `hold-preferred` は「ホールド対象キーを押している間に別のキーが押された（割り込みが発生した）時点」で即座にHoldへ確定する。離す順序に依存しないため、balancedが抱えるジッターの影響を受けにくくなる。
5. 同じ判定不安定の理屈は、プレーンな `&lt`（`&lt 2 SPACE` 等のレイヤータップ全般）にも当てはまる。`&lt` は現状ZMK既定のまま（グローバルoverride無し＝`flavor=tap-preferred`）で運用されているが、tap-preferredも「割り込み時、tapping-term経過までは基本Tap優先」というタイミング依存の判定方式である点は変わらない。ユーザーの合意のもと、`mt_idle` と `&lt` の両方のflavorを揃えて変更する1セットの変更として扱う。

## 対応

### 1. `mt_idle` のflavor変更（`config/keymap.keymap:151`）

```dts
mt_idle: mt_idle {
    compatible = "zmk,behavior-hold-tap";
    #binding-cells = <2>;
    flavor = "hold-preferred";
    tapping-term-ms = <250>;
    quick-tap-ms = <0>;
    require-prior-idle-ms = <80>;
    bindings = <&kp>, <&kp>;
};
```

`tapping-term-ms=250`, `require-prior-idle-ms=80` は据え置き。変更は `flavor` の1変数のみで、効果の切り分けをしやすくした。

### 2. `&lt` へのグローバルoverride新規追加（`config/keymap.keymap`、`&mt {}` の直後）

```dts
&lt {
    tapping-term-ms = <200>;
    flavor = "hold-preferred";
};
```

ZMK標準の `&lt` は `flavor` 既定が `tap-preferred`、`tapping-term-ms` 既定が200ms。`tapping-term-ms` は既定と同値だが、`&mt` overrideとの対比を明示するためあえて記載した。適用対象は `&lt 2 SPACE`, `&lt 9 TAB`, `&lt 3 SPACE`, `&lt 6 ENTER`, `&lt 7 ENTER` の5箇所（`;`キー用の `lt_idle` は別ビヘイビアであり対象外、今回変更なし）。

---

## 効果（想定・検証はこれから）

* Zキー等でShiftホールドの意図がタイミングのジッターで潰されにくくなり、「zs」「ー→j」のような意図しないTap化が減ることを期待。
* トレードオフ: 高速なロールオーバー打鍵時に、意図しないHold（Shiftの誤発動、レイヤーの誤起動）が増える可能性がある。ユーザーは現時点でこちらの失敗（誤ホールド側）を問題視していないため、許容した上で投入する。
* `&lt` 側は、Space/Tab/Enterの各キーで「タップ直後に次のキーを素早く重ねる」と、まれに意図せずレイヤーがホールド確定してしまう可能性がある点は要観察（特に `&lt 9 TAB` はトラックボールジェスチャーのトリガーも兼ねるため、ジェスチャーの誤発火が増えないかも合わせて確認）。
* 今回は `mt_idle` と `&lt` の `flavor` 変更のみを1セットとして投入し、`tapping-term-ms` や `require-prior-idle-ms` など他パラメータは変更していない。改善が不十分、または誤ホールドの副作用が大きい場合は、次の調整候補として `tapping-term-ms` の調整（250→200ms程度）や `flavor="tap-unless-interrupted"` への変更を検討する。
