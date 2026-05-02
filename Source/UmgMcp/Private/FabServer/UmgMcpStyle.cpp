// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/UmgMcpStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "FabServer/UmgMcpStyle.h"

TSharedPtr< FSlateStyleSet > FUmgMcpStyle::StyleInstance = NULL;

namespace
{
   static constexpr float UserAvatarDisplaySize = 128;
}

void FUmgMcpStyle::Initialize()
{
    if (!StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

void FUmgMcpStyle::Shutdown()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    ensure(StyleInstance.IsUnique());
    StyleInstance.Reset();
}

FName FUmgMcpStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("UmgMcpStyle"));
    return StyleSetName;
}

float FUmgMcpStyle::GetUserAvatarDisplaySize()
{
    return UserAvatarDisplaySize;
}

#define IMAGE_BRUSH( RelativePath, ... ) FUmgMcpStyle::CreateSafeImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )

const ISlateStyle& FUmgMcpStyle::Get()
{
    return *StyleInstance;
}

TSharedRef< FSlateStyleSet > FUmgMcpStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("UmgMcpStyle"));
	
	// Determine content root. In packaged builds, we expect resources in Content/UmgMcp/
	// In editor, we can look at the source folder or project content.
	FString ContentDir = FPaths::ProjectContentDir() / TEXT("Resources");
	Style->SetContentRoot(ContentDir);

	// --- Rich Text Styles Definition ---
	// Use AppStyle's NormalText as a reliable base for packaged builds
	const FTextBlockStyle& BaseStyle = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	FSlateFontInfo BaseFont = BaseStyle.Font;
	BaseFont.Size = 14;

	// 基础正文
	FTextBlockStyle RichNormal = BaseStyle;
	RichNormal.SetFont(BaseFont);
	RichNormal.SetColorAndOpacity(FLinearColor::White);
	Style->Set("default", RichNormal); 

	// 加粗 (b)
	FTextBlockStyle BoldText = RichNormal;
	FSlateFontInfo BoldFont = BaseFont;
	BoldFont.TypefaceFontName = TEXT("Bold"); // Explicitly request Bold typeface
	BoldText.SetFont(BoldFont);
	Style->Set("b", BoldText);

	// 斜体 (i)
	FTextBlockStyle ItalicText = RichNormal;
	FSlateFontInfo ItalicFont = BaseFont;
	ItalicFont.TypefaceFontName = TEXT("Italic");
	ItalicText.SetFont(ItalicFont);
	Style->Set("i", ItalicText);

	// 代码块 (code)
	FTextBlockStyle CodeText = RichNormal;
	FSlateFontInfo MonoFont = BaseFont;
	MonoFont.TypefaceFontName = TEXT("Mono");
	MonoFont.Size = 13;
	CodeText.SetFont(MonoFont);
	CodeText.SetColorAndOpacity(FLinearColor(0.4f, 0.8f, 1.0f)); 
	Style->Set("code", CodeText);

	// 标题 (h1, h2, h3)
	FTextBlockStyle H1 = BoldText;
	H1.SetFontSize(22);
	H1.SetColorAndOpacity(FLinearColor(1.0f, 0.7f, 0.1f));
	Style->Set("h1", H1);

	FTextBlockStyle H2 = BoldText;
	H2.SetFontSize(20);
	H2.SetColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.2f));
	Style->Set("h2", H2);

	FTextBlockStyle H3 = BoldText;
	H3.SetFontSize(18);
	H3.SetColorAndOpacity(FLinearColor(1.0f, 0.9f, 0.3f));
	Style->Set("h3", H3);

	// 补全缺失的标签定义 (li, quote, hr)
	Style->Set("li", RichNormal);
	
	FTextBlockStyle QuoteText = RichNormal;
	QuoteText.SetColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f));
	Style->Set("quote", QuoteText);

	FTextBlockStyle HRStyle = RichNormal;
	HRStyle.SetFontSize(6);
	Style->Set("hr", HRStyle);

	// 使用 Runtime 安全的笔刷作为气泡背景
	Style->Set("UmgMcp.Bubble.Agent", new FSlateColorBrush(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f)));
	Style->Set("UmgMcp.Bubble.User",  new FSlateColorBrush(FLinearColor(0.15f, 0.35f, 0.75f, 1.0f)));

	// Avatars and Icons (Unified to Icon128 as requested)
	Style->Set("UmgMcp.PluginIcon", IMAGE_BRUSH(TEXT("Icon128"), FVector2D(128.0f, 128.0f)));
	
	// UI Icons
	Style->Set("UmgMcp.Icon.Plus",    IMAGE_BRUSH(TEXT("Icon/plus"),    FVector2D(32.0f, 32.0f)));
	Style->Set("UmgMcp.Icon.History", IMAGE_BRUSH(TEXT("Icon/layout"),  FVector2D(32.0f, 32.0f)));

	// Default to color brushes if textures are missing in packaging
	Style->Set("UmgMcp.Avatar.Agent",    IMAGE_BRUSH(TEXT("Icon128"),    FVector2D(128.0f, 128.0f)));
	Style->Set("UmgMcp.Avatar.User",     IMAGE_BRUSH(TEXT("Icon128"),     FVector2D(128.0f, 128.0f)));
	Style->Set("UmgMcp.Avatar.Default",  new FSlateColorBrush(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f)));
	
	// Ensure these are never null for the system
	Style->Set("UmgMcp.ChatAvatar",      IMAGE_BRUSH(TEXT("Icon128"),    FVector2D(128.0f, 128.0f)));

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

void FUmgMcpStyle::ReloadTextures()
{
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
    }
}

FSlateImageBrush* FUmgMcpStyle::CreateSafeImageBrush(const FString& InPath, const FVector2D& InSize)
{
	if (FPaths::FileExists(InPath))
	{
		// 战术清晰度：确保使用正确的资源加载方式，防止打包后被过度压缩或模糊
		FSlateImageBrush* Brush = new FSlateImageBrush(InPath, InSize);
		return Brush;
	}
	
	// 如果物理文件不存在（打包漏掉），回退到引擎内置的用户图标
	return (FSlateImageBrush*)FAppStyle::Get().GetBrush("Icons.User");
}

FSlateFontInfo FUmgMcpStyle::GetBoldFont()
{
	FSlateFontInfo Font = FAppStyle::Get().GetFontStyle("NormalFont");
	Font.TypefaceFontName = TEXT("Bold");
	return Font;
}

FSlateFontInfo FUmgMcpStyle::GetItalicFont()
{
	FSlateFontInfo Font = FAppStyle::Get().GetFontStyle("NormalFont");
	Font.TypefaceFontName = TEXT("Italic");
	return Font;
}

FSlateFontInfo FUmgMcpStyle::GetMonoFont()
{
	FSlateFontInfo Font = FAppStyle::Get().GetFontStyle("NormalFont");
	Font.TypefaceFontName = TEXT("Mono");
	return Font;
}
