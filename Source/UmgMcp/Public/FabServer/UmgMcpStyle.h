// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FUmgMcpStyle
{
public:

    static void Initialize();

    static void Shutdown();

    /** reloads textures used by slate renderer */
    static void ReloadTextures();

    /** @return The Slate style set for the Shooter game */
    static const ISlateStyle& Get();

    static FName GetStyleSetName();

    /** Global avatar display size used by chat widgets. */
    static float GetUserAvatarDisplaySize();

    static struct FSlateImageBrush* CreateSafeImageBrush(const FString& InPath, const FVector2D& InSize);

    static FSlateFontInfo GetBoldFont();
    static FSlateFontInfo GetItalicFont();
    static FSlateFontInfo GetMonoFont();

private:

    static TSharedRef< class FSlateStyleSet > Create();

private:

    static TSharedPtr< class FSlateStyleSet > StyleInstance;
};
