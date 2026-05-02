#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WinyunqMcpWrapper.generated.h"

/**
 * UMG Wrapper for the SUmgMcpChatWindow Slate widget.
 */
UCLASS()
class LITERTDEMO_API UWinyunqMcpWrapper : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<class SUmgMcpChatWindow> ChatWindowSlate;
};
