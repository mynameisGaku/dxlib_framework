// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_CONTENT_SCHEMA_READER_H
#define DXF_CONTENT_SCHEMA_READER_H
#include "Dxf/SceneContentParser.h"
namespace Dxf::ContentPrivate
{
/**
 * 一つの文書だけで使う純粋な型・位置検査。Worldや資源を変更しない。
 */
class FSchemaReader
{
public:
	/**
	 * Documentを借り、PathとLimitsを診断と読取制限へ保持する。
	 */
	FSchemaReader(const Toolbox::FJsonDocument& Document, Toolbox::FString Path, FSceneContentLimits Limits);
	/**
	 * Indexの位置とReasonを診断へ保存して例外を投げる。
	 */
	[[noreturn]] void Fail(Toolbox::int32 Index, const char* Reason) const;
	/**
	 * ObjectのKeyを探す。不在は負値、Objectの型違いは例外。
	 */
	Toolbox::int32 Find(Toolbox::int32 Object, const char* Key) const;
	/**
	 * Objectの必須Keyを探す。不在または型違いは例外。
	 */
	Toolbox::int32 Required(Toolbox::int32 Object, const char* Key) const;
	/**
	 * Objectの項目名をKeysと照合する。未知項目は例外。
	 */
	void Fields(Toolbox::int32 Object, Toolbox::TInitializerList<const char*> Keys) const;
	/**
	 * Indexが配列であることを検査する。型違いは例外。
	 */
	void Array(Toolbox::int32 Index) const;
	/**
	 * Indexの文字列を値で返す。型違いと途中のNULは例外。
	 */
	Toolbox::FString String(Toolbox::int32 Index) const;
	/**
	 * IndexのIDまたは資源パラメーターを読み、ASCIIと長さを検査する。
	 */
	Toolbox::FString Id(Toolbox::int32 Index) const;
	/**
	 * Indexのキーを論理IDとして検査する。
	 */
	Toolbox::FString KeyId(Toolbox::int32 Index) const;
	/**
	 * Indexのパスを読む。曖昧な絶対指定とRoot外の参照は例外。
	 */
	Toolbox::FString Path(Toolbox::int32 Index) const;
	/**
	 * Indexの有限数値または型付き数値パラメーターを返す。
	 */
	Toolbox::f64 Number(Toolbox::int32 Index) const;
	/**
	 * Indexの数値をf32へ変換する。範囲外は例外。
	 */
	Toolbox::f32 Scalar(Toolbox::int32 Index) const;
	/**
	 * Indexの整数を読み、Maximum以下であることを検査する。
	 */
	Toolbox::uint32 Integer(Toolbox::int32 Index, Toolbox::uint32 Maximum) const;
	/**
	 * Indexの真偽値または型付きパラメーターを返す。
	 */
	bool Boolean(Toolbox::int32 Index) const;
	/**
	 * Indexの二成分を有限な値として読む。型と成分数の違いは例外。
	 */
	Toolbox::FVector2 Vector2(Toolbox::int32 Index) const;
	/**
	 * Indexの三成分を有限な値として読む。型と成分数の違いは例外。
	 */
	Toolbox::FVector3 Vector3(Toolbox::int32 Index) const;
	/**
	 * IndexのQuaternionを検査して正規化する。零と非有限値は例外。
	 */
	Toolbox::FQuaternion Rotation(Toolbox::int32 Index) const;
	/**
	 * Indexの色を読む。各成分は0から255まで。
	 */
	FColor Color(Toolbox::int32 Index) const;
	// 共通Frameの座標系だけをSceneの接続読解で切り替える。
	/**
	 * Spaceを共通Frameの読取規約へ設定する。呼出し中は文字列が生存すること。
	 */
	void SetFrameSpace(const char* Space) noexcept
	{
		m_FrameSpace = Space;
	}
	/**
	 * 現在の共通Frameの座標系名を借用参照で返す。
	 */
	const char* GetFrameSpace() const noexcept
	{
		return m_FrameSpace;
	}
	/**
	 * Indexに対応するJSON値を借用参照で返す。範囲外は例外。
	 */
	const Toolbox::FJsonValue& Get(Toolbox::int32 Index) const;
	/**
	 * Indexの宣言とOverridesを検証し、この読取だけの値を作る。
	 */
	void Parameters(Toolbox::int32 Index, const Toolbox::TVector<FContentParameterValue>& Overrides);
	/**
	 * 検証済みの個体用パラメーターを借用参照で返す。
	 */
	const Toolbox::TVector<FContentParameterValue>& GetParameters() const noexcept;

private:
	/**
	 * Textの各文字と長さを検査する。Indexは失敗の診断位置。
	 */
	Toolbox::FString ValidateId(Toolbox::FString Text, Toolbox::int32 Index) const;
	/**
	 * Indexの参照をKindの値へ解決する。非参照はnull、未知名や型違いは例外。
	 */
	const FContentParameterValue* Parameter(Toolbox::int32 Index, EContentParameterKind Kind) const;
	/**
	 * 共通Frameの座標系名。
	 */
	const char* m_FrameSpace = "Prefab";
	/**
	 * 読取中に借用するJSON文書。
	 */
	const Toolbox::FJsonDocument& m_Document;
	/**
	 * 失敗時に表示する論理パス。
	 */
	Toolbox::FString m_Path;
	/**
	 * 一要求の読取と展開の制限。
	 */
	FSceneContentLimits m_Limits;
	/**
	 * 個体だけに適用する検証済み値。
	 */
	Toolbox::TVector<FContentParameterValue> m_Parameters;
};
} // namespace Dxf::ContentPrivate
#endif
