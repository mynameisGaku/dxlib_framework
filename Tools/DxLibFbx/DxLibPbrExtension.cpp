// SPDX-License-Identifier: NOASSERTION
// DxLibセッションがGPU資源を所有する。描画スレッド以外から呼ばない。
#define DX_MAKE
#pragma warning(push)
#pragma warning(disable : 4828)
#include "DxLib.h"
#include "DxStatic.h"
#include "Windows/DxGraphicsWin.h"
#include "Windows/DxGraphicsD3D11.h"
#pragma warning(pop)
#include "ModelPbrBytecode.h"
#include <string.h>
namespace
{
// セッション中に一度だけ作成し、終了前に解放する。
int Shader = -1;
DxLib::CONSTANTBUFFER_DIRECT3D11* Buffer = nullptr;
// 1回のモデル描画の間だけ有効。
bool Active = false;
// 内部の描画が失敗しても、呼出し側へ伝える。
bool Failed = false;
D_ID3D11Buffer* PreviousBuffer = nullptr;
} // namespace
extern "C" void DxfFailPbr()
{
	Failed = true;
}
extern "C" int DxfPbrActive()
{
	return Active ? 1 : 0;
}
extern "C" int DxfBeginPbr(const float* Parameters)
{
	if (Active || DxLib::GraphicsManage_Win.Setting.UseGraphicsAPI != GRAPHICS_API_DIRECT3D11_WIN32)
		return -1;
	if (Shader < 0)
		Shader = DxLib::NS_LoadPixelShaderFromMem(ModelPbrBytecode, sizeof(ModelPbrBytecode));
	if (Shader < 0)
		return -1;
	if (Buffer == nullptr)
		Buffer = DxLib::Graphics_D3D11_ConstantBuffer_Create(32);
	if (Buffer == nullptr)
		return -1;
	memcpy(Buffer->SysmemBuffer, Parameters, 32);
	Buffer->ChangeFlag = TRUE;
	if (DxLib::Graphics_D3D11_ConstantBuffer_Update(Buffer) < 0)
		return -1;
	PreviousBuffer = DxLib::GD3D11.Device.State.SetPixelShaderConstantBuffer[4];
	Failed = false;
	Active = true;
	return 0;
}
extern "C" int DxfBindPbr()
{
	if (!Active)
		return 0;
	if (DxLib::Graphics_D3D11_DeviceState_SetPixelShaderToHandle(Shader) < 0 ||
	    DxLib::Graphics_D3D11_ConstantBuffer_PSSet(4, 1, &Buffer) < 0)
	{
		Failed = true;
		return -1;
	}
	return 0;
}
extern "C" int DxfEndPbr()
{
	if (!Active)
		return 0;
	Active = false;
	const int Restore = DxLib::Graphics_D3D11_DeviceState_SetPixelShaderConstantBuffers(4, 1, &PreviousBuffer);
	return Failed || Restore < 0 ? -1 : 0;
}
extern "C" void DxfReleasePbr()
{
	DxfEndPbr();
	if (Shader >= 0)
		DxLib::NS_DeleteShader(Shader);
	if (Buffer != nullptr)
		DxLib::Graphics_D3D11_ConstantBuffer_Delete(Buffer);
	Shader = -1;
	Buffer = nullptr;
	PreviousBuffer = nullptr;
}
