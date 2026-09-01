# EnemyManager 검색 최적화와 생성 순서 유지

## 문제 상황

언리얼 게임에서 적 상태를 다음처럼 관리하고 있다.

```cpp
struct FEnemyState
{
    int32 EnemyId;
    float HP;
    FVector Position;
};

TArray<FEnemyState> Enemies;
```

특정 적은 `EnemyId`로 찾는다.

```cpp
FEnemyState* FindEnemy(int32 EnemyId)
{
    for (FEnemyState& Enemy : Enemies)
    {
        if (Enemy.EnemyId == EnemyId)
        {
            return &Enemy;
        }
    }

    return nullptr;
}
```

적이 많아지자 프로파일링에서 **`FindEnemy()`가 자주 호출되면서 비용이 커지고 있는 것**을 발견했다.

여러 시스템이 이 함수를 사용한다.

```text
AI
투사체
UI
```

UI에는 이런 코드도 있다.

```cpp
if (EnemyManager.FindEnemy(EnemyId))
{
    float HP =
        EnemyManager.FindEnemy(EnemyId)->HP;
}
```

적은 전투 중 추가되고 삭제된다.

```cpp
Enemies.Add(NewEnemy);
Enemies.RemoveAt(Index);
```

그리고 중요한 요구사항이 하나 있다.

> **Enemies는 적이 생성된 순서를 유지해야 한다.**

한 개발자가 반복 검색을 없애기 위해 다음처럼 수정했다.

```cpp
FEnemyState* CachedTarget;
```

그런데 **적이 많이 추가된 직후 `CachedTarget`을 사용할 때 가끔 크래시가 발생한다.**

### 해결 조건

이 시스템을 개선해라.

단순히 `FindEnemy()`만 빠르게 만드는 것이 아니라 다음을 모두 고려해야 한다.

```text
EnemyId 검색
전체 순회
적 추가
적 삭제
생성 순서 유지
참조의 안전성
```

그리고 해결책을 하나 제시할 때마다:

> **“이 해결책 때문에 새로 생기는 문제는 무엇인가?”**

를 확인해라.

---

## 풀이 — 생각을 마친 뒤 열기

> [!question]- 1부 — CachedTarget 크래시를 따라가기
> ### STEP 1 — 반복 검색을 캐시하면?
>
> **당시 첫 생각**
>
> “`FindEnemy()`를 여러 곳에서 반복하네. 한번 찾은 결과를 캐시하면 되지 않을까?”
>
> 문제에도 이미 `FEnemyState* CachedTarget`이 있다. 그런데 적이 추가된 뒤 이 포인터를 사용하면 가끔 크래시한다.
>
> ---
>
> `FEnemyState*`가 기억하는 것은 Enemy의 정체성이 아니라 **현재 메모리 주소**다.
>
> ```text
> TArray<FEnemyState>
> ↓
> FEnemyState 객체가 배열의 내부 메모리에 직접 저장됨
>
> FindEnemy()
> ↓
> &Enemy 반환
> ↓
> CachedTarget가 그 주소를 저장
> ```
>
> `TArray::Add()` 중 Capacity가 부족하면 더 큰 메모리를 확보하고 기존 원소를 옮길 수 있다.
>
> ```text
> 재할당 전 Enemy 100 주소 = 0x1000
> CachedTarget             = 0x1000
>
> TArray 재할당
>
> 재할당 후 Enemy 100 주소 = 0x5000
> CachedTarget             = 0x1000
> ```
>
> Enemy는 여전히 배열에 있지만, 이전 주소를 저장한 `CachedTarget`은 이제 **dangling pointer**다.
>
> ---
>
> ### STEP 2 — 방어 처리하면 되지 않을까?
>
> **당시 다음 생각**
>
> “사용하기 전에 null 체크하면 되는 것 아닌가?”
>
> ```cpp
> if (CachedTarget)
> {
>     float HP = CachedTarget->HP;
> }
> ```
>
> 이 조건이 묻는 것은 하나뿐이다.
>
> ```text
> CachedTarget의 주소 값이 0인가?
> ```
>
> 다음 질문에는 답하지 못한다.
>
> ```text
> 이 주소가 지금도 내가 원한 FEnemyState를 가리키는가?
> ```
>
> ```text
> nullptr
> ≠
> dangling pointer
> ```
>
> 재할당 뒤에도 포인터 변수에는 예전 주소 값이 그대로 남아 있으므로 `CachedTarget != nullptr`은 참일 수 있다. 이 상태에서 역참조하면 안전하지 않다.
>
> ---
>
> ### STEP 3 — 유효한 주소인지 검사하면?
>
> **당시 다음 생각**
>
> “그러면 이 포인터가 아직 유효한 주소인지 확인하면 되지 않을까?”
>
> 일반 `FEnemyState*`에는 객체의 수명을 추적하거나, 나중에 **같은 Enemy인지** 확인해주는 범용 장치가 없다.
>
> ```cpp
> // 일반 raw pointer에 이런 검사가 자동으로 제공되는 것은 아니다.
> if (IsStillValidFEnemyStatePointer(CachedTarget))
> {
> }
> ```
>
> 메모리 주소가 읽을 수 있는 상태인지도 충분한 조건이 아니다.
>
> ```text
> 예전: 0x1000 = Enemy 100
> 이동: Enemy 100 = 0x5000
> 나중: 0x1000은 다른 데이터가 재사용할 수도 있음
> ```
>
> `TMap<int32, FEnemyState*>`에 넣어도 Value가 같은 `TArray` 원소 주소라면 문제는 그대로다.
>
> **핵심**
>
> 오래 저장한 raw pointer를 나중에 검사해서 살려내려 하지 않는다. 수명을 보장할 수 없는 주소를 장기 식별자로 사용하지 않는다.
>
> ---
>
> ### STEP 4 — 주소 말고 무엇을 기억해야 할까?
>
> 여기서 주소와 정체성을 분리한다.
>
> ```text
> FEnemyState* = Enemy가 지금 메모리 어디에 있는가
> EnemyId      = 내가 원하는 Enemy가 누구인가
> ```
>
> 재할당이 일어나면 주소는 변할 수 있지만 `EnemyId`는 그대로다.
>
> ```cpp
> int32 CachedTargetId = 200;
> ```
>
> 필요할 때 현재 저장 구조에서 다시 찾는다.
>
> ```cpp
> if (FEnemyState* Enemy = EnemyMap.Find(CachedTargetId))
> {
>     // 현재 조회에서만 짧게 사용한다.
>     UseEnemy(*Enemy);
> }
> ```
>
> `Find()`가 `nullptr`이면 **현재 그 ID의 Enemy가 없다**는 의미 있는 판단도 가능하다.
>
> ```text
> ID는 오래 저장
> ↓
> 필요할 때 Find
> ↓
> 반환된 pointer는 짧게 사용
> ↓
> 장기 저장하지 않음
> ```
>
> `Find()`가 반환한 포인터도 컨테이너 변경 뒤까지 안전하다는 뜻은 아니다. 장기 보관하는 것은 ID이고, 포인터는 현재 작업 범위에서만 사용한다.

> [!question]- 2부 — Stable ID를 빠르게 다시 찾기
> ### STEP 5 — 재조회 횟수부터 줄일 수 있을까?
>
> ID로 다시 찾기로 했다고 같은 ID를 한 코드 블록에서 두 번 찾을 필요는 없다.
>
> ```cpp
> if (FEnemyState* Enemy = EnemyManager.FindEnemy(EnemyId))
> {
>     float HP = Enemy->HP;
> }
> ```
>
> 이 포인터 재사용은 장기 캐시가 아니다. 호출 사이에 컨테이너 변경이 없는 짧은 범위에서 방금 찾은 결과를 바로 쓰는 것이다.
>
> **핵심:** 더 빠른 자료구조를 찾기 전에 불필요한 작업부터 제거한다.
>
> ---
>
> ### STEP 6 — 남은 한 번의 조회를 빠르게 만들려면?
>
> 이제 캐시로 조회를 없애는 대신, 안정적인 ID를 빠르게 다시 찾는 구조를 비교한다.
>
> ```text
> Linear Search → 정렬 불필요, 조회할 때 앞에서부터 비교
> Binary Search → 빠르지만 EnemyId 기준 정렬 필요
> TMap          → EnemyId로 직접 조회하기 적합
> ```
>
> **Binary Search는 왜 정렬이 필요한가?**
>
> Binary Search는 가운데 값을 본 뒤, 찾는 값이 더 크거나 작은지를 이용해 탐색 범위의 절반을 버린다.
>
> **정렬된 경우**
>
> ```text
> 정렬된 배열 [10, 20, 30, 40, 50]
> 찾는 값 40, 가운데 값 30
>
> 40 > 30
> → 30의 왼쪽에는 40이 없다고 확신
> → 왼쪽 절반을 버릴 수 있음
> ```
>
> 정렬되지 않은 배열에서는 이 판단을 할 수 없다.
>
> **정렬되지 않은 경우**
>
> ```text
> 정렬되지 않은 배열 [30, 10, 50, 20, 40]
> 찾는 값 40, 가운데 값 50
>
> 40 < 50이어도
> 40이 가운데 값의 왼쪽에 있다는 보장이 없음
> → 어느 절반도 안전하게 버릴 수 없음
> ```
>
> 따라서 `EnemyId`로 Binary Search하려면 배열도 `EnemyId` 기준으로 정렬되어 있어야 한다.
>
> Binary Search를 실제 `Enemies`에 적용하면 생성 순서가 바뀐다.
>
> ```text
> 생성 순서 [30, 10, 40, 20]
>               ↓ EnemyId 정렬
> 정렬 결과 [10, 20, 30, 40]
> ```
>
> 빠른 알고리즘의 전제조건이 기존 요구사항과 충돌했다. 이 문제에서는 상태를 `EnemyId`로 다시 찾는 구조로 `TMap<int32, FEnemyState>`가 더 자연스럽다.
>
> ---
>
> ### STEP 7 — 주소 대신 Array Index를 저장하면?
>
> 포인터 대신 `EnemyId -> Array Index`를 저장하면 재할당으로 주소가 바뀌는 문제는 피한다.
>
> 하지만 `RemoveAt()` 뒤에는 Index가 낡는다.
>
> ```text
> [100, 200, 300, 400]
>        ↓ 200 삭제
> [100, 300, 400]
>
> 저장된 Index: 300 -> 2
> 실제 Index  : 300 -> 1
> ```
>
> `RemoveAtSwap()`은 수정해야 할 Index 범위를 줄이지만 생성 순서를 깨뜨린다.
>
> ```text
> [100, 200, 300, 400]
>        ↓ 200 RemoveAtSwap
> [100, 400, 300]
>
> 필요한 생성 순서: [100, 300, 400]
> ```
>
> 주소와 Index를 캐시하는 후보가 계속 막히면서 질문이 바뀐다.
>
> “검색과 생성 순서를 왜 하나의 컨테이너가 모두 책임지고 있지?”

> [!question]- 3부 — 검색·순서·삭제의 역할 나누기
> ### STEP 8 — 검색과 생성 순서를 분리하면?
>
> ```text
> TMap<int32, FEnemyState>    → EnemyId 검색과 살아 있는 상태
> TArray<int32> CreationOrder → 생성 순서
> 외부 시스템                 → Stable EnemyId 보관
> ```
>
> `CreationOrder`에는 주소나 상태를 중복 저장하지 않고 ID만 둔다. 순회할 때 ID로 Map의 현재 상태를 조회한다.
>
> 검색 구조를 바꿔도 생성 순서는 유지되고, 외부 시스템도 움직일 수 있는 주소를 장기 보관하지 않는다.
>
> ---
>
> ### STEP 9 — Order에서 삭제는 즉시 할까, 미룰까?
>
> Tombstone은 필수적인 최종 답이 아니다. `CreationOrder.RemoveAt()` 비용이 실제로 문제가 될 때 선택하는 **Lazy / Deferred Deletion 전략**이다.
>
> ```text
> 적이 적고 삭제가 드묾
> → RemoveAt으로 즉시 삭제
> → 구현이 단순함
>
> 수천~수만 개가 전투 중 빈번하게 삭제됨
> + 프레임 스파이크에 민감함
> → Map에서는 즉시 삭제
> → CreationOrder에는 죽은 ID를 남김
> → 순회에서 Map에 없으면 건너뜀
> ```
>
> Tombstone을 선택하면 삭제 시 배열을 당기는 비용을 피하지만, 죽은 ID까지 검사하는 순회 비용이 생긴다.
>
> ---
>
> ### STEP 10 — 미룬 비용은 언제 지불할까?
>
> Tombstone이 충분히 쌓이면 전투·웨이브 종료 같은 안전한 시점이나 정해둔 임계치에서 살아 있는 ID만 남기도록 `Compact`한다.
>
> ```text
> 즉시 RemoveAt 비용
> ↓ Tombstone
> 삭제 시점에는 미룸
> ↓ Compact
> 안전한 시점에 한꺼번에 지불
> ```
>
> 비용을 없앤 것이 아니다. 현재 workload에서 덜 민감한 시점으로 옮기고, 언제 지불할지 통제한 것이다.

## 해설 2 — 테스트 검증

> [!example]- 테스트 전체 보기
> ### 테스트 실행 정보
>
> 테스트 파일은 `EnemyManagerSearchProblemTest.cpp`, Automation 경로는 `TestProject.ProblemSolving.EnemyManagerSearchProblem`이다.
>
> 현재 수정본은 아래 10개 실험과 assertion을 포함한다. 시간값은 실행 환경마다 달라질 수 있으므로 비교 횟수와 상태 변화가 핵심 근거다.
>
> ### 1. CachedTarget은 주소가 바뀐 뒤 nullptr이 될까?
>
> **상황**
>
> 반복 검색을 없애려고 `TArray` 원소 포인터를 저장했다.
>
> **실험**
>
> `AddEnemiesAfterCachingPointer()`는 `Reserve(1)` 뒤 원소를 추가해 재할당을 유도하고, 저장한 주소와 현재 원소 주소를 비교한다. 오래된 포인터는 역참조하지 않는다.
>
> **관찰할 결과**
>
> ```text
> TArray 주소 변경             = YES
> CachedTarget != nullptr      = YES
> CachedTarget == CurrentEnemy = NO
> ```
>
> **판단**
>
> null 체크는 dangling pointer를 검출하지 못한다. `nullptr`은 주소 값이 없다는 뜻이고, dangling은 주소 값은 남아 있지만 대상의 수명이 끝났거나 이동한 상태다.
>
> **다음**
>
> raw pointer를 나중에 검사하거나 다른 컨테이너에 저장하면 달라지는지 본다.
>
> ---
>
> ### 2. TMap에 raw pointer를 저장하면 달라질까?
>
> **실험**
>
> `StoreArrayPointerInMapThenGrowArray()`는 `TMap<int32, FEnemyState*>`에 `TArray` 원소 주소를 넣고 배열을 성장시킨다.
>
> **관찰할 결과**
>
> ```text
> TArray 주소 변경               = YES
> Map 포인터가 현재 원소를 가리킴 = NO
> ```
>
> **판단**
>
> 포인터를 담은 컨테이너가 문제가 아니라 포인터가 가리키는 대상의 lifetime이 문제다. 일반 raw pointer에는 같은 객체인지 사후 검증해주는 추적 정보가 없다.
>
> **다음**
>
> 주소 대신 Enemy의 정체성을 나타내는 ID를 저장한다.
>
> ---
>
> ### 3. ID 재조회가 왜 안전한가?
>
> **실험**
>
> `StoreIdAndFindCurrentEnemy()`는 `CachedTargetId=200`을 저장한다. Map에 10,000개를 더 추가한 뒤 ID로 다시 찾고, Enemy 200을 삭제한 뒤 한 번 더 찾는다.
>
> **관찰할 결과**
>
> ```text
> 저장한 ID                   = 200
> 성장 후 찾은 EnemyId        = 200
> Enemy 200 삭제 후 Find(200) = nullptr
> ```
>
> **판단**
>
> ID는 `누구인지`를 보존한다. 재조회는 현재 위치의 포인터를 새로 얻고, 삭제 뒤에는 현재 존재하지 않는다는 결과를 돌려준다.
>
> **다음**
>
> ID 재조회 과정에서 불필요한 호출부터 제거한다.
>
> ---
>
> ### 4. 중복 Find를 제거하면?
>
> **실험**
>
> `CompareDuplicateAndReusedFind()`는 같은 ID를 두 번 찾는 경우와 한 번 찾은 포인터를 같은 범위에서 즉시 재사용하는 경우를 비교한다.
>
> **결과**
>
> ```text
> 1,999,800 comparisons → 999,900 comparisons
> ```
>
> **판단**
>
> 장기 raw pointer 캐시를 만들지 않고도 중복 호출을 절반으로 줄였다.
>
> **다음**
>
> 남은 한 번의 ID 조회 비용을 비교한다.
>
> ---
>
> ### 5. ID 조회 구조를 비교한다
>
> **실험**
>
> `CompareSearchMethods()`는 같은 ID들을 Linear Search, Binary Search, `TMap`으로 조회한다.
>
> **이전 실행의 예시**
>
> ```text
> Linear 15.029 ms / 평균 비교 4991.00
> Binary 0.249 ms / 평균 비교 12.39
> TMap 0.018 ms
> ```
>
> **판단**
>
> 안정적인 ID를 저장하는 것과 ID를 빠르게 조회하는 것은 별개의 문제다.
>
> **다음**
>
> Binary Search가 요구하는 정렬이 생성 순서와 충돌하는지 확인한다.
>
> ---
>
> ### 6. Binary Search의 조건을 확인한다
>
> **결과**
>
> ```text
> 생성 순서 [30, 10, 40, 20]
> → EnemyId 정렬 [10, 20, 30, 40]
> ```
>
> **판단**
>
> Binary Search는 정렬된 값의 대소 관계를 근거로 탐색 범위의 절반을 버린다. 정렬되지 않았다면 어느 절반에 찾는 값이 있는지 보장할 수 없다. 따라서 `EnemyId` 기준 정렬이 필요하지만, 실제 상태 배열을 정렬하면 생성 순서가 바뀐다.
>
> **다음**
>
> 주소 대신 Index를 저장하는 후보를 확인한다.
>
> ---
>
> ### 7. Index와 RemoveAtSwap의 한계를 확인한다
>
> **실험 1 — EnemyId → Index**
>
> ```text
> [100, 200, 300, 400] → [100, 300, 400]
> Enemy 300의 저장된 Index=2, 실제 Index=1
> ```
>
> `RemoveAt()`으로 뒤 원소가 이동하면서 Index가 낡았다.
>
> **실험 2 — RemoveAtSwap**
>
> ```text
> [100, 200, 300, 400] → [100, 400, 300]
> 요구한 생성 순서=[100, 300, 400]
> ```
>
> Index 수정 범위는 줄었지만 생성 순서를 깨뜨렸다.
>
> **다음**
>
> 검색과 생성 순서를 같은 컨테이너가 맡는 전제를 바꾼다.
>
> ---
>
> ### 8. 검색과 생성 순서를 분리한다
>
> **실험**
>
> `SeparateSearchFromCreationOrder()`는 상태를 Map에, 생성 순서를 ID 배열에 둔다.
>
> ```text
> Find(20)=20
> CreationOrder=[30, 10, 40, 20]
> ```
>
> **판단**
>
> ID 검색과 생성 순서를 각각 독립적으로 만족시켰다.
>
> **다음**
>
> Order 배열의 삭제 비용이 문제가 되는 workload를 가정한다.
>
> ---
>
> ### 9. 삭제를 미루는 선택을 검증한다
>
> **선택 조건**
>
> ```text
> 적이 적고 삭제가 드물다      → 즉시 RemoveAt
> 대량 전투에서 삭제가 빈번하다 → Tombstone 고려
> ```
>
> `LeaveDeadIdInCreationOrder()`는 Enemy 200을 Map에서만 삭제하고 Order에는 남긴다.
>
> ```text
> CreationOrder=[100, 200, 300, 400]
> 실제 순회=[100, 300, 400]
> Tombstone=1
> ```
>
> **판단**
>
> Tombstone은 필수 구조가 아니라 즉시 삭제 비용이 문제일 때 선택하는 지연 삭제다.
>
> **다음**
>
> 지연된 삭제가 누적됐을 때의 비용을 확인한다.
>
> ---
>
> ### 10. Tombstone 누적과 Compact를 비교한다
>
> `MeasureTraversalBeforeAndAfterCompact()`는 20,000개 중 75%를 Map에서 삭제하고 Compact 전후를 같은 횟수로 순회한다.
>
> ```text
> Alive=5,000 / Tombstone=15,000
> Compact 전 1,000,000 checks
> Compact 후   250,000 checks
> ```
>
> **판단**
>
> Compact 후 실제 처리 결과는 같고 검사 횟수는 4분의 1이 된다. 삭제 비용을 없앤 것이 아니라 안전한 시점의 일괄 정리 비용으로 옮겼다.
>
> ---
>
> ### 현재 요구사항에서 도달한 설계안
>
> | 요구사항 | 담당 구조와 동작 |
> |---|---|
> | 장기 참조 | `EnemyId` 저장 |
> | 현재 상태 접근 | `TMap<int32, FEnemyState>::Find()` 결과를 짧게 사용 |
> | 생성 순서 순회 | `TArray<int32> CreationOrder` |
> | 추가 | Map과 Order에 같은 ID 추가 |
> | 일반적인 삭제 | workload가 작으면 Order에서 즉시 제거 가능 |
> | 삭제 스파이크 완화 | 필요할 때만 Tombstone 사용 |
> | Tombstone 정리 | 안전한 시점이나 임계치에서 Compact |

## 결론

> [!success] 이 문제에서 가져갈 것
> **한 문장으로:** raw pointer는 `어디`를 가리키는지 기억하고 Stable ID는 `누구`인지 기억한다. 주소는 짧게 사용하고 ID를 오래 보관한다.
>
> ```text
> 외부 시스템
> → EnemyId를 장기 저장
> → 필요할 때 TMap에서 현재 상태를 Find
> → 반환된 pointer는 현재 작업에서만 사용
> ```
>
> **Stable ID의 조건:** 살아 있는 서로 다른 Enemy 사이에서 ID가 유일해야 하며, 오래된 참조가 남아 있을 수 있는 동안 같은 ID를 새로운 Enemy에게 함부로 재사용하지 않아야 한다. ID 재사용까지 안전하게 구분해야 한다면 `ID + Generation` 형태의 Handle로 확장할 수 있다.
>
> **검색과 순서:** `TMap<int32, FEnemyState>`는 검색과 상태를, `TArray<int32> CreationOrder`는 생성 순서를 담당한다.
>
> **삭제 정책:** Tombstone은 필수가 아니다. 즉시 `RemoveAt()` 비용이 실제 workload에서 문제일 때만 지연 삭제와 `Compact`를 선택한다.
>
> **후보 해법의 한계:** `TMap<int32, FEnemyState*>`는 같은 오래된 주소를 저장한다. Index는 `RemoveAt()` 뒤 낡고, `RemoveAtSwap()`은 생성 순서를 깨뜨린다.

### 이 문제에서 사용한 일반적인 설계 개념

| 개념 | 이 문제에서의 의미 |
|---|---|
| Stable ID / Handle | 움직일 수 있는 주소 대신 `EnemyId`로 대상을 식별 |
| Indirection | ID로 현재 저장 위치의 객체를 다시 조회 |
| Separation of Concerns | 검색과 생성 순서를 다른 구조가 담당 |
| Secondary Index / Ordered ID List | 상태 저장 구조와 별도로 생성 순서 ID 목록 유지 |
| Lazy / Deferred Deletion | 즉시 삭제 비용이 문제일 때 Tombstone으로 연기 |
| Compaction | 누적된 Tombstone을 안전한 시점에 일괄 정리 |
