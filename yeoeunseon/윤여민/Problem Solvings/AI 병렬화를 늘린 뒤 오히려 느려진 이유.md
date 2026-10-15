## 문제 상황

언리얼 액션 게임에서 대규모 전투를 추가했다.

평소에는 약 500~1,000명의 AI가 존재해 문제가 없었지만, 대규모 전투에서는 약 **20,000명**까지 증가하면서 AI 갱신 비용이 커졌다.

기존에는 Game Thread에서 다음처럼 처리하고 있었다.

```cpp
for (FEnemyState& Enemy : Enemies)
{
    UpdateDecision(Enemy);
}
```

Unreal Insights에서 확인한 결과:

```text
Game Thread
└─ UpdateAllEnemies     5.8 ms
```

`UpdateDecision()`은 Enemy의 상태를 읽고 간단한 판단 결과를 갱신하는 CPU 작업이다.

대부분의 Enemy는 서로 독립적으로 처리할 수 있다.

---

개발자는 이 작업을 Unreal Tasks로 나눴다.

```text
Enemy 20,000명

Task 0  → Enemy     0 ~   499
Task 1  → Enemy   500 ~   999
Task 2  → Enemy 1,000 ~ 1,499
...
Task 39 → Enemy 19,500 ~ 19,999
```

총 40개의 Task로 나눈 뒤 다시 측정했다.

```text
UpdateDecision 전체 완료 시간

기존        5.8 ms
40 Tasks    2.4 ms
```

성능이 크게 좋아졌다.

---

이후 개발자는 작업을 더 잘게 나누면 병렬성이 더 높아질 것이라고 생각했다.

이번에는 Enemy 하나마다 Task 하나를 만들었다.

```text
Enemy 20,000명
→ Task 20,000개
```

그리고 AI 작업을 더 많이 동시에 처리하기 위해 별도의 Worker Thread 16개도 추가했다.

테스트 PC:

```text
8 Physical Cores
16 Hardware Threads
```

변경 후 다시 측정했다.

```text
                       40 Tasks     20,000 Tasks + 추가 Worker

AI 완료 시간             2.4 ms              4.1 ms
Game Thread              6.1 ms              7.0 ms
CPU Utilization           높음                높음
```

Thread와 Task를 더 많이 사용했는데도 AI 처리 시간은 오히려 증가했다.

---

추가 프로파일링에서는 다음 변화가 보였다.

```text
- Thread 관련 scheduling event가 크게 늘었다.

- 일부 AI 작업은 실행 도중 일정 시간 진행되지 않았다.

- 같은 Thread가 이전보다 자주 다른 CPU Core에서 다시 실행됐다.

- AI 갱신은 수 MB 크기의 Enemy 데이터를 반복해서 읽는다.

- CPU 사용률은 여전히 높다.
```

일부 AI 작업은 Pathfinding 결과가 준비되어야 다음 처리를 진행할 수 있다.

팀에서는 몇 가지 의견이 나왔다.

```text
“CPU 사용률은 높은데 왜 오히려 느려졌지?”

“Task를 더 잘게 나누면 병렬성이 더 좋아지는 것 아닌가?”

“16 Hardware Threads면 Worker도 더 많이 만들수록 좋은 것 아닌가?”
```

### 해결 조건

현재 구조를 분석하고, **병렬화를 더 늘렸는데 왜 실제 실행 시간은 증가했는지** 설명한 뒤 개선 방향을 제시해라.

다음 요구사항은 유지해야 한다.

```text
AI 수 약 20,000명 유지
기존 AI 판단 결과 유지
Game Thread와 전체 AI 완료 시간 감소
```

단순히 Thread 수를 줄이거나 늘리는 것으로 끝내지 말고, 현재 병렬 처리 구조에서 어떤 비용이 발생하고 있는지 먼저 확인해라.

---

## 풀이 — 생각을 마친 뒤 열기

> [!question]- 1부 — Task를 많이 만들면 병렬성도 계속 늘어날까?
> ### STEP 1 — Task 수와 Thread 수부터 구분한다
>
> **Task 20,000개는 할 일 20,000개라는 뜻이지, Thread 20,000개가 동시에 실행된다는 뜻은 아니다.**
>
> 처음에는 Task 수가 Hardware Thread 수보다 많으므로 바로 Oversubscription이라고 생각했다.
>
> ```text
> Task   = 실행해야 할 일
> Thread = Task를 실행하는 실행 흐름
> Core   = Thread를 실제로 실행하는 하드웨어
> ```
>
> Unreal Task Scheduler는 많은 Task를 Queue에 두고 제한된 Worker들이 하나씩 가져가게 한다.
>
> ```text
> Task 20,000개
> ↓
> 기존 Worker Pool의 Queue에 쌓임
> ↓
> 사용 가능한 Worker가 나눠 실행
> ```
>
> 따라서 `Task 수 > Hardware Thread 수`만으로는 OS Thread Oversubscription이라고 할 수 없다. 일반적으로 Oversubscription은 **동시에 실행 가능한 Software Thread가 Hardware Thread보다 많은 상태**를 가리킨다.
>
> 문제의 변경안에는 별도 Worker Thread 16개도 있으므로 실제 Oversubscription 후보가 있지만, 먼저 Task를 지나치게 잘게 나눈 비용부터 분리한다.
>
> ---
>
> ### STEP 2 — Enemy마다 Task 하나를 만들면 무엇이 늘어날까?
>
> **Task가 작아질수록 병렬로 나눌 기회는 늘지만 실제 판단보다 Task 관리 비용이 커질 수 있다.**
>
> Task 하나에는 `UpdateDecision()` 외에도 다음 과정이 필요하다.
>
> ```text
> Task 생성
> → Queue 등록
> → Worker가 Task 선택
> → Scheduling
> → 완료 상태 기록
> → Wait와 Handle 정리
> ```
>
> 이 부가 비용이 **Task Overhead**다. Task 하나의 실제 계산이 작을수록 Overhead 비중이 커진다.
>
> 그래서 Enemy 20,000명을 다음처럼 비교했다.
>
> ```text
> Batch 1,000 → Task    20개
> Batch   500 → Task    40개
> Batch   250 → Task    80개
> Batch   125 → Task   160개
> Batch     1 → Task 20,000개
> ```
>
> `500 → 250 → 125`로 더 잘게 나눠도 시간은 거의 줄지 않았다. 반면 Enemy마다 Task를 만든 `Batch=1`은 순차 실행보다 느렸다.
>
> ```text
> 병렬성이 부족한 구간
> → Task를 더 나누면 개선될 수 있음
>
> 충분히 나뉜 구간
> → 더 나누어도 이득이 거의 없음
>
> 지나치게 잘게 나눈 구간
> → Task Overhead가 실제 계산을 압도
> ```
>
> 이 실행에서는 `Batch=500`부터 시간이 거의 평평해졌다. `125`가 가장 짧게 나왔다는 이유만으로 고정 정답으로 삼지는 않는다. Task 수는 네 배가 됐지만 차이는 약 1.5%뿐이어서 실행 오차와 환경 변화에 쉽게 뒤집힐 수 있다.

> [!question]- 2부 — Pathfinding 결과가 아직 없다면?
> ### STEP 3 — Dependency 자체를 없앨 수 있을까?
>
> **Pathfinding 결과가 실제 다음 판단에 필요하다면 Dependency를 없애는 것이 아니라 실행 순서로 표현해야 한다.**
>
> 처음에는 Pathfinding과 AI 판단이 서로 기다리지 않도록 Task를 짜면 된다고 생각했다. 하지만 다음 판단이 Path 결과를 사용한다면 선행 관계는 실제 요구사항이다.
>
> ```text
> Pathfinding 완료
> ↓
> 결과를 사용하는 AI 판단
> ```
>
> 문제는 Dependency가 있다는 사실이 아니라 기다리는 방식이다.
>
> ```cpp
> // Task 본문이 이미 Worker에서 실행된 뒤 멈춘다.
> PathReadyEvent->Wait();
> ContinueDecision();
> ```
>
> 대신 Task Scheduler가 선행 관계를 알도록 연결할 수 있다.
>
> ```cpp
> UE::Tasks::Launch(
>     UE_SOURCE_LOCATION,
>     []
>     {
>         ContinueDecision();
>     },
>     UE::Tasks::Prerequisites(PathReady));
> ```
>
> 이 경우 의존 Task 본문은 Path 결과가 준비된 뒤에만 시작한다.
>
> ---
>
> ### STEP 4 — `FEvent::Wait()`는 Worker를 완전히 고갈시킬까?
>
> **현재 Worker는 멈추지만 UE 5.8 Windows는 이 대기를 Scheduler에 알려 보충 Worker 실행을 허용할 수 있다.**
>
> 처음 예상은 단순했다.
>
> ```text
> Worker가 Wait
> → 사용할 Worker 감소
> → 준비된 다른 Task도 진행하지 못함
> ```
>
> 하지만 UE 5.8 Windows의 `FEvent::Wait()`는 내부에서 `FOversubscriptionScope`를 사용한다. Scheduler가 Worker의 대기를 알 수 있고 필요하면 Standby Worker를 깨워 처리 능력을 보충할 수 있다.
>
> ```text
> 기존 Worker
> → FEvent::Wait에서 멈춤
>
> Scheduler
> → 대기를 인식
> → 보충 Worker 실행 가능
>
> 준비된 독립 Task
> → 계속 처리될 수 있음
> ```
>
> 실제 테스트에서도 Blocking 방식의 의존 Task 20개가 모두 대기에 들어갔지만, Path와 무관한 Task 20개도 결과가 준비되기 전에 전부 완료됐다.
>
> Prerequisite 방식은 결과가 준비되기 전에 시작한 의존 Task가 0개였다. 구조적으로 Worker를 멈춘 뒤 보충하는 과정이 필요 없다.
>
> 다만 완료 시간은 두 방식이 사실상 같았다.
>
> ```text
> Blocking Wait  3.115 ms
> Prerequisite   3.125 ms
> ```
>
> 따라서 이번 조건에서는 Path 대기만으로 원래 문제의 성능 저하가 재현되지 않았다. Prerequisite는 더 빠르다고 증명된 해법이 아니라, 의존성을 Scheduler에 직접 표현해 불필요한 Worker 대기를 만들지 않는 구조다.

> [!question]- 3부 — 원래 문제로 돌아가기
> ### STEP 5 — CPU 사용률이 높은데 왜 느릴 수 있을까?
>
> **CPU가 바쁘다는 사실은 유용한 AI 계산만 하고 있다는 뜻이 아니다.**
>
> 다음 작업도 CPU 시간과 실행 자원을 사용한다.
>
> ```text
> Task 관리
> Scheduling
> Context Switch
> 대기와 깨우기
> Worker 보충
> Cache에서 밀려난 데이터 다시 읽기
> ```
>
> Task Granularity 테스트에서는 20,000개의 작은 Task가 실제 계산보다 큰 비용을 만든다는 사실을 확인했다.
>
> 별도 OS Thread를 추가하는 경우도 분리해서 비교했다. 처음에는 `UE Runner Task 18개 + 추가 Thread 16개`를 실행했지만, 실제 계산 구간에 들어온 Worker의 peak는 20개였다. 논리 프로세서 20개를 넘지 않았고, 이 모형 계산에 참여한 Worker 수가 늘면서 시간은 줄었다. 이 수치만으로 나머지 CPU 실행 자원이 실제로 idle이었다고 단정하지는 않는다.
>
> 그래서 Task Scheduler를 제외하고 전용 OS Thread 20개와 36개가 같은 40개 Batch를 처리하게 했다. 모든 Thread를 시작선에 세운 뒤 한꺼번에 깨웠으므로 두 번째 조건에서는 36개의 runnable CPU Worker가 20개 논리 프로세서를 경쟁했다.
>
> ```text
> 20 Threads  11.930 ms
> 36 Threads  11.981 ms
> 비율         1.004x
> ```
>
> Oversubscription 조건은 만들었지만 완료 시간은 사실상 같았다. 따라서 **Thread 수가 하드웨어 실행 자원을 넘었다는 사실만으로 원래 문제의 `2.4 → 4.1 ms`를 설명할 수 없다.** 원래 프로파일링의 Scheduling event와 Context Switch 증가는 계속 확인할 대상이지만, 이번 Automation에서는 각각의 비용이 분리되어 측정되지 않았다.
>
> 같은 Thread가 다른 Core에서 실행됐다는 관찰도 Cache 관점에서 볼 수 있다.
>
> ```text
> Core A에서 Enemy 데이터 사용
> → A의 L1/L2에 일부 데이터가 남음
>
> Thread가 Core B에서 다시 실행
> → A의 private cache locality를 그대로 쓰기 어려울 수 있음
> ```
>
> 이것은 Cache 전체가 초기화된다는 뜻이 아니다. Thread Migration과 다른 작업의 Working Set 때문에 유용한 데이터가 밀릴 가능성이 생긴다는 뜻이다. 실제 Cache Miss 증가량은 CPU counter를 측정해야 확정할 수 있다.
>
> ---
>
> ### STEP 6 — 현재 증거로 어떤 구조부터 적용할까?
>
> **기존 UE Task Scheduler에서 충분히 큰 Batch를 사용하고, 실제 Dependency는 Wait보다 prerequisite나 callback으로 연결한다. 별도 Worker는 실제 프레임 측정에서 이득이 확인될 때만 추가한다.**
>
> ```text
> 실제 UpdateDecision의 공유 접근 확인
> ↓
> 별도 AI Worker를 추가하지 않고 기존 UE Task Scheduler 사용
> ↓
> 연속된 Enemy 500명 정도의 Batch에서 시작
> ↓
> 주변 Batch 크기를 실제 workload로 재측정
> ↓
> Path 결과가 필요한 판단은 prerequisite / callback으로 연결
> ↓
> 결과 동일성과 전체 프레임 시간 확인
> ```
>
> 테스트에서는 `Enemies`를 읽기 전용으로 두고 `Results` 크기를 Task 실행 전에 확정했다. 각 Task는 겹치지 않는 Index 범위만 썼다.
>
> 실제 `UpdateDecision()`도 같은 조건인지 확인해야 한다.
>
> ```text
> 안전한 후보
> → Task별로 겹치지 않는 FEnemyState 범위 수정
> → 읽기 전용 공유 데이터 사용
>
> 다시 설계해야 하는 접근
> → 같은 Manager 상태를 여러 Task가 동시 수정
> → 실행 중 TArray Add / Remove
> → Game Thread 전용 Actor / UObject 상태에 임의 접근
> ```

> [!note]- 같이 볼 개념
> | 개념 | 이 문제에서의 의미 |
> |---|---|
> | Task | Worker가 실행할 작업 단위다. Task 수가 Thread 수는 아니다. |
> | Worker Thread | Queue에서 Task를 가져와 실행하는 Thread다. |
> | Task Granularity | Task 하나가 담당하는 작업 크기다. 너무 크면 병렬성이 부족하고 너무 작으면 Overhead가 커진다. |
> | Overhead | Task 생성·Queue·Scheduling·완료 확인처럼 실제 AI 판단 외에 드는 비용이다. |
> | Dependency | 다른 작업의 결과가 먼저 필요한 관계다. 필요하다면 없애지 말고 실행 순서로 표현한다. |
> | Blocking | Thread가 조건이 만족될 때까지 다음 계산을 진행하지 못하는 상태다. |
> | Prerequisite | 선행 작업이 완료되기 전에는 Task 본문이 시작되지 않도록 Scheduler에 관계를 알려준다. |
> | Oversubscription | Runnable Software Thread가 Hardware Thread보다 많아 Scheduling 경쟁이 커질 수 있는 상태다. 많은 Task가 Queue에 있다는 것과는 다르다. |
> | Context Switch | 실행 Thread를 교체하는 일이다. Cache 전체를 Flush하지는 않지만 Working Set 유지에 불리할 수 있다. |
> | Thread Migration | 같은 Thread가 다른 Core에서 다시 실행되는 일이다. Load Balancing에는 도움이 될 수 있지만 private cache locality를 잃을 수 있다. |
> | Park / Wake | 대기 중인 Worker를 재우고 필요할 때 다시 깨우는 동작이다. Parked Worker는 Task Queue에 들어간 작업이 아니다. |

## 테스트 — Task 크기, Path 대기, Thread 수

> [!example]- 테스트 전체 보기
> 테스트 코드는 실제 게임의 `UpdateDecision()`이나 Navigation System을 그대로 재현하지 않는다. Enemy끼리 독립적인 CPU 작업, Path 결과 도착 시점, CPU-bound Thread 경쟁을 각각 단순화했다.
>
> - `AIParallelTaskSizeTest.cpp`
> - `AIPathfindingWaitTest.cpp`
> - `AIThreadOversubscriptionTest.cpp`
>
> 테스트 PC에서는 `14 Physical Cores / 20 Logical Processors`로 감지됐다. 문제의 가상 PC인 `8 / 16`과 다르므로 절대 시간과 가장 좋은 Batch 크기를 그대로 옮기지 않는다.
>
> ### 1. Task Granularity
>
> 같은 Enemy 20,000명과 같은 `UpdateDecision()`을 사용했다. `Enemies`는 읽기 전용이며, 미리 크기를 정한 `Results`의 서로 겹치지 않는 범위에만 썼다.
>
> warm-up 뒤 7 samples를 측정했고 실행 순서는 sample마다 반대로 바꿨다. 측정 범위에는 `Launch`, Queue/Scheduling, `Wait`, Task Handle 정리가 포함된다.
>
> | 실행 방식 | Task 수 | Median | 순차 대비 | 결과 |
> |---|---:|---:|---:|---:|
> | Sequential | 0 | 4.712 ms | 1.000x | mismatch 0 |
> | Batch 1,000 | 20 | 0.951 ms | 4.953x | mismatch 0 |
> | Batch 500 | 40 | 0.795 ms | 5.924x | mismatch 0 |
> | Batch 250 | 80 | 0.787 ms | 5.990x | mismatch 0 |
> | Batch 125 | 160 | 0.783 ms | 6.014x | mismatch 0 |
> | Batch 1 | 20,000 | 6.383 ms | 0.738x | mismatch 0 |
>
> `mismatch=0`과 `wait failure=0`은 모든 방식이 같은 판단 결과를 만들고 Task가 정상적으로 완료됐다는 뜻이다. 특정 Batch가 빠르다는 뜻은 아니다.
>
> `Batch=1,000 → 500`에서는 시간이 줄었지만 `500 → 250 → 125`에서는 거의 달라지지 않았다. 현재 CPU와 모형 작업에서는 Task 40개 부근부터 병렬성 이득이 포화됐다고 볼 수 있다.
>
> ```text
> Batch 500 → 0.795 ms / Task 40개
> Batch 125 → 0.783 ms / Task 160개
> ```
>
> Task 수를 네 배로 늘려 얻은 차이는 약 1.5%였다. 반면 `Batch=1`은 Task 20,000개를 관리하면서 6.383 ms가 걸려 순차 실행 4.712 ms보다 약 35.5% 느렸다.
>
> **이 테스트는 Task가 너무 작으면 관리 비용이 병렬화 이득보다 커질 수 있다는 것을 재현했다. Task 20,000개가 Thread 20,000개나 OS Oversubscription을 뜻하지는 않는다.**
>
> ---
>
> ### 2. Pathfinding 결과를 Task 안에서 기다린다면
>
> 두 방식 모두 AI Task 40개, Batch 500, Path 의존 Task 20개, 2 ms 뒤 도착하는 같은 Path 결과를 사용했다. 신호용 1회성 Thread는 외부 결과 도착만 모형화하고 AI 계산은 하지 않았다.
>
> ```text
> Blocking Wait
> → AI Task 본문 시작
> → FEvent::Wait
> → PathReady 뒤 계속 실행
>
> Prerequisite
> → PathReady를 선행 조건으로 등록
> → 준비 뒤에만 AI Task 본문 시작
> ```
>
> | 측정값 | Blocking Wait | Prerequisite |
> |---|---:|---:|
> | Median | 3.115 ms | 3.125 ms |
> | Path 준비 전 시작한 의존 Task | sample당 20 | 0 |
> | Path 준비 전 완료한 독립 Task | median 20 | median 20 |
> | Result mismatch | 0 | 0 |
> | Wait failure | 0 | 0 |
>
> Blocking에서는 7 samples 동안 의존 Task 140개가 모두 Path 준비 전에 시작했다. 매 sample 20개가 `FEvent::Wait()`에 들어갔지만 독립 Task 20개도 Path 준비 전에 모두 완료됐다.
>
> 이 결과는 UE Scheduler의 보상 때문에 Blocking 중에도 준비된 작업이 계속 진행될 수 있음을 보여준다. Prerequisite에서는 의존 Task가 신호 전에 한 번도 시작되지 않아 선행 관계가 의도대로 적용됐다.
>
> 완료 시간은 `0.997x`로 사실상 같았다. **따라서 이 조건에서는 Path 대기만으로 성능 저하가 재현되지 않았다.** 대기·깨우기와 보충 Worker 비용이 존재할 수는 있지만 이번 시간값에서 유의미한 병목으로 관찰되지는 않았다.
>
> 첫 실행에서는 Blocking이 약 18.2% 느렸지만 재실행에서 차이가 유지되지 않았다. 한 번의 실행 차이를 원인으로 확정하지 않는 이유가 여기서 드러난다.
>
> ---
>
> ### 3. OS Thread 수가 논리 프로세서를 넘는다면
>
> 첫 실험은 기존 UE Scheduler와 추가 Thread를 비교했다.
>
> | 실행 방식 | 계산 구간 Worker peak | Median | 작업 분배 |
> |---|---:|---:|---:|
> | UE Only | 12 | 20.475 ms | UE 40 Jobs |
> | UE + 추가 Thread 16개 | 20 | 10.271 ms | UE 8 / 추가 Thread 32 Jobs |
>
> `UE Runner Task 18개`는 동시에 실행 중인 OS Thread 18개라는 뜻이 아니었다. 추가 Thread 조건에서도 계산 구간의 peak는 논리 프로세서 수와 같은 20개였다. 이 실험은 Oversubscription 비용이 아니라 이 모형 계산에 들어온 Worker의 peak가 `12 → 20`으로 늘어난 결과를 보여줬다. CPU의 실제 idle 시간은 별도로 측정하지 않았다.
>
> Oversubscription만 분리하기 위해 다음 실험에서는 Task Scheduler를 사용하지 않았다. 같은 40개 Batch를 전용 OS Thread 20개와 36개로 처리했고, Thread 생성은 측정에서 제외했다. 모든 Thread가 시작선에 도착한 뒤 한꺼번에 깨웠다.
>
> | 실행 방식 | 풀려난 runnable Thread | 계산 구간 peak | Median | 결과 |
> |---|---:|---:|---:|---:|
> | Hardware-sized | 20 | 20 | 11.930 ms | mismatch 0 |
> | Oversubscribed | 36 | 21 | 11.981 ms | mismatch 0 |
>
> 36개의 CPU-bound Thread가 동시에 runnable 상태가 되어 20개 논리 프로세서를 경쟁했지만 완료 시간은 `1.004x`로 사실상 같았다. `계산 구간 peak=21`은 함수에 들어와 아직 끝나지 않은 Thread 수이며, 한 순간에 CPU에서 실제 실행된 Thread 수는 아니다.
>
> **이 테스트는 Oversubscription 상태를 만들었지만, 이 workload에서는 유의미한 시간 손실을 재현하지 못했다.** Oversubscription이 항상 공짜라는 뜻도, 항상 느리다는 뜻도 아니다. 작업 길이, 대기, 다른 Engine Thread와의 경쟁, Scheduling 패턴에 따라 비용이 달라진다.
>
> 완료 시간만으로 Context Switch, Core Migration, Cache Miss 중 무엇이 얼마나 발생했는지는 알 수 없다. 원래 문제의 프로파일링 현상과 연결하려면 같은 workload의 Scheduling 및 CPU counter를 따로 수집해야 한다.
>
> ---
>
> ### 이 테스트로 확인하지 않은 것
>
> - 실제 게임처럼 장시간 유지되는 추가 Worker Pool과 전체 프레임 workload는 재현하지 않았다.
> - Context Switch 횟수와 Scheduling 시간을 직접 측정하지 않았다.
> - Thread Migration으로 Cache Miss가 얼마나 증가했는지 측정하지 않았다.
> - 실제 Navigation System의 Pathfinding 비용과 callback 흐름은 포함하지 않았다.
> - 실제 게임의 `UpdateDecision()` 공유 접근 안전성은 검증하지 않았다.

## 결론

> [!success] 이 문제에서 가져갈 것
>
> **병렬성은 Task와 Thread를 많이 만드는 데서 나오지 않는다. 실제 작업보다 충분히 큰 Task를 기존 Worker Pool에 나누고, 필요한 Dependency는 Worker가 기다리게 하지 말고 Scheduler가 알 수 있는 실행 순서로 표현한다.**

### 이번 문제의 흐름

```text
Enemy 20,000명의 UpdateDecision이 느리다
→ 500명씩 40 Tasks
→ 5.8 ms에서 2.4 ms로 개선

더 잘게 나누면 더 빠를까?
→ Enemy마다 Task 하나
→ Task Granularity와 Overhead 의심

Batch 비교
→ 500명 부근부터 이득 포화
→ Task 20,000개는 순차 실행보다 느림

Pathfinding을 기다리는 Task도 문제일까?
→ Blocking Wait와 Prerequisite 비교

Blocking Worker는 멈춤
→ UE Scheduler가 보충 실행을 허용할 수 있음
→ 독립 Task는 계속 진행
→ 이번 조건에서는 완료 시간 차이 없음

원래 프로파일링의 추가 Worker와 Context Switch 증가
→ OS Thread 수만 분리해 20개와 36개 비교
→ Oversubscription 조건은 만들었지만 완료 시간은 사실상 같음
→ 원래 2.4 ms → 4.1 ms를 Thread 수 초과 하나로 설명할 수 없음
```

### 확인한 것

- Task 수와 Thread 수는 다르다.
- 20,000개의 작은 Task는 Task 관리 비용 때문에 순차 실행보다 느렸다.
- 현재 모형에서는 500명 단위부터 Batch를 더 줄여도 이득이 거의 없었다.
- 모든 실행 방식이 같은 AI 판단 결과를 만들었다.
- Blocking 방식에서는 Path 의존 Task가 Worker에서 시작된 뒤 기다렸다.
- UE Scheduler의 보상으로 독립 Task는 Path 대기 중에도 진행됐다.
- Prerequisite는 결과 준비 전 의존 Task 실행을 0회로 막았다.
- 이번 조건에서는 Blocking과 Prerequisite의 완료 시간 차이가 없었다.
- `UE Runner Task 수`와 실제 계산에 참여한 OS Thread 수는 같지 않았다.
- 전용 CPU Thread 36개를 20개 논리 프로세서에 경쟁시켰지만 이번 조건의 완료 시간은 `1.004x`로 사실상 같았다.
- Oversubscription의 존재와 유의미한 성능 저하는 별도로 검증해야 한다.

### 아직 확정하지 않은 것

- 별도 Worker 16개가 실제 전체 프레임에서 성능 저하에 기여한 정확한 비율
- Context Switch와 Thread Migration이 Cache에 준 실제 비용
- 실제 Pathfinding 대기 빈도에서 Scheduler 보상 비용이 병목이 되는지
- 실제 `UpdateDecision()`의 공유 접근과 Game Thread 전용 데이터 사용 여부

### 현재 개선 방향

```text
UpdateDecision의 읽기·쓰기 범위 확인
↓
기존 Unreal Worker Pool 사용
↓
500명 정도의 연속 Batch에서 시작
↓
실제 workload로 주변 Batch 크기 재측정
↓
Path 의존 작업은 prerequisite 또는 callback으로 연결
↓
AI 결과 동일성과 전체 프레임 시간 재확인
```

문제에서 관찰된 추가 Worker, Context Switch, Core Migration은 무시하지 않는다. 다만 이번 테스트에서 직접 확인한 것은 `20개보다 많은 runnable Thread를 만든 조건`과 `그 조건에서 완료 시간이 거의 같았다는 사실`까지다. Context Switch와 Cache 손실의 실제 비용은 별도 측정 전까지 원인으로 확정하지 않는다.
