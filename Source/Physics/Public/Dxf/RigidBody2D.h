// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_RIGID_BODY_2D_H
#define DXF_PHYSICS_RIGID_BODY_2D_H
#include "Dxf/BodyType.h"
#include "Toolbox/Contact2D.h"
#include "Toolbox/Variant.h"
#include "Toolbox/UniquePtr.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 平面剛体を識別する、世代付きの非所有ハンドル。
 */
struct FBodyId2D
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
	bool operator==(const FBodyId2D&) const = default;
};
/**
 * 平面剛体の初期条件。位置は重心を基準とするメートル単位。
 */
struct FBodyDescription2D
{
	/**
	 * 運動区分。
	 */
	EBodyType Type = EBodyType::Dynamic;
	/**
	 * 重心の初期位置。単位はメートルでY軸が上向き。
	 */
	Toolbox::FVector2 Position;
	/**
	 * 初期姿勢。ラジアン単位で反時計回りが正。
	 */
	Toolbox::f32 Angle = 0;
	/**
	 * 重心の初期速度。単位はメートル毎秒。
	 */
	Toolbox::FVector2 Velocity;
	/**
	 * 初期角速度。ラジアン毎秒で反時計回りが正。
	 */
	Toolbox::f32 AngularVelocity = 0;
	/**
	 * Dynamicだけが使う質量。キログラム単位の有限な正値。
	 */
	Toolbox::f32 Mass = 1;
	/**
	 * Dynamicだけが使う重心回りの慣性。キログラム平方メートル単位の有限な正値。
	 */
	Toolbox::f32 Inertia = 1;
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
	/**
	 * 連続衝突で移動区間を調べるか。高速なDynamicに指定する。
	 */
	bool bUseContinuous = false;
	/**
	 * 速度が落ちた休止を許可するか。
	 */
	bool bAllowSleep = true;
};
/**
 * 平面コライダーを識別する、世代付きの非所有ハンドル。
 */
struct FColliderId2D
{
	/**
	 * 取り付け先の剛体。
	 */
	FBodyId2D Body;
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
	bool operator==(const FColliderId2D&) const = default;
};
/**
 * 剛体へ取り付ける平面形状と材質。形状の中心は重心からの相対位置。
 */
struct FColliderDescription2D
{
	/**
	 * 取り付ける形状。円または回転矩形。
	 */
	Toolbox::TVariant<Toolbox::FCircle2D, Toolbox::FOrientedBox2D> Shape;
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
struct FContactSettings2D
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
 * 連続衝突の反復設定。
 */
struct FContinuousSettings2D
{
	/**
	 * 移動区間の接触解決を行うか。
	 */
	bool bEnabled = false;
	/**
	 * 一分割の接触解決回数。1〜32。
	 */
	Toolbox::uint32 MaxIterations = 4;
	/**
	 * 進行とみなす最小秒数。有限な非負値。
	 */
	Toolbox::f64 MinAdvanceSeconds = 1e-9;
};
/**
 * 直近更新の連続衝突診断。未処理時間は保守停止で残した秒数。
 */
struct FContinuousDiagnostics2D
{
	/**
	 * 接触走査の実行回数。
	 */
	Toolbox::uint32 ToiIterations = 0;
	/**
	 * 解決した最初接触の回数。
	 */
	Toolbox::uint32 HitsResolved = 0;
	/**
	 * 対象外で離散処理へ回した組数。
	 */
	Toolbox::uint32 FallbackPairs = 0;
	/**
	 * 保守停止で進めなかった秒数。
	 */
	Toolbox::f64 UnprocessedSeconds = 0;
};
/**
 * 休止の条件。プロジェクトの試験条件に合わせた初期値。
 */
struct FSleepSettings2D
{
	/**
	 * 速度低下による休止を行うか。
	 */
	bool bEnabled = true;
	/**
	 * 休止までの接触継続秒数。有限な正値。
	 */
	Toolbox::f32 TimeoutSeconds = 0.5f;
	/**
	 * 休止可能な速度。メートル毎秒単位の有限な非負値。
	 */
	Toolbox::f32 LinearSpeedLimit = 0.05f;
	/**
	 * 休止可能な角速度。ラジアン毎秒単位の有限な非負値。
	 */
	Toolbox::f32 AngularSpeedLimit = 0.05f;
};
/**
 * 力・重力・Impulseで動く平面剛体を所有し、接触拘束を解く。
 * 単一スレッドで使用し、DxLibや描画を知らない。
 */
class FPhysicsWorld2D
{
public:
	/**
	 * 空のワールドを作る。
	 */
	FPhysicsWorld2D();
	/**
	 * 登録剛体を破棄する。
	 */
	~FPhysicsWorld2D();
	/**
	 * 登録IDの混同を避けるためコピーを禁止する。
	 */
	FPhysicsWorld2D(const FPhysicsWorld2D&) = delete;
	/**
	 * 登録IDの混同を避けるためコピー代入を禁止する。
	 */
	FPhysicsWorld2D& operator=(const FPhysicsWorld2D&) = delete;
	/**
	 * 剛体を所有して登録する。不正な値は例外で通知し、状態を変更しない。
	 * @param Description 初期条件。
	 */
	FBodyId2D CreateBody(const FBodyDescription2D& Description);
	/**
	 * 登録を削除し、そのIDを失効させる。期限切れIDはfalseを返す。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool DestroyBody(FBodyId2D Id) noexcept;
	/**
	 * IDが有効な登録を指すか調べる。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool IsAlive(FBodyId2D Id) const noexcept;
	/**
	 * 重心位置を返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::FVector2 GetPosition(FBodyId2D Id) const;
	/**
	 * 姿勢角をラジアンで返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::f32 GetAngle(FBodyId2D Id) const;
	/**
	 * 重心速度を返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::FVector2 GetVelocity(FBodyId2D Id) const;
	/**
	 * 角速度を返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::f32 GetAngularVelocity(FBodyId2D Id) const;
	/**
	 * 重心回りの角運動量を返す。非Dynamicは追跡しないためゼロ。
	 * 期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::f32 GetAngularMomentum(FBodyId2D Id) const;
	/**
	 * 速度を指定する。Staticへの指定と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Velocity 毎秒メートル単位の速度。
	 */
	void SetVelocity(FBodyId2D Id, Toolbox::FVector2 Velocity);
	/**
	 * 角速度を指定する。Staticへの指定と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param AngularVelocity 毎秒ラジアン単位の角速度。
	 */
	void SetAngularVelocity(FBodyId2D Id, Toolbox::f32 AngularVelocity);
	/**
	 * 次の更新で使う力を加算する。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Force ニュートン単位の力。
	 */
	void ApplyForce(FBodyId2D Id, Toolbox::FVector2 Force);
	/**
	 * 次の更新で使うトルクを加算する。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Torque ニュートンメートル単位のトルク。反時計回りが正。
	 */
	void ApplyTorque(FBodyId2D Id, Toolbox::f32 Torque);
	/**
	 * 速度へ即時反映する力積を与える。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Impulse ニュートン秒単位の力積。
	 */
	void ApplyLinearImpulse(FBodyId2D Id, Toolbox::FVector2 Impulse);
	/**
	 * 角速度へ即時反映する力積モーメントを与える。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Impulse ニュートンメートル秒単位の力積モーメント。反時計回りが正。
	 */
	void ApplyAngularImpulse(FBodyId2D Id, Toolbox::f32 Impulse);
	/**
	 * 重心外の点へ力積を与え、並進と回転の両方を生む。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Impulse ニュートン秒単位の力積。
	 * @param WorldPoint 力積を与えるワールド位置。メートル単位。
	 */
	void ApplyImpulseAtPoint(FBodyId2D Id, Toolbox::FVector2 Impulse, Toolbox::FVector2 WorldPoint);
	/**
	 * ワールド重力を設定する。非有限値は例外で通知する。
	 * @param Gravity メートル毎秒毎秒単位の重力加速度。
	 */
	void SetGravity(Toolbox::FVector2 Gravity);
	/**
	 * ワールド重力を返す。
	 */
	Toolbox::FVector2 GetGravity() const noexcept;
	/**
	 * コライダーを剛体へ取り付ける。不正な値は例外で通知し、状態を変更しない。
	 * @param Body 取り付け先の剛体。
	 * @param Description 取り付ける形状と材質。
	 */
	FColliderId2D AttachCollider(FBodyId2D Body, const FColliderDescription2D& Description);
	/**
	 * コライダーを外してIDを失効させる。期限切れIDはfalseを返す。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool DetachCollider(FColliderId2D Id) noexcept;
	/**
	 * 接触拘束の解決設定を変更する。不正な値は例外で通知する。
	 * @param Settings 接触拘束の解決設定。
	 */
	void SetContactSettings(const FContactSettings2D& Settings);
	/**
	 * 接触拘束の解決設定を返す。
	 */
	FContactSettings2D GetContactSettings() const noexcept;
	/**
	 * 連続衝突の反復設定を変更する。不正な値は例外で通知する。
	 * @param Settings 連続衝突の反復設定。
	 */
	void SetContinuousSettings(const FContinuousSettings2D& Settings);
	/**
	 * 連続衝突の反復設定を返す。
	 */
	FContinuousSettings2D GetContinuousSettings() const noexcept;
	/**
	 * 剛体の連続衝突の利用を切り替える。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param bEnabled 移動区間の接触解決を行うか。
	 */
	void SetContinuous(FBodyId2D Id, bool bEnabled);
	/**
	 * 剛体が連続衝突を利用するかを調べる。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool IsContinuous(FBodyId2D Id) const;
	/**
	 * 二つのコライダー組の線形CCD対応を調べる。期限切れIDは例外で通知する。
	 * @param A 一つ目のコライダー。
	 * @param B 二つ目のコライダー。
	 */
	EContinuousSupport QueryContinuousSupport(FColliderId2D A, FColliderId2D B) const;
	/**
	 * 直近更新の連続衝突診断を返す。
	 */
	FContinuousDiagnostics2D GetContinuousDiagnostics() const noexcept;
	/**
	 * 休止の条件を変更する。不正な値は例外で通知する。
	 * @param Settings 休止の条件。
	 */
	void SetSleepSettings(const FSleepSettings2D& Settings);
	/**
	 * 休止の条件を返す。
	 */
	FSleepSettings2D GetSleepSettings() const noexcept;
	/**
	 * 剛体が休止しているかを調べる。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool IsSleeping(FBodyId2D Id) const;
	/**
	 * 剛体を起こす。期限切れIDはfalseを返す。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool WakeUp(FBodyId2D Id) noexcept;
	/**
	 * 剛体の姿勢を直接設定する。速度と蓄積力は変更しない。
	 * テレポート後は接触キャッシュの消去と補間履歴の破棄を呼び出し元が行う。
	 * 期限切れIDと非有限値は例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Position メートル単位の重心位置。
	 * @param Angle ラジアン単位の姿勢角。
	 */
	void SetBodyTransform(FBodyId2D Id, Toolbox::FVector2 Position, Toolbox::f32 Angle);
	/**
	 * 前回Impulseの再利用記録を消去する。
	 */
	void ClearContactCache() noexcept;
	/**
	 * コライダーIDが有効な登録を指すか調べる。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool IsColliderAlive(FColliderId2D Id) const noexcept;
	/**
	 * 指定秒数だけ物理状態を進める。力とトルクは更新後に一度だけ消去する。
	 * 取り付け済みのコライダー同士の接触拘束も解く。箱同士は最大二点の多様体になる。
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
