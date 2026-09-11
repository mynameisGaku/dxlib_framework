#pragma once
#include "Dxf/Font.h"
namespace Dxf
{
class FFontLoader
{
public:
	FFontLoader(IFontBackend& Backend, FResourceRegistry& Registry) : m_pBackend(&Backend), m_pRegistry(&Registry)
	{
	}
	TResult<FFont> Load(const FFontOptions& Options);
private:
	IFontBackend* m_pBackend;
	FResourceRegistry* m_pRegistry;
};
}
