// SPDX-License-Identifier: NOASSERTION
#include "Dxf/ContentViews.h"
namespace Dxf::ContentPrivate
{
// 画素矩形は半開区間。描画先へ収まるかは、寸法が決まった既存描画境界で検査する。
FIntRect ReadRect(FSchemaReader& R, Toolbox::int32 Node)
{
	R.Array(Node);
	Toolbox::int32 Values[4] = {};
	Toolbox::uint32 Count = 0;
	for (auto I = R.Get(Node).FirstChild; I >= 0; I = R.Get(I).NextSibling)
	{
		if (Count == 4)
		{
			R.Fail(Node, "Rectangle needs exactly four integer coordinates");
		}
		Values[Count++] = static_cast<Toolbox::int32>(R.Integer(I, 2147483647));
	}
	if (Count != 4 || Values[2] <= Values[0] || Values[3] <= Values[1])
	{
		R.Fail(Node, "Invalid nonempty rectangle");
	}
	return {Values[0], Values[1], Values[2], Values[3]};
}
// 両次元で表示数の契約を揃え、描画する領域だけを分ける。
template <typename TDefinition, typename TRead>
void ReadViewsArray(FSchemaReader& R, Toolbox::int32 Node, TDefinition& D, TRead Read)
{
	if (Node < 0)
	{
		return;
	}
	R.Array(Node);
	D.ViewCount = 0;
	for (auto I = R.Get(Node).FirstChild; I >= 0; I = R.Get(I).NextSibling)
	{
		if (D.ViewCount == 2)
		{
			R.Fail(Node, "Scene supports one or two views");
		}
		Read(I, D.Views[D.ViewCount++]);
	}
	if (D.ViewCount == 0)
	{
		R.Fail(Node, "Scene needs at least one view");
	}
}
void ReadViews(FSchemaReader& R, Toolbox::int32 Node, FSceneDefinition2D& D)
{
	ReadViewsArray(R, Node, D,
	               [&R](Toolbox::int32 I, FContentView2D& View)
	               {
		               R.Fields(I, {"origin", "pixelsPerMeter", "clip"});
		               const auto Origin = R.Find(I, "origin");
		               if (Origin >= 0)
		               {
			               const auto P = R.Vector2(Origin);
			               View.Origin = {P.X, P.Y};
		               }
		               const auto Scale = R.Find(I, "pixelsPerMeter");
		               if (Scale >= 0)
		               {
			               View.PixelsPerMeter = R.Scalar(Scale);
		               }
		               const auto Clip = R.Find(I, "clip");
		               if (Clip >= 0)
		               {
			               View.bClip = true;
			               View.ClipRect = ReadRect(R, Clip);
		               }
		               if (!IsValidContentView2D(View))
		               {
			               R.Fail(I, "Invalid 2D view");
		               }
	               });
}
void ReadViews(FSchemaReader& R, Toolbox::int32 Node, FSceneDefinition3D& D)
{
	ReadViewsArray(R, Node, D,
	               [&R](Toolbox::int32 I, FRenderView3D& View)
	               {
		               R.Fields(I, {"eye", "target", "up", "fov", "near", "far", "orthographic", "height", "viewport", "lightDirection", "lightColor", "ambient", "lightEnabled"});
		               const auto Vector = [&R, I](const char* Key, Toolbox::FVector3& Value)
		               {
			               const auto Field = R.Find(I, Key);
			               if (Field >= 0)
			               {
				               Value = R.Vector3(Field);
			               }
		               };
		               const auto Scalar = [&R, I](const char* Key, Toolbox::f32& Value)
		               {
			               const auto Field = R.Find(I, Key);
			               if (Field >= 0)
			               {
				               Value = R.Scalar(Field);
			               }
		               };
		               const auto Color = [&R, I](const char* Key, FColor& Value)
		               {
			               const auto Field = R.Find(I, Key);
			               if (Field >= 0)
			               {
				               Value = R.Color(Field);
			               }
		               };
		               Vector("eye", View.Eye);
		               Vector("target", View.Target);
		               Vector("up", View.Up);
		               Vector("lightDirection", View.LightDirection);
		               Scalar("fov", View.VerticalFov);
		               Scalar("near", View.NearPlane);
		               Scalar("far", View.FarPlane);
		               Scalar("height", View.OrthographicHeight);
		               Color("lightColor", View.LightColor);
		               Color("ambient", View.AmbientColor);
		               const auto Orthographic = R.Find(I, "orthographic");
		               const auto Light = R.Find(I, "lightEnabled");
		               if (Orthographic >= 0)
		               {
			               View.bOrthographic = R.Boolean(Orthographic);
		               }
		               if (Light >= 0)
		               {
			               View.bLightEnabled = R.Boolean(Light);
		               }
		               const auto Rect = R.Find(I, "viewport");
		               if (Rect >= 0)
		               {
			               View.bViewport = true;
			               View.Viewport = ReadRect(R, Rect);
		               }
		               if (!IsValidRenderView3D(View))
		               {
			               R.Fail(I, "Invalid 3D camera or light");
		               }
	               });
}
} // namespace Dxf::ContentPrivate
