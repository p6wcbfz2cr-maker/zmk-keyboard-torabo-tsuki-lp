# 05. マウスレイヤー脱出キーをもう一箇所追加（左親指Spaceキー）

`mouse` レイヤー（Layer 5）を手動で抜けるための `exit_mouse_macro` を、既存の左親指 `lt 9 TAB` の位置に加えて、左親指 `lt 2 SPACE` の位置にも追加しました。

---

## 背景

[[05_hold_mouse_layer]]（ホールド式マウスレイヤーへの全面移行）を試験導入したものの、操作性がイマイチだったため不採用とし、そのブランチ（`worktree-hold-mouse-layer`）はリモートに残したまま使わないことにした。オートマウス（トラックボール動作で `zip_temp_layer 5 5000` により自動遷移する方式）は元のまま維持する。

その上で、既存の脱出キー（`lt 9 TAB` の位置、タップで `exit_mouse_macro` が発火し `tog_off 5` でLayer5を強制解除）だけでは足りず、もう一箇所、左親指のSpaceキー（`lt 2 SPACE` の位置）からも同様に脱出できるようにしたいという要望があった。

---

## 解決策

`mouse` レイヤーの親指行（`config/keymap.keymap:79`）で、`lt 2 SPACE` に対応する位置（Mac レイヤーでは左から4番目の親指キー、行の5番目のバインド）を `&trans` から `&exit_mouse_macro` に変更した。

```dts
mouse {
    bindings = <
&trans  &trans  &trans  &trans  &trans             &trans                                                   &trans             &trans  &trans  &trans  &trans  &trans
&trans  &trans  &trans  &trans  &trans             &trans                                                   &trans             &trans  &trans  &trans  &trans  &trans
&trans  &mo 8   &mo 8   &mo 8   &mo 8              &trans             &trans             &trans             &trans             &mkp LCLK  &mkp MCLK  &mkp RCLK  &trans  &trans
&trans  &trans  &trans  &trans  &trans             &trans             &trans             &trans             &trans             &mkp LCLK  &mkp MCLK  &mkp RCLK  &trans  &trans
&trans  &trans  &trans  &trans  &exit_mouse_macro  &trans             &exit_mouse_macro  &trans             &trans     &trans     &trans     &trans  &trans  &trans
    >;
};
```

`exit_mouse_macro`（マクロ本体、`config/keymap.keymap:134-138`）は既存のものをそのまま2箇所から参照するだけで、新規定義は不要。

Mac/Winレイヤーでの `lt 2 SPACE`（Tap=Space、Hold=Number レイヤー）本来の機能には一切影響しない。`mouse` レイヤーが有効なときだけ、このオーバーライドが効く。

---

## 効果（想定）

* `mouse` レイヤーに入っている間、左親指の Tab キーだけでなく Space キーからもタップ一つで即座に抜けられるようになり、オートマウスの居座り（5秒）を待たずに手動で切り上げやすくなる。
* 実機での検証はこれから。
