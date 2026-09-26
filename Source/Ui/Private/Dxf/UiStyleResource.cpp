// SPDX-License-Identifier: NOASSERTION
#include "Dxf/UiStyleResource.h"
namespace Dxf
{
// 接続時にも現在の版を反映する。
void FUiStyleResource::Attach(FUiRoot& Root)
{
	for (const auto& Ref : m_Roots)
	{
		if (Ref.Get() == &Root)
		{
			return;
		}
	}
	m_Roots.PushBack(Root.GetHandle());
	if (m_pCurrent)
	{
		Root.SetStyleSheet(m_pCurrent);
	}
}
// 弱い参照だけを外す。
void FUiStyleResource::Detach(const FUiRoot& Root) noexcept
{
	for (Toolbox::size_t I = m_Roots.Size(); I > 0; --I)
	{
		if (m_Roots[I - 1].Get() == nullptr || m_Roots[I - 1].Get() == &Root)
		{
			m_Roots.Erase(m_Roots.Begin() + I - 1);
		}
	}
}
// 解決済みの新しい表を作り終えてから、確保も利用者通知も伴わない差替えを行う。
TResult<void> FUiStyleResource::Reload(const Toolbox::TVector<FUiStyleSource>& Sources, const FUiStyleLimits& Limits)
{
	try
	{
		auto Parsed = ParseUiStyleSheet(Sources, Limits);
		if (!Parsed)
		{
			return TResult<void>::Failure(Parsed.Error());
		}
		if (m_Revision == Toolbox::TNumericLimits<Toolbox::uint64>::Max())
		{
			return TResult<void>::Failure(EErrorCode::InvalidState, "UI style revision exhausted");
		}
		Toolbox::TSharedPtr<const FUiStyleSheet> Next =
		    Toolbox::MakeShared<FUiStyleSheet>(Toolbox::Move(Parsed).Value());
		m_pCurrent = Next;
		++m_Revision;
		for (const auto& Ref : m_Roots)
		{
			if (auto* Root = Ref.Get())
			{
				Root->SetStyleSheet(Next);
			}
		}
		return {};
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, Error.What());
	}
	catch (...)
	{
		return TResult<void>::Failure(EErrorCode::UserException, "UI style reload failed");
	}
}
// 読込の上限を適用し、ファイル不在でも旧版を保持する。
TResult<void> FUiStyleResource::ReloadFile(const Toolbox::FPath& Path, const FUiStyleLimits& Limits)
{
	try
	{
		Toolbox::TVector<Toolbox::uint8> Bytes;
		if (!Toolbox::ReadFileBytes(Path, Bytes, Limits.MaxSourceBytes))
		{
			return TResult<void>::Failure(EErrorCode::NotFound,
			                              Path.ToUtf8() + ": UI style read failed or exceeds limit");
		}

		return Reload({{Path.ToUtf8(), Toolbox::FString(reinterpret_cast<const char*>(Bytes.Data()), Bytes.Size())}},
		              Limits);
	}
	catch (const Toolbox::FException& Error)
	{
		return TResult<void>::Failure(EErrorCode::InvalidArgument, Error.What());
	}
	catch (...)
	{
		return TResult<void>::Failure(EErrorCode::UserException, "UI style file read failed");
	}
}
} // namespace Dxf
