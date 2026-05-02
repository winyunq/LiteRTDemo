// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpAgentResponseGroup.h"
#include "FabServer/ChatUI/TopBar/SUmgMcpAgentAvatar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "FabServer/ChatUI/MessageInteractionHub/Messages/SUmgMcpAgentResponseGroup/SUmgMcpAgentStatusBar.h"
#include "HAL/PlatformApplicationMisc.h"
#include "FabServer/UmgMcpStyle.h"

// Slate 文本框架头文件
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/IRun.h"
#include "CoreMinimal.h"

namespace 
{
	/** 
	 * 深度 Markdown 解析器
	 * 转换语法并清除原始标记，支持 H1-H5, HR, Bold, Italic, Code
	 */

	FString ConvertMarkdownToRichText(const FString& InText)
	{
		if (InText.IsEmpty()) return FString();

		// 1. 转义关键字符 (HTML 实体化)
		FString Escaped = InText;
		Escaped = Escaped.Replace(TEXT("&"), TEXT("&amp;"));
		Escaped = Escaped.Replace(TEXT("<"), TEXT("&lt;"));
		Escaped = Escaped.Replace(TEXT(">"), TEXT("&gt;"));

		TArray<FString> Lines;
		// 传入 false 保留空行，这样可以维持段落间距
		Escaped.ParseIntoArrayLines(Lines, false);

		// --- 闭包1：严格成对匹配的行内样式解析 ---
		auto ApplyInlineMD = [](FString& Str, const FString& MDTag, const FString& RichTag) {
			int32 StartIdx = 0;
			while (StartIdx < Str.Len())
			{
				// 找起始符号
				int32 OpenPos = Str.Find(MDTag, ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIdx);
				if (OpenPos == INDEX_NONE) break;

				// 找闭合符号
				int32 ClosePos = Str.Find(MDTag, ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenPos + MDTag.Len());
				if (ClosePos == INDEX_NONE)
				{
					// 没有闭合符号，说明不是合法的 Markdown 语法，跳过当前符号
					StartIdx = OpenPos + MDTag.Len();
					continue;
				}

				// 提取中间的文本内容
				FString InsideText = Str.Mid(OpenPos + MDTag.Len(), ClosePos - (OpenPos + MDTag.Len()));
				FString Replacement = FString::Printf(TEXT("<%s>%s</>"), *RichTag, *InsideText);

				// 替换原字符串
				int32 TotalLengthToRemove = ClosePos - OpenPos + MDTag.Len();
				Str.RemoveAt(OpenPos, TotalLengthToRemove);
				Str.InsertAt(OpenPos, Replacement);

				// 更新搜索起始位置
				StartIdx = OpenPos + Replacement.Len();
			}
			};

		// --- 闭包2：清除行内标记（用于标题防嵌套） ---
		auto StripInlineMD = [](FString& Str) {
			Str = Str.Replace(TEXT("***"), TEXT("")).Replace(TEXT("**"), TEXT("")).Replace(TEXT("*"), TEXT(""));
			Str = Str.Replace(TEXT("~~"), TEXT("")).Replace(TEXT("`"), TEXT("")).Replace(TEXT("__"), TEXT(""));
			};

		// 逐行解析
		for (FString& Line : Lines)
		{
			FString TrimmedLine = Line.TrimEnd(); // 保留左侧空格（应对缩进）
			if (TrimmedLine.IsEmpty()) continue;

			FString FinalLine;

			// --- 步骤一：处理块级样式 (Block) ---

			// 1. 分割线 (Horizontal Rule)
			if (TrimmedLine.StartsWith(TEXT("---")) || TrimmedLine.StartsWith(TEXT("***")))
			{
				// 验证这一行是不是只有 - 或 *
				FString Temp = TrimmedLine.Replace(TEXT("-"), TEXT("")).Replace(TEXT("*"), TEXT("")).Replace(TEXT(" "), TEXT(""));
				if (Temp.IsEmpty() && TrimmedLine.Len() >= 3)
				{
					// 【优化】：使用一个不可见字符(零宽空格 \u200B)撑起高度，并赋予专门的 hr 样式
					FinalLine = TEXT("<hr style=\"hr\">\u200B</>");
					// 如果你配置了 URichTextImageDecorator，也可以换成：FinalLine = TEXT("<img id=\"hr\"/>");
					Line = FinalLine;
					continue;
				}
			}

			// 2. 标题 (Headers)
			int32 HeaderLevel = 0;
			while (HeaderLevel < TrimmedLine.Len() && TrimmedLine[HeaderLevel] == '#' && HeaderLevel < 6)
			{
				HeaderLevel++;
			}
			if (HeaderLevel > 0 && TrimmedLine.Len() > HeaderLevel && TrimmedLine[HeaderLevel] == ' ')
			{
				FString HeaderText = TrimmedLine.Mid(HeaderLevel + 1).TrimStartAndEnd();

				// 【核心修复防嵌套】：去除标题内部的 ** 等符号，强制全部使用标题 DataTable 的字体
				StripInlineMD(HeaderText);

				FinalLine = FString::Printf(TEXT("<h%d style=\"h%d\">%s</>"), HeaderLevel, HeaderLevel, *HeaderText);
				Line = FinalLine;
				continue;
			}

			// 3. 引用 (Blockquote)
			if (TrimmedLine.StartsWith(TEXT("> ")))
			{
				FString QuoteText = TrimmedLine.Mid(2).TrimStartAndEnd();
				ApplyInlineMD(QuoteText, TEXT("***"), TEXT("bi"));
				ApplyInlineMD(QuoteText, TEXT("**"), TEXT("b"));
				ApplyInlineMD(QuoteText, TEXT("*"), TEXT("i"));
				ApplyInlineMD(QuoteText, TEXT("`"), TEXT("code"));
				FinalLine = FString::Printf(TEXT("<quote style=\"quote\">%s</>"), *QuoteText);
				Line = FinalLine;
				continue;
			}

			// 4. 无序列表 (Unordered Lists)
			if (TrimmedLine.StartsWith(TEXT("- ")) || TrimmedLine.StartsWith(TEXT("* ")))
			{
				FString ListText = TrimmedLine.Mid(2).TrimStartAndEnd();
				ApplyInlineMD(ListText, TEXT("***"), TEXT("bi"));
				ApplyInlineMD(ListText, TEXT("**"), TEXT("b"));
				ApplyInlineMD(ListText, TEXT("*"), TEXT("i"));
				ApplyInlineMD(ListText, TEXT("`"), TEXT("code"));
				// 添加项目符号 (圆点)
				FinalLine = FString::Printf(TEXT("<li style=\"li\">\u2022 %s</>"), *ListText);
				Line = FinalLine;
				continue;
			}

			// --- 步骤二：处理普通段落的行内样式 (Inline) ---
			ApplyInlineMD(TrimmedLine, TEXT("***"), TEXT("bi"));
			ApplyInlineMD(TrimmedLine, TEXT("**"), TEXT("b"));
			ApplyInlineMD(TrimmedLine, TEXT("__"), TEXT("b")); // 支持 __加粗__
			ApplyInlineMD(TrimmedLine, TEXT("*"), TEXT("i"));
			ApplyInlineMD(TrimmedLine, TEXT("~~"), TEXT("s")); // 删除线
			ApplyInlineMD(TrimmedLine, TEXT("`"), TEXT("code"));

			Line = TrimmedLine;
		}

		return FString::Join(Lines, TEXT("\n"));
	}

	/**
	 * 简易富文本装饰器
	 */
	class FSimpleRichTextDecorator : public ITextDecorator
	{
	public:
		static TSharedRef<FSimpleRichTextDecorator> Create(const ISlateStyle* InStyleSet)
		{
			return MakeShareable(new FSimpleRichTextDecorator(InStyleSet));
		}

		virtual bool Supports(const FTextRunParseResults& RunParseResults, const FString& Text) const override
		{
			// 如果标签名是 b, i, code, h1, h2, h3 等，我们在 StyleSet 中都有定义
			return StyleSet && StyleSet->HasWidgetStyle<FTextBlockStyle>(FName(*RunParseResults.Name));
		}

		virtual TSharedRef<ISlateRun> Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResults, const FString& OriginalText, const TSharedRef<FString>& InOutModelText, const ISlateStyle* InStyleSet) override
		{
			// 1. 计算 ModelRange
			// ModelRange 代表当前这个 Run 在最终生成的 InOutModelText 中的起始和结束位置
			FTextRange ModelRange;
			ModelRange.BeginIndex = InOutModelText->Len();

			// 提取内容并追加到 ModelText
			FString RunContent = OriginalText.Mid(RunParseResults.ContentRange.BeginIndex, RunParseResults.ContentRange.EndIndex - RunParseResults.ContentRange.BeginIndex);
			*InOutModelText += RunContent;

			ModelRange.EndIndex = InOutModelText->Len();

			// 2. 获取 Style
			// 假设你通过 RunParseResults.Name 来查找样式，如果找不到，通常需要一个默认样式
			// 注意：这里使用传入的参数 InStyleSet 而不是 StyleSet
			const FTextBlockStyle& Style = InStyleSet->GetWidgetStyle<FTextBlockStyle>(FName(*RunParseResults.Name));

			// 3. 构建 RunInfo
			FRunInfo RunInfo;
			RunInfo.Name = RunParseResults.Name;

			for (const auto& MetaPair : RunParseResults.MetaData)
			{
				const FTextRange& Range = MetaPair.Value;
				FString MetaValue = OriginalText.Mid(Range.BeginIndex, Range.EndIndex - Range.BeginIndex);
				RunInfo.MetaData.Add(MetaPair.Key, MetaValue);
			}

			// 4. 返回创建的 Run
			// 需要确保包含了 #include "Framework/Text/SlateTextRun.h"
			TSharedRef<const FString> ConstModelText = InOutModelText;
			return FSlateTextRun::Create(RunInfo, ConstModelText, Style, ModelRange);
		}

	private:
		FSimpleRichTextDecorator(const ISlateStyle* InStyleSet) : StyleSet(InStyleSet) {}
		const ISlateStyle* StyleSet;
	};
}

void SUmgMcpAgentResponseGroup::Construct(const FArguments& InArgs)
{
	AgentName = InArgs._AgentName;
	MessageText = InArgs._MessageText;

	ChildSlot
	[
		SNew(SHorizontalBox)
		// 1. 左侧头像 - 战术加固：使用 SBox 锁定 128 像素宽度，彻底杜绝因父容器挤压导致的缩放模糊
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
		[
			SNew(SBox)
			.WidthOverride(128.0f)
			[
				SNew(SUmgMcpAgentAvatar)
				.AgentName(AgentName)
			]
		]

		// 2. 消息内容区 - 核心：使用 FillWidth 配合 WrapTextAt(0.0f) 实现自动折行
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SVerticalBox)
			// 顶部：名字+状态区
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(AgentNameTextBlock, STextBlock)
					.Text(FText::FromString(AgentName))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f, 1.0f))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(AgentStatusBarWidget, SUmgMcpAgentStatusBar)
					.StatusText("")
					.bShowSpinner(false)
				]
			]
			// 消息气泡展示区
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FUmgMcpStyle::Get().GetBrush("UmgMcp.Bubble.Agent"))
				.Padding(FMargin(14.0f, 10.0f))
				[
					SAssignNew(TurnContentBox, SVerticalBox)
				]
			]
			
			// 底部操作区 (复制按钮)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked_Lambda([this]() {
						FPlatformApplicationMisc::ClipboardCopy(*MessageText);
						return FReply::Handled();
					})
					.ToolTipText(FText::FromString(TEXT("Copy message")))
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("GenericCommands.Copy"))
						.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f, 0.8f))
					]
				
			]
		]
	];

	if (!MessageText.IsEmpty())
	{
		AddTextOutputBlock(MessageText);
	}
}

void SUmgMcpAgentResponseGroup::AddTextOutputBlock(const FString& NewText)
{
	if (TurnContentBox.IsValid() && !NewText.IsEmpty())
	{
		ActiveTextBlock.Reset();

		TArray<TSharedRef<class ITextDecorator>> Decorators;
		Decorators.Add(FSimpleRichTextDecorator::Create(&FUmgMcpStyle::Get()));

		TurnContentBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f, 0.0f, 6.0f)
		[
			// 使用 SBox 辅助 SRichTextBlock 确定换行边界
			SAssignNew(ActiveTextBlock, SRichTextBlock)
			.Text(FText::FromString(ConvertMarkdownToRichText(NewText)))
			.TextStyle(&FUmgMcpStyle::Get().GetWidgetStyle<FTextBlockStyle>("default"))
			.DecoratorStyleSet(&FUmgMcpStyle::Get())
			.Decorators(Decorators)
			.AutoWrapText(true)
			.WrapTextAt(0.0f) // 0.0f 表示让 SRichTextBlock 自动根据容器宽度换行
		];
	}
}

void SUmgMcpAgentResponseGroup::AppendToCurrentTextOutputBlock(const FString& PartialText)
{
	if (PartialText.IsEmpty()) return;
	MessageText += PartialText;

	if (ActiveTextBlock.IsValid())
	{
		ActiveTextBlock->SetText(FText::FromString(ConvertMarkdownToRichText(MessageText)));
	}
	else
	{
		AddTextOutputBlock(PartialText);
	}
}

void SUmgMcpAgentResponseGroup::RemoveWidget(TSharedRef<SWidget> WidgetToRemove)
{
	if (TurnContentBox.IsValid())
	{
		TurnContentBox->RemoveSlot(WidgetToRemove);
	}
}

void SUmgMcpAgentResponseGroup::SetAgentStatus(const FText& StatusText, bool bShowSpinner, const FLinearColor& StatusColor)
{
	if (AgentStatusBarWidget.IsValid())
	{
		AgentStatusBarWidget->SetStatus(StatusText.ToString(), bShowSpinner, StatusColor);
	}
}

void SUmgMcpAgentResponseGroup::SetAgentName(const FString& InName)
{
	AgentName = InName;
	if (AgentNameTextBlock.IsValid())
	{
		AgentNameTextBlock->SetText(FText::FromString(InName));
	}
}

void SUmgMcpAgentResponseGroup::AddToolExecutionBlock(const FString& ToolName, const FText& Status, bool bIsError)
{
	if (TurnContentBox.IsValid())
	{
		ActiveTextBlock.Reset();
		TurnContentBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::Format(FText::FromString(TEXT("[{0}] {1}")), FText::FromString(ToolName), Status))
			.ColorAndOpacity(bIsError ? FLinearColor::Red : FLinearColor::Yellow)
		];
	}
}

void SUmgMcpAgentResponseGroup::AddToolExecutionWidget(TSharedRef<SWidget> ToolWidget)
{
	if (TurnContentBox.IsValid())
	{
		ActiveTextBlock.Reset();
		TurnContentBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 4.0f)
		[
			ToolWidget
		];
	}
}

bool SUmgMcpAgentResponseGroup::IsEmpty() const
{
	return MessageText.IsEmpty() && (!TurnContentBox.IsValid() || TurnContentBox->NumSlots() == 0);
}

FString SUmgMcpAgentResponseGroup::GetAgentName() const
{
	return AgentNameTextBlock.IsValid() ? AgentNameTextBlock->GetText().ToString() : TEXT("");
}
