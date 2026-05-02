// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"

#if !WITH_EDITOR
    // Define GEditor as nullptr in non-editor builds to avoid compilation errors
    #define GEditor nullptr
    
    // GEditorPerProjectIni is usually not defined in non-editor builds
    #ifndef GEditorPerProjectIni
        #define GEditorPerProjectIni GGameIni
    #endif
#endif
