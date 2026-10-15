## 문제 상황

언리얼 액션 게임에 대규모 전투를 추가했다.

평소에는 약 300~1,000개의 Projectile이 존재해 문제가 없었다. 대규모 전투에서는 약 **35,000~50,000개**까지 늘어나면서 `Game Thread` 프레임 시간이 눈에 띄게 증가했다.

문제에서 주어진 `ProjectileManager::TickProjectiles()`의 초기 프로파일링 결과는 다음과 같았다.

```text
ProjectileManager::TickProjectiles  1.74 ms
├─ UpdateMovement                   0.81 ms
├─ UpdateLifetime                   0.69 ms
└─ 기타                             0.24 ms
```

현재 Projectile은 다음처럼 관리하고 있다.

```cpp
struct FProjectileState
{
    FVector Position;
    FVector Velocity;

    float LifeTime;
    float Damage;

    uint8 Team;
    uint8 Flags;
};

TArray<FProjectileState> Projectiles;
```

Movement는 매 프레임 모든 Projectile을 갱신한다.

```cpp
for (FProjectileState& Projectile : Projectiles)
{
    Projectile.Position += Projectile.Velocity * DeltaTime;
}
```

Lifetime도 매 프레임 모든 Projectile을 갱신한다.

```cpp
for (FProjectileState& Projectile : Projectiles)
{
    Projectile.LifeTime -= DeltaTime;

    if (Projectile.LifeTime <= 0.0f)
    {
        Projectile.Flags |= ExpiredFlag;
    }
}
```

충돌할 때는 `Damage`, `Team`, `Position`, `Velocity` 등을 사용하지만, 한 프레임에 실제로 충돌하는 Projectile은 전체에 비해 적다. 삭제 대상은 Tick이 끝난 뒤 정리한다.

팀에서는 몇 가지 의견이 나왔다.

```text
“단순 반복문이니까 멀티스레딩하면 되지 않을까?”

“TArray는 연속 메모리인데 이미 cache-friendly한 것 아닌가?”

“Projectile이 수만 개라면 MassEntity 같은 구조를 써야 하는 것 아닌가?”
```

Projectile 내부 자료구조는 변경할 수 있다. 다만 기존 게임플레이의 상당 부분은 Actor/UObject 기반이고, 목표 Projectile 수는 최대 약 50,000개다.

대규모 전투에서 Projectile 관련 CPU 비용이 증가하는 원인을 조사하고, 필요한 경우 개선안을 제시해라.

---

## 풀이 — 생각을 마친 뒤 열기

> [!question]- 1부 — 어디부터 조사할까?
> ### STEP 1 — 처음 떠오른 후보
>
> **멀티스레딩, 자료 배치, MassEntity가 떠올랐지만 아직 어느 것도 원인을 설명하지 못한다.**
>
> ```text
> “Actor/UObject 기반인데 멀티스레딩이 가능한가?”
> “TArray는 연속인데 AoS가 문제일 수 있나?”
> “Projectile이 수만 개면 MassEntity로 가야 하나?”
> ```
>
> 현재 두 루프가 직접 수정하는 것은 `FProjectileState`의 일반 데이터다. Actor/UObject가 있다고 병렬 처리가 무조건 불가능한 것은 아니지만, 원인을 모른 채 Thread부터 늘리면 scheduling과 synchronization 비용이 추가된다.
>
> `TArray`의 원소가 연속이라는 사실도 hot field가 조밀하다는 뜻은 아니다. MassEntity 역시 시스템 전체를 바꾸는 선택이라 hotspot 하나만 보고 결정할 수 없다.
>
> ---
>
> ### STEP 2 — 두 루프를 나눠서 본다
>
> **Movement와 Lifetime은 같은 배열을 순회하지만 사용하는 데이터가 다르다.**
>
> ```text
> UpdateMovement → Position / Velocity
> UpdateLifetime → LifeTime / Flags
> ```
>
> 같은 배열을 돈다고 병목 원인까지 같다고 볼 수는 없다. 먼저 연산이 단순하고 접근 필드가 적은 `UpdateLifetime()`을 떼어서 조사하고, `UpdateMovement()`는 별도 workload로 남겨둔다.

> [!question]- 2부 — UpdateLifetime을 분리해서 확인하기
> ### STEP 3 — 계산보다 데이터 배치를 의심했다
>
> **단순한 연산인데 AoS에서는 다음 `LifeTime`까지 64 bytes를 건너간다.**
>
> ```text
> sizeof(FVector)        = 24 bytes
> sizeof(FProjectileAoS) = 64 bytes
>
> [Position | Velocity | LifeTime | Damage | Team | Flags | Padding]
>                          ↑                        ↑
>                          UpdateLifetime이 쓰는 필드
> ```
>
> `UpdateLifetime()`은 수명 감소, 비교, 비트 설정만 한다. 하지만 구조체가 64 bytes라 `LifeTime`끼리도 64 bytes 떨어져 있고, 쓰지 않는 주변 필드가 같은 cache block에 들어올 수 있다.
>
> 여기서 `LifeTime`과 `Flags`를 별도 배열에 모은 SoA를 후보로 뒀다. 아직 memory traffic이나 cache miss를 측정한 것은 아니므로, 이 단계에서는 **working set과 stride가 큰 것 아닐까?**라는 가설만 세운다.
>
> ---
>
> ### STEP 4 — 같은 결과인지 확인한 뒤 시간을 잰다
>
> **결과 불일치는 없었고, 반복 측정에서는 두 규모 모두 SoA가 더 짧았다.**
>
> 같은 초기값과 연산을 적용해 `LifeTime`, `Flags`, expired 수와 checksum을 비교한 뒤 warm-up, 실행 순서 교대, 여러 sample의 median으로 시간을 쟀다.
>
> ```text
> 결과 mismatch       = 0
> 실제 규모 50,000개  → SoA 약 1.14배
> Stress 500,000개    → SoA 약 3.83배
> ```
>
> 500,000개는 운영 규모가 아니라 데이터가 커졌을 때 같은 방향이 더 뚜렷해지는지 보기 위한 조건이다. 상세 수치와 실행 조건은 아래 테스트 기록에 남겼다.

> [!question]- 3부 — 실제 프레임과 CPU 수준에서도 확인하기
>
> ```text
> Automation     → 같은 조건에서 AoS/SoA 차이가 있는지 확인
> Unreal Insights → 실제 UE 프레임에서도 차이가 나타나는지 확인
> VTune           → CPU가 시간을 어떻게 쓰는지 원인 후보를 좁힘
> Assembly        → 생성된 명령과 메모리 접근 간격을 확인
> ```
>
> ### STEP 5 — 실제 UE 프레임에서도 차이가 날까?
>
> **PIE의 50,000개 workload에서도 SoA의 중앙값이 약 3.08배 짧았다.**
>
> Automation은 같은 배열을 연속해서 순회하므로 cache 상태와 branch 분포가 실제 프레임과 다를 수 있다. Lifetime 루프에 trace scope를 붙여 프레임당 한 번 실행한 결과도 SoA 쪽이 짧았다.
>
> Automation에서는 약 1.14배, PIE에서는 약 3.08배였다. 실행 조건이 다르므로 배수가 같을 필요는 없지만, 차이의 방향은 같았다.
>
> ---
>
> ### STEP 6 — 측정값과 Assembly에서 무엇을 확인했나?
>
> **명령 수 감소는 아니었다. 확인한 갱신 명령은 둘 다 `subss`였고, 주소 계산에서는 64-byte와 4-byte stride가 보였다.**
>
> VTune에서 SoA는 CPU Time이 약 6.03배 짧았지만 Instructions Retired는 오히려 약 1.47배 많았다. “명령이 적어서 빨라졌다”는 설명은 맞지 않았다.
>
> 직접 확인한 `LifeTime` 갱신 지점에서는 두 함수 모두 scalar `subss`를 사용했다. 반면 주소 계산은 달랐다.
>
> ```text
> SoA → [rax+rdi*4]              → LifeTime 간격 4 bytes
> AoS → [rdi+0x30], rdi += 0x40 → LifeTime 간격 64 bytes
> ```
>
> `UpdateLifetime()`에서는 필요한 필드를 연속 배치한 SoA가 더 짧게 실행됐다. 다만 확인한 것은 갱신 지점의 Assembly 일부이므로 루프 전체의 자동 벡터화 여부까지 결론낼 수는 없다. cache miss와 bandwidth도 직접 측정하지 못했다.

> [!note]- 성능이 느릴 때 같이 볼 것
> 실행 시간이 길다는 결과만으로 원인은 알 수 없다. 같은 현상도 작업량, 메모리 접근, CPU 실행 효율, 측정 환경 때문에 생길 수 있다.
>
> | 의심할 것 | 문제가 되는 경우 | 확인할 것 |
> |---|---|---|
> | 작업량 | 원소·반복·중복 계산·전체 순회가 많다. | 호출 횟수, 처리한 원소 수, 알고리즘 복잡도 |
> | 자료 배치 | 현재 작업에 필요한 값들이 멀리 떨어져 있다. | Hot Data, 구조체 크기, Stride, A/B 비교 |
> | Cache / Bandwidth | 데이터를 기다리거나 일정 시간에 너무 많은 데이터를 옮긴다. | Cache Miss, Memory Access, Bandwidth 지표 |
> | 명령 수 | CPU가 완료해야 할 명령 자체가 많다. | Instructions Retired |
> | 실행 효율 | 명령 수가 비슷해도 stall이나 자원 대기가 많다. | CPI, Clockticks, Front-End / Back-End 지표 |
> | SIMD | 독립적인 반복문이 여러 값을 한 번에 처리하지 못한다. | Vectorization Report, 전체 함수 Assembly |
> | Branch | 불규칙한 분기를 CPU가 자주 잘못 예측한다. | 입력 분포, Branch Misses |
> | 측정 환경 | 실행 순서, CPU 상태와 Frequency가 다르다. | 교대·반복 측정, Frequency 확인 |
> | 병렬 처리 | 작업 분배보다 동기화 비용이 크거나 cache line이 충돌한다. | Thread timeline, Lock, False Sharing |
>
> ---
>
> #### Working Set
>
> 한 작업이 반복해서 접근하는 데이터 범위다. 이번 AoS 배열이 차지하는 주소 범위는 단순 계산으로 50,000개에서 약 3.2MB, 500,000개에서 약 32MB다. 범위가 cache보다 커지면 같은 데이터를 다시 사용할 가능성이 낮아질 수 있다.
>
> #### Cache Miss와 Memory Bandwidth
>
> Cache Miss는 필요한 데이터가 현재 cache에 없어 더 아래 cache 계층이나 메모리에서 가져와야 하는 상황이다. Memory Bandwidth는 일정 시간에 옮길 수 있는 데이터 양의 한계다.
>
> 둘은 관련 있지만 같은 현상은 아니다. Cache Miss가 병목인지, 대량의 연속 전송이 Bandwidth 한계에 닿았는지는 별도 지표로 확인해야 한다. 이번 테스트에서는 둘 다 직접 측정하지 않았다.
>
> #### Instructions Retired와 CPI
>
> Instructions Retired는 CPU가 완료한 명령 수다. 명령 수가 줄면 빨라질 수 있지만, 명령마다 하는 일이 다르고 대기 시간도 있으므로 이것만으로 성능이 결정되지는 않는다.
>
> ```text
> CPI = 전체 CPU cycles ÷ 완료한 instructions
> ```
>
> CPI는 측정 구간 전체의 평균이다. 개별 명령 하나의 고정 실행 시간이 아니다. CPU는 한 cycle에 여러 명령을 완료할 수 있으므로 CPI가 1보다 작을 수도 있다.
>
> #### Back-End Bound
>
> CPU가 명령을 가져오고 해석한 뒤 실제로 실행하는 단계에서 바로 진행하지 못한 비율을 보는 단서다.
>
> ```text
> Memory Bound → Cache, 메모리 지연, Bandwidth
> Core Bound   → 실행 유닛, 처리 포트, 데이터 의존성
> ```
>
> Back-End Bound가 높다는 사실만으로 Cache Miss를 원인으로 확정할 수 없다. 어떤 하위 지표가 높은지 더 확인해야 한다.
>
> #### SIMD / Vectorization
>
> Scalar 명령은 값 하나를 처리하고 SIMD 명령은 여러 값을 한 번에 처리할 수 있다. 독립적인 반복문은 vectorization 후보가 될 수 있지만 실제 적용 여부는 Compiler Report나 전체 Assembly로 확인해야 한다.
>
> 이번에 직접 본 갱신 지점은 둘 다 scalar `subss`였다. 이것만으로 전체 루프가 vectorization되지 않았다고 결론낼 수는 없다.
>
> #### Branch Prediction
>
> CPU는 분기 결과를 예상해 다음 작업을 미리 진행한다. 예측이 틀리면 진행한 작업을 버리고 pipeline을 다시 채우는 비용이 생긴다.
>
> 분기 결과의 비율이 변했다는 사실만으로 Branch Miss가 많다고 판단할 수는 없다. 같은 결과가 반복되면 오히려 예측하기 쉬울 수도 있으므로 실제 Branch Misses를 확인해야 한다.
>
> #### False Sharing
>
> 여러 스레드가 서로 다른 값을 수정하더라도 그 값들이 같은 cache line에 있으면 cache line 소유권이 코어 사이를 오갈 수 있다. 단일 스레드인 현재 비교의 원인은 아니며, 나중에 병렬화할 때 확인할 항목이다.
>
> ```text
> 이번 문제에서 확인
> → 같은 결과, 실행 시간, Instructions, CPI, Frequency, 64B/4B Stride
>
> 일부만 확인
> → 갱신 지점의 scalar 명령
>
> 아직 확인하지 않음
> → Cache Miss, Bandwidth, 전체 Vectorization, Branch Miss, 병렬화 비용
> ```

## 테스트 — 측정과 프로파일링

> [!example]- 테스트 전체 보기
> **아래 수치는 전체 Projectile Tick이 아니라, 분리한 `UpdateLifetime()` workload의 기록이다.** `UpdateMovement()`, 충돌, 삭제 비용은 포함하지 않았다.
>
> - Automation 수치: `TestProject-backup-2026.09.04-12.13.29.log`
> - PIE 수치: `ProjectileAoS_Run2.utrace`, `ProjectileSoA_Run1.utrace`를 Insights에서 읽어 기록
> - VTune과 Assembly 수치: 분석 화면에서 확인해 이 문서에 옮긴 값
> - 코드: `ProjectileDataLayoutTest.cpp`, `ProjectileDataLayoutInsightsRunner.cpp`
>
> ### 1. 결과 동일성과 Automation 비교
>
> 같은 초기 데이터와 연산을 사용했다. warm-up 뒤 8 samples를 측정했고 AoS/SoA 실행 순서는 교대했다.
>
> ```text
> sizeof(FVector)=24 / sizeof(FProjectileAoS)=64
> Count=50,000 / Updates=100 / mismatch=0
> AoS expired=25,217 / checksum=7325511784757245806
> SoA expired=25,217 / checksum=7325511784757245806
> ```
>
> ```text
> 50,000개 / Iterations=200
> AoS 12.049 ms total / 0.0602 ms per update
> SoA 10.533 ms total / 0.0527 ms per update
> AoS / SoA = 1.144x
>
> 500,000개 / Iterations=20
> AoS 34.002 ms total / 1.7001 ms per update
> SoA 8.888 ms total / 0.4444 ms per update
> AoS / SoA = 3.826x
> ```
>
> `mismatch=0`과 같은 checksum은 **이 테스트의 입력과 연산에서 두 배치가 같은 게임 결과를 만들었다**는 뜻이다. 이 확인이 있어야 시간 차이를 기능 차이가 아닌 배치 차이로 비교할 수 있다.
>
> 50,000개와 500,000개 모두 SoA가 짧았으므로 한 번의 측정에서만 나타난 방향은 아니었다. 500,000개에서 격차가 더 커졌다는 결과는 데이터 규모와 반복 패턴이 달라질 때 차이도 달라진다는 것을 보여주지만, 그 이유가 cache miss나 bandwidth라고 직접 증명하지는 않는다.
>
> 또한 같은 배열을 여러 번 갱신하므로 cache가 데워지고 `LifeTime` 감소에 따라 분기 결과도 변한다. 따라서 이 실험은 배치만 통제한 비교에는 유효하지만 실제 프레임을 그대로 재현한 것은 아니다. 다음 확인 대상은 PIE 실행이다.
>
> ---
>
> ### 2. PIE / Unreal Insights
>
> 50,000개를 고정 `DeltaTime=0.016`으로 프레임당 한 번 실행하고 각각 별도 trace로 기록했다.
>
> ```text
> Trace.File ProjectileAoS_Run2.utrace cpu,frame
> ProjectileDataLayout.Start AoS 50000 1
> ProjectileDataLayout.Stop
> Trace.Stop
>
> Trace.File ProjectileSoA_Run1.utrace cpu,frame
> ProjectileDataLayout.Start SoA 50000 1
> ProjectileDataLayout.Stop
> Trace.Stop
> ```
>
> | 측정값 | AoS 50,000 | SoA 50,000 | 비율 |
> |---|---:|---:|---:|
> | Count | 533 | 729 | 비교 기준 아님 |
> | Average | 0.1862 ms | 0.0594 ms | 3.14x |
> | Median | 0.1776 ms | 0.0577 ms | 3.08x |
> | P95 | 0.2467 ms | 0.0691 ms | 3.57x |
> | Min | 0.1489 ms | 0.0433 ms | 3.44x |
> | Max | 0.3825 ms | 0.1159 ms | 3.30x |
>
> AoS trace의 10개 사전 실행은 제외하고 50,000개 event만 비교했다. 호출 수가 달라 총합 대신 호출당 통계를 사용했다.
>
> Median뿐 아니라 Average, P95, Min, Max가 모두 같은 방향이었다. **Automation에서 본 SoA의 시간 차이가 실제 UE 프레임에서도 반복됐고, 일부 outlier 하나가 차이를 만든 것은 아니었다.**
>
> 다만 이 결과는 어느 배치가 짧았는지만 보여준다. 왜 짧았는지는 설명하지 않으며, 각 배치를 한 번씩만 capture했으므로 실행 간 CPU 상태와 백그라운드 환경 차이도 남아 있다. 다음에는 VTune으로 시간 차이를 만드는 CPU 동작을 좁힌다.
>
> ---
>
> ### 3. VTune
>
> PIE 중인 `UnrealEditor.exe`에 `Microarchitecture Exploration / Detailed`로 attach했다. 아래 workload를 각각 수집한 뒤 Bottom-up에서 `UnrealEditor-TestProject.dll`과 `UpdateLifetimeAoS/SoA`를 확인했다.
>
> ```text
> AoS: ProjectileDataLayout.Start AoS 500000 20 300
> SoA: ProjectileDataLayout.Start SoA 500000 20 300
> ```
>
> | 측정값 | `UpdateLifetimeAoS` | `UpdateLifetimeSoA` | 변화 |
> |---|---:|---:|---:|
> | CPU Time | 9.376 s | 1.556 s | 약 6.03배 감소 |
> | Clockticks | 49,831,428,000 | 8,518,372,000 | 약 5.85배 감소 |
> | Instructions Retired | 31,526,362,000 | 46,225,620,000 | SoA가 약 1.47배 많음 |
> | CPI | 1.581 | 0.184 | 약 8.59배 감소 |
> | Average CPU Frequency | 5.3 GHz | 5.5 GHz | 약 4% 차이 |
>
> SoA는 더 많은 instruction을 retire했지만 CPU Time과 Clockticks는 크게 줄었다. **따라서 이번 차이를 “SoA가 더 적은 명령을 실행했기 때문”이라고 설명할 수 없다.** 평균 CPU 주파수 차이도 약 4%라 약 6배의 CPU Time 차이를 설명하지 못한다.
>
> 낮아진 CPI는 이 capture에서 SoA가 명령 하나를 처리하는 동안 소비한 cycle이 훨씬 적었다는 뜻이다. 하지만 이 표만으로 그 cycle 차이가 cache miss, bandwidth, dependency 중 무엇 때문인지는 확정할 수 없다.
>
> 개별 Assembly 줄의 clocktick은 sampling과 대기 시간 귀속의 영향을 받으므로 그 명령의 고정 실행 시간으로 읽지 않는다. 두 배치를 별도로 수집한 stress workload라는 조건도 남아 있다. 다음에는 생성된 Assembly에서 산술 명령과 주소 계산이 실제로 어떻게 다른지 확인한다.
>
> ---
>
> ### 4. Assembly
>
> ```asm
> ; SoA: LifeTime stride 4 bytes
> movss xmm0, dword ptr [rax+rdi*4]
> subss xmm0, xmm7
> movss dword ptr [rax+rdi*4], xmm0
>
> ; AoS: LifeTime offset 0x30, stride 0x40 = 64 bytes
> movss xmm0, dword ptr [rdi+0x30]
> subss xmm0, xmm6
> movss dword ptr [rdi+0x30], xmm0
> add rdi, 0x40
> ```
>
> 실제로 확인한 `LifeTime` 갱신 지점에서는 두 함수 모두 scalar `subss`를 사용했다. **이 지점의 산술 명령은 비슷했지만, 다음 `LifeTime`으로 이동하는 간격은 AoS 64 bytes, SoA 4 bytes였다.**
>
> 이 차이는 구조체 크기만 보고 예상한 배치가 실제 생성 코드의 주소 계산에도 남았다는 뜻이다. 같은 수의 `LifeTime`을 갱신할 때 AoS와 SoA가 서로 다른 간격으로 메모리를 순회한다는 사실까지는 확인했다.
>
> 따라서 측정된 시간 차이를 데이터 배치와 접근 간격에 연결할 근거는 생겼다. 다만 이 몇 줄만으로 루프 전체에 packed SIMD 경로가 없었다거나 자동 벡터화되지 않았다고 확정할 수는 없다. 당시 Assembly 검색도 화면에 보이는 `subss`를 찾지 못했으므로 검색 결과를 부재의 근거로 쓸 수 없다.
>
> cache miss와 bandwidth를 직접 비교한 것도 아니다. 그 수치까지 주장하려면 hardware counter를 다시 수집해야 한다.

## 결론

> [!success] 이 문제에서 가져갈 것
>
> **대규모 Projectile Tick이 느리다고 바로 멀티스레딩이나 MassEntity를 고르지 않는다. 비용이 보이는 루프를 workload별로 나누고, 각 루프가 실제로 쓰는 데이터와 배치를 확인한 뒤 같은 조건에서 검증한다.**

### 이번 문제의 흐름

```text
대규모 전투에서 TickProjectiles가 느리다
→ Insights에서 UpdateMovement와 UpdateLifetime 비용 확인

처음 떠오른 후보
→ 멀티스레딩?
→ AoS 문제?
→ MassEntity?

셋 중 어느 것도 아직 원인을 설명하지 못함
→ 먼저 workload를 나눔

Movement와 Lifetime은 사용하는 필드가 다름
→ Lifetime부터 분리

AoS에서는 다음 LifeTime까지 64 bytes
→ LifeTime / Flags를 연속 배치한 SoA 비교

결과 동일성 확인
→ Automation과 PIE에서 SoA가 더 짧음

VTune
→ 오히려 Instructions Retired는 더 많음

Assembly
→ 확인한 갱신 지점은 둘 다 scalar subss
→ AoS 64 bytes / SoA 4 bytes
→ 루프 전체의 vectorization 여부는 확인하지 못함

UpdateLifetime에서는 SoA가 더 짧게 실행됨
→ 이 결과를 UpdateMovement에 그대로 적용하지 않음
```

### 확인한 것

- 측정 범위는 전체 Projectile 시스템이 아니라 `UpdateLifetime()`이다.
- AoS와 SoA는 같은 결과를 만들었다.
- Automation, PIE, VTune에서 모두 SoA 쪽 실행 시간이 더 짧았다.
- SoA는 Instructions Retired가 오히려 더 많았다.
- 직접 본 `LifeTime` 갱신 지점은 두 함수 모두 scalar `subss`였다.
- Assembly에서는 AoS 64-byte / SoA 4-byte stride를 확인했다.
- 루프 전체의 자동 벡터화 여부는 확인하지 못했다.
- cache miss와 bandwidth는 직접 비교하지 못했다.
- 이 결과만으로 모든 Projectile 연산이나 다른 플랫폼에서도 SoA가 더 빠르다고 결론낼 수는 없다.

### 다음

- `LifeTime`과 `Flags`를 연속된 hot storage로 분리하는 방안을 우선 적용 후보로 둔다. 추가·삭제 때 배열 간 같은 Index를 함께 관리해야 한다.
- 변경 뒤 전체 `TickProjectiles()`를 다시 측정한다.
- `UpdateMovement()`는 `Position/Velocity`가 hot한 별도 workload로 측정한다.
- 그래도 CPU 예산을 넘으면 Game Thread 전용 Actor/UObject 접근과 독립 데이터 작업을 분리해 병렬화를 검토한다.
- Projectile의 생성·조회·수명 관리까지 시스템 전체가 병목일 때 MassEntity 수준의 구조 변경을 검토한다.
