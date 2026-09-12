// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_RIGID_BODY_3D_H
#define DXF_PHYSICS_RIGID_BODY_3D_H
#include "Dxf/BodyType.h"
#include "Toolbox/Contact3D.h"
#include "Toolbox/Variant.h"
#include "Toolbox/Vector3.h"
#include "Toolbox/Quaternion.h"
#include "Toolbox/UniquePtr.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 立体剛体を識別する、世代付きの非所有ハンドル。
 */
struct FBodyId3D
{
	/**
	 * 登録先ワールドの識別子。
	 */
	Toolbox::uint64 World = 0;
	/**
	 * 登録スロットの番号。
	 */
	Toolbox::size_t Index = 0;
	/**
	 * 同じスロットを再使用した際の世代。
	 */
	Toolbox::uint64 Generation = 0;
	/**
	 * 同じ登録を指すか調べる。
	 */
	bool operator==(const FBodyId3D&) const = default;
};
/**
 * 立体剛体の初期条件。位置と姿勢は重心を基準とするメートル単位。
 */
struct FBodyDescription3D
{
	/**
	 * 運動区分。
	 */
	EBodyType Type = EBodyType::Dynamic;
	/**
	 * 重心の初期位置。単位はメートルで右手系。
	 */
	Toolbox::FVector3 Position;
	/**
	 * 初期姿勢。右手則の単位四元数。
	 */
	Toolbox::FQuaternion Orientation;
	/**
	 * 重心の初期速度。単位はメートル毎秒。
	 */
	Toolbox::FVector3 Velocity;
	/**
	 * 初期角速度。ワールド軸回りのラジアン毎秒で右手則。
	 */
	Toolbox::FVector3 AngularVelocity;
	/**
	 * Dynamicだけが使う質量。キログラム単位の有限な正値。
	 */
	Toolbox::f32 Mass = 1;
	/**
	 * Dynamicだけが使う重心回りの対角慣性。キログラム平方メートル単位の有限な正値。
	 */
	Toolbox::FVector3 DiagonalInertia{1, 1, 1};
	/**
	 * 速度の減衰率。1毎秒単位の有限な非負値。
	 */
	Toolbox::f32 LinearDamping = 0;
	/**
	 * 角速度の減衰率。1毎秒単位の有限な非負値。
	 */
	Toolbox::f32 AngularDamping = 0;
	/**
	 * ワールド重力への追従倍率。無次元の有限値。
	 */
	Toolbox::f32 GravityScale = 1;
};
/**
 * 立体コライダーを識別する、世代付きの非所有ハンドル。
 */
struct FColliderId3D
{
	/**
	 * 取り付け先の剛体。
	 */
	FBodyId3D Body;
	/**
	 * 登録スロットの番号。
	 */
	Toolbox::size_t Index = 0;
	/**
	 * 同じスロットを再使用した際の世代。
	 */
	Toolbox::uint64 Generation = 0;
	/**
	 * 同じ登録を指すか調べる。
	 */
	bool operator==(const FColliderId3D&) const = default;
};
/**
 * 剛体へ取り付ける立体形状と材質。形状の中心は重心からの相対位置。
 */
struct FColliderDescription3D
{
	/**
	 * 取り付ける形状。球または任意姿勢の箱。箱の軸は重心回りの相対回転。
	 */
	Toolbox::TVariant<Toolbox::FSphere, Toolbox::FOBB> Shape;
	/**
	 * 有限な非負の摩擦係数。
	 */
	Toolbox::f32 Friction = 0.5f;
	/**
	 * 0〜1の反発係数。
	 */
	Toolbox::f32 Restitution = 0;
};
/**
 * 接触拘束の解決設定。プロジェクトの試験条件に合わせた初期値。
 */
struct FContactSettings3D
{
	/**
	 * 許容する貫通量。メートル単位の有限な非負値。
	 */
	Toolbox::f32 ContactSlop = 0.005f;
	/**
	 * 位置補正の緩和係数。0〜1。
	 */
	Toolbox::f32 BaumgarteBeta = 0.2f;
	/**
	 * 一分割で許す最大補正量。メートル単位の有限な正値。
	 */
	Toolbox::f32 MaxCorrection = 0.05f;
	/**
	 * 反発を適用する衝突前速度。メートル毎秒単位の有限な非負値。
	 */
	Toolbox::f32 RestitutionThreshold = 1.0f;
	/**
	 * 速度拘束の反復数。1〜64。
	 */
	Toolbox::uint32 VelocityIterations = 8;
};
/**
 * 力・重力・Impulseで動く立体剛体を所有し、接触拘束を解く。
 * 単一スレッドで使用し、DxLibや描画を知らない。箱同士の組は接触を生成しない。
 */
class FPhysicsWorld3D
{
public:
	/**
	 * 空のワールドを作る。
	 */
	FPhysicsWorld3D();
	/**
	 * 登録剛体を破棄する。
	 */
	~FPhysicsWorld3D();
	/**
	 * 登録IDの混同を避けるためコピーを禁止する。
	 */
	FPhysicsWorld3D(const FPhysicsWorld3D&) = delete;
	/**
	 * 登録IDの混同を避けるためコピー代入を禁止する。
	 */
	FPhysicsWorld3D& operator=(const FPhysicsWorld3D&) = delete;
	/**
	 * 剛体を所有して登録する。不正な値は例外で通知し、状態を変更しない。
	 * @param Description 初期条件。
	 */
	FBodyId3D CreateBody(const FBodyDescription3D& Description);
	/**
	 * 登録を削除し、そのIDを失効させる。期限切れIDはfalseを返す。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool DestroyBody(FBodyId3D Id) noexcept;
	/**
	 * IDが有効な登録を指すか調べる。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool IsAlive(FBodyId3D Id) const noexcept;
	/**
	 * 重心位置を返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::FVector3 GetPosition(FBodyId3D Id) const;
	/**
	 * 姿勢を返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::FQuaternion GetOrientation(FBodyId3D Id) const;
	/**
	 * 重心速度を返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::FVector3 GetVelocity(FBodyId3D Id) const;
	/**
	 * 角速度を返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::FVector3 GetAngularVelocity(FBodyId3D Id) const;
	/**
	 * ワールド座標の角運動量を返す。非Dynamicは追跡しないためゼロ。
	 * 期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::FVector3 GetAngularMomentum(FBodyId3D Id) const;
	/**
	 * 速度を指定する。Staticへの指定と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Velocity 毎秒メートル単位の速度。
	 */
	void SetVelocity(FBodyId3D Id, Toolbox::FVector3 Velocity);
	/**
	 * 角速度を指定する。Staticへの指定と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param AngularVelocity ワールド軸回りの毎秒ラジアン単位の角速度。
	 */
	void SetAngularVelocity(FBodyId3D Id, Toolbox::FVector3 AngularVelocity);
	/**
	 * 次の更新で使う力を加算する。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Force ニュートン単位の力。
	 */
	void ApplyForce(FBodyId3D Id, Toolbox::FVector3 Force);
	/**
	 * 次の更新で使うトルクを加算する。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Torque ワールド軸回りのニュートンメートル単位のトルク。
	 */
	void ApplyTorque(FBodyId3D Id, Toolbox::FVector3 Torque);
	/**
	 * 速度へ即時反映する力積を与える。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Impulse ニュートン秒単位の力積。
	 */
	void ApplyLinearImpulse(FBodyId3D Id, Toolbox::FVector3 Impulse);
	/**
	 * 角速度へ即時反映する力積モーメントを与える。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Impulse ワールド軸回りのニュートンメートル秒単位の力積モーメント。
	 */
	void ApplyAngularImpulse(FBodyId3D Id, Toolbox::FVector3 Impulse);
	/**
	 * 重心外の点へ力積を与え、並進と回転の両方を生む。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Impulse ニュートン秒単位の力積。
	 * @param WorldPoint 力積を与えるワールド位置。メートル単位。
	 */
	void ApplyImpulseAtPoint(FBodyId3D Id, Toolbox::FVector3 Impulse, Toolbox::FVector3 WorldPoint);
	/**
	 * ワールド重力を設定する。非有限値は例外で通知する。
	 * @param Gravity メートル毎秒毎秒単位の重力加速度。
	 */
	void SetGravity(Toolbox::FVector3 Gravity);
	/**
	 * ワールド重力を返す。
	 */
	Toolbox::FVector3 GetGravity() const noexcept;
	/**
	 * コライダーを剛体へ取り付ける。不正な値は例外で通知し、状態を変更しない。
	 * @param Body 取り付け先の剛体。
	 * @param Description 取り付ける形状と材質。
	 */
	FColliderId3D AttachCollider(FBodyId3D Body, const FColliderDescription3D& Description);
	/**
	 * コライダーを外してIDを失効させる。期限切れIDはfalseを返す。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool DetachCollider(FColliderId3D Id) noexcept;
	/**
	 * 接触拘束の解決設定を変更する。不正な値は例外で通知する。
	 * @param Settings 接触拘束の解決設定。
	 */
	void SetContactSettings(const FContactSettings3D& Settings);
	/**
	 * 接触拘束の解決設定を返す。
	 */
	FContactSettings3D GetContactSettings() const noexcept;
	/**
	 * 剛体の姿勢を直接設定する。速度と蓄積力は変更しない。
	 * テレポート後は接触キャッシュの消去と補間履歴の破棄を呼び出し元が行う。
	 * 期限切れIDと非有限値は例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Position メートル単位の重心位置。
	 * @param Orientation 右手則の単位姿勢。
	 */
	void SetBodyTransform(FBodyId3D Id, Toolbox::FVector3 Position, Toolbox::FQuaternion Orientation);
	/**
	 * 前回Impulseの再利用記録を消去する。
	 */
	void ClearContactCache() noexcept;
	/**
	 * コライダーIDが有効な登録を指すか調べる。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool IsColliderAlive(FColliderId3D Id) const noexcept;
	/**
	 * 指定秒数だけ物理状態を進める。力とトルクは更新後に一度だけ消去する。
	 * 取り付け済みのコライダー同士の接触拘束も解く。箱同士の組は接触を生成しない。
	 * 非有限・非正の秒数と範囲外の分割数は例外で通知し、状態を変更しない。
	 * @param DeltaSeconds 有限な正の秒数。
	 * @param SubSteps 1〜1024の分割数。
	 */
	void Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps = 1);

private:
	/**
	 * 実装と登録データの所有領域。
	 */
	struct FImpl;
	/**
	 * 公開ヘッダーから隠した登録データ。
	 */
	Toolbox::TUniquePtr<FImpl> m_pImpl;
};
} // namespace Dxf
#endif
