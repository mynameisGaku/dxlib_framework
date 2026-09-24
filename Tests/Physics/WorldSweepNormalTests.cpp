// SPDX-License-Identifier: NOASSERTION
// 円／球スイープ（SweepClosest）の接触法線。対象から問い合わせ円／球の中心へ向く単位方向。
// 期待値は手計算の解析値で、製品の変換・距離関数から作らない。実FPhysicsWorld2D／3Dの両方で実行する。
// 許容差の根拠: 公開法線はf64の値をf32へ丸めるため、各成分の丸め誤差は2^-25（約3e-8）以下。
// 解析値との比較は、その約3倍の1e-7とする。World経由で回転を合成する配置は、姿勢・軸のf32演算（数ulp）を含めて1e-6とする。
#include "TestCases.h"
#include "Dxf/RigidBody2D.h"
#include "Dxf/RigidBody3D.h"
#include "Toolbox/ShapeSweep2D.h"
#include "Toolbox/ShapeSweep3D.h"
using namespace Toolbox;
using namespace Dxf;
namespace
{
constexpr uint32 ObstacleCategory = 1u << 0;
constexpr uint32 CharacterCategory = 1u << 1;
constexpr f32 SixthPi = 0.523598775598f;
constexpr f32 TwelfthPi = 0.261799387799f;
// f32へ丸めた法線の成分・長さの許容差。
constexpr f64 NormalTolerance = 1e-7;
constexpr f64 LengthTolerance = 2e-7;
constexpr f64 WorldTolerance = 1e-6;

bool Near_Internal(f64 Value, f64 Expected, f64 Tolerance)
{
	return Abs(Value - Expected) <= Tolerance;
}

// 2D法線が期待方向と一致し、有限で単位長か。
bool Normal_Internal(const TOptional<FVector2>& Normal, f64 X, f64 Y, f64 Tolerance)
{
	if (!Normal || !IsFinite(Normal->X) || !IsFinite(Normal->Y))
	{
		return false;
	}
	const f64 Length = Sqrt(f64(Normal->X) * Normal->X + f64(Normal->Y) * Normal->Y);
	return Near_Internal(Normal->X, X, Tolerance) && Near_Internal(Normal->Y, Y, Tolerance) &&
	       Near_Internal(Length, 1, LengthTolerance);
}

// 3D法線が期待方向と一致し、有限で単位長か。
bool Normal_Internal(const TOptional<FVector3>& Normal, f64 X, f64 Y, f64 Z, f64 Tolerance)
{
	if (!Normal || !IsFinite(Normal->X) || !IsFinite(Normal->Y) || !IsFinite(Normal->Z))
	{
		return false;
	}
	const f64 Length = Sqrt(f64(Normal->X) * Normal->X + f64(Normal->Y) * Normal->Y + f64(Normal->Z) * Normal->Z);
	return Near_Internal(Normal->X, X, Tolerance) && Near_Internal(Normal->Y, Y, Tolerance) &&
	       Near_Internal(Normal->Z, Z, Tolerance) && Near_Internal(Length, 1, LengthTolerance);
}

// 外側のヒットがあり、割合と初期接触が期待どおりで、法線だけが空か。
template <typename THit> bool NoNormal_Internal(const TOptional<THit>& Hit, f64 Fraction, bool bInitial, f64 Tolerance)
{
	return Hit && Near_Internal(Hit->Time, Fraction, Tolerance) && Hit->bInitialContact == bInitial && !Hit->Normal;
}

// 割合と初期接触の確認（法線は別に確認する）。
template <typename THit> bool Hit_Internal(const TOptional<THit>& Hit, f64 Fraction, bool bInitial, f64 Tolerance)
{
	return Hit && Near_Internal(Hit->Time, Fraction, Tolerance) && Hit->bInitialContact == bInitial;
}

FWorldQueryFilter Mask_Internal(uint32 Categories)
{
	FWorldQueryFilter Filter;
	Filter.IncludeCategories = Categories;
	return Filter;
}

// 円／球同士: 正面・斜め・接線・終点・対象半径0。
void CircleSphereNormals_Internal()
{
	// 正面: 対象(5,0)半径1、問い合わせ半径0.5で0→10。中心x=3.5、法線-X。
	const auto Front2D = SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{5, 0}, 1});
	PHYSICS_REQUIRE(Hit_Internal(Front2D, 0.35, false, 1e-12) && Normal_Internal(Front2D->Normal, -1, 0, 0));
	const auto Front3D = SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 0}, FSphere{{5, 0, 0}, 1});
	PHYSICS_REQUIRE(Hit_Internal(Front3D, 0.35, false, 1e-12) && Normal_Internal(Front3D->Normal, -1, 0, 0, 0));
	// 斜め: 対象(5,0.9)。中心間1.5で(x-5)^2+0.81=2.25、x=3.8。法線は(3.8-5,-0.9)/1.5=(-0.8,-0.6)。
	// 対象のyはf32の0.9なので、期待値もその格納値から求める。
	const f64 Y09 = f64(0.9f);
	const f64 Dx09 = -Sqrt(2.25 - Y09 * Y09);
	const auto Oblique2D = SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{5, 0.9f}, 1});
	PHYSICS_REQUIRE(Hit_Internal(Oblique2D, (5 + Dx09) / 10, false, 1e-12) &&
	                Normal_Internal(Oblique2D->Normal, Dx09 / 1.5, -Y09 / 1.5, NormalTolerance));
	const auto Oblique3D = SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 0}, FSphere{{5, 0, 0.9f}, 1});
	PHYSICS_REQUIRE(Hit_Internal(Oblique3D, (5 + Dx09) / 10, false, 1e-12) &&
	                Normal_Internal(Oblique3D->Normal, Dx09 / 1.5, 0, -Y09 / 1.5, NormalTolerance));
	// 接線: 対象(5,1.5)。x=5で接し、法線(0,-1)は移動方向と直交する（内積を負へ反転しない）。
	const auto Tangent2D = SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{5, 1.5f}, 1});
	PHYSICS_REQUIRE(Hit_Internal(Tangent2D, 0.5, false, 1e-12) && Normal_Internal(Tangent2D->Normal, 0, -1, 0));
	const auto Tangent3D = SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 0}, FSphere{{5, 0, 1.5f}, 1});
	PHYSICS_REQUIRE(Hit_Internal(Tangent3D, 0.5, false, 1e-12) && Normal_Internal(Tangent3D->Normal, 0, 0, -1, 0));
	// 終点ちょうど（割合1）。
	const auto End2D = SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{11.5f, 0}, 1});
	PHYSICS_REQUIRE(Hit_Internal(End2D, 1, false, 0) && Normal_Internal(End2D->Normal, -1, 0, 0));
	const auto End3D = SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{0, 10, 0}, FSphere{{0, 11.5f, 0}, 1});
	PHYSICS_REQUIRE(Hit_Internal(End3D, 1, false, 0) && Normal_Internal(End3D->Normal, 0, -1, 0, 0));
	// 対象半径0（点）でも、問い合わせ半径が正なら外側接触の法線を返す。左向きの移動では+X。
	const auto Point2D = SweepToCenter(FCircle2D{{10, 0}, 0.5f}, FVector2{0, 0}, FCircle2D{{5, 0}, 0});
	PHYSICS_REQUIRE(Hit_Internal(Point2D, 0.45, false, 1e-12) && Normal_Internal(Point2D->Normal, 1, 0, 0));
	const auto Point3D = SweepToCenter(FSphere{{0, 0, 10}, 0.5f}, FVector3{0, 0, 0}, FSphere{{0, 0, 5}, 0});
	PHYSICS_REQUIRE(Hit_Internal(Point3D, 0.45, false, 1e-12) && Normal_Internal(Point3D->Normal, 0, 0, 1, 0));
}

// 箱の面・辺・頂点。面法線・箱中心からの方向・最寄りの座標軸とは区別できる配置。
void BoxFeatureNormals_Internal()
{
	// 面: x=5の壁（中心(6,0)、半幅(1,3)）に半径0.5で0→10。中心x=4.5、法線-X。
	const auto Face2D = SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FOrientedBox2D{{6, 0}, {1, 3}, 0});
	PHYSICS_REQUIRE(Hit_Internal(Face2D, 0.45, false, 1e-12) && Normal_Internal(Face2D->Normal, -1, 0, 0));
	const auto Face3D = SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 0}, FOBB{{6, 0, 0}, {1, 3, 3}});
	PHYSICS_REQUIRE(Hit_Internal(Face3D, 0.45, false, 1e-12) && Normal_Internal(Face3D->Normal, -1, 0, 0, 0));
	// 2Dの頂点: 左下頂点(5,0.5)（中心(6,1.5)、半幅1）、半径1でX軸上を右へ。
	// 中心x=5-√3/2、法線(-√3/2,-0.5)。下面は中心から0.5で、頂点より先には当たらない。
	const f64 HalfRoot3 = Sqrt(3.0) / 2;
	const auto Vertex2D = SweepToCenter(FCircle2D{{0, 0}, 1}, FVector2{10, 0}, FOrientedBox2D{{6, 1.5f}, {1, 1}, 0});
	PHYSICS_REQUIRE(Hit_Internal(Vertex2D, (5 - HalfRoot3) / 10, false, 1e-12) &&
	                Normal_Internal(Vertex2D->Normal, -HalfRoot3, -0.5, NormalTolerance));
	// 3Dの頂点(5,0.5,0.5)（中心(6,1.5,1.5)、半幅1）。中心x=5-√0.5、法線(-√0.5,-0.5,-0.5)。
	const f64 RootHalf = Sqrt(0.5);
	const auto Vertex3D = SweepToCenter(FSphere{{0, 0, 0}, 1}, FVector3{10, 0, 0}, FOBB{{6, 1.5f, 1.5f}, {1, 1, 1}});
	PHYSICS_REQUIRE(Hit_Internal(Vertex3D, (5 - RootHalf) / 10, false, 1e-12) &&
	                Normal_Internal(Vertex3D->Normal, -RootHalf, -0.5, -0.5, NormalTolerance));
	// 3Dの辺: x=5・y=0.5のZ方向の辺（中心(6,1.5,0)、半幅(1,1,3)）。中心x=5-√3/2、法線(-√3/2,-0.5,0)。
	const auto Edge3D = SweepToCenter(FSphere{{0, 0, 0}, 1}, FVector3{10, 0, 0}, FOBB{{6, 1.5f, 0}, {1, 1, 3}});
	PHYSICS_REQUIRE(Hit_Internal(Edge3D, (5 - HalfRoot3) / 10, false, 1e-12) &&
	                Normal_Internal(Edge3D->Normal, -HalfRoot3, -0.5, 0, NormalTolerance));
	// 斜めの移動で頂点に当たる: 2D箱[-1,1]^2、半径1、(3,3)→(1,1)。法線は角から中心への(1,1)/√2。
	const auto Diagonal2D = SweepToCenter(FCircle2D{{3, 3}, 1}, FVector2{1, 1}, FOrientedBox2D{{0, 0}, {1, 1}, 0});
	PHYSICS_REQUIRE(Diagonal2D && Normal_Internal(Diagonal2D->Normal, RootHalf, RootHalf, NormalTolerance));
	const f64 InvRoot3 = 1 / Sqrt(3.0);
	const auto Diagonal3D = SweepToCenter(FSphere{{3, 3, 3}, 1}, FVector3{1, 1, 1}, FOBB{{0, 0, 0}, {1, 1, 1}});
	PHYSICS_REQUIRE(Diagonal3D && Normal_Internal(Diagonal3D->Normal, InvRoot3, InvRoot3, InvRoot3, NormalTolerance));
}

// 回転の向き、XY平面に閉じない軸、格納軸の非単位性・せん断、半幅0。
void BoxAxisNormals_Internal()
{
	// 2D +30度の半幅(2,0.5)、半径0.25、(-5,1)→(5,1)。上面に当たり法線は(-sin30,cos30)=(-0.5,√3/2)。
	// -30度では左端の面に当たり法線は(-cos30,-sin30)を反転した(-√3/2,0.5)。f32の角度を含むためWorldToleranceとする。
	const f64 HalfRoot3 = Sqrt(3.0) / 2;
	const auto Plus =
	    SweepToCenter(FCircle2D{{-5, 1}, 0.25f}, FVector2{5, 1}, FOrientedBox2D{{0, 0}, {2, 0.5f}, SixthPi});
	PHYSICS_REQUIRE(Plus && Normal_Internal(Plus->Normal, -0.5, HalfRoot3, WorldTolerance));
	const auto Minus =
	    SweepToCenter(FCircle2D{{-5, 1}, 0.25f}, FVector2{5, 1}, FOrientedBox2D{{0, 0}, {2, 0.5f}, -SixthPi});
	PHYSICS_REQUIRE(Minus && Normal_Internal(Minus->Normal, -HalfRoot3, 0.5, WorldTolerance));
	// 3D: X軸回り30度のOBB（Y'=(0,c,s)、Z'=(0,-s,c)）、半幅1。-Zへ動く半径0.5の球は+Z'側の面に当たる。
	// 法線はZ'=(0,-0.5,√3/2)、中心z=1.5/(√3/2)=√3。軸はf32の格納値のため、許容差はWorldTolerance。
	FOBB Tilted{{0, 0, 0}, {1, 1, 1}};
	const f32 Cosine = static_cast<f32>(HalfRoot3);
	Tilted.Axes = {FVector3{1, 0, 0}, FVector3{0, Cosine, 0.5f}, FVector3{0, -0.5f, Cosine}};
	const auto TiltedHit = SweepToCenter(FSphere{{0, 0, 5}, 0.5f}, FVector3{0, 0, -5}, Tilted);
	PHYSICS_REQUIRE(Hit_Internal(TiltedHit, (5 - Sqrt(3.0)) / 10, false, WorldTolerance) &&
	                Normal_Internal(TiltedHit->Normal, 0, -0.5, HalfRoot3, WorldTolerance));
	// せん断した格納軸（Y'=(ε,1,0)）。+X側面の法線はY'×Z'=(1,-ε,0)の方向。軸を直交化すると(1,0,0)になる。
	const f32 Epsilon = static_cast<f32>(5e-5);
	FOBB Sheared{{0, 0, 0}, {1, 1, 1}};
	Sheared.Axes = {FVector3{1, 0, 0}, FVector3{Epsilon, 1, 0}, FVector3{0, 0, 1}};
	const f64 ShearLength = Sqrt(1 + f64(Epsilon) * f64(Epsilon));
	const auto ShearHit = SweepToCenter(FSphere{{3, 0.5f, 0}, 0.25f}, FVector3{0, 0.5f, 0}, Sheared);
	PHYSICS_REQUIRE(ShearHit && Normal_Internal(ShearHit->Normal, 1 / ShearLength, -f64(Epsilon) / ShearLength, 0,
	                                            NormalTolerance));
	// 長さ1+3e-5の格納軸でも、面の法線は向きだけの単位ベクトル（軸の長さを法線へ持ち込まない）。
	const f32 Scale = static_cast<f32>(1 + 3e-5);
	FOBB Long{{0, 0, 0}, {2, 0.5f, 0.5f}};
	Long.Axes = {FVector3{Scale, 0, 0}, FVector3{0, 1, 0}, FVector3{0, 0, 1}};
	const auto LongHit = SweepToCenter(FSphere{{5, 0, 0}, 0.5f}, FVector3{0, 0, 0}, Long);
	PHYSICS_REQUIRE(LongHit && Normal_Internal(LongHit->Normal, 1, 0, 0, NormalTolerance));
	// 半幅0: 線分（上から）、点（左から）、板（上から）、厚さ0の板を横切る。
	const auto Segment = SweepToCenter(FCircle2D{{0, 3}, 0.5f}, FVector2{0, -3}, FOrientedBox2D{{0, 0}, {1, 0}, 0});
	PHYSICS_REQUIRE(Segment && Normal_Internal(Segment->Normal, 0, 1, 0));
	const auto Point = SweepToCenter(FCircle2D{{-3, 0}, 0.5f}, FVector2{3, 0}, FOrientedBox2D{{0, 0}, {0, 0}, 0});
	PHYSICS_REQUIRE(Point && Normal_Internal(Point->Normal, -1, 0, 0));
	const auto Sheet = SweepToCenter(FSphere{{0, 0, 3}, 0.5f}, FVector3{0, 0, -3}, FOBB{{0, 0, 0}, {1, 1, 0}});
	PHYSICS_REQUIRE(Sheet && Normal_Internal(Sheet->Normal, 0, 0, 1, 0));
	// 線分の端点（半幅(1,0)の右端(1,0)）へ斜め上から。中心(1+0.6,0.8)で接し、法線(0.6,0.8)。
	const auto SegmentEnd =
	    SweepToCenter(FCircle2D{{1.6f, 3}, 1}, FVector2{1.6f, -3}, FOrientedBox2D{{0, 0}, {1, 0}, 0});
	PHYSICS_REQUIRE(SegmentEnd && Normal_Internal(SegmentEnd->Normal, f64(1.6f) - 1,
	                                              Sqrt(1 - (f64(1.6f) - 1) * (f64(1.6f) - 1)), NormalTolerance));
}

// 法線を返さない場合。外側のヒット・割合・初期接触は保持する。
void OmittedNormals_Internal()
{
	// 開始時の境界接触・深い重なり・同中心・静止（開始＝終点）。
	PHYSICS_REQUIRE(NoNormal_Internal(SweepToCenter(FCircle2D{{1.5f, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{0, 0}, 1}),
	                                  0, true, 0));
	PHYSICS_REQUIRE(NoNormal_Internal(
	    SweepToCenter(FSphere{{1.5f, 0, 0}, 0.5f}, FVector3{-10, 0, 0}, FSphere{{0, 0, 0}, 1}), 0, true, 0));
	PHYSICS_REQUIRE(NoNormal_Internal(SweepToCenter(FCircle2D{{0.2f, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{0, 0}, 1}),
	                                  0, true, 0));
	PHYSICS_REQUIRE(
	    NoNormal_Internal(SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{0, 0}, 1}), 0, true, 0));
	PHYSICS_REQUIRE(NoNormal_Internal(SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{0, 0, 0}, FSphere{{0, 0, 0}, 1}),
	                                  0, true, 0));
	PHYSICS_REQUIRE(NoNormal_Internal(
	    SweepToCenter(FSphere{{0.5f, 0, 0}, 0.5f}, FVector3{0.5f, 0, 0}, FSphere{{0, 0, 0}, 1}), 0, true, 0));
	// 箱の初期接触（面の境界ちょうど・内部）。
	const FOrientedBox2D Square{{0, 0}, {1, 1}, 0};
	const FOBB Cube{{0, 0, 0}, {1, 1, 1}};
	PHYSICS_REQUIRE(NoNormal_Internal(SweepToCenter(FCircle2D{{2, 0}, 1}, FVector2{9, 0}, Square), 0, true, 0));
	PHYSICS_REQUIRE(NoNormal_Internal(SweepToCenter(FSphere{{0, 0, 0}, 0.5f}, FVector3{0, 0, 10}, Cube), 0, true, 0));
	PHYSICS_REQUIRE(NoNormal_Internal(SweepToCenter(FSphere{{2, 0, 0}, 1}, FVector3{2, 0, 0}, Cube), 0, true, 0));
	// 問い合わせ半径0は、対象が円／球でも箱でも法線を返さない（Worldでは移動ありをRaycastClosestへ委譲する）。
	PHYSICS_REQUIRE(NoNormal_Internal(SweepToCenter(FCircle2D{{0, 0}, 0}, FVector2{10, 0}, FCircle2D{{5, 0}, 1}), 0.4,
	                                  false, 1e-12));
	PHYSICS_REQUIRE(NoNormal_Internal(SweepToCenter(FSphere{{0, 0, 0}, 0}, FVector3{10, 0, 0}, FSphere{{5, 0, 0}, 1}),
	                                  0.4, false, 1e-12));
	PHYSICS_REQUIRE(NoNormal_Internal(
	    SweepToCenter(FCircle2D{{-5, 0}, 0}, FVector2{5, 0}, FOrientedBox2D{{0, 0}, {1, 1}, 0}), 0.4, false, 1e-12));
	PHYSICS_REQUIRE(
	    NoNormal_Internal(SweepToCenter(FSphere{{-5, 0, 0}, 0}, FVector3{5, 0, 0}, Cube), 0.4, false, 1e-12));
	PHYSICS_REQUIRE(
	    NoNormal_Internal(SweepToCenter(FCircle2D{{0.5f, 0}, 0}, FVector2{0.5f, 0}, FCircle2D{{0, 0}, 1}), 0, true, 0));
	// 非交差は外側のOptionalが空（法線が空のヒットとは別）。
	PHYSICS_REQUIRE(!SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{5, 3}, 1}));
}

// 大きな共通座標、小さな半径、長い移動、接線付近と、法線を構成できないと判定する条件。
// 条件: 最終残差の最大成分が、残差の計算に使った相対位置（箱では移動量・半幅も）と半径の最大成分の2^-30倍以下なら空。
void NumericNormals_Internal()
{
	// 2^20の共通オフセット。相対値だけで計算するので正面の法線はそのまま。
	const auto Offset2D =
	    SweepToCenter(FCircle2D{{1048576, 0}, 0.5f}, FVector2{1048586, 0}, FCircle2D{{1048581, 0}, 1});
	PHYSICS_REQUIRE(Hit_Internal(Offset2D, 0.35, false, 1e-12) && Normal_Internal(Offset2D->Normal, -1, 0, 0));
	const auto Offset3D = SweepToCenter(FSphere{{0, 1048576, 1048576}, 0.5f}, FVector3{10, 1048576, 1048576},
	                                    FOBB{{6, 1048576, 1048576}, {1, 3, 3}});
	PHYSICS_REQUIRE(Hit_Internal(Offset3D, 0.45, false, 1e-12) && Normal_Internal(Offset3D->Normal, -1, 0, 0, 0));
	// 小さな半径の長い移動: -1e5→1e5、対象(0,1.5e-3)半径1e-3、問い合わせ半径1e-3（格納したf32値で計算）。
	const f64 SumRadius = 2 * f64(1e-3f);
	const f64 Offset = f64(1.5e-3f);
	const f64 Along = -Sqrt(SumRadius * SumRadius - Offset * Offset);
	const auto Long = SweepToCenter(FCircle2D{{-1e5f, 0}, 1e-3f}, FVector2{1e5f, 0}, FCircle2D{{0, 1.5e-3f}, 1e-3f});
	PHYSICS_REQUIRE(Hit_Internal(Long, (1e5 + Along) / 2e5, false, 1e-12) &&
	                Normal_Internal(Long->Normal, Along / SumRadius, -Offset / SumRadius, NormalTolerance));
	// 小さな形状の短い移動は切り捨てない: 半径1e-6同士、対象y=1e-6（和の半分）。法線(-√3/2,-0.5)。
	const auto Tiny = SweepToCenter(FCircle2D{{-1, 0}, 1e-6f}, FVector2{1, 0}, FCircle2D{{0, 1e-6f}, 1e-6f});
	PHYSICS_REQUIRE(Tiny && !Tiny->bInitialContact &&
	                Normal_Internal(Tiny->Normal, -Sqrt(3.0) / 2, -0.5, NormalTolerance));
	// 接線付近: 対象y=1.5-2^-20。法線のX成分は-√(1.5^2-y^2)/1.5で、0や代替軸にしない。
	const f64 NearY = 1.5 - 1.0 / 1048576;
	const auto NearTangent =
	    SweepToCenter(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0}, FCircle2D{{5, static_cast<f32>(NearY)}, 1});
	PHYSICS_REQUIRE(
	    NearTangent &&
	    Normal_Internal(NearTangent->Normal, -Sqrt(2.25 - NearY * NearY) / 1.5, -NearY / 1.5, NormalTolerance) &&
	    NearTangent->Normal->X < 0);
	// 判定境界: 相対位置2^20、半径の和2^-10（=2^20×2^-30）ちょうどは空、2^-10+2^-16は-X。
	const f32 Quarter = 1.0f / 2048;
	const auto AtLimit =
	    SweepToCenter(FCircle2D{{0, 0}, Quarter}, FVector2{2097152, 0}, FCircle2D{{1048576, 0}, Quarter});
	PHYSICS_REQUIRE(AtLimit && !AtLimit->bInitialContact && !AtLimit->Normal);
	const auto AboveLimit = SweepToCenter(FCircle2D{{0, 0}, Quarter}, FVector2{2097152, 0},
	                                      FCircle2D{{1048576, 0}, Quarter + 1.0f / 65536});
	PHYSICS_REQUIRE(AboveLimit && Normal_Internal(AboveLimit->Normal, -1, 0, 0));
	// 長い移動に対して半径が小さすぎる場合: ヒットと割合は保持し、法線だけ空（NaN・代替軸にしない）。
	const auto Lost2D = SweepToCenter(FCircle2D{{-1e7f, 0}, 1e-6f}, FVector2{1e7f, 0}, FCircle2D{{0, 1e-6f}, 1e-6f});
	PHYSICS_REQUIRE(Lost2D && !Lost2D->bInitialContact && Near_Internal(Lost2D->Time, 0.5, 1e-6) && !Lost2D->Normal);
	const auto Lost3D =
	    SweepToCenter(FSphere{{0, 0, -1e7f}, 1e-6f}, FVector3{0, 0, 1e7f}, FSphere{{1e-6f, 0, 0}, 1e-6f});
	PHYSICS_REQUIRE(Lost3D && !Lost3D->bInitialContact && Near_Internal(Lost3D->Time, 0.5, 1e-6) && !Lost3D->Normal);
	const auto LostBox =
	    SweepToCenter(FCircle2D{{-1e7f, 0.5f}, 1e-6f}, FVector2{1e7f, 0.5f}, FOrientedBox2D{{0, 0}, {1, 1}, 0});
	PHYSICS_REQUIRE(LostBox && !LostBox->bInitialContact && Near_Internal(LostBox->Time, 0.5, 1e-6) &&
	                !LostBox->Normal);
}

// 2D World: 実登録と姿勢変換を通した法線と、既存4メンバー・順序・フィルター・半径0の契約。
void World2D_Internal()
{
	FPhysicsWorld2D World;
	FBodyDescription2D Static;
	const FBodyId2D Ground = World.CreateBody(Static);
	FColliderDescription2D Wall;
	Wall.Shape = FOrientedBox2D{{6, 0}, {1, 3}, 0};
	Wall.QueryCategory = ObstacleCategory;
	const FColliderId2D WallId = World.AttachCollider(Ground, Wall);
	const auto Hit = World.SweepClosest(FCircle2D{{0, 0}, 0.5f}, FVector2{10, 0});
	PHYSICS_REQUIRE(Hit && Hit->Collider == WallId && Hit->Fraction == 0.45 && Hit->CenterAtHit == FVector2{4.5f, 0} &&
	                !Hit->bInitialContact && Normal_Internal(Hit->Normal, -1, 0, 0));
	// Body角度30度＋Colliderローカル角度15度＋ローカル中心(2,0)。世界の箱は中心(√3,21)、45度の正方形。
	// y=21.8を右へ動く半径0.5の円は左上の面に当たり、法線(-√2/2,√2/2)、中心x=√3+0.8-1.5√2。
	FBodyDescription2D Rotated;
	Rotated.Position = {0, 20};
	Rotated.Angle = SixthPi;
	const FBodyId2D Turned = World.CreateBody(Rotated);
	FColliderDescription2D Diamond;
	Diamond.Shape = FOrientedBox2D{{2, 0}, {1, 1}, TwelfthPi};
	const FColliderId2D DiamondId = World.AttachCollider(Turned, Diamond);
	const f64 RootHalf = Sqrt(0.5);
	const f64 CenterX = Sqrt(3.0) + 0.8 - 1.5 * Sqrt(2.0);
	const auto Turn = World.SweepClosest(FCircle2D{{-10, 21.8f}, 0.5f}, FVector2{10, 21.8f});
	PHYSICS_REQUIRE(Turn && Turn->Collider == DiamondId && Near_Internal(Turn->Fraction, (CenterX + 10) / 20, 1e-6) &&
	                Normal_Internal(Turn->Normal, -RootHalf, RootHalf, WorldTolerance));
	// 同じ割合は先のスロット（上下対称の2円）。法線は先のスロットの円から中心へ向く(-√(2.25-d^2),-d)/1.5。
	// dは格納したf32の40.9と40の差（39.1側と同じ値）。
	const f64 TieY = f64(40.9f) - 40;
	FColliderDescription2D Upper;
	Upper.Shape = FCircle2D{{5, 40.9f}, 1};
	const FColliderId2D UpperId = World.AttachCollider(Ground, Upper);
	FColliderDescription2D Lower;
	Lower.Shape = FCircle2D{{5, 39.1f}, 1};
	(void)World.AttachCollider(Ground, Lower);
	const auto Tie = World.SweepClosest(FCircle2D{{0, 40}, 0.5f}, FVector2{10, 40});
	PHYSICS_REQUIRE(Tie && Tie->Collider == UpperId &&
	                Normal_Internal(Tie->Normal, -Sqrt(2.25 - TieY * TieY) / 1.5, -TieY / 1.5, NormalTolerance));
	// 法線がある後ろの候補より、割合0の初期接触（法線なし）を優先する。
	FColliderDescription2D Touching;
	Touching.Shape = FCircle2D{{0, 61}, 0.5f};
	Touching.QueryCategory = CharacterCategory;
	const FColliderId2D TouchingId = World.AttachCollider(Ground, Touching);
	FColliderDescription2D Ahead;
	Ahead.Shape = FOrientedBox2D{{6, 60}, {1, 3}, 0};
	Ahead.QueryCategory = ObstacleCategory;
	(void)World.AttachCollider(Ground, Ahead);
	const auto First = World.SweepClosest(FCircle2D{{0, 60}, 0.5f}, FVector2{10, 60});
	PHYSICS_REQUIRE(First && First->Collider == TouchingId && First->Fraction == 0 && First->bInitialContact &&
	                !First->Normal);
	// 手前の対象を除外すると、奥の対象の法線を返す（対象外の形状へ法線目的で変換しない）。
	const auto Filtered =
	    World.SweepClosest(FCircle2D{{0, 60}, 0.5f}, FVector2{10, 60}, {}, Mask_Internal(ObstacleCategory));
	PHYSICS_REQUIRE(Filtered && Filtered->Fraction == 0.45 && Normal_Internal(Filtered->Normal, -1, 0, 0));
	// 半径0の移動はRaycastClosestと同じID・割合・位置で、法線は空。
	const auto Ray = World.RaycastClosest(FVector2{0, 0}, FVector2{10, 0});
	const auto Thin = World.SweepClosest(FCircle2D{{0, 0}, 0}, FVector2{10, 0});
	PHYSICS_REQUIRE(Ray && Thin && Thin->Collider == Ray->Collider && Thin->Fraction == Ray->Fraction &&
	                Thin->CenterAtHit == Ray->Position && !Thin->bInitialContact && !Thin->Normal);
	// Stepなしの姿勢変更を次の問い合わせへ反映する（壁を180度回しても面は同じ位置で法線は-X）。
	World.SetBodyTransform(Ground, {0, 0}, 3.14159265f);
	const auto Flipped = World.SweepClosest(FCircle2D{{0, 0}, 0.5f}, FVector2{-10, 0});
	PHYSICS_REQUIRE(Flipped && Flipped->Collider == WallId && Near_Internal(Flipped->Fraction, 0.45, 1e-6) &&
	                Normal_Internal(Flipped->Normal, 1, 0, WorldTolerance));
}

// 3D World: XY平面に閉じない姿勢・ローカル中心を通した法線と、既存4メンバー・半径0の契約。
void World3D_Internal()
{
	FPhysicsWorld3D World;
	FBodyDescription3D Static;
	const FBodyId3D Ground = World.CreateBody(Static);
	FColliderDescription3D Wall;
	Wall.Shape = FOBB{{6, 0, 0}, {1, 3, 3}};
	Wall.QueryCategory = ObstacleCategory;
	const FColliderId3D WallId = World.AttachCollider(Ground, Wall);
	const auto Hit = World.SweepClosest(FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 0});
	PHYSICS_REQUIRE(Hit && Hit->Collider == WallId && Hit->Fraction == 0.45 &&
	                Hit->CenterAtHit == FVector3{4.5f, 0, 0} && !Hit->bInitialContact &&
	                Normal_Internal(Hit->Normal, -1, 0, 0, 0));
	// Body: 位置(0,0,30)、X軸回り30度。Collider: ローカル中心(0,2,0)の立方体（半幅1）。
	// 世界の中心は(0,2cos30,30+2sin30)=(0,√3,31)。-Zへ動く球（x=0,y=√3）は+Z'面に当たり、法線(0,-0.5,√3/2)。
	FBodyDescription3D Tilted;
	Tilted.Position = {0, 0, 30};
	Tilted.Orientation = FQuaternion::FromAxisAngle({1, 0, 0}, SixthPi);
	const FBodyId3D Turned = World.CreateBody(Tilted);
	FColliderDescription3D Cube;
	Cube.Shape = FOBB{{0, 2, 0}, {1, 1, 1}};
	const FColliderId3D CubeId = World.AttachCollider(Turned, Cube);
	const f32 RootThree = static_cast<f32>(Sqrt(3.0));
	const auto Tilt = World.SweepClosest(FSphere{{0, RootThree, 40}, 0.5f}, FVector3{0, RootThree, 20});
	PHYSICS_REQUIRE(Tilt && Tilt->Collider == CubeId && Near_Internal(Tilt->Fraction, (9 - Sqrt(3.0)) / 20, 1e-6) &&
	                Normal_Internal(Tilt->Normal, 0, -0.5, Sqrt(3.0) / 2, WorldTolerance));
	// 初期接触は法線なし。
	const auto Initial = World.SweepClosest(FSphere{{4.8f, 0, 0}, 0.5f}, FVector3{10, 0, 0});
	PHYSICS_REQUIRE(Initial && Initial->Collider == WallId && Initial->bInitialContact && !Initial->Normal);
	// 半径0の移動はRaycastClosestと同じで、法線は空。
	const auto Ray = World.RaycastClosest(FVector3{0, 0, 0}, FVector3{10, 0, 0});
	const auto Thin = World.SweepClosest(FSphere{{0, 0, 0}, 0}, FVector3{10, 0, 0});
	PHYSICS_REQUIRE(Ray && Thin && Thin->Collider == Ray->Collider && Thin->Fraction == Ray->Fraction &&
	                Thin->CenterAtHit == Ray->Position && !Thin->bInitialContact && !Thin->Normal);
	// カテゴリ変更をStepなしで反映する。壁を外すと非交差（外側のOptionalが空）。
	World.SetColliderQueryCategory(WallId, 0u);
	PHYSICS_REQUIRE(!World.SweepClosest(FSphere{{0, 0, 0}, 0.5f}, FVector3{10, 0, 0}));
}

// ゲーム側の利用: 問い合わせ結果だけを使い、衝突した方向を床・壁に分類する。Worldは移動しない。
// 分類の閾値（0.7）はこの例の方針で、フレームワークの規則ではない。
void Usage2D_Internal()
{
	FPhysicsWorld2D World;
	const FBodyId2D Level = World.CreateBody({});
	FColliderDescription2D Floor;
	Floor.Shape = FOrientedBox2D{{0, -1}, {20, 1}, 0};
	Floor.QueryCategory = ObstacleCategory;
	(void)World.AttachCollider(Level, Floor);
	FColliderDescription2D Wall;
	Wall.Shape = FOrientedBox2D{{6, 3}, {1, 3}, 0};
	Wall.QueryCategory = ObstacleCategory;
	(void)World.AttachCollider(Level, Wall);
	const FBodyId2D Self = World.CreateBody({});
	FColliderDescription2D Body;
	Body.Shape = FCircle2D{{0, 2}, 0.5f};
	Body.QueryCategory = CharacterCategory;
	(void)World.AttachCollider(Self, Body);
	const FWorldQueryFilter Obstacles = Mask_Internal(ObstacleCategory);
	// 分類: 0 衝突なし、1 床、2 壁、3 その他の向き（天井など）、4 法線なし（初期接触など）。
	const auto Classify = [&](FVector2 From, FVector2 To) -> int32
	{
		const auto Hit = World.SweepClosest(FCircle2D{From, 0.5f}, To, Self, Obstacles);
		if (!Hit)
		{
			return 0;
		}
		if (!Hit->Normal)
		{
			// 衝突は成立している。押し出しや通過を自動で選ばない。
			return 4;
		}
		if (Hit->Normal->Y >= 0.7f)
		{
			return 1;
		}
		if (Abs(Hit->Normal->X) >= 0.7f)
		{
			return 2;
		}
		return 3;
	};
	PHYSICS_REQUIRE(Classify({0, 2}, {0, -2}) == 1);
	PHYSICS_REQUIRE(Classify({0, 2}, {10, 2}) == 2);
	PHYSICS_REQUIRE(Classify({0, 2}, {-3, 2}) == 0);
	PHYSICS_REQUIRE(Classify({0, 0.2f}, {0, 5}) == 4);
	// 壁の上の角(5,6)をかすめる: 法線は面の(-1,0)ではなく角からの(-0.8,0.6)方向。この例の規則では壁に分類される。
	const f64 Above = f64(6.3f) - 6;
	const auto Corner = World.SweepClosest(FCircle2D{{0, 6.3f}, 0.5f}, FVector2{10, 6.3f}, Self, Obstacles);
	PHYSICS_REQUIRE(Corner &&
	                Normal_Internal(Corner->Normal, -Sqrt(0.25 - Above * Above) / 0.5, Above / 0.5, NormalTolerance) &&
	                Classify({0, 6.3f}, {10, 6.3f}) == 2);
}

// 3Dのカメラ候補: 頭からカメラ希望位置までの球。法線は遮蔽物から戻る向きで、視線の向きとは逆側。
void Usage3D_Internal()
{
	FPhysicsWorld3D World;
	const FBodyId3D Level = World.CreateBody({});
	FColliderDescription3D Wall;
	Wall.Shape = FOBB{{0, 2, -5}, {5, 3, 0.5f}};
	Wall.QueryCategory = ObstacleCategory;
	const FColliderId3D WallId = World.AttachCollider(Level, Wall);
	const FVector3 Head{0, 1.6f, 0};
	const FVector3 Desired{0, 1.6f, -8};
	const auto Hit = World.SweepClosest(FSphere{Head, 0.3f}, Desired, {}, Mask_Internal(ObstacleCategory));
	PHYSICS_REQUIRE(Hit && Hit->Collider == WallId && !Hit->bInitialContact &&
	                Normal_Internal(Hit->Normal, 0, 0, 1, 0) && Near_Internal(Hit->CenterAtHit.Z, -4.2, 1e-6));
	// 法線とカメラの移動方向の内積は負（面に正面から当たる場合）。
	const f64 Dot = f64(Hit->Normal->X) * (Desired.X - Head.X) + f64(Hit->Normal->Y) * (Desired.Y - Head.Y) +
	                f64(Hit->Normal->Z) * (Desired.Z - Head.Z);
	PHYSICS_REQUIRE(Dot < 0);
}

// 2D／3Dの各ケース。
const PhysicsTest::FCase Cases_Internal[] = {
    {"sweep normal circle and sphere", &CircleSphereNormals_Internal},
    {"sweep normal box faces edges vertices", &BoxFeatureNormals_Internal},
    {"sweep normal rotation axes and degenerate boxes", &BoxAxisNormals_Internal},
    {"sweep normal omitted cases", &OmittedNormals_Internal},
    {"sweep normal numeric range", &NumericNormals_Internal},
    {"2D sweep normal in world", &World2D_Internal},
    {"3D sweep normal in world", &World3D_Internal},
    {"2D sweep normal floor and wall usage", &Usage2D_Internal},
    {"3D sweep normal camera usage", &Usage3D_Internal}};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetWorldSweepNormalCases(size_t& Count) noexcept
{
	Count = sizeof(Cases_Internal) / sizeof(Cases_Internal[0]);
	return Cases_Internal;
}
