#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderCommand.h"

#include <functional>

class FRenderer;
struct FViewportScenePassRequest;

struct FScenePassExecutionContext
{
    FRenderer& Renderer;
    const FViewportScenePassRequest& ScenePass;
    FRenderCommandQueue SceneQueue;
    bool bSceneQueueBuilt = false;
};

using FScenePassCallback = std::function<bool(FScenePassExecutionContext&)>;

struct FNamedScenePassCallback
{
    FString Name;
    FScenePassCallback Callback;
};

class ENGINE_API FScenePassSequence
{
public:
    void Reserve(size_t Count)
    {
        Passes.reserve(Count);
    }

    void AddPass(const FString& Name, FScenePassCallback Callback)
    {
        if (!Callback)
        {
            return;
        }

        Passes.push_back({Name, std::move(Callback)});
    }

    bool Execute(FScenePassExecutionContext& Context) const
    {
        for (const FNamedScenePassCallback& Pass : Passes)
        {
            if (!Pass.Callback)
            {
                continue;
            }

            if (!Pass.Callback(Context))
            {
                return false;
            }
        }

        return true;
    }

    bool IsEmpty() const
    {
        return Passes.empty();
    }

private:
    TArray<FNamedScenePassCallback> Passes;
};
