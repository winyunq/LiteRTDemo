#include "WinyunqMcpWrapper.h"
#include "FabServer/ChatUI/SUmgMcpChatWindow.h"

void UWinyunqMcpWrapper::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	ChatWindowSlate.Reset();
}

TSharedRef<SWidget> UWinyunqMcpWrapper::RebuildWidget()
{
	ChatWindowSlate = SNew(SUmgMcpChatWindow);
	return ChatWindowSlate.ToSharedRef();
}
