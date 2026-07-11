# 05. mt_idle の require-prior-idle-ms チューニング（150ms → 80ms）

Zキー（`&mt_idle LEFT_SHIFT Z`）のHold/Tap誤判定について、実機での切り分けテストの結果を踏まえ、`mt_idle` の `require-prior-idle-ms` を 150ms から 80ms に下げた。

---

## 経緯

Zキーが「Holdとして期待するときにzとして出力されたり、その逆もある」という報告があり、[[01_z_a_shift_ctrl_idle]] で導入した `mt_idle`（`flavor=balanced`, `tapping-term-ms=250`, `require-prior-idle-ms=150`）の挙動を実機で切り分けた。

1. **十分に間隔を空けてZを長押し** → Shiftとして正しく機能。`require-prior-idle-ms` のゲートが想定通り効いていることを確認。
2. **Zホールド中にJを押す** → 大文字「J」が出力され、押しっぱなしならJJJ…と連続。Shiftが正しくホールドされている状態でJのOSキーリピートが起きているだけで、正常な物理Shiftキーと同じ挙動。
3. **Jを押したままZだけ離す** → 以降のリピートは小文字jに切り替わる。これもShiftを離せば以降の入力が小文字に戻るという、物理キーボードと同じ正常な挙動。

つまりテスト1〜3はいずれも「間隔を意識的に空けた」好条件下での確認であり、`mt_idle` 自体の判定ロジックにバグは見当たらなかった。

一方、ユーザーからは「普段のタイピングでは、Shiftのつもりが誤ってzになる（ホールド見逃し）」方が主な悩みだという申告があった。これは、前の単語を打ち終えてすぐ次の単語の頭文字をShift+Zで打とうとした際、直前キー（スペース等）からの間隔が `require-prior-idle-ms = 150ms` 未満になりやすく、意図したホールドがそもそも判定の土俵に上がれず強制タップになっているケースが濃厚と判断した。

---

## 対応

`require-prior-idle-ms` を 150ms → **80ms** に変更（`config/keymap.keymap:154`）。他のパラメータ（`tapping-term-ms=250`, `flavor=balanced`）は今回は変更せず、1変数だけを動かして効果を切り分けやすくした。

```dts
mt_idle: mt_idle {
    compatible = "zmk,behavior-hold-tap";
    #binding-cells = <2>;
    flavor = "balanced";
    tapping-term-ms = <250>;
    quick-tap-ms = <0>;
    require-prior-idle-ms = <80>;
    bindings = <&kp>, <&kp>;
};
```

`mt_idle` は Z だけでなく `a`（右Ctrl）、Number/Functionレイヤーの同ポジションキーにも共通で使われるビヘイビアのため、この変更は他の対象キーにも同様に効く（誤ホールドがやや増える方向）。ただし、それらのキーで問題報告は無かったため、大きな悪化は想定していない。

---

## 効果（想定・検証はこれから）

* 直前キーのすぐ後でも、Zを意図的にホールドすればShiftとして拾われやすくなるはず。
* トレードオフとして、高速タイピング中のロールオーバーによる誤ホールド（zのつもりでShiftが暴発）がやや増える可能性がある。ユーザーからは元々こちらの失敗は問題になっていないとの申告だが、実機で悪化しないか要確認。
* 改善が不十分な場合は、次のステップとして `tapping-term-ms`（250→200ms程度）や `flavor`（balanced→hold-preferred寄り）の調整も検討する。
