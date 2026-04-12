# Decal Culling And Clustered Notes

## Summary

오늘 적용한 변경은 데칼 렌더링 비용을 줄이기 위한 2단계 후보 축소와 통계 노출이다.

1. `OBB-AABB + Scene BVH` 기반 receiver pruning
2. `screen tile + depth slice` 기반 3D clustered decal assignment
3. 클러스터 범위를 이용한 decal draw scissor 적용
4. 에디터 통계 창에 decal 관련 수치 표시

현재 구현은 GPU에서 클러스터 리스트를 직접 조회하는 방식은 아니고, 기존 엔진 구조를 유지한 채 CPU에서 coarse culling과 scissor 축소를 수행하는 형태다.

## Background

이 엔진의 현재 decal은 `decal volume box mesh`를 그린 뒤, 픽셀 셰이더에서 현재 픽셀의 depth를 읽고 월드 위치를 복원해서 decal box 내부인지 판정하는 방식이다.

즉:

- decal 자체는 box mesh를 가진다
- receiver mesh를 별도로 생성해서 붙이는 mesh decal 방식은 아니다
- depth를 전 화면 스캔하는 것은 아니지만, decal volume가 덮는 픽셀에 대해서는 픽셀 셰이더가 실행된다

최적화 전에는:

- 프러스텀 컬링으로 visible primitive만 추렸고
- visible decal은 거의 그대로 draw command로 들어갔다
- decal receiver가 실제로 없는 경우도 draw가 발생할 수 있었다

## Stage 1: OBB-AABB + Scene BVH

### What it does

각 decal에 대해 OBB를 만들고, Scene BVH 안의 primitive AABB와 교차 검사를 수행한다.

이때:

- 현재 프레임에서 보이는 receiver 후보 집합을 먼저 만든다
- decal OBB와 교차하는 primitive가 하나도 없으면 그 decal은 렌더링에서 제외한다

즉 이 단계는 다음 질문에 답하는 용도다.

`이 decal을 아예 그릴 필요가 있는가?`

### Receiver definition

현재 receiver 후보는 다음 조건을 만족하는 visible primitive다.

- `RenderMesh`가 있다
- `UDecalComponent`는 아니다

현재 구현에서는 주로 다음이 포함된다.

- `MeshPrimitives`
- `SubUVPrimitives`

### Effect

이 단계에서 줄어드는 것은 주로 다음이다.

- 불필요한 decal draw command
- 불필요한 decal volume draw
- 그에 따른 pixel shader 실행

## Stage 2: 3D Clustered Decal Assignment

### Grid layout

화면은 다음 기준으로 3D 클러스터로 나뉜다.

- `X/Y`: 화면 타일
- `Z`: depth slice

현재 기본값:

- tile size: `16 x 16`
- depth slices: `16`

즉 클러스터 인덱스는 개념적으로 다음과 같다.

`(tileX, tileY, depthSlice)`

### How assignment works

각 decal box의 8개 코너를 사용한다.

1. local box corners 생성
2. `World -> Clip -> NDC -> Screen`으로 투영
3. screen min/max 범위 계산
4. view-space 깊이 min/max 범위 계산
5. 해당하는 tile 범위와 depth slice 범위를 구함
6. 그 범위에 포함되는 모든 클러스터에 decal 인덱스를 등록

### Why depth is not world Z

이 엔진은:

- left-handed
- `Z-up`

이지만, 카메라 전방축은 world `X`다.

따라서 clustered slicing에서 사용하는 깊이는 world `Z`가 아니라 `view-space forward distance`다.

현재 구현에서 depth 기준은 다음과 같다.

- `view-space X`

즉:

- world up axis가 무엇인지는 중요하지 않다
- 카메라 기준 전방 거리축이 무엇인지가 중요하다

## Logarithmic Depth Slicing

처음 clustered는 `NDC/depth-buffer` 값을 균등 분할했지만, 이후 로그 분할로 변경했다.

현재 slice 계산 기준:

- Perspective: logarithmic slicing
- Orthographic: linear slicing

Perspective에서 개념식은 다음과 같다.

```cpp
viewDepth = viewPos.X;
sliceT = log(viewDepth / near) / log(far / near);
slice = clamp(int(sliceT * depthSlices), 0, depthSlices - 1);
```

Orthographic에서는 비선형 depth 이점이 없으므로 선형 분할을 사용한다.

## Scissor Optimization

cluster assignment 결과로 얻은 screen tile 범위를 이용해 decal draw command에 scissor rect를 설정한다.

즉:

- decal이 화면 일부 타일만 덮으면
- 그 타일 범위 바깥에서는 rasterization/pixel shader 실행을 막는다

이 최적화는 pixel cost를 줄이는 방향이다.

단, near plane 뒤로 일부 코너가 걸치는 경우에는 보수적으로 scissor를 끄고 진행한다.

## Stats In Editor

메모리/통계 오버레이에서 다음 decal 수치를 확인할 수 있다.

### Visible

프러스텀 컬링까지 통과해 이번 프레임 scene packet에 들어온 decal 수.

### Culled

`Visible` 중에서 OBB-AABB + BVH 및 clustered 단계에서 제거되어 draw되지 않은 decal 수.

### Rendered

실제로 decal draw command가 생성된 수.

### Receivers

현재 프레임에서 decal receiver로 간주한 visible primitive pool 크기.

이 값은 per-decal receiver count가 아니라, 프레임 전체의 receiver 후보 집합 크기다.

### Draw Calls

현재 구현상 decal draw call 수.
대체로 `Rendered`와 동일하다.

### Cluster Assignments

렌더된 decal들이 총 몇 개의 3D 클러스터에 등록되었는지의 합.

예:

- 한 decal이 `3 x 2 x 4` 클러스터를 덮으면 `24`

### Max Cluster Load

하나의 단일 클러스터에 들어간 decal 개수의 최대값.

이 값이 높을수록 특정 화면/깊이 구역에 decal 중첩이 심하다.

### Cull CPU

CPU가 이번 프레임 decal pruning/assignment/scissor 계산에 사용한 시간.

현재 포함 범위:

- receiver BVH 검사
- cluster range 계산
- cluster assignment
- scissor rect 계산

## Files Touched

- `Engine/Source/Renderer/SceneCommandBuilder.cpp`
- `Engine/Source/Renderer/SceneCommandBuilder.h`
- `Engine/Source/Renderer/Feature/DecalRenderFeature.h`
- `Engine/Source/Renderer/Feature/DecalRenderFeature.cpp`
- `Engine/Source/Renderer/RenderCommand.h`
- `Engine/Source/Renderer/RenderState.h`
- `Engine/Source/Renderer/RenderState.cpp`
- `Engine/Source/Renderer/SceneRenderer.cpp`
- `Engine/Source/Renderer/Renderer.h`
- `Engine/Source/Core/ViewportClient.cpp`
- `Engine/Source/Camera/Camera.h`
- `Engine/Source/Component/CameraComponent.h`
- `Engine/Source/Component/CameraComponent.cpp`
- `Editor/Source/Viewport/PreviewViewportClient.cpp`
- `Editor/Source/Viewport/Services/EditorViewportRenderService.cpp`
- `Editor/Source/UI/StatWindow.h`
- `Editor/Source/UI/StatWindow.cpp`
- `Editor/Source/UI/EditorUI.cpp`

## Current Limitations

- clustered 정보는 아직 GPU shader 쪽에서 직접 조회하지 않는다
- receiver pruning은 coarse OBB-AABB 수준이다
- mesh BVH를 이용한 triangle-level receiver refinement는 아직 없다
- near plane을 가로지르는 decal은 보수적으로 처리한다
- `Cluster Assignments`는 실제 shading count가 아니라 cluster 등록 수의 총합이다

## Build Verification

다음 빌드를 통과했다.

- `Engine\\Engine.vcxproj` `Debug|x64`
- `Editor\\Editor.vcxproj` `Debug|x64`

## Practical Reading Guide

수치 해석은 보통 이렇게 보면 된다.

- `Culled`가 늘면 1차/2차 pruning이 잘 먹히는 것
- `Rendered`가 줄면 실제 decal draw가 줄어든 것
- `Cluster Assignments`가 크면 decal이 넓게 퍼져 있거나 많다는 뜻
- `Max Cluster Load`가 크면 특정 구역에 decal 중첩이 심한 것
- `Cull CPU`가 너무 커지면 pruning 비용이 이득을 잠식할 수 있음
