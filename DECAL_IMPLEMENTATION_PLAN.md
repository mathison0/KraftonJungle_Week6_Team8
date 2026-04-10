# 픽셀 데칼 구현 계획

## 목표

이 문서는 현재 엔진 구조에서 픽셀 데칼을 구현할 때의 기본 방향과 단계별 작업 순서를 정리한 문서다.

이번 구현의 전제는 아래와 같다.

- 프로젝션 방향은 로컬 `+X Forward Vector` 기준이다.
- 데칼 전용 `Vertex Shader`, `Pixel Shader`를 추가한다.
- 데칼 전용 렌더 패스를 추가한다.
- `UDecalComponent`를 구현한다.
- `ADecalActor`가 `UDecalComponent`를 멤버로 가진다.
- `Decal Show Flag`를 구현한다.
- `Decal Stat`을 구현한다.
- 최적화가 중요하므로 `BVH + SAT`와 `클러스터드 스크린 분할`을 기본 전략으로 사용한다.

## 1. 전체 방향

엔진 전체는 `Forward Rendering` 구조를 유지한다.

데칼은 아래 흐름으로 처리한다.

1. 씬에서 데칼 컴포넌트를 수집한다.
2. 각 데칼을 월드 공간 `OBB`로 만든다.
3. `Scene BVH`를 순회하면서 `Decal OBB vs BVH Node AABB`에 대해 `SAT`를 수행한다.
4. 겹치는 수신 후보만 추린다.
5. 추린 후보를 화면 기준 `클러스터드 그리드`에 분류한다.
6. 데칼 전용 패스에서 해당 클러스터에 걸린 데칼만 처리한다.

즉 이 설계는 아래 세 가지를 결합한 구조다.

- `Forward Renderer`
- `BVH/SAT 기반 공간 필터링`
- `Clustered Screen-Space Filtering`

이 문서는 이 조합을 기본 구현 방향으로 본다.

## 2. 왜 BVH + SAT + 클러스터드를 같이 써야 하는가

데칼 최적화에서 가장 안 좋은 형태는 아래다.

- 보이는 모든 메시를 데칼마다 검사
- 보이는 모든 픽셀에서 모든 데칼을 검사

이 방식은 씬이 커질수록 급격히 비싸진다.

권장 구조는 두 단계 필터링이다.

### 2-1. 1차 필터: BVH + SAT

월드 공간 기준으로 수신 후보를 줄인다.

- 데칼은 `OBB`
- BVH 노드는 `AABB`
- `SAT`로 겹침 판정
- 겹치지 않으면 노드 전체를 제거

이 단계의 목적은 "이 데칼이 어느 오브젝트 근처에 있는가"를 빠르게 줄이는 것이다.

### 2-2. 2차 필터: 클러스터드 스크린 분할

화면 공간 기준으로 픽셀 셰이더가 검사할 데칼 수를 줄인다.

- 스크린을 일정 크기 타일 또는 클러스터로 나눈다.
- 각 데칼의 screen-space bounds를 계산한다.
- 데칼이 걸치는 클러스터 목록에 인덱스를 등록한다.
- 패스에서는 현재 픽셀이 속한 클러스터의 데칼 목록만 검사한다.

이 단계의 목적은 "이 픽셀이 어떤 데칼 후보를 봐야 하는가"를 줄이는 것이다.

즉 역할이 다르다.

- `BVH + SAT`: 월드 기준 후보 오브젝트 축소
- `Clustered`: 화면 기준 후보 데칼 축소

둘 다 있어야 공간 비용과 픽셀 비용을 동시에 줄일 수 있다.

## 3. 현재 코드베이스에서 재사용 가능한 구조

이미 존재하는 관련 구조:

- 씬 BVH
  - `Engine/Source/Level/BVH.h`
  - `Engine/Source/Level/BVH.cpp`
- 메시 단위 BVH
  - `Engine/Source/Level/MeshBVH.h`
  - `Engine/Source/Level/MeshBVH.cpp`
- 스태틱 메시 BVH 접근
  - `Engine/Source/Renderer/MeshData.h`
  - `Engine/Source/Renderer/MeshData.cpp`
- 씬 렌더 패킷
  - `Engine/Source/Level/SceneRenderPacket.h`
- 메인 렌더 실행기
  - `Engine/Source/Renderer/SceneRenderer.h`
  - `Engine/Source/Renderer/SceneRenderer.cpp`

즉 이 엔진은 데칼 최적화를 처음부터 새로 설계할 필요가 없다.
이미 있는 `BVH`, `MeshBVH`, `SceneRenderPacket`, `SceneRenderer` 위에 얹는 방식이 가장 자연스럽다.

## 4. 기본 데이터 구조

### Step 1. `UDecalComponent` 추가

추가 파일:

- `Engine/Source/Component/DecalComponent.h`
- `Engine/Source/Component/DecalComponent.cpp`

권장 상속:

- `UDecalComponent : public UPrimitiveComponent`

초기 멤버 추천:

- `FVector DecalExtent`
- `FVector4 TintColor`
- `float Opacity`
- 데칼 텍스처 경로 또는 텍스처 참조
- `bool bVisible`
- 필요 시 `int32 SortOrder`

좌표 규칙:

- `+X`: projection forward
- `+Y`: decal width
- `+Z`: decal height

Extent 해석:

- `Extent.X`: 투영 깊이
- `Extent.Y`: 반너비
- `Extent.Z`: 반높이

필수 함수:

- `GetWorldBounds()`
- `CalcBounds()`
- `Serialize()`
- `DuplicateShallow()`

### Step 2. `ADecalActor` 추가

추가 파일:

- `Engine/Source/Actor/DecalActor.h`
- `Engine/Source/Actor/DecalActor.cpp`

구조:

- `ADecalActor : public AActor`
- `UDecalComponent* DecalComponent` 보유
- `PostSpawnInitialize()`에서 생성
- `FixupDuplicatedReferences()` 구현

형태는 `ASubUVActor`와 비슷하게 가져가면 된다.

## 5. 씬 수집 구조

### Step 3. `FSceneRenderPacket`에 데칼 버킷 추가

추가 구조:

- `struct FSceneDecalPrimitive { UDecalComponent* Component = nullptr; };`
- `TArray<FSceneDecalPrimitive> DecalPrimitives;`

수정 파일:

- `Engine/Source/Level/SceneRenderPacket.h`
- scene packet 생성 경로
- editor viewport render service
- game / preview world scene packet 생성 경로

수집 조건:

- `UDecalComponent`
- visible
- pending kill 아님
- frustum bounds 통과
- `ShowFlags.HasFlag(EEngineShowFlags::SF_Decal)` 통과

이 단계에서는 데칼만 모은다.
수신 후보 축소와 클러스터 분류는 렌더 준비 단계에서 수행한다.

## 6. 1차 최적화: Scene BVH + SAT

### Step 4. 데칼 OBB vs BVH AABB SAT를 기본 수신 후보 탐색으로 사용

이 단계는 선택 사항이 아니라 기본 동작이어야 한다.

데칼 하나당 처리 순서:

1. 데칼 transform으로 월드 공간 `OBB`를 만든다.
2. 레벨 `BVH` 루트부터 순회한다.
3. 각 노드 `AABB`와 `SAT(OBB, AABB)`를 수행한다.
4. 겹치지 않으면 하위 노드 전체를 버린다.
5. 겹치면 하위로 내려간다.
6. leaf에 도달하면 포함된 primitive 후보를 수집한다.

이 단계의 출력:

- `VisibleDecalCount`
- `SceneBVHVisitedNodeCount`
- `SceneBVHCulledNodeCount`
- `ReceiverPrimitiveCount`

### Step 5. 큰 메시에 대해서는 `MeshBVH`까지 내려간다

Scene BVH만으로는 큰 스태틱 메시 하나가 통째로 수신 후보가 될 수 있다.
최적화가 중요하면 여기서 끝내면 부족할 수 있다.

권장 방식:

1. Scene BVH에서 `UStaticMeshComponent` 후보를 찾는다.
2. 해당 메시가 크거나 triangle 수가 많으면 `MeshBVH`로 한 번 더 좁힌다.
3. `Decal OBB`와 메시 BVH node bounds를 다시 비교한다.
4. 영향이 없는 하위 노드는 버린다.

즉 수신 후보 축소는 두 단계다.

- 1단계: `Scene BVH`
- 2단계: `Mesh BVH`

## 7. 2차 최적화: Clustered Screen-Space Binning

### Step 6. 화면을 그리드로 나누는 클러스터드를 적용

이 문서에서는 클러스터드를 기본 전략에 포함한다.

첫 버전은 복잡한 3D frustum cluster보다 단순한 `2D screen tile cluster`부터 시작하는 것이 좋다.

초기 추천:

- 화면을 `16x16` 또는 `32x32` 픽셀 타일로 분할
- 각 타일마다 "영향을 주는 데칼 인덱스 목록"을 가진다

클러스터 생성 순서:

1. 데칼 OBB의 월드 공간 꼭짓점 8개를 구한다.
2. 이를 clip / NDC / screen space로 변환한다.
3. 화면 bounding rect를 만든다.
4. 이 rect가 겹치는 tile 범위를 계산한다.
5. 해당 tile 목록에 데칼 인덱스를 추가한다.

클러스터드의 목적:

- 모든 픽셀이 모든 데칼을 보는 것을 방지
- 현재 픽셀이 속한 screen tile의 데칼만 검사

즉 픽셀 셰이더 또는 데칼 패스 입력 단계에서 아래처럼 줄일 수 있다.

- 전체 데칼 N개 검사
- 현재 tile의 데칼 K개만 검사

보통 `K << N`이 되도록 만드는 것이 목표다.

### Step 7. 클러스터드는 BVH를 대체하는 것이 아니라 보완하는 것이다

중요한 점:

- `BVH + SAT`는 월드 공간 필터
- `Clustered`는 화면 공간 필터

둘은 서로 대체 관계가 아니다.
권장 파이프라인은 아래와 같다.

1. 데칼 수집
2. 데칼 OBB 생성
3. Scene BVH + SAT로 수신 후보 축소
4. 필요 시 Mesh BVH로 추가 축소
5. screen-space bounds 계산
6. tile / cluster binning
7. decal pass 실행

## 8. 전용 데칼 볼륨 메시를 쓰는 이유

`GetRenderMesh()`에 수신 메시를 직접 넘기기보다, 데칼 전용 박스 메시를 쓰는 쪽이 맞다.

이유:

- 데칼은 "자기 표면 메시"가 아니라 "프로젝터 볼륨"이 본체다.
- `GetRenderMesh()`는 보통 그 컴포넌트 자신의 렌더 메시를 의미한다.
- 데칼은 수신 메시를 소유하지 않는다.
- 수신 메시 탐색은 `BVH + SAT`가 담당하고, 프로젝터 표현은 데칼 박스가 담당해야 책임이 분리된다.

즉 역할을 나누는 것이 중요하다.

- `UDecalComponent`: 프로젝터 데이터 보유
- `BVH + SAT`: 수신 오브젝트 탐색
- `Clustered`: 화면 기준 데칼 후보 축소
- `Decal Box Mesh`: 렌더 시 프로젝터 표현

## 9. Show Flag

### Step 8. `Decal Show Flag` 추가

수정 파일:

- `Engine/Source/Core/ShowFlags.h`
- 필요 시 editor viewport 토글 UI

추가 값:

- `SF_Decal = 1 << N`

권장 규칙:

- `SF_Primitives`가 꺼져 있으면 데칼도 끈다.
- `SF_Decal`이 꺼져 있으면 데칼만 별도로 끈다.

기본값:

- `On`

## 10. 셰이더 작업

### Step 9. 데칼 전용 셰이더 추가

추가 파일:

- `Engine/Shaders/DecalVertexShader.hlsl`
- `Engine/Shaders/DecalPixelShader.hlsl`
- 컴파일 결과 `Content/Shaders/DecalVertexShader_main.cso`
- 컴파일 결과 `Content/Shaders/DecalPixelShader_main.cso`

Vertex Shader 역할:

- unit box를 데칼 월드 변환으로 보낸다.
- clip position을 출력한다.
- 필요한 경우 local position, world position 관련 데이터를 넘긴다.

Pixel Shader 역할:

- 데칼 텍스처 샘플링
- tint / opacity 적용
- 로컬 `+X` 축 기준 투영 계산
- 박스 범위 밖은 clip 또는 discard

여기서 구현 방향은 두 가지가 있다.

### 방향 A. Depth 복원 기반 픽셀 데칼

- scene depth를 읽는다.
- world position을 복원한다.
- `WorldToDecal`로 로컬 좌표를 만든다.
- 로컬 X, Y, Z로 데칼 적용 여부를 판단한다.

### 방향 B. 수신 후보 제한 기반 데칼 패스

- BVH/SAT로 추린 receiver에 대해서만 데칼 작업을 수행한다.
- screen-space cluster도 반영해서 tile 단위로 데칼 후보를 제한한다.

이번 문서에서는 `최적화 우선`이므로 아래를 기본으로 본다.

- `BVH/SAT`는 필수
- `Clustered`도 필수
- depth 복원은 패스 형태에 따라 선택

즉 depth 기반 계산을 하더라도, "모든 픽셀에 모든 데칼" 방식으로 흘러가면 안 된다.

## 11. 데칼 패스 설계

### Step 10. 데칼 전용 Render Pass 추가

수정 파일:

- `Engine/Source/Renderer/SceneRenderer.h`
- `Engine/Source/Renderer/SceneRenderer.cpp`
- 필요 시
  - `Engine/Source/Renderer/Feature/DecalRenderFeature.h`
  - `Engine/Source/Renderer/Feature/DecalRenderFeature.cpp`

권장 패스 순서:

1. Default Opaque
2. Decal Pass
3. Transparent
4. Overlay

프레임 준비 순서:

1. `Packet.DecalPrimitives`를 읽는다.
2. 각 데칼의 월드 OBB를 계산한다.
3. `Scene BVH + SAT`로 수신 후보를 찾는다.
4. 필요 시 `Mesh BVH`로 더 좁힌다.
5. 각 데칼의 screen bounds를 계산한다.
6. tile / cluster 목록에 데칼을 등록한다.
7. 실제 패스에서는 현재 cluster 기준으로 데칼만 처리한다.

권장 구조:

- `FSceneRenderer`는 데칼 패스를 호출
- 실제 구현은 `FDecalRenderFeature` 같은 전용 클래스로 분리

전용 feature가 담당할 것:

- 데칼 OBB 생성
- Scene BVH SAT 탐색
- Mesh BVH refinement
- cluster binning
- 통계 집계
- draw submission

## 12. SAT 구현 메모

### Step 11. OBB vs AABB SAT 유틸리티 추가

권장 위치:

- `Engine/Source/Math/` 아래 새 helper
- 또는 `DecalRenderFeature` 내부 private utility

입력:

- OBB center
- OBB axes 3개
- OBB extents
- AABB min / max

검사 축:

- OBB 축 3개
- AABB 축 3개
- 외적 축 9개

총 15개 축을 검사한다.

구현 주의점:

- OBB 축은 정규화
- 길이가 거의 0인 외적 축은 예외 처리
- epsilon을 둬서 수치 오차를 완화

추천 함수 형태:

- `bool IntersectOBBAABB(const FOBB& OBB, const FAABB& AABB);`

## 13. 클러스터드 구현 메모

### Step 12. Screen Tile Cluster 자료구조 추가

첫 버전 추천 자료구조:

- `FDecalScreenClusterGrid`
- `TileSizeX`, `TileSizeY`
- `TilesX`, `TilesY`
- `TArray<TArray<int32>> TileDecalIndices`

첫 버전의 단순한 처리 순서:

1. 프레임 시작 시 클러스터 그리드를 clear
2. 해상도 기반으로 tile 수 계산
3. 각 데칼의 screen rect 계산
4. 겹치는 tile에 decal index push

추후 개선 가능 항목:

- 깊이 축까지 포함한 3D clustered
- tile별 고정 크기 인덱스 버퍼
- prefix sum 기반 압축 인덱스 버퍼

하지만 첫 구현은 `2D tile cluster`만으로도 충분히 의미가 있다.

## 14. Stat

### Step 13. `Decal Stat` 추가

최소 추적 항목:

- 전체 데칼 수
- frustum 통과 데칼 수
- scene BVH 방문 노드 수
- scene BVH cull 노드 수
- receiver primitive 수
- mesh BVH 방문 노드 수
- cluster 등록 수
- tile당 평균 데칼 수
- 최대 tile 데칼 수
- 실제 렌더된 데칼 수
- decal draw call 수
- decal pass CPU ms

가능하면 나중에 추가:

- GPU ms
- tile occupancy heatmap

권장 구조:

- `FDecalPassStats`

## 15. 구현 마일스톤

### Milestone 1. 타입과 수집

- `UDecalComponent`
- `ADecalActor`
- `FSceneDecalPrimitive`
- `SF_Decal`

### Milestone 2. 공간 최적화

- `OBB vs AABB SAT`
- `Scene BVH` 탐색
- `Mesh BVH` refinement

### Milestone 3. 화면 최적화

- screen-space bounds 계산
- `2D tile clustered` 추가
- cluster stat 추가

### Milestone 4. 데칼 패스

- decal VS / PS
- decal material
- decal pass 실행

### Milestone 5. 확장

- 3D clustered
- receiver 캐시
- GPU profiling

## 16. 가장 중요한 리스크

### A. BVH만으로는 큰 메시 비용이 남는다

그래서 `Mesh BVH` refinement가 필요하다.

### B. Clustered가 없으면 픽셀 비용이 크게 남는다

그래서 screen-space tile 분할을 기본 전략으로 둬야 한다.

### C. Depth 기반 방식이 전체 화면 검사로 흐를 수 있다

그렇게 되면 clustered를 넣은 의미가 줄어든다.
반드시 tile별 데칼 후보 제한을 유지해야 한다.

## 17. 최종 권장안

이 프로젝트에서 데칼 구현 기본안은 아래로 정리한다.

1. 엔진 전체는 `Forward Rendering` 유지
2. 데칼은 전용 `Render Pass`로 추가
3. 수신 후보 축소는 `Decal OBB vs Scene BVH AABB SAT`
4. 큰 메시는 `Mesh BVH`로 한 번 더 축소
5. 화면은 tile 기반 `Clustered`로 나눠 데칼 후보를 제한
6. 첫 구현부터 `Decal Stat`을 함께 넣는다

즉 이 문서의 기준 구현은 단순한 픽셀 데칼이 아니라 아래 구조다.

`Forward + Decal Pass + BVH/SAT + MeshBVH + Clustered Screen Grid`
