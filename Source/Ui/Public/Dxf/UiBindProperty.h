// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_UI_BIND_PROPERTY_H
#define DXF_UI_BIND_PROPERTY_H
#include "Dxf/UiElement.h"
#include "Dxf/UiProperty.h"
namespace Dxf
{
/**
 * 現在値を一度表示し、以後の変更を接続期間へ結び付ける。OnAttachから使用する。
 * Setterは表示だけを変更し、操作イベントを発行しない。対象は通知直前に再解決する。
 * @param Element 対象要素の世代付き参照。
 * @param Property ゲーム側の表示データ。
 * @param Setter (要素&, const 値&)を受け取る表示処理。
 */
template <typename TElement, typename TValue, typename TSetter>
void BindUiProperty(const TUiRef<TElement>& Element, TUiProperty<TValue>& Property, TSetter Setter)
{
	TElement* Target = Element.Get();
	if (Target == nullptr)
	{
		throw Toolbox::FException("Cannot bind a destroyed UI element");
	}
	const TValue Current = Property.Get();
	auto SharedSetter = Toolbox::MakeShared<TSetter>(Toolbox::Move(Setter));
	auto Subscription = Property.Subscribe(
	    [Element, SharedSetter](const TValue& Value)
	    {
		    if (TElement* Live = Element.Get(); Live != nullptr && Live->IsAttached())
		    {
			    (*SharedSetter)(*Live, Value);
		    }
	    });
	Target->GetAttachScope().Add(Toolbox::Move(Subscription));
	(*SharedSetter)(*Target, Current);
}
} // namespace Dxf
// namespace Dxf
#endif
