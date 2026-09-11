#pragma once
#include "Dxf/Platform.h"
namespace Dxf
{
/** The platform must outlive the session. Other resource owners must end first. */
class FDxLibSession
{
public:
	explicit FDxLibSession(IPlatform& Platform) : m_pPlatform(&Platform)
	{
	}
	~FDxLibSession()
	{
		Shutdown();
	}
	FDxLibSession(const FDxLibSession&) = delete;
	FDxLibSession& operator=(const FDxLibSession&) = delete;
	TResult<void> Initialize(const FWindowSettings& Settings)
	{
		if (m_bInitialized)
		{
			return TResult<void>::Failure(EErrorCode::InvalidState, "Session already initialized");
		}
		if (Settings.Width <= 0 || Settings.Height <= 0)
		{
			return TResult<void>::Failure(EErrorCode::InvalidArgument, "Invalid window size");
		}
		auto Result = m_pPlatform->Initialize(Settings);
		m_bInitialized = static_cast<bool>(Result);
		return Result;
	}
	void Shutdown() noexcept
	{
		if (m_bInitialized)
		{
			m_bInitialized = false;
			m_pPlatform->Shutdown();
		}
	}
	bool IsInitialized() const noexcept
	{
		return m_bInitialized;
	}
private:
	IPlatform* m_pPlatform;
	bool m_bInitialized = false;
};
}
