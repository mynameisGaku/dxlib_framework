// SPDX-License-Identifier: NOASSERTION
#include "Toolbox/JsonNumber.h"
#include <stdlib.h>
#include <stdio.h>
namespace Toolbox::JsonPrivate
{
FNumberLocale::FNumberLocale()
{
#ifdef _WIN32
	m_Locale = _create_locale(LC_NUMERIC, "C");
#else
	m_Locale = newlocale(LC_NUMERIC_MASK, "C", nullptr);
#endif
	if (m_Locale == nullptr)
	{
		throw FException("JSON numeric locale allocation failed");
	}
}
FNumberLocale::~FNumberLocale() noexcept
{
#ifdef _WIN32
	_free_locale(m_Locale);
#else
	freelocale(m_Locale);
#endif
}
f64 FNumberLocale::Convert(const char* Text) const noexcept
{
#ifdef _WIN32
	return _strtod_l(Text, nullptr, m_Locale);
#else
	return strtod_l(Text, nullptr, m_Locale);
#endif
}
int32 FNumberLocale::Format(char* Buffer, size_t Capacity, f64 Value) const
{
	if (!IsFinite(Value))
	{
		throw FException("JSON number must be finite");
	}
#ifdef _WIN32
	const int32 Count = _snprintf_l(Buffer, Capacity, "%.17g", m_Locale, Value);
#else
	// POSIXでは現在のスレッドだけを変更し、文字列確保や例外より先に必ず元へ戻す。
	const locale_t Previous = uselocale(m_Locale);
	if (Previous == nullptr)
	{
		throw FException("Cannot select JSON thread numeric locale");
	}
	const int32 Count = snprintf(Buffer, Capacity, "%.17g", Value);
	uselocale(Previous);
#endif
	if (Count < 0 || static_cast<size_t>(Count) >= Capacity)
	{
		throw FException("JSON number output failed");
	}
	return Count;
}
} // namespace Toolbox::JsonPrivate
