# 08. z キーの mt_idle 再導入（専用ビヘイビア `mt_z`、flavor=tap-preferred）

Macレイヤーの `z` キー（現状 `&kp Z` の単純キー）を、tap=Z / hold=LEFT_SHIFT の hold-tap に戻したいという要望を受け、`mt_idle` とは別の専用ビヘイビア `mt_z`（`flavor = "tap-preferred"`）を新設して再導入する。

---

## 経緯

1. `z` に hold-tap（tap=Z, hold=LEFT_SHIFT）を割り当てる設定は、実は過去に `mt_idle LEFT_SHIFT Z` として一度実装されていたものであり、[[06_mt_idle_lt_flavor_hold_preferred]]・[[07_mt_idle_require_prior_idle_re_raise]] で分析・調整対象になっていた設定そのものである。最終的にコミット `9e8520a`「make Mac z key a plain keypress」で `&kp Z`（単純キー）へ明示的に差し戻された。理由は次の2点:
   - ローマ字入力のロールオーバー（z+a/z+u/z+o → 「ざ/ず/ぞ」）がShift+母音に誤爆する（07番で報告された問題そのもの）。
   - 左Shiftが既に左親指キー（`&mt LEFT_SHIFT TAB`, position 57）にあり、`z` に割り当てると重複する。
2. 今回、「新しいflavorであれば誤爆パターンが解消されるのでは」という仮説から再検討したが、ZMK公式ドキュメント（hold-tap behaviorのFlavorsセクション）を確認した結果、次の点が判明した:
   - **hold-preferred**: `tapping-term-ms` 満了 **または** 別キーが押された時点でhold。
   - **balanced**: `tapping-term-ms` 満了 **または** 別キーが押されて**かつ離された**時点でhold。
   - **tap-preferred**: `tapping-term-ms` 満了時のみhold。別キー押下は判定に一切影響しない。
   - **tap-unless-interrupted**: `tapping-term-ms` 満了前に別キーが押された場合のみhold。それ以外はtap。
   - このうち `hold-preferred` / `balanced` / `tap-unless-interrupted` の3つは、いずれも「別キーが押されたらhold」という経路を持つ。ローマ字ロールオーバー（zの直後に母音キーが押される）はまさにこの経路を通るため、**この3 flavorではローマ字ロールオーバー誤爆を原理的に避けられない**。
   - 誤爆を避けられる可能性があるのは、後続キーの押下を一切判定に使わない `tap-preferred` のみ。ただし `tap-preferred` は `tapping-term-ms` 満了でしかholdを発動しないため、意図的なホールドの反応がその分遅くなるトレードオフがある。
3. 上記を踏まえ、`tapping-term-ms` を短く（50〜80ms程度）設定した `tap-preferred` の専用ビヘイビアであれば、「zキーを押している実時間」が閾値を境に判定されるだけになり、通常の高速タイピング（zをすぐ離す）ではtap、意図的な長押しではholdになりやすいのではないか、という仮説のもとユーザーと合意し、実験的に導入する。誤爆の完全解消は期待できないが、体感上ストレスの少ないバランスを狙う。
4. 左Shiftの重複割当（`z` と position 57 の左親指キー）は、今回は許容した上で進める。

## 対応

`mt_idle` 本体（`a`/`/`/`N1`/`N6`/`F6`/`F11`）の flavor は変更しない（`hold-preferred` のまま据え置き）。`z` 専用に新規ビヘイビア `mt_z` を追加し、Macレイヤーの `z` のバインディングをこれに差し替える。

```dts
mt_z: mt_z {
    compatible = "zmk,behavior-hold-tap";
    #binding-cells = <2>;
    flavor = "tap-preferred";
    tapping-term-ms = <60>;
    quick-tap-ms = <0>;
    require-prior-idle-ms = <150>;
    bindings = <&kp>, <&kp>;
};
```

`config/keymap.keymap` の Mac レイヤーで `&kp Z` → `&mt_z LEFT_SHIFT Z` に変更（`config/keymap.keymap:28`）。Win/Number/Function レイヤーの同ポジションは変更不要（Winは`&trans`で継承、Number/Functionは元々 `mt_idle LEFT_SHIFT N6`/`F11` のまま）。

---

## 効果・注意点（検証はこれから）

* `tapping-term-ms = <60>` は暫定値。50〜80msの範囲で実機タイピングしながら調整する前提。短すぎると通常のZタイプでも誤ってhold判定されるリスクが増え、長すぎると意図的なホールドが遅く感じられる。
* `require-prior-idle-ms = <150>`（`mt_idle` と同値）は、直前キーとの間隔が短ければ強制的にtap扱いになる安全網として引き続き機能する。
* ローマ字ロールオーバー（za/zu/zo等）の誤爆は、`tap-preferred` を使ってもZキーを実際に押している時間が `tapping-term-ms` を超えてしまえば発生し得る。完全解消は狙っておらず、体感での許容可否を実機で確認する。
* 左Shiftの重複割当（`z` と左親指 `&mt LEFT_SHIFT TAB`）は既知のトレードオフとして許容している。
