# 09. mt_z の tapping-term-ms を60ms→80msへ引き上げ（Windows Ctrl+Z対策）

[[08_mt_z_tap_preferred]] で `mt_z`（`flavor=tap-preferred`, `tapping-term-ms=60`）を実機投入した結果、Windowsで `Ctrl+Z`（元に戻す）が効かなくなることが多いと報告された。`tapping-term-ms` を60ms→**80ms**へ引き上げて対応する。

---

## 経緯

1. `Ctrl+Z` はCtrlキーをホールドしたまま `z` をタップする操作。`mt_z` は `flavor=tap-preferred` なので、`z` の押下実時間（press〜release）が `tapping-term-ms` 未満ならtap（Z）、以上ならhold（LEFT_SHIFT）になる。
2. `tapping-term-ms=60` では、Ctrlキーを押しながらのタップ動作は通常の単独タイプよりも押下時間が伸びやすく、60msを超えてhold（LEFT_SHIFT）と誤判定される頻度が高かったと考えられる。Ctrl+Shiftが発火してしまい、Ctrl+Z（Undo）としては機能しない。
3. `tap-preferred` は後続キー押下を判定に一切使わないため、この誤判定は「zキーを押している実時間 vs tapping-term-ms」の単純な比較でしか起きない。したがって `tapping-term-ms` を伸ばせば、この種の誤判定は素直に減る。[[08_mt_z_tap_preferred]] の時点で「50〜80msの範囲で実機調整する」前提だったため、まず範囲上限の80msへ引き上げて様子を見る。

## 対応

`config/keymap.keymap` の `mt_z` の `tapping-term-ms` を 60 → **80** に変更。他のパラメータ（`flavor=tap-preferred`, `require-prior-idle-ms=150`）は変更せず、1変数のみ動かす。

```dts
mt_z: mt_z {
    compatible = "zmk,behavior-hold-tap";
    #binding-cells = <2>;
    flavor = "tap-preferred";
    tapping-term-ms = <80>;
    quick-tap-ms = <0>;
    require-prior-idle-ms = <150>;
    bindings = <&kp>, <&kp>;
};
```

---

## 効果・注意点（検証はこれから）

* Windowsで `Ctrl+Z` の成功率が上がることを期待。
* `tapping-term-ms` を伸ばした分、意図的にzを長押ししてShiftとして使う場合の反応はわずかに遅くなるが、80msは依然として短い部類でありタイピング体感への影響は小さいと見込む。
* 80msでも `Ctrl+Z` 等の修飾キー併用ショートカットで誤判定が残る場合、[[08_mt_z_tap_preferred]] で想定していた範囲（50〜80ms）の外まで伸ばす、または `quick-tap-ms` の活用や、修飾キー併用時のみ挙動を変える別の仕組み（`hold-trigger-key-positions` 等）の検討が次の選択肢になる。
