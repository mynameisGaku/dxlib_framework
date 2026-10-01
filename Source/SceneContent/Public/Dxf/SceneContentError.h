// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SCENE_CONTENT_ERROR_H
#define DXF_SCENE_CONTENT_ERROR_H
#include "Dxf/SceneContentDiagnostic.h"
namespace Dxf
{
/**
 * Contentの失敗場所を値として保持する例外。実行時例外を成功へ変換しない。
 */
class FSceneContentError : public Toolbox::FException
{
public:
	/**
	 * @param Diagnostic 失敗場所と原因。構築中の確保失敗も通常どおり伝播する。
	 */
	explicit FSceneContentError(FSceneContentDiagnostic Diagnostic);
	/**
	 * 保持した診断を変更せず返す。
	 */
	const FSceneContentDiagnostic& GetDiagnostic() const noexcept;

private:
	/**
	 * 失敗時の診断。
	 */
	FSceneContentDiagnostic m_Diagnostic;
};
} // namespace Dxf
#endif
