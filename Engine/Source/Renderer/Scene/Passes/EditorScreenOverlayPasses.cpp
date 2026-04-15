#include "Renderer/Scene/Passes/ScenePasses.h"

#include "Renderer/Features/Debug/DebugLineRenderFeature.h"
#include "Renderer/Renderer.h"
#include "Renderer/Scene/Passes/ScenePassExecutionUtils.h"

bool FEditorPrimitivePass::Execute(FPassContext& Context)
{
	ID3D11RenderTargetView* OverlayRenderTarget = Context.Targets.OverlayColorRTV
		? Context.Targets.OverlayColorRTV
		: Context.Targets.SceneColorRTV;

	if (!OverlayRenderTarget)
	{
		return true;
	}

	BeginPass(
		Context.Renderer,
		OverlayRenderTarget,
		Context.Targets.SceneDepthDSV,
		Context.SceneViewData.View.Viewport,
		Context.SceneViewData.Frame,
		Context.SceneViewData.View);
	Processor.ExecutePass(Context.Renderer, Context.Targets, Context.SceneViewData, EMeshPassType::EditorPrimitive);
	EndPass(
		Context.Renderer,
		Context.Targets.SceneColorRTV,
		Context.Targets.SceneDepthDSV,
		Context.SceneViewData.View.Viewport,
		Context.SceneViewData.Frame,
		Context.SceneViewData.View);
	return true;
}

bool FEditorLinePass::Execute(FPassContext& Context)
{
	FDebugLineRenderFeature* DebugLineFeature = Context.Renderer.GetDebugLineFeature();
	if (!DebugLineFeature || Context.SceneViewData.DebugInputs.LinePass.IsEmpty())
	{
		return true;
	}

	return DebugLineFeature->Render(
		Context.Renderer,
		Context.SceneViewData.Frame,
		Context.SceneViewData.View,
		Context.Targets,
		Context.SceneViewData.DebugInputs.LinePass);
}
