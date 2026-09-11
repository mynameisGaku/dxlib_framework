#pragma once
#include "Dxf/Sound.h"
namespace Dxf
{
class FSoundLoader
{
public:
	FSoundLoader(ISoundBackend& Backend, FResourceRegistry& Registry) : m_pBackend(&Backend), m_pRegistry(&Registry)
	{
	}
	TResult<FSound> Load(const std::string& Path, const FSoundLoadOptions& Options);
private:
	ISoundBackend* m_pBackend;
	FResourceRegistry* m_pRegistry;
};
}
