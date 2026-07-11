# 07. mt_idle の require-prior-idle-ms を80ms→150msへ再度引き上げ（hold-preferred化後のロールオーバー誤爆対策）

[[06_mt_idle_lt_flavor_hold_preferred]] で `mt_idle` の `flavor` を `balanced` から `hold-preferred` に変更し実機投入した結果、当初のShiftホールド見逃し問題（「zs」になる等）は解消したが、新たに別の誤爆が発生した。`require-prior-idle-ms` を80ms→150msへ再度引き上げることで対応する。

---

## 経緯

1. [[06_mt_idle_lt_flavor_hold_preferred]] の実機テストで、Shift+Zの大文字化がうまく出力されない問題（誤タップ）は解消されたことをユーザーが確認した。
2. 一方で、日本語ローマ字入力の「ざ」「ず」「ぞ」（= z+a, z+u, z+o）で誤爆（Shift+母音になってしまう）が発生する新しい問題が報告された。さらに、「zやaの後にくるキーが無視される」現象も新たに確認された。後者はflavorをbalancedにしていた際には発生しておらず、hold-preferred化後に初めて気づいたとのこと。
3. `hold-preferred` は「ホールド対象キーを押している間に別のキーが押された時点」で即座にHold確定する（ZMK公式ドキュメント参照）。ローマ字入力ではZの直後に母音キーが連続するロールオーバー入力が非常に高頻度で発生するため、`require-prior-idle-ms` のゲートさえ通過すれば（直前キーとの間隔が80ms以上あれば）ほぼ確実にHold（Shift+母音）と誤判定されてしまう構造だったと判断した。
4. `require-prior-idle-ms` は「直前の非modifierキー入力からこの時間未満なら強制的にTap」というゲート。値を大きくするほど誤ホールドを抑制しやすくなる（逆に小さくするほど誤ホールドが増える）というのがZMKの仕様であり、80msへ下げていた [[05_mt_idle_require_prior_idle_tuning]] の調整とは逆方向の変更になる。ただし [[05_mt_idle_require_prior_idle_tuning]] で80msへ下げた目的（「直前キーのすぐ後でも意図的なホールドを取りこぼさない」）は、flavorがbalancedのままだと `require-prior-idle-ms` を長くするとホールド自体が検出されにくくなるというトレードオフがあったのに対し、`flavor=hold-preferred` の現在は「ゲートさえ通過すれば割り込みで即Hold」なので、`require-prior-idle-ms` を長くしても意図的なホールドの検出力自体はflavor側が担保する。そのため、`require-prior-idle-ms` は素直に「ロールオーバー保護」の役割へ寄せて調整して問題ないと判断した。
5. 「後続キーが無視される」現象については、balancedのときには発生していなかったことから、hold-preferred化（または`&lt`へのグローバルoverride追加）が引き金になっている可能性が高いが、原因は未特定。今回はまず `require-prior-idle-ms` の引き上げのみを1変更として投入し、効果を見てから必要であれば別途切り分ける。

## 対応

`require-prior-idle-ms` を 80ms → **150ms**（元の値）に変更（`config/keymap.keymap:157`）。他のパラメータ（`flavor=hold-preferred`, `tapping-term-ms=250`）は変更せず、1変数のみ動かす。

```dts
mt_idle: mt_idle {
    compatible = "zmk,behavior-hold-tap";
    #binding-cells = <2>;
    flavor = "hold-preferred";
    tapping-term-ms = <250>;
    quick-tap-ms = <0>;
    require-prior-idle-ms = <150>;
    bindings = <&kp>, <&kp>;
};
```

`&lt` のグローバルoverride（`flavor=hold-preferred`, `tapping-term-ms=200`）は今回変更しない。

---

## 効果（想定・検証はこれから）

* ローマ字入力のロールオーバー（za/zu/zo等）で、直前キーからの間隔が150ms未満の高速タイピング中は強制的にTapとなり、「ざ」「ず」「ぞ」の誤爆が減ることを期待。
* `flavor=hold-preferred` により、意図的にZを単独で長押しする場合（直前キーから150ms以上空いていれば、割り込みキーが来た時点で即Hold）は引き続き正しく検出されるはずで、当初のShift見逃し問題（05番以前の状態）には後退しないと見込む。
* 「後続キーが無視される」現象がこの変更で改善するかは未検証。改善しない場合、flavor自体（hold-preferred）かhold-tapの実装上の別要因（キャプチャイベントの処理順序等）を疑い、次のステップとして `&lt` 側のoverride見直しや、`hold-trigger-key-positions` 等の高度な設定の検討、あるいはflavorをbalanced寄りに戻す選択肢も視野に入れる。
* 今回も1変数のみの変更とし、複数パラメータを同時に動かさない方針を継続する。
