# 11. 右手LANGUAGE_1キーをFunctionレイヤーのhold-tap化

Macレイヤーの右手下から2段目・一番左のキー（現状 `&kp LANGUAGE_1` の単純キー、`config/keymap.keymap:28`）を、tap=LANGUAGE_1 / hold=Functionレイヤー（Layer 3）のlayer-tapに変更する。

---

## 経緯

Functionレイヤー（Layer 3）へは、これまで右親指の `&lt 3 SPACE`（`config/keymap.keymap:29`）からのみ入れた。右手側のLANGUAGE_1キーからも同じレイヤーへ入れるようにしたいという要望を受けて追加する。

## 対応

`config/keymap.keymap` のMacレイヤーで `&kp LANGUAGE_1` → `&lt 3 LANGUAGE_1` に変更（`config/keymap.keymap:28`）。既存の `&lt` グローバルoverride（`flavor=hold-preferred`, `tapping-term-ms=200`、[[06_mt_idle_lt_flavor_hold_preferred]]参照）がそのまま適用される。

Win/Number/Functionレイヤーの同ポジションはいずれも `&trans` でMacの値を継承しているため、変更は不要（Winでも同じ`&lt 3 LANGUAGE_1`が使われる。Function自身のこの位置がtransなのは、Layer3が既に有効な状態でこのキーを押しても実害がないため問題ない）。

---

## 注意点

* `torabo_tsuki_lp_layouts.dtsi`の`position_map_l_1`（Lサイズのposition対応表）から逆算すると、このLANGUAGE_1キーは**position 45**（行3=`&kp N`(46)/`&kp M`(47)/`&kp COMMA`(48)/`&kp DOT`(49)/`mt_idle RIGHT_SHIFT SLASH`(50)の並びから、その1つ左と特定）にあたる。
* `zip_temp_layer`（`&pointing_listener`内、auto-mouseレイヤーのタイムアウト解除）の`excluded-positions`（L/M/Sサイズいずれのリストにも45相当の位置は含まれない）にこの位置は含まれていない。[[exit_mouse_macroの既知の競合]]と同様、mouseレイヤーが有効な状態でこのキーを押すと、hold判定が確定する前にauto-mouseレイヤーが即座に解除され、下位レイヤー（Mac/Win）側のtap側（LANGUAGE_1）だけが実行される可能性がある。通常のタイピング中（mouseレイヤー非有効時）は影響なし。
* `combos`の`bt_clear`（`key-positions = <28 29>` = 27行目の`F`/`G`、Layer 2限定）とはposition番号が異なり無関係であることを確認済み。
