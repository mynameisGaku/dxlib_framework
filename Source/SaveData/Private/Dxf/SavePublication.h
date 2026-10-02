// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_SAVE_PUBLICATION_PRIVATE_H
#define DXF_SAVE_PUBLICATION_PRIVATE_H
#include "Dxf/SaveError.h"
#include "Toolbox/FileError.h"
namespace Dxf::SavePrivate
{
/**
 * 公開APIの戻り値だけで名前状態を断定せず、予定バイトの読戻しで確定する。
 * @param Io 所有を保持した公開先と一時ファイルの操作。正常な最新面は操作しない。
 */
template <typename TIo>
void VerifyPublication(TIo& Io)
{
	// 名前公開が失敗しても、その後の実バイト照合を省かない。
	bool Failed = false;
	Toolbox::uint32 FailureCode = 0;
	try
	{
		Io.Publish();
	}
	catch (const Toolbox::FFileError& Error)
	{
		Failed = true;
		FailureCode = Error.GetCode();
	}
	try
	{
		if (!Io.Matches())
		{
			if (Failed)
			{
				throw Toolbox::FFileError(Toolbox::EFileOperation::Publish, FailureCode);
			}
			throw FSaveError(ESaveError::PublicationUnknown, "Published save could not be verified");
		}
	}
	catch (const FSaveError&)
	{
		throw;
	}
	catch (const Toolbox::FFileError& Error)
	{
		if (Error.GetOperation() != Toolbox::EFileOperation::Publish)
		{
			throw FSaveError(ESaveError::PublicationUnknown, "Publication read-back I/O failed; ownership retained");
		}
		throw;
	}
	catch (...)
	{
		throw FSaveError(ESaveError::PublicationUnknown, "Publication read-back failed; ownership retained");
	}
}
} // namespace Dxf::SavePrivate
#endif
