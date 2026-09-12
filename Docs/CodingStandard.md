# コーディング規則

## 標準ライブラリとToolbox

Source・Examples・TestsのC++コードではSTLを使用しません。コンテナ、文字列、スマートポインタ、アルゴリズム、関数オブジェクトなどが必要になった場合は、まずToolboxに実装し、利用側からその機能を使います。`std`の型を別名にするだけの実装や、STLを内部に隠すラッパーも禁止です。既存機能と移行例は[Toolbox](Toolbox.md)を参照してください。

C++言語の構築・破棄を支える`<new>`・`<initializer_list>`と、`std::align_val_t`・`std::initializer_list`のみ、Toolbox内部で例外として使用します。OS API、Cランタイム、SIMD intrinsicはToolboxの実装境界で使用できます。DxLibの呼び出しとWindowsのエントリーポイントはNative／プラットフォーム境界に置きます。C++20、標準RTTI、C++例外は引き続き使用します。

`python Tools/CheckNoStl.py`はSource・Examples・TestsのSTLヘッダーと`std`参照を検出します。テンプレートの品質、所有権、計算量まで保証する検査ではないため、追加する基盤機能には用途に応じた動作検証も行います。

## 数値型と命名

整数は`int8`・`int16`・`int32`・`int64`、符号なし整数は`uint8`・`uint16`・`uint32`・`uint64`を使います。浮動小数点は`f32`・`f64`を使い、`FInt`のような接頭辞は付けません。配列の要素数・添字にはToolboxの`size_t`を使います。`bool`、文字・文字列の`char`／`wchar_t`、`void`は役割どおりに使用します。基本型の定義とOS ABIが要求する宣言を除き、生の`int`・`long`・`float`・`double`を新たに使いません。

数値型は`Toolbox/Utility.h`のToolbox名前空間で定義します。Toolbox外では`Toolbox::f32`のように修飾します。精度や表現範囲を考えて型を選び、境界での変換は明示します。

| 接頭辞 | 用途 | 例 |
|---|---|---|
| `A` | 新規の所有される多態オブジェクト | 既存の`DScene`等は互換性のため維持 |
| `D` | 既存のDObject系公開型 | `DScene`, `DGameObject` |
| `F` | 値、設定、ハンドル、サービス | `FApplication`, `FVector3` |
| `I` | 抽象インターフェース | `ITextureBackend` |
| `T` | テンプレート | `TVector<T>`, `TSharedPtr<T>` |
| `E` | 列挙型 | `EKey`, `ESpatialIndex` |

型・公開関数・引数・ローカル変数はPascalCaseにします。振る舞いを持つ型のメンバーは値を`m_`、所有／生ポインタを`m_p`、boolを`m_b`で始めます。設定・結果・Contextなどのデータ保持用フィールドには付けません。非所有の世代付きハンドルは値として扱います。

## コメント

既存の日本語説明に合わせて、型、関数、引数、メンバー変数、ローカル変数、定数、列挙値の役割を短く説明します。ヘッダーの型・関数・フィールドなどの宣言説明は複数行の`/** ... */`にし、関数内およびcpp内は`//`コメントにします。名前の読み替えだけでなく、その場所で何を表すかを記述します。関数の引数は関数コメント内の`@param`で説明できます。戻り値の意味、単位、所有権、失敗条件が自明でなければ一緒に記述します。

```cpp
/**
 * 再生要求に適用する音量と繰り返し設定。
 */
struct FPlaybackOptions
{
	/**
	 * 終端に達したら先頭から再生する。
	 */
	bool bLoop = false;
	/**
	 * 元の音量に掛ける0〜1の倍率。
	 */
	Toolbox::f32 Volume = 1.0f;
};

/**
 * 経過時間を上限以内に収める。
 * @param Elapsed 実際の経過秒数。
 * @param Maximum 1回の更新へ渡す最大秒数。
 */
Toolbox::f64 ClampElapsed(Toolbox::f64 Elapsed, Toolbox::f64 Maximum)
{
	// 負の経過時間を除いた更新候補。
	const Toolbox::f64 NonNegative = Toolbox::Max(Elapsed, 0.0);
	return Toolbox::Min(NonNegative, Maximum);
}
```

## レイアウトと設計

明示的なインライン関数には`Toolbox/Compiler.h`の`FORCEINLINE`を使います。クラス内定義や`constexpr`関数も、短い値の取得・比較・変換・配列アクセスなどには適用します。ループ、資源確保・解放、仮想関数、長い処理は一律に強制展開しません。変数の`inline`は共有定義のための指定なのでそのまま残します。`FORCEINLINE`はMSVCでは`__forceinline`、GCC／Clangでは`inline`と`always_inline`属性を使用し、それ以外では`inline`へ切り替わります。既存のプラットフォーム定義がある場合は尊重します。展開の可否はコンパイラが判断するため、速度向上を保証する指定ではありません。

波括弧はAllman、インデントはタブ（表示幅4）です。複数の処理・宣言を1行に詰めず、短い関数・if・ループも展開します。初期化リストや引数列のカンマは許容しますが、`f32 X = 0, Y = 0;`のような変数宣言は分けます。設定はルートの`.clang-format`と`.editorconfig`を使います。

公開型は原則1主要型1ヘッダー、通常実装は同名cpp、テンプレート実装は同名ヘッダーまたはinlへ置きます。新しいヘッダーはSPDXとinclude guardで始め、PublicとPrivateを分離します。cppでは対応する公開ヘッダーを先頭にincludeします。

コピーで所有関係が壊れる型はコピー禁止にします。基底経由で破棄する型にはvirtualデストラクタを置きます。個々のオブジェクトは基本的に単独所有、実際に共有する資源だけ共有所有にします。STL撤廃を理由に所有・入力・モジュール構成を不用意に変更しません。

基盤専用のディスパッチ入口は`_Internal`を付けます。`OnInitialize`・`OnTick`・`OnDraw`・`OnDeinitialize`・`OnEnter`・`OnExit`はユーザー拡張点なので付けません。コンストラクタ・デストラクタ・演算子にも付けません。

`OnDeinitialize`・`OnEnter`・`OnExit`はnoexcept契約です。それ以外のユーザーフックの例外はApplication境界などでエラーへ変換します。一般の構築やメモリ確保まで例外を投げないAPIではありません。
