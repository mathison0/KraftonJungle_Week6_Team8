# Screen-Space Decal Implementation

## 목적

이 문서는 현재 엔진에 적용한 `depth-based screen-space decal` 구현을, 렌더 패스 진입부터 실제 픽셀 셰이더에서 월드 좌표를 복원해 데칼을 합성하는 과정까지 코드 기준으로 정리한 문서다.

같이 다루는 내용:

- 현재 데칼 렌더 패스가 어디서 시작되는지
- 어떤 시점에 depth를 복사하는지
- 데칼 커맨드가 어떻게 만들어지는지
- 셰이더에서 depth로 월드 좌표를 어떻게 복원하는지
- 왜 이 방식이 기존 receiver mesh 재렌더 방식과 다른지
- 뒷면 번짐, 수직면 번짐을 줄이는 방향

---

## 변경 전 방식과 변경 후 방식

### 변경 전

기존 데칼은 `receiver mesh를 다시 그리는 방식`이었다.

- 데칼 컴포넌트를 기준으로 후보 리시버 메시를 찾음
- 리시버 메시를 데칼 패스에서 다시 draw
- 픽셀 셰이더에서 `리시버 픽셀의 WorldPosition`을 받아서 데칼 박스 로컬 좌표로 변환
- 로컬 좌표가 박스 안이면 텍스처를 샘플링

이 방식은 구조적으로 다음 특징이 있었다.

- 리시버 메시가 실제로 draw 되어야 함
- 리시버가 바뀌면 draw call이 증가함
- 화면에 보이는 최종 픽셀 기준이 아니라, 선택된 receiver mesh 기준으로만 동작함

### 변경 후

현재는 `screen-space decal` 방식이다.

- 먼저 일반 씬을 depth까지 포함해 렌더링
- 데칼 패스 직전에 현재 depth buffer를 shader resource로 복사
- 데칼은 receiver mesh 대신 `데칼 볼륨 박스`만 그림
- 픽셀 셰이더에서 현재 화면 픽셀의 depth를 읽음
- depth와 inverse projection/view를 사용해 그 픽셀의 월드 좌표를 복원
- 복원한 월드 좌표를 데칼 로컬 좌표로 변환
- 박스 내부면 데칼 텍스처를 샘플링하여 색을 합성

즉, 지금 방식의 핵심은:

`화면 픽셀의 depth -> view/world position 복원 -> decal volume 내부 판정 -> UV 생성 -> 텍스처 합성`

---

## 전체 렌더 흐름

렌더링의 큰 흐름은 다음과 같다.

1. 프레임 시작
2. 일반 씬 패스 렌더
3. 데칼 패스 직전 depth copy 생성
4. 데칼 볼륨 draw
5. 투명 오브젝트 패스
6. 오버레이 패스

실제 진입점은 [Engine/Source/Renderer/Renderer.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/Renderer.cpp) 와 [Engine/Source/Renderer/SceneRenderer.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/SceneRenderer.cpp) 이다.

---

## 1. 프레임 시작

### `FRenderer::BeginFrame`

파일:

- [Engine/Source/Renderer/Renderer.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/Renderer.cpp)

역할:

- back buffer / depth buffer 초기화
- scene renderer 프레임 상태 초기화
- decal feature 프레임 상태 초기화

중요 포인트:

- `DecalFeature->BeginFrame()` 에서 depth SRV 참조와 통계를 초기화한다.

---

## 2. Scene queue 구성

### `FSceneRenderer::BuildQueue`

파일:

- [Engine/Source/Renderer/SceneRenderer.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/SceneRenderer.cpp)

이 함수는 scene packet과 scene view를 받아 렌더 커맨드 큐를 만든다.

여기서 데칼 관련으로 하는 일:

- `BuildContext.DecalFeature = Renderer.GetSceneDecalFeature()`
- `BuildContext.ViewportSize = FVector2(Viewport.Width, Viewport.Height)`
- `DecalFeature->PrepareFrame(...)` 호출

`ViewportSize`를 같이 넘기는 이유:

- 픽셀 셰이더가 `SV_Position.xy`를 normalized screen UV로 바꾸려면
- 현재 viewport width/height와 그 역수가 필요하기 때문이다.

---

## 3. 데칼 커맨드 생성

### `FSceneCommandBuilder::BuildQueue`

파일:

- [Engine/Source/Renderer/SceneCommandBuilder.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/SceneCommandBuilder.cpp)

현재 구현에서는 데칼 컴포넌트 하나당 `데칼 볼륨 박스 mesh draw call`을 만든다.

변경 후 흐름:

1. `Packet.DecalPrimitives` 순회
2. 각 `UDecalComponent`에 대해 동적 데칼 머티리얼 확보
3. 데칼 extent로 박스 mesh 생성 또는 갱신
4. `ERenderLayer::Decal` 커맨드 추가

### 기존과 가장 큰 차이

기존:

- scene BVH에서 receiver 후보 검색
- receiver static mesh들을 decal layer에서 다시 그림

현재:

- receiver를 찾지 않음
- `DecalComponent` 자신의 volume box만 그림

이 변화 때문에 데칼 패스는 `receiver mesh 수`가 아니라 `decal 수`에 더 직접적으로 비례한다.

### 데칼 mesh

박스 mesh 생성은 다음 함수에서 한다.

- [Engine/Source/Renderer/Feature/DecalRenderFeature.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/Feature/DecalRenderFeature.cpp) 의 `FDecalRenderFeature::BuildMesh`

구조:

- 로컬 공간 기준 박스 코너 8개 생성
- 인덱스로 12 triangle box 구성
- 로컬 박스는 `X 방향으로 길이`, `Y/Z 방향으로 반폭` 형태

즉 데칼은 여전히 `데칼 박스 로컬 X축 방향으로 투영`하는 개념을 유지한다.
다만 월드 좌표를 얻는 방식이 `리시버 메시 interpolated world pos`에서 `depth 복원 world pos`로 바뀌었다.

---

## 4. 데칼 머티리얼과 상수 버퍼

### `FSceneCommandBuilder::GetOrCreateDecalMaterial`

파일:

- [Engine/Source/Renderer/SceneCommandBuilder.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/SceneCommandBuilder.cpp)

이 함수는 데칼 머티리얼에 필요한 파라미터를 넣는다.

설정하는 값:

- `BaseColor`
- `DecalExtent`
- `ScreenSize`
- `DecalOrigin`
- `DecalAxisX`
- `DecalAxisY`
- `DecalAxisZ`
- 데칼 텍스처

### `ScreenSize`

`ScreenSize`는 다음 의미를 가진다.

- `x = viewport width`
- `y = viewport height`
- `z = 1 / width`
- `w = 1 / height`

픽셀 셰이더에서:

- `SV_Position.xy`를 스크린 UV로 바꾸는 데 사용
- `Load()`용 픽셀 좌표와 normalized UV 계산에 사용

### 데칼 축 정보

`DecalOrigin`, `DecalAxisX/Y/Z`는 데칼 컴포넌트의 월드 transform에서 추출한다.

의미:

- `Origin`: 데칼 박스의 시작점
- `AxisX`: 투영 방향 축
- `AxisY`, `AxisZ`: 데칼 텍스처가 펼쳐지는 평면 축

현재 구현은 여전히:

- `X`를 깊이 방향
- `Y/Z`를 UV 평면

으로 사용한다.

---

## 5. 데칼 베이스 머티리얼 생성

### `FDecalRenderFeature::InitializeBaseMaterial`

파일:

- [Engine/Source/Renderer/Feature/DecalRenderFeature.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/Feature/DecalRenderFeature.cpp)

여기서 데칼용 기본 머티리얼 상태를 만든다.

### 셰이더

- VS: `DecalVertexShader.hlsl`
- PS: `DecalPixelShader.hlsl`

### 렌더 상태

현재 설정:

- Rasterizer: `Cull Front`
- Depth: `DepthEnable = false`, `DepthWrite = ZERO`
- Blend: alpha blend

### 왜 `Cull Front` 인가

screen-space decal volume은 보통 박스 안쪽 면을 기준으로 샘플링하는 편이 더 안정적이다.
카메라가 박스 밖에 있을 때 front face를 버리고 back face만 그리면, 볼륨 내부를 향한 면을 사용하게 된다.

완전히 충분한 해법은 아니지만, 일반적인 decal volume 렌더에서 흔히 쓰는 시작점이다.

### 상수 버퍼 레이아웃

`b2`:

- `BaseColor`
- `DecalExtent`
- `ScreenSize`

`b3`:

- `DecalOrigin`
- `DecalAxisX`
- `DecalAxisY`
- `DecalAxisZ`

---

## 6. Frame constant buffer를 PS에서도 사용

### 변경 이유

screen-space decal은 픽셀 셰이더에서 다음 값이 필요하다.

- `InvProjection`
- `InvView`

이 값은 기존에도 frame constant buffer에 있었지만, 원래는 VS에만 바인딩되어 있었다.

### 수정 내용

파일:

- [Engine/Source/Renderer/Renderer.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/Renderer.cpp)

`FRenderer::SetConstantBuffers()`에서:

- `VSSetConstantBuffers(0, 2, CBs)`
- `PSSetConstantBuffers(0, 2, CBs)`

둘 다 하도록 바꿨다.

이제 픽셀 셰이더는 `ShaderCommon.hlsli`의 `FrameData(b0)`를 그대로 읽을 수 있다.

---

## 7. 일반 씬 패스 이후 depth copy 생성

### `FSceneRenderer::ExecuteCommands`

파일:

- [Engine/Source/Renderer/SceneRenderer.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/SceneRenderer.cpp)

실행 순서:

1. Default pass 실행
2. 데칼 패스 전에 `UpdateDepthCopy(Renderer, DepthStencilView)` 호출
3. depth copy가 성공하면 데칼 커맨드에 depth SRV 바인딩
4. Decal pass 실행
5. Transparent pass 실행
6. Overlay pass 실행

### 왜 default pass 뒤인가

데칼은 이미 렌더된 실제 화면 표면에 붙어야 한다.
그래서 데칼 패스가 읽는 depth는 `일반 opaque geometry가 모두 기록된 뒤의 depth`여야 한다.

만약 default pass 이전에 depth를 읽으면:

- 복원되는 월드 좌표가 비어 있거나
- 아직 그려지지 않은 표면 기준이 되어
- 데칼이 제대로 붙지 않는다.

---

## 8. Depth buffer 복사

### `FDecalRenderFeature::UpdateDepthCopy`

파일:

- [Engine/Source/Renderer/Feature/DecalRenderFeature.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/Feature/DecalRenderFeature.cpp)

역할:

- 현재 DSV의 underlying texture를 얻음
- 같은 크기의 typeless depth copy texture가 준비되어 있는지 확인
- `CopyResource`로 복사
- 그 복사본을 SRV로 읽게 함

### 사용하는 포맷

소스 depth:

- DSV 쪽은 `D24_UNORM_S8_UINT`

복사 대상:

- `R24G8_TYPELESS`

SRV:

- `R24_UNORM_X8_TYPELESS`

이렇게 하는 이유:

- depth/stencil 텍스처는 그대로 픽셀 셰이더에서 샘플할 수 없는 경우가 많음
- typeless + SRV view를 따로 만들어야 shader read가 가능함

---

## 9. 데칼 패스에서 depth SRV 바인딩

### 어디서 바인딩하는가

파일:

- [Engine/Source/Renderer/SceneRenderer.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/SceneRenderer.cpp)

`UpdateDepthCopy()` 성공 후:

- `DecalFeature->GetDepthTextureSRV()`로 depth SRV 획득
- 각 데칼 커맨드의 `Material->SetPixelTextureBinding(1, DepthSRV, nullptr)` 호출

### 왜 queue build 시점이 아니라 execute 시점인가

queue build 시점에는 아직 현재 프레임의 depth copy가 생성되지 않았다.

그래서:

- 커맨드 빌드 때는 데칼 텍스처와 상수 버퍼만 준비
- 실제 draw 직전, depth copy가 생긴 뒤에 `t1` 슬롯에 depth SRV를 넣는 방식으로 처리한다.

---

## 10. 데칼 버텍스 셰이더

파일:

- [Engine/Shaders/DecalVertexShader.hlsl](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Shaders/DecalVertexShader.hlsl)

현재 VS는 매우 단순하다.

하는 일:

- 데칼 박스의 로컬 vertex를 월드로 변환
- 월드를 view로 변환
- projection 적용
- 최종 `SV_POSITION` 출력

중요한 점:

- 더 이상 `WorldPosition`을 interpolator로 넘기지 않는다
- 월드 좌표는 PS에서 scene depth 기반으로 복원한다

즉, VS의 역할은 이제 `데칼 volume rasterization`만 담당한다.

---

## 11. 데칼 픽셀 셰이더

파일:

- [Engine/Shaders/DecalPixelShader.hlsl](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Shaders/DecalPixelShader.hlsl)

이 셰이더가 현재 구현의 핵심이다.

### 11-1. 현재 픽셀의 depth 읽기

```hlsl
int2 PixelCoord = int2(Input.Position.xy);
float SceneDepth = SceneDepthTexture.Load(int3(PixelCoord, 0)).r;
clip(0.999999f - SceneDepth);
```

의미:

- 현재 화면 픽셀 좌표에 해당하는 depth 값을 읽는다
- depth가 거의 far plane이면 버린다

이 단계에서 이미:

- 실제로 씬 표면이 존재하는 픽셀만 남긴다

### 11-2. 스크린 좌표를 clip space로 변환

```hlsl
float2 ScreenUV = Input.Position.xy * ScreenSize.zw;
float2 ClipXY = float2(ScreenUV.x * 2.0f - 1.0f, 1.0f - ScreenUV.y * 2.0f);
float4 ClipPosition = float4(ClipXY, SceneDepth, 1.0f);
```

의미:

- 픽셀 좌표를 0..1 화면 UV로 바꿈
- UV를 NDC/clip 범위로 바꿈

Y를 뒤집는 이유:

- 화면 좌표계와 clip/NDC 좌표계의 방향 차이를 맞추기 위해서다

### 11-3. view/world 좌표 복원

```hlsl
float4 ViewPosition = mul(ClipPosition, InvProjection);
ViewPosition.xyz /= ViewPosition.w;

float4 WorldPosition = mul(float4(ViewPosition.xyz, 1.0f), InvView);
```

이 부분이 depth-based screen-space decal의 핵심이다.

복원 순서:

1. clip position
2. inverse projection 적용 -> view position
3. perspective divide
4. inverse view 적용 -> world position

즉, 현재 화면 픽셀이 실제 세계의 어디를 보고 있는지 알아내는 과정이다.

### 11-4. 데칼 로컬 좌표 변환

```hlsl
float3 DeltaToReceiver = WorldPosition.xyz - DecalOrigin.xyz;
DecalLocalPosition.x = dot(DeltaToReceiver, DecalAxisX.xyz) / dot(DecalAxisX.xyz, DecalAxisX.xyz);
DecalLocalPosition.y = dot(DeltaToReceiver, DecalAxisY.xyz) / dot(DecalAxisY.xyz, DecalAxisY.xyz);
DecalLocalPosition.z = dot(DeltaToReceiver, DecalAxisZ.xyz) / dot(DecalAxisZ.xyz, DecalAxisZ.xyz);
```

의미:

- 복원된 월드 좌표를 데칼의 로컬 박스 좌표로 바꾼다

### 11-5. 데칼 볼륨 내부 판정

```hlsl
if (DecalLocalPosition.x < 0.0f ||
	DecalLocalPosition.x > DecalExtent.x ||
	abs(DecalLocalPosition.y) > DecalExtent.y ||
	abs(DecalLocalPosition.z) > DecalExtent.z)
{
	discard;
}
```

의미:

- 복원된 surface point가 데칼 박스 내부에 있을 때만 그린다

현재 정의는:

- `X`: 0 .. Extent.x
- `Y`: -Extent.y .. +Extent.y
- `Z`: -Extent.z .. +Extent.z

즉 현재도 여전히 `단방향 X projection`이다.

screen-space decal로 바뀌었다고 해서 투영 정의까지 자동으로 바뀌는 것은 아니다.
바뀐 것은 `표면 좌표를 얻는 방법`이다.

### 11-6. UV 생성과 텍스처 샘플링

```hlsl
ProjectedUV.x = DecalLocalPosition.y / (DecalExtent.y * 2.0f) + 0.5f;
ProjectedUV.y = 0.5f - DecalLocalPosition.z / (DecalExtent.z * 2.0f);
```

즉:

- `Y/Z` 평면을 2D UV로 사용
- `X`는 깊이 방향이다

마지막으로:

- `DecalTexture.Sample(...)`
- `BaseColor` 곱셈
- 알파 clip
- 블렌딩으로 최종 합성

---

## 12. 지금 방식의 장점

### 12-1. receiver mesh 재렌더 제거

기존에는 receiver mesh 수에 따라 데칼 draw가 늘었다.
지금은 데칼 당 박스 draw 하나로 간다.

### 12-2. 실제 화면 결과 기준

depth에서 직접 복원하므로:

- 이미 화면에 보이는 표면
- 최종 depth를 가진 표면

에 데칼이 붙는다.

### 12-3. 메시 높이 변화에 덜 민감

기존처럼 리시버 메시의 interpolated vertex world pos에 의존하지 않고,
현재 화면 픽셀의 실제 depth에서 위치를 복원하므로 높이 변화나 표면 굴곡에 더 자연스럽게 대응한다.

---

## 13. 현재 구현의 한계

현재 구현은 screen-space decal로 바뀌었지만, 아직 다음 문제는 남아 있다.

### 13-1. 뒷면 번짐

데칼 박스 안에 들어오는 표면이면 방향과 상관없이 찍힐 수 있다.

예:

- 벽 앞면에 붙이고 싶었는데 벽 뒤쪽 표면에도 영향
- 얇은 물체 앞뒤 면 둘 다 영향

### 13-2. 수직면 번짐

바닥용 데칼인데:

- 벽 같은 수직면도 데칼 박스 안에 있으면 같이 찍힐 수 있다

### 13-3. 카메라가 박스 내부에 들어갈 때의 경계 이슈

front-face cull만으로는 특정 시점에서 부자연스러운 잘림이나 누락이 날 수 있다.

### 13-4. 노멀 정보 부재

현재 PS는 depth만 읽는다.

즉:

- 표면이 어느 방향을 향하는지
- 데칼 프로젝션 방향과 얼마나 정렬되는지

를 알지 못한다.

번짐을 제대로 줄이려면 결국 `normal` 정보가 필요하다.

---

## 14. 뒷면 번짐 해결 방향

가장 일반적인 방법은 `surface normal 기반 rejection`이다.

### 방법 A: GBuffer normal 사용

가장 좋은 방법:

1. 일반 씬 패스에서 월드 노멀 또는 뷰 노멀을 텍스처로 출력
2. 데칼 픽셀 셰이더에서 그 normal을 샘플링
3. 데칼 투영 방향과 normal의 내적을 검사

예시:

```hlsl
float3 SurfaceNormal = normalize(NormalTexture.Sample(...).xyz * 2.0f - 1.0f);
float3 ProjectionDir = normalize(DecalAxisX.xyz);
float Facing = dot(SurfaceNormal, -ProjectionDir);
clip(Facing - NormalThreshold);
```

의미:

- 표면이 데칼을 향하고 있지 않으면 버림
- 뒤집힌 면은 제거 가능

보통 `NormalThreshold`는 `0.1 ~ 0.4` 정도에서 튜닝한다.

### 방법 B: depth 기반 normal 근사

normal buffer가 없으면 임시 대안으로 depth의 화면 미분으로 normal을 근사할 수 있다.

개념:

- 현재 픽셀, 오른쪽 픽셀, 아래 픽셀의 복원 world pos를 구함
- 두 벡터의 cross product로 표면 normal 추정

예시 개념:

```hlsl
float3 P  = ReconstructWorld(uv);
float3 Px = ReconstructWorld(uv + dx);
float3 Py = ReconstructWorld(uv + dy);
float3 SurfaceNormal = normalize(cross(Px - P, Py - P));
```

장점:

- 별도 normal RT 없이 가능

단점:

- 깊이 불연속에서 노이즈가 큼
- geometry edge에서 불안정
- MSAA/얇은 물체에서 오류가 많음

### 권장

가능하면 `normal texture 추가`가 맞다.

---

## 15. 수직면 번짐 해결 방향

수직면 번짐은 보통 `데칼 의도 표면 방향`과 `surface normal`을 비교해서 줄인다.

### 바닥용 데칼 예시

바닥에만 붙이려면:

- 표면 normal이 `월드 Up`과 충분히 가까운 표면만 허용

예:

```hlsl
float UpAlignment = dot(SurfaceNormal, float3(0, 0, 1));
clip(UpAlignment - 0.7f);
```

이렇게 하면:

- 바닥은 통과
- 벽은 제거
- 경사진 면은 threshold에 따라 선택 가능

### 데칼 로컬 축 기준으로 일반화

월드 업 고정이 아니라 데칼의 의도 축을 기준으로 필터링할 수도 있다.

예:

- 데칼 로컬 `-X` 방향으로 향한 면만 허용
- 또는 `Z`에 수직인 표면만 허용

이 방식이 더 일반적이다.

예시:

```hlsl
float3 ProjectionDir = normalize(-DecalAxisX.xyz);
float Alignment = dot(SurfaceNormal, ProjectionDir);
clip(Alignment - 0.3f);
```

이렇게 하면:

- 프로젝션 방향과 너무 비스듬한 표면은 잘라낼 수 있다

---

## 16. 번짐 억제용 추가 기법

### Angle fade

딱 자르지 않고 각도에 따라 alpha를 줄이는 방식이다.

예:

```hlsl
float Facing = saturate((dot(SurfaceNormal, ProjectionDir) - MinAngle) / (MaxAngle - MinAngle));
FinalColor.a *= Facing;
```

장점:

- 딱 끊기는 느낌이 줄어듦
- 경사면에서 부드럽게 사라짐

### Depth thickness clamp

박스 깊이 방향 두께를 엄격하게 제한한다.

현재도 `DecalLocalPosition.x` 범위로 어느 정도 하고 있지만,
더 보수적으로:

- 아주 얇은 decal thickness 사용
- 박스 길이를 줄이기

로 뒤쪽 표면 침투를 줄일 수 있다.

### Receiver mask / stencil

특정 표면에만 데칼을 허용하려면:

- receiver pass에서 stencil 작성
- decal pass에서 stencil test

방식도 가능하다.

장점:

- 바닥 전용, 도로 전용, 캐릭터 제외 같은 규칙 적용 가능

단점:

- 렌더 상태와 파이프라인 복잡도가 증가

---

## 17. 지금 코드에서 뒷면/수직면 번짐을 해결하려면 무엇이 추가되어야 하나

현재 상태에서 가장 현실적인 다음 단계는 다음 순서다.

### 1단계: 노멀 버퍼 추가

일반 씬 패스에서 별도 render target에 normal 출력

필요 작업:

- scene render target 확장
- 기본 머티리얼/셰이더에서 normal output 경로 추가
- decal pass에서 normal SRV 접근

### 2단계: decal pixel shader에 normal rejection 추가

데칼 PS에서:

- normal 읽기
- projection direction과 dot 검사
- threshold 또는 fade 적용

### 3단계: 데칼 설정값 확장

`UDecalComponent`에 다음 옵션을 추가하면 좋다.

- `NormalFadeThreshold`
- `NormalFadeRange`
- `bAffectBackfaces`
- `bAffectSteepSurfaces`
- `ProjectionThickness`

이렇게 하면 에디터에서 데칼 용도별로 조절 가능하다.

---

## 18. 추천 구현 순서

실제로 확장할 때는 아래 순서를 권장한다.

1. 현재 screen-space decal 경로 안정화
2. 카메라 inside volume 상황 테스트
3. normal texture 추가
4. normal-based rejection 추가
5. angle fade 추가
6. 필요하면 stencil / receiver mask 추가

이 순서가 좋은 이유:

- 먼저 depth 복원 경로 자체를 안정화해야 함
- 그 다음에 번짐 억제를 넣어야 원인 분리가 쉬움

---

## 19. 디버깅 포인트

문제가 생기면 다음 순서로 보는 것이 좋다.

### 1. depth copy가 유효한가

- `UpdateDepthCopy()`가 true를 반환하는지
- viewport 크기와 depth copy 크기가 맞는지

### 2. PS에서 월드 좌표가 정상 복원되는가

임시 디버그로:

- `WorldPosition.xyz`를 색으로 출력
- `DecalLocalPosition.xyz`를 색으로 출력

해보면 좌표계가 뒤집혔는지 바로 보인다.

### 3. 박스 내부 판정이 맞는가

문제 대부분은 다음 중 하나다.

- `DecalOrigin` 기준점이 잘못됨
- `DecalAxisX/Y/Z` 방향이 기대와 다름
- `Extent` 정의가 셰이더와 mesh 생성에서 다름

### 4. 컬링 상태가 적절한가

현재는 `Cull Front`인데,
카메라 위치와 박스 내부/외부 조건에 따라

- `Cull Back`
- `Cull None`

과 비교 테스트가 필요할 수 있다.

---

## 20. 현재 구현 요약

현재 엔진의 screen-space decal 구현은 다음과 같이 동작한다.

- scene opaque pass가 먼저 depth를 기록한다
- decal pass 직전에 depth buffer를 SRV용 텍스처로 복사한다
- decal component마다 receiver mesh가 아니라 decal volume box를 그린다
- 픽셀 셰이더는 현재 화면 픽셀의 depth를 읽는다
- inverse projection / inverse view로 월드 좌표를 복원한다
- 복원한 월드 좌표를 decal local로 바꿔 박스 내부 여부를 판단한다
- 내부면 Y/Z 기반 UV를 만들고 decal texture를 샘플링한다
- alpha blend로 최종 색을 합성한다

이 구현으로 `receiver mesh 재렌더 의존성`은 제거되었고, 화면 픽셀 기준 데칼 투영 구조로 전환되었다.

다만 아직:

- 뒷면 번짐
- 수직면 번짐
- 노멀 기반 억제 부재

는 남아 있으므로, 다음 단계는 `normal buffer + normal rejection` 추가가 맞다.

---

## 관련 파일

- [Engine/Source/Renderer/Renderer.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/Renderer.cpp)
- [Engine/Source/Renderer/SceneRenderer.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/SceneRenderer.cpp)
- [Engine/Source/Renderer/SceneRenderer.h](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/SceneRenderer.h)
- [Engine/Source/Renderer/SceneCommandBuilder.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/SceneCommandBuilder.cpp)
- [Engine/Source/Renderer/SceneCommandBuilder.h](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/SceneCommandBuilder.h)
- [Engine/Source/Renderer/Feature/DecalRenderFeature.cpp](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/Feature/DecalRenderFeature.cpp)
- [Engine/Source/Renderer/Feature/DecalRenderFeature.h](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Source/Renderer/Feature/DecalRenderFeature.h)
- [Engine/Shaders/DecalVertexShader.hlsl](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Shaders/DecalVertexShader.hlsl)
- [Engine/Shaders/DecalPixelShader.hlsl](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Shaders/DecalPixelShader.hlsl)
- [Engine/Shaders/ShaderCommon.hlsli](C:/Users/jungle/00.Archive/06.team08/KraftonJungle_Week6_Team8/Engine/Shaders/ShaderCommon.hlsli)

---

## 비고

현재 변경 이후 셰이더 컴파일은 통과했다.
다만 C++ 전체 빌드는 프로젝트 자체의 PDB 경합 문제(`C1041`, `/MP`와 PDB 공유 충돌) 때문에 끝까지 검증하지 못했다.

이 문서는 현재 저장된 코드 상태를 기준으로 작성했다.
