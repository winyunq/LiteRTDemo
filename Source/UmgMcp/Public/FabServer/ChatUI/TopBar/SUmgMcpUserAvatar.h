// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "FabServer/ChatUI/TopBar/SUmgMcpAvatar.h"

// 用户头像派生类：点击弹出登录/登出面板
class UMGMCP_API SUmgMcpUserAvatar : public SUmgMcpAvatar
{
public:
	SLATE_BEGIN_ARGS(SUmgMcpUserAvatar)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// 实现菜单内容：登录选项
	virtual TSharedRef<SWidget> OnGetMenuContent() override;
};
