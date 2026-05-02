// Copyright (c) 2025-2026 Winyunq. All rights reserved.

#include "FabServer/History/UmgMcpConversationHistoryNormalizer.h"
#include "Internationalization/Regex.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FUmgMcpConversationHistoryNormalizer::ExtractTextFromParts(const TArray<TSharedPtr<FJsonValue>>& Parts)
{
    TArray<FString> TextPieces;
    for (const auto& PartVal : Parts)
    {
        if (!PartVal.IsValid()) continue;
        TSharedPtr<FJsonObject> PartObj = PartVal->AsObject();
        if (!PartObj.IsValid()) continue;

        FString Text;
        if (PartObj->TryGetStringField(TEXT("text"), Text))
        {
            TextPieces.Add(Text);
            continue;
        }

        if (PartObj->HasField(TEXT("functionCall")))
        {
            const TSharedPtr<FJsonObject> FnCall = PartObj->GetObjectField(TEXT("functionCall"));
            FString Name = FnCall->GetStringField(TEXT("name"));
            FString ArgsString;
            const TSharedPtr<FJsonObject> ArgsObj = FnCall->GetObjectField(TEXT("args"));
            if (ArgsObj.IsValid())
            {
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArgsString);
                FJsonSerializer::Serialize(ArgsObj.ToSharedRef(), Writer);
            }
            TextPieces.Add(FString::Printf(TEXT("[FunctionCall] %s %s"), *Name, *ArgsString));
            continue;
        }

        if (PartObj->HasField(TEXT("functionResponse")))
        {
            const TSharedPtr<FJsonObject> FnResp = PartObj->GetObjectField(TEXT("functionResponse"));
            FString Name = FnResp->GetStringField(TEXT("name"));
            FString RespString;
            const TSharedPtr<FJsonObject>* RespObjPtr = nullptr;
            if (FnResp->TryGetObjectField(TEXT("response"), RespObjPtr) && RespObjPtr && RespObjPtr->IsValid())
            {
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RespString);
                FJsonSerializer::Serialize((*RespObjPtr).ToSharedRef(), Writer);
            }
            TextPieces.Add(FString::Printf(TEXT("[FunctionResponse] %s %s"), *Name, *RespString));
            continue;
        }

        if (PartObj->HasField(TEXT("inline_data")))
        {
            TextPieces.Add(TEXT("[Image: inline_data]"));
            continue;
        }
    }

    return TextPieces.Num() > 0 ? FString::Join(TextPieces, TEXT("\n")) : FString();
}

TArray<TSharedPtr<FJsonObject>> FUmgMcpConversationHistoryNormalizer::Normalize(const TArray<TSharedPtr<FJsonObject>>& InConversationHistory)
{
    TArray<TSharedPtr<FJsonObject>> Normalized;

    for (const auto& Entry : InConversationHistory)
    {
        if (!Entry.IsValid()) continue;
        FString Role;
        if (!Entry->TryGetStringField(TEXT("role"), Role)) continue;

        Role = Role.ToLower();
        if (Role == TEXT("model")) Role = TEXT("assistant");
        if (Role == TEXT("function")) Role = TEXT("tool");

        FString Content;
        Entry->TryGetStringField(TEXT("content"), Content);
        if (Content.IsEmpty())
        {
            const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
            if (Entry->TryGetArrayField(TEXT("parts"), Parts) && Parts)
            {
                Content = ExtractTextFromParts(*Parts);
            }
        }

        TSharedPtr<FJsonObject> NormalizedEntry = MakeShared<FJsonObject>();
        NormalizedEntry->SetStringField(TEXT("role"), Role);
        if (!Content.IsEmpty())
        {
            NormalizedEntry->SetStringField(TEXT("content"), Content);
        }

        FString ImageBase64;
        if (Entry->TryGetStringField(TEXT("image_base64"), ImageBase64) && !ImageBase64.IsEmpty())
        {
            NormalizedEntry->SetStringField(TEXT("image_base64"), ImageBase64);
        }

        if (Entry->HasField(TEXT("tool_call_id")))
        {
            NormalizedEntry->SetStringField(TEXT("tool_call_id"), Entry->GetStringField(TEXT("tool_call_id")));
        }

        if (Entry->HasField(TEXT("name")))
        {
            NormalizedEntry->SetStringField(TEXT("name"), Entry->GetStringField(TEXT("name")));
        }

        if (Entry->HasField(TEXT("tool_calls")))
        {
            NormalizedEntry->SetArrayField(TEXT("tool_calls"), Entry->GetArrayField(TEXT("tool_calls")));
        }
        Normalized.Add(NormalizedEntry);
    }

    return Normalized;
}

FString FUmgMcpConversationHistoryNormalizer::CleanHtmlForRichText(const FString& InHtml)
{
	if (InHtml.IsEmpty()) return InHtml;

	FString Result = InHtml;

	// 战术正则：匹配 <tag style="..."> 并替换为 <tag>
	// 表达式解释：匹配 < 加上字母数字标签名，后面跟着空格和任意直到 > 的内容
	const FString Pattern = TEXT("<([a-zA-Z0-9]+)\\s+[^>]*>");
	const FRegexPattern RegexPattern(Pattern);
	FRegexMatcher Matcher(RegexPattern, Result);

	int32 Offset = 0;
	while (Matcher.FindNext())
	{
		int32 Start = Matcher.GetMatchBeginning() + Offset;
		int32 End = Matcher.GetMatchEnding() + Offset;
		FString TagName = Matcher.GetCaptureGroup(1);
		FString Replacement = FString::Printf(TEXT("<%s>"), *TagName);

		Result.RemoveAt(Start, End - Start);
		Result.InsertAt(Start, Replacement);

		// 更新偏移量，因为替换后的字符串长度变了
		Offset += (Replacement.Len() - (End - Start));
		
		// 重新创建 Matcher 避免索引失效
		Matcher = FRegexMatcher(RegexPattern, Result);
	}

	return Result;
}
