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
> ### STEP 1 — 포인터를 캐시하면 검색을 없앨 수 있을까?
>
> **`TArray` 원소 주소를 오래 저장하면 재할당 뒤 주소가 낡는다. 그 포인터는 null이 아닐 수도 있다.**
>
> 처음에는 `FindEnemy()` 결과를 `CachedTarget`에 저장하면 반복 검색을 없앨 수 있다고 생각했다. 하지만 `TArray::Add()` 중 Capacity가 부족하면 원소가 더 큰 메모리로 이동한다.
>
> ```text
> 재할당 전 Enemy 100 = 0x1000
> CachedTarget         = 0x1000
>
> TArray 재할당
>
> 현재 Enemy 100      = 0x5000
> CachedTarget         = 0x1000
> ```
>
> `CachedTarget != nullptr`은 주소 값이 0이 아니라는 뜻일 뿐, 지금도 같은 Enemy를 가리킨다는 뜻이 아니다. 테스트도 낡은 포인터를 역참조하지 않고 저장된 주소와 현재 주소만 비교했다.
>
> `TMap<int32, FEnemyState*>`에 넣어도 Value가 같은 배열 원소 주소라면 달라지지 않는다. 저장 장소가 아니라 포인터가 가리키는 대상의 lifetime이 문제다.
>
> ---
>
> ### STEP 2 — 주소 대신 무엇을 저장할까?
>
> **주소 대신 Stable `EnemyId`를 오래 저장하고 현재 객체를 다시 찾는다.**
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
> ```text
> ID는 오래 저장
> ↓
> 필요할 때 Find
> ↓
> 반환된 pointer는 짧게 사용
> ```
>
> `Find()`가 `nullptr`이면 현재 그 ID의 Enemy가 없다고 판단할 수 있다. 반환된 포인터는 컨테이너가 바뀌지 않는 현재 작업 범위에서만 쓴다.
>
> Stable ID로 쓰려면 살아 있는 Enemy 사이에서 유일해야 하며, 오래된 참조가 남아 있을 때 같은 ID를 새 Enemy에게 바로 재사용하지 않는 정책도 필요하다.

> [!question]- 2부 — Stable ID를 빠르게 다시 찾기
> ### STEP 3 — 재조회 비용을 어떻게 줄일까?
>
> **같은 범위의 중복 `Find`부터 없애고, 남은 한 번의 조회에 맞는 구조를 비교한다.**
>
> ```cpp
> if (FEnemyState* Enemy = EnemyManager.FindEnemy(EnemyId))
> {
>     float HP = Enemy->HP;
> }
> ```
>
> 이 방식은 장기 캐시가 아니라 방금 찾은 결과를 즉시 재사용하는 것이다. 실제 테스트에서는 비교 횟수가 절반으로 줄었다.
>
> ```text
> Linear Search → 정렬 불필요, 조회할 때 앞에서부터 비교
> Binary Search → 빠르지만 EnemyId 기준 정렬 필요
> TMap          → EnemyId로 직접 조회하기 적합
> ```
>
> Binary Search는 가운데 값을 기준으로 절반을 버리므로 `EnemyId` 정렬이 필요하다. 실제 배열을 정렬하면 생성 순서가 바뀐다.
>
> ```text
> 생성 순서 [30, 10, 40, 20]
> EnemyId 정렬 [10, 20, 30, 40]
> ```
>
> 생성 순서를 건드리지 않고 ID로 현재 상태를 찾는 용도에는 `TMap<int32, FEnemyState>`이 더 잘 맞는다.
>
> ---
>
> ### STEP 4 — 주소 대신 Index를 저장하면?
>
> **Index는 `RemoveAt()` 뒤 낡고, `RemoveAtSwap()`은 생성 순서를 깨뜨린다.**
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
> “검색과 생성 순서를 왜 하나의 컨테이너가 모두 책임지고 있지?”

> [!question]- 3부 — 검색·순서·삭제의 역할 나누기
> ### STEP 5 — 검색과 생성 순서를 분리한다
>
> **검색과 생성 순서를 한 컨테이너에 맡기지 않고 책임을 분리한다.**
>
> ```text
> TMap<int32, FEnemyState>    → EnemyId 검색과 살아 있는 상태
> TArray<int32> CreationOrder → 생성 순서
> 외부 시스템                 → Stable EnemyId 보관
> ```
>
> `CreationOrder`에는 주소나 상태를 중복 저장하지 않고 ID만 둔다. 순회할 때 ID로 Map의 현재 상태를 조회한다.
>
> ---
>
> ### STEP 6 — 삭제 비용은 언제 지불할까?
>
> **삭제가 드물면 즉시 제거하고, 삭제 시점의 이동 비용이 문제일 때만 Tombstone과 Compact를 쓴다.**
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
> Tombstone은 즉시 삭제 비용을 미루는 대신 죽은 ID까지 검사하게 만든다. 충분히 쌓이면 전투·웨이브 종료 같은 시점이나 정해둔 임계치에서 살아 있는 ID만 남기도록 Compact한다.
>
> ```text
> 즉시 RemoveAt 비용
> ↓ Tombstone
> 삭제 시점에는 미룸
> ↓ Compact
> 안전한 시점에 한꺼번에 지불
> ```
>
> 비용을 없앤 것이 아니라 지불 시점을 옮긴 것이다. 삭제 비용이 실제 문제가 아니라면 Tombstone까지 도입할 이유는 없다.

## 테스트 — 코드로 확인한 것

> [!example]- 테스트 전체 보기
> - 코드: `EnemyManagerSearchProblemTest.cpp`
> - Automation: `TestProject.ProblemSolving.EnemyManagerSearchProblem`
> - 수치: `TestProject-backup-2026.09.01-13.10.23.log`
>
> ### 1. CachedTarget과 raw pointer
>
> `Reserve(1)` 뒤 Enemy를 추가해 재할당을 유도했다. 낡은 포인터는 역참조하지 않고 주소만 비교했다.
>
> ```text
> Capacity 1 → 18 / 1개 추가 후 주소 변경
> CachedTarget != nullptr      = YES
> CachedTarget == CurrentEnemy = NO
>
> TMap<int32, FEnemyState*>에 저장한 경우
> TArray 주소 변경               = YES
> Map 포인터가 현재 원소를 가리킴 = NO
> ```
>
> ---
>
> ### 2. Stable ID와 중복 Find
>
> ```text
> 저장한 ID                   = 200
> Map 성장 후 찾은 EnemyId     = 200
> Enemy 200 삭제 후 Find(200) = nullptr
>
> 중복 Find 1,999,800 comparisons
> 즉시 재사용 999,900 comparisons
> ```
>
> ID는 오래 보관했고, 조회로 얻은 포인터는 같은 작업 범위에서만 재사용했다.
>
> ---
>
> ### 3. 조회 방법과 Binary Search 조건
>
> 같은 10,000개 Enemy에서 10,000번 조회한 결과다.
>
> ```text
> Linear 14.972 ms / 평균 비교 4,991.00
> Binary 0.253 ms / 평균 비교 12.39
> TMap 0.017 ms
>
> 생성 순서 [30, 10, 40, 20]
> EnemyId 정렬 [10, 20, 30, 40]
> ```
>
> 시간값은 실행 환경에 따라 달라진다. 비교 횟수와 정렬 뒤 순서 변화가 구조 선택의 근거다.
>
> ---
>
> ### 4. Index, RemoveAtSwap, 역할 분리
>
> ```text
> RemoveAt
> [100, 200, 300, 400] → [100, 300, 400]
> Enemy 300의 저장된 Index=2, 실제 Index=1
>
> RemoveAtSwap
> [100, 200, 300, 400] → [100, 400, 300]
> 필요한 생성 순서=[100, 300, 400]
>
> 역할 분리
> Find(20)=20
> CreationOrder=[30, 10, 40, 20]
> ```
>
> ---
>
> ### 5. Tombstone과 Compact
>
> Map에서 Enemy 200만 삭제하고 Order 순회에서는 Map에 없는 ID를 건너뛰었다.
>
> ```text
> CreationOrder=[100, 200, 300, 400]
> 실제 순회=[100, 300, 400]
> Tombstone=1
>
> 20,000개 중 생존=5,000 / Tombstone=15,000
> Compact 전 1,000,000 checks / 0.638 ms
> Compact 후   250,000 checks / 0.269 ms
> Compact 자체 0.015 ms
> ```
>
> Compact 전후 checksum은 같았다. 이 실험은 Tombstone이 항상 더 낫다는 뜻이 아니라, 지연 삭제를 선택했을 때 생기는 순회 비용과 정리 효과를 보여준다.

## 결론

> [!success] 이 문제에서 가져갈 것
>
> **움직일 수 있는 주소를 장기 참조로 쓰지 않는다. `EnemyId`로 현재 상태를 다시 찾고, 검색과 생성 순서는 다른 구조에 맡긴다.**

### 이번 문제의 흐름

```text
반복 Find
→ pointer를 캐시하면?
→ TArray 재할당 뒤 주소가 낡음
→ null 체크로도 잡히지 않음

TMap에 같은 pointer를 저장하면?
→ 낡은 주소를 다른 곳에 복사했을 뿐

주소 대신 EnemyId를 저장
→ 중복 Find를 먼저 제거
→ 남은 조회 비용을 줄일 구조 비교

Binary Search
→ 정렬이 필요해 생성 순서와 충돌

EnemyId → Index
→ RemoveAt 뒤 Index가 낡음

RemoveAtSwap
→ 생성 순서를 깨뜨림

검색과 순서를 분리
→ TMap + CreationOrder

삭제 비용이 실제 문제라면
→ Tombstone으로 미루고 안전한 시점에 Compact
```

### 확인한 것

- `TArray`가 성장하자 첫 원소의 주소가 바뀌었다. 기존 pointer는 `nullptr`이 아니었지만 현재 원소 주소와 달랐다.
- 같은 pointer를 `TMap`에 저장해도 재할당 뒤에는 똑같이 낡았다. 테스트에서는 이 pointer를 역참조하지 않고 주소만 비교했다.
- `EnemyId`는 배열과 Map이 성장해도 같은 Enemy를 다시 찾았고, 삭제 뒤에는 `Find()`가 `nullptr`을 반환했다.
- 같은 범위의 중복 Find를 즉시 재사용하자 비교 횟수가 `1,999,800 → 999,900`으로 줄었다.
- Binary Search와 `TMap`은 선형 검색보다 짧았지만, 실제 배열을 정렬하면 생성 순서가 바뀌었다.
- 저장한 Index는 `RemoveAt()` 뒤 낡았고, `RemoveAtSwap()`은 생성 순서를 바꿨다.
- `TMap`과 `CreationOrder`를 나누자 빠른 ID 검색과 생성 순서를 함께 유지할 수 있었다.
- Tombstone을 Compact하자 순회 확인 횟수가 `1,000,000 → 250,000`으로 줄었고 결과 checksum은 같았다.

시간값은 실행 환경에 따라 달라진다. 또한 이 테스트는 Tombstone이 언제나 더 낫다는 뜻이 아니라, 삭제 비용을 미뤘을 때 생기는 비용과 Compact 효과를 확인한 것이다.

### 현재 요구사항에서의 설계

```text
외부 시스템
→ EnemyId를 오래 보관

EnemyId
→ TMap에서 현재 EnemyState를 다시 찾음
→ 반환된 pointer는 짧게 사용

TMap
→ 살아 있는 Enemy의 상태와 ID 검색

CreationOrder
→ 생성 순서만 보관

삭제가 드물고 단순함
→ Order에서도 즉시 제거

전투 중 삭제 비용이 실제 병목임
→ 죽은 ID를 남기고 Map에 없으면 건너뜀
→ Tombstone이 쌓이면 안전한 시점에 Compact
```

`EnemyId`를 Stable ID로 쓰려면 살아 있는 Enemy 사이에서 유일해야 한다. 오래된 참조가 남아 있는 동안 같은 ID를 다른 Enemy에게 재사용할 수 있다면 `ID + Generation` 형태의 Handle이 필요하다.

> [!note]- 부록 — 이 문제에서 나온 설계 개념
> | 개념 | 이 문제에서의 의미 |
> |---|---|
> | Stable ID | 움직일 수 있는 주소 대신 `EnemyId`로 대상을 식별한다. |
> | Handle | ID 재사용까지 구분해야 할 때 `ID + Generation`으로 참조를 강화한다. |
> | Indirection | 주소를 오래 들고 있지 않고 ID로 현재 객체를 다시 찾는다. |
> | Separation of Concerns | 검색·상태와 생성 순서를 다른 구조가 담당한다. |
> | Ordered ID List | 실제 상태 저장소와 별도의 ID 목록으로 순서를 보존한다. |
> | Deferred Deletion | 즉시 삭제 비용이 문제일 때 죽은 ID를 남겨 비용을 미룬다. |
> | Compaction | 누적된 Tombstone을 한꺼번에 제거한다. |
