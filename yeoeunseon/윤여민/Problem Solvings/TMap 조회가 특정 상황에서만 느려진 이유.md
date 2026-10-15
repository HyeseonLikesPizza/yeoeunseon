## 문제 상황

네가 Unreal 게임의 AI 시스템을 맡고 있다.

적을 빠르게 찾기 위해 다음 자료구조를 사용했다.

```cpp
struct FEnemyKey
{
    int32 ZoneId;
    int32 EnemyId;

    bool operator==(const FEnemyKey& Other) const
    {
        return ZoneId == Other.ZoneId
            && EnemyId == Other.EnemyId;
    }
};

FORCEINLINE uint32 GetTypeHash(const FEnemyKey& Key)
{
    return GetTypeHash(Key.ZoneId);
}

TMap<FEnemyKey, FEnemyData> EnemyMap;
```

QA에서 이상한 현상이 보고됐다.

```text
초반 지역       적 2,000마리  → 문제 없음
필드 지역       적 10,000마리 → 문제 없음
대규모 전투     적 10,000마리 → 심한 CPU 스파이크
```

프로파일링에서 비용이 크게 잡힌 곳은 다음 조회였다.

```cpp
EnemyMap.Find(Key);
```

팀원이 말한다.

> "`TMap` 조회는 평균 `O(1)`인데, 적이 10,000마리라는 이유만으로 `Find()`가 갑자기 느려질 수 있나요?"

느린 대규모 전투의 Key는 대략 이렇다.

```text
{ ZoneId=50, EnemyId=1 }
{ ZoneId=50, EnemyId=2 }
{ ZoneId=50, EnemyId=3 }
...
{ ZoneId=50, EnemyId=10000 }
```

반면 정상적인 테스트 데이터는 `ZoneId`도 다양했다.

```text
{ ZoneId=1, EnemyId=1 }
{ ZoneId=2, EnemyId=2 }
{ ZoneId=3, EnemyId=3 }
...
```

---

## 풀이 — 생각을 마친 뒤 열기

> [!question]- 1부 — 무엇이 다른가
> ### STEP 1 — 적이 많아서 느린 걸까?
>
> **두 상황 모두 10,000명이다. 개수만으로는 한쪽에서만 생긴 비용을 설명할 수 없다.**
>
> 같은 `TMap::Find()`를 호출하는데 결과가 다르다면 먼저 입력의 차이를 찾는다.
>
> ```text
> 필드 전투      → ZoneId가 다양함
> 대규모 전투    → ZoneId가 모두 50
> 현재 Hash      → ZoneId만 사용
> ```
>
> 느린 전투에서는 서로 다른 Enemy 10,000명이 모두 같은 Hash에서 출발한다.
>
> 평균 `O(1)`은 모든 입력에서 조회 횟수가 항상 한 번이라는 뜻이 아니다. 실제 Key가 적절히 분산되고 테이블 상태도 적절할 때 기대할 수 있는 평균적인 특성이다.
>
> ---
>
> ### STEP 2 — 같은 Hash인데 왜 결과는 맞을까?
>
> **Hash는 후보를 좁히고, `operator==`는 후보 중 실제 Key를 판정한다.**
>
> ```text
> Hash 계산
> → 확인할 후보를 찾음
> → operator==로 ZoneId와 EnemyId 비교
> → 일치하는 Enemy 확정
> ```
>
> `{50,1}`과 `{50,3}`의 Hash가 같아도 `operator==`는 두 필드를 모두 비교한다. 따라서 Collision이 곧바로 오답을 만드는 것은 아니다. 대신 같은 후보에 몰린 Key를 더 많이 비교하게 된다.
>
> Hash와 equality가 지켜야 할 계약은 한 방향이다.
>
> ```text
> A == B
> → Hash(A) == Hash(B)
>
> Hash(A) == Hash(B)
> ↛ A == B
> ```
>
> 기존 Hash도 이 계약은 지킨다. 문제는 계약 위반이 아니라 이 데이터에서의 나쁜 분포다.

> [!question]- 2부 — 원인을 바꾸고 다시 확인하기
> ### STEP 3 — 무엇만 바꿔볼까?
>
> **Enemy 수와 Key는 그대로 두고 Hash 입력만 바꾼다.**
>
> 이 Key의 동일성은 `ZoneId + EnemyId`로 결정된다. 현재 데이터에서는 `EnemyId`를 Hash에 반영해야 같은 Zone의 적도 더 잘 분산된다.
>
> ```cpp
> FORCEINLINE uint32 GetTypeHash(const FEnemyKey& Key)
> {
>     return HashCombineFast(
>         ::GetTypeHash(Key.ZoneId),
>         ::GetTypeHash(Key.EnemyId));
> }
> ```
>
> `HashCombineFast()`가 Collision을 없애주는 것은 아니다. 유한한 Hash와 Bucket을 사용하는 이상 서로 다른 Key가 겹칠 수 있다. 여기서는 실제 입력이 하나의 Hash에 대량으로 몰리는 현상을 줄이는 것이 목적이다.
>
> 수정 전후를 같은 데이터로 조회해 equality 횟수가 함께 줄어드는지 확인한다.
>
> ---
>
> ### STEP 4 — Probing을 쓰면 해결될까?
>
> **Chaining과 Probing은 Collision이 생긴 뒤 처리하는 방식이다. 나쁜 Hash 분포 자체를 고치지는 않는다.**
>
> ```text
> Chaining
> → 같은 후보 연결을 더 오래 따라감
>
> Open Addressing / Probing
> → 같은 시작점에서 더 멀리 탐색함
> ```
>
> 둘의 저장 방식과 메모리 접근 특성은 다르다. 하지만 같은 Hash에 대량으로 몰리는 원인을 그대로 두면 긴 Chain이나 긴 Probe로 비용이 나타난다.
>
> 이번 테스트의 Probing 코드는 이 원리만 확인한 단순 모델이다. Unreal `TMap`의 실제 구현과 Chaining·Probing의 전체 성능을 비교한 실험은 아니다.

## 테스트 — Hash 분포와 조회 비용

> [!example]- 테스트 전체 보기
> 테스트 코드는 `TMapSlowFindTest.cpp`, Automation 경로는 `TestProject.ProblemSolving.TMapSlowFind`다.
>
> 아래 수치는 `TestProject-backup-2026.09.01-13.10.23.log`의 `Success` 실행에서 가져왔다.
>
> ### 1. 같은 Hash와 입력 분포
>
> 서로 다른 두 Key가 실제로 같은 Hash를 만들었다.
>
> ```text
> BadHash {50,1}=50
> BadHash {50,2}=50
> ```
>
> 다음 두 실험은 Enemy 10,000명, 조회 횟수, Bad Hash를 같게 두고 `ZoneId` 분포만 바꿨다.
>
> ```text
> Zone 다양함 :   0.011 ms / equality=11,808
> Zone 전부 50: 100.162 ms / equality=50,005,000
>
> 조회 1회당 평균 equality
> Zone 다양함 : 약 1.18회
> Zone 전부 50: 약 5,000.5회
> ```
>
> 개수는 같았지만 같은 Hash에 몰린 입력에서 equality가 급증했다. 시간은 실행 환경과 equality 계측 비용에 따라 흔들리므로, 이 실험에서는 비교 횟수가 더 직접적인 증거다.
>
> ---
>
> ### 2. 정확성과 Hash 수정
>
> 같은 Hash를 갖는 `{50,1}`부터 `{50,4}`까지 넣은 뒤 `{50,3}`을 찾았다.
>
> ```text
> Find({50,3}) → "Enemy 3"
> equality 비교=2
> ```
>
> 후보 비교가 생겨도 `operator==`가 정확한 Enemy를 골랐다.
>
> 모든 `ZoneId`가 50인 데이터를 유지한 채 `EnemyId`까지 Hash에 반영했다.
>
> ```text
> Bad Hash  : 100.162 ms / equality=50,005,000
> Good Hash :   0.012 ms / equality=11,808
>
> GoodHash {50,1}=2,654,439,028
> GoodHash {50,2}=2,654,439,029
> ```
>
> 적의 수와 조회할 Key는 같았다. Hash 입력만 바꾸자 equality가 약 4,235분의 1로 줄었고 checksum은 같았다.
>
> Good Hash의 equality가 정확히 10,000회는 아니다. 서로 다른 Hash도 제한된 Bucket 수에서는 같은 후보 영역에 들어갈 수 있다.
>
> ---
>
> ### 3. 실행 스레드와 Probing 개념 실험
>
> Automation은 Game Thread에서 실행됐다.
>
> ```text
> Game Thread에서 실행 중=YES
> ```
>
> 이 실행에서 `GetTypeHash()`, `TMap::Find()`, `operator==`의 비용은 Game Thread CPU 시간에 포함된다. 다른 스레드에서 호출하면 그 스레드의 비용이 된다.
>
> 별도의 단순 Linear Probing 실험에서는 시작 Slot만 다르게 했다.
>
> ```text
> 같은 시작 Slot : total probes=36 / max probe=8
> 분산된 시작 Slot: total probes=8  / max probe=1
> ```
>
> 이 결과는 같은 시작점에 몰리면 Probing도 길어진다는 것만 보여준다. 실제 `TMap`의 구현이나 Chaining과 Probing 중 어느 쪽이 더 빠른지는 이 수치로 판단할 수 없다.

## 결론

> [!success] 이 문제에서 가져갈 것
>
> **같은 코드가 특정 입력에서만 느리다면 데이터의 개수뿐 아니라 분포를 비교한다. 평균 복잡도가 성립하는 조건도 실제 입력에서 다시 확인한다.**

### 이번 문제의 흐름

```text
같은 10,000명인데 한쪽만 느림
→ 개수 외의 차이를 찾음
→ 느린 전투는 ZoneId가 모두 50
→ Hash는 ZoneId만 사용
→ 서로 다른 Enemy가 같은 Hash에 집중
→ equality 비교 폭증
→ ZoneId + EnemyId를 Hash
→ 같은 데이터에서 equality 감소

결과는 왜 정확했나?
→ Hash는 후보 선택
→ operator==는 최종 Key 판정

Probing으로 바꾸면?
→ Collision 처리 방식만 달라짐
→ Bad Hash 문제는 남음
```

### 확인한 것

- Bad Hash에서는 `{50,1}`과 `{50,2}`가 모두 Hash 50을 만들었다.
- Enemy 수를 고정하고 `ZoneId` 분포만 바꾸자 equality가 `11,808 → 50,005,000`으로 늘었다.
- Collision이 있어도 `operator==`가 정확한 Enemy를 찾았다.
- 같은 Key에서 `EnemyId`까지 Hash하자 equality가 다시 11,808회로 줄었고 checksum은 같았다.
- 단순 Probing 모델도 같은 시작 Slot에 몰릴 때 Probe가 길어졌다.

이번 결과는 `HashCombineFast()`가 항상 정답이거나 Collision을 없앤다는 뜻이 아니다. Key의 실제 분포를 보고 Hash 함수를 설계해야 한다는 뜻이다. 필수 계약은 `A == B`이면 같은 Hash를 반환하는 것이며, equality에 쓰는 모든 필드를 반드시 Hash에 넣어야 한다는 규칙은 없다.

### 현재 코드에서 고칠 것

```cpp
FORCEINLINE uint32 GetTypeHash(const FEnemyKey& Key)
{
    return HashCombineFast(
        ::GetTypeHash(Key.ZoneId),
        ::GetTypeHash(Key.EnemyId));
}
```

수정 뒤에는 실제 게임과 가까운 Key 분포로 다시 측정한다. 여전히 비용이 크다면 그때 Load Factor, 메모리 접근, 조회 횟수와 호출 위치를 차례로 확인한다.

> [!note]- 부록 — 이 문제에서 나온 개념
> | 개념 | 이 문제에서의 의미 |
> |---|---|
> | Hash / Equality Contract | `A == B`라면 같은 Hash를 반환해야 한다. 반대는 성립하지 않는다. |
> | Hash Distribution | 실제 Key가 일부 Hash나 후보 영역에 과도하게 몰리지 않는지 본다. |
> | Collision | 서로 다른 Key가 같은 Hash나 후보 영역에 겹치는 현상이다. 과도하면 비교 비용이 늘어난다. |
> | Average-case Complexity | 평균 `O(1)`은 좋은 분포와 적절한 테이블 상태 등을 전제로 한다. |
> | Key Identity | `operator==`가 어떤 필드로 같은 Key를 판정하는지 정의한다. |
> | Collision Resolution | Chaining과 Probing은 Collision 이후 후보를 처리하는 방식이다. |
> | Controlled Experiment | Enemy 수와 Hash는 고정하고 `ZoneId` 분포만 바꾸어 원인을 분리했다. |
