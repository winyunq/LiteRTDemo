#include "WinyunqMcpWrapper.h"

#if WITH_EDITOR
#include "FabServer/ChatUI/SUmgMcpChatWindow.h"
#else
#include "Widgets/SNullWidget.h"
#endif

void UWinyunqMcpWrapper::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	ChatWindowSlate.Reset();
}

TSharedRef<SWidget> UWinyunqMcpWrapper::RebuildWidget()
{
#if WITH_EDITOR
	ChatWindowSlate = SNew(SUmgMcpChatWindow);
	return ChatWindowSlate.ToSharedRef();
#else
	return SNullWidget::NullWidget;
#endif
}
