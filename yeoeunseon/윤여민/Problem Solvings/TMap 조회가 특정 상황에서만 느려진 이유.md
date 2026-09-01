# TMap 조회가 특정 상황에서만 느려진 이유

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
초반 지역       적 2,000마리 → 문제 없음
필드 지역       적 10,000마리 → 문제 없음
대규모 전투     적 10,000마리 → 심한 CPU 스파이크
```

그런데 프로파일링해보니 CPU 시간을 많이 먹는 곳은 의외로 이것이다.

```cpp
EnemyMap.Find(Key);
```

팀원이 말한다.

> "이상한데? TMap 조회는 평균 O(1)이잖아요.  
> 적이 10,000마리여도 Find 하나가 갑자기 느려질 이유가 있나요?"

더 이상한 사실이 하나 있다.

느린 대규모 전투에서는 10,000마리의 Key가 대략 이렇다.

```text
{ ZoneId=50, EnemyId=1 }
{ ZoneId=50, EnemyId=2 }
{ ZoneId=50, EnemyId=3 }
...
{ ZoneId=50, EnemyId=10000 }
```

반면 정상적인 테스트 데이터는:

```text
{ ZoneId=1, EnemyId=1 }
{ ZoneId=2, EnemyId=2 }
{ ZoneId=3, EnemyId=3 }
...
```

처럼 `ZoneId`도 다양했다.

---

## 풀이 — 생각을 마친 뒤 열기

> [!question]- 1부 — 두 상황의 차이 좁히기
> ### STEP 1 — 첫 번째 단서
>
> **상황**
>
> 필드와 대규모 전투 모두 적은 10,000명이고 같은 `TMap::Find()`를 호출한다. 그런데 대규모 전투에서만 CPU 비용이 급증한다.
>
> ---
>
> **첫 질문**
>
> 적의 수가 원인이라면 같은 수의 두 상황이 왜 다를까?
>
> **다음**
>
> 컨테이너 크기 외에 두 입력 데이터가 어떻게 다른지 비교한다.
>
> ### STEP 2 — 두 상황에서 비교할 것
>
> ---
>
> **핵심 발견**
>
> - 정상 데이터: `ZoneId`가 다양하다.
> - 느린 데이터: 모든 `ZoneId`가 50이다.
> - 현재 Hash: `EnemyId`를 사용하지 않고 `ZoneId`만 반영한다.
>
> **추론**
>
> 느린 전투의 10,000개 Key는 서로 다른 적인데도 같은 Hash에서 출발할 수 있다.
>
> ```text
> 적이 많아서 느린가?
> → 같은 수인데 왜 한쪽만 느린가?
> → 데이터 분포가 다른가?
> → Hash 입력이 그 차이를 구분하는가?
> ```
>
> **주의**
>
> > 평균 O(1)은 Hash가 실제 입력을 Bucket에 적절히 분산하고 Load Factor도 적절하게 관리된다는 전제 위의 평균이다.
>
> **다음**
>
> 같은 Hash에 후보가 몰렸을 때 정확성과 성능이 각각 어떻게 달라지는지 확인한다.

> [!question]- 2부 — 조회 과정에서 원인 확인하기
> ### STEP 3 — 느려져도 결과는 왜 맞을까?
>
> ---
>
> **Hash의 역할**
>
> 먼저 찾아볼 후보 영역을 좁힌다.
>
> **equality의 역할**
>
> `operator==`가 후보 중 실제 Key를 최종 판정한다.
>
> ```text
> Hash 계산
> → 같은 Bucket의 후보 선택
> → operator==로 ZoneId와 EnemyId 비교
> → 일치하는 Enemy 확정
> ```
>
> **결론**
>
> `{50, 1}`과 `{50, 3}`이 같은 Hash를 가져도 `operator==`가 두 필드를 확인하므로 잘못된 적을 반환하지 않는다. Collision은 곧바로 정확성 오류가 아니라 **확인할 후보가 늘어나는 성능 문제**다.
>
> **필수 계약**
>
> ```text
> A == B
> → GetTypeHash(A) == GetTypeHash(B)
>
> GetTypeHash(A) == GetTypeHash(B)
> ↛ A == B
> ```
>
> 기존 Hash도 이 계약은 지킨다. 다만 서로 다른 실제 Key를 지나치게 같은 Hash로 묶어 분포가 나쁘다.
>
> **다음**
>
> Key의 identity를 이루는 두 필드를 모두 Hash에 반영한다.
>
> ### STEP 4 — 원인을 좁힌 뒤 바꿀 것
>
> ---
>
> **아이디어**
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
> **목표**
>
> Collision을 완전히 없애는 것이 아니라 실제 게임 데이터가 몇 개의 Bucket에 과도하게 집중되지 않도록 만든다.
>
> > 유한한 `uint32` Hash에서는 서로 다른 Key가 같은 Hash를 가질 수 있다. `HashCombineFast()`도 Collision이 없다고 보장하지 않는다.
>
> **다음**
>
> 개선 전후의 equality 횟수와 조회 시간을 같은 데이터로 비교한다.

> [!question]- 3부 — 다른 처리 방식도 원인을 해결할까?
> ### STEP 5 — 다른 처리 방식이라면?
>
> **질문**
>
> 충돌 처리 방식을 Chaining에서 Probing으로 바꾸면 Bad Hash 문제가 사라질까?
>
> ---
>
> **구분**
>
> - Separate Chaining은 충돌한 후보를 같은 Bucket의 연결 구조에서 따라간다.
> - Open Addressing은 충돌한 원소를 테이블의 다른 Slot에 저장하고 정해진 규칙으로 Probe한다.
>
> ```text
> Chaining       : 한 Bucket의 후보 연결이 길어진다.
> Open Addressing: 같은 시작 Slot에서 Probe 구간이 길어진다.
> ```
>
> 두 방식은 저장 위치와 메모리 접근 방식은 다르지만, 모두 Collision이 발생한 뒤 처리하는 방법이다.
>
> **결론**
>
> 나쁜 Hash를 그대로 두면 문제가 사라지지 않고 긴 Chain 또는 긴 Probe로 나타난다. 먼저 Hash를 개선한 뒤에도 병목이 남을 때 메모리 지역성, Load Factor, 삭제 정책을 비교한다.
>
> > UE 5.8 공식 문서는 `TSet`을 sparse-array 기반 Hash Container로 설명하고 `TMap`도 `TSet` 계열의 Hashing 구조를 사용한다. 일반적인 충돌 탐색은 개념적으로 Chaining 계열과 연결해 이해할 수 있지만, 정확한 내부 타입과 링크 표현은 엔진 버전·설정·구현을 확인해야 한다.
>
> ### 보충 — 이 비용은 어느 스레드에 잡힐까?
>
> `GetTypeHash()`, `TMap::Find()`, `operator==`는 CPU가 실행하는 일반 C++ 연산이다. Game Thread도 CPU에서 실행되는 여러 스레드 중 하나다.
>
> 현재 Automation Test가 Game Thread에서 `Find()`를 호출하므로 equality 비교와 메모리 접근 비용은 Game Thread의 CPU 시간에 포함된다. Worker Thread에서 실행했다면 비용은 해당 Worker Thread에 기록된다.

## 핵심 결론

> [!success]- 최종 결론 확인하기
> **한 줄 결론**
>
> `EnemyId`를 Hash에서 빠뜨려 같은 Zone의 서로 다른 Key가 하나의 후보군에 몰렸다.
>
> ```text
> ZoneId만 Hash
> → equality 50,005,000회
>
> ZoneId + EnemyId Hash
> → equality 11,808회
> ```
>
> **해결**
>
> Key의 identity를 이루는 `ZoneId`와 `EnemyId`를 함께 Hash하고, 같은 입력으로 다시 측정한다.
>
> **정확성이 유지된 이유**
>
> Hash는 후보를 좁히고 `operator==`가 최종 Key를 판정했기 때문이다.
>
> **남는 조건**
>
> 서로 다른 Hash도 같은 Bucket에 매핑될 수 있고 Load Factor의 영향도 남는다. Chaining과 Probing은 나쁜 Hash 분포를 대신 고치는 방법이 아니다.

## 해설 2 — 테스트 검증

> [!example]- 테스트 전체 보기
> ### 테스트 실행 정보
>
> 제공된 Test Run 3은 이름 변경 전 경로인 `TestProject.ProblemSolving.TMapHashCollision`에서 `Success`로 완료됐다. 현재 테스트 파일은 `TMapSlowFindTest.cpp`, Automation 경로는 `TestProject.ProblemSolving.TMapSlowFind`이다. 검증할 현상은 유지하되, 코드 자체도 관찰 → 원인 분리 → 수정 → 재측정 순서로 읽히도록 재구성했다.
>
> ### 1. 서로 다른 Key가 같은 Hash를 만드는가
>
> **확인**
>
> 테스트는 먼저 다음 두 Key의 Hash를 직접 비교한다.
>
> ```text
> BadHash {50,1}=50 / {50,2}=50
> ```
>
> **결과**
>
> `FBadEnemyKey::GetTypeHash()`가 `ZoneId`만 반환하므로 `EnemyId`가 달라도 Hash는 둘 다 50이다. `TestEqual()`도 이 관계를 검증한다.
>
> **해석**
>
> “같을 것 같다”는 추측을 실제 Hash 값으로 확정했다.
>
> ### 2. 개수는 고정하고 ZoneId 분포만 바꾼다
>
> **확인**
>
> `RunBadHashDifferentZoneExperiment()`과 `RunBadHashSameZoneExperiment()`은 적의 수를 모두 10,000명으로 고정한다. Hash 함수도 동일하게 나쁜 함수를 사용하고, 오직 `ZoneId` 분포만 바꾼다.
>
> ```text
> Zone 다양함 : 0.010 ms / equality=11,808
> Zone 전부 50 : 97.623 ms / equality=50,005,000
> ```
>
> **결과**
>
> ```text
> 조회 1회당 평균 equality
>
> Zone 다양함 : 약 1.18회
> Zone 전부 50: 약 5,000.5회
> ```
>
> 적의 수와 조회 횟수는 같았다. equality 차이는 데이터가 같은 Hash에 집중되면서 생겼다.
>
> **측정 해석**
>
> > 시간값에는 실행 환경과 equality 카운터의 계측 비용이 반영된다. 실제 게임의 성능 배율이 아니라 `11,808 → 50,005,000`이라는 비교 횟수 증가를 핵심 근거로 본다.
>
> 두 실험의 checksum도 같으므로 비교 대상이 동일한 검색 결과를 냈다.
>
> ### 3. Collision인데도 정확한 Enemy를 찾는가
>
> **확인**
>
> `RunEqualityExperiment()`은 같은 Hash를 갖는 `{50,1}`부터 `{50,4}`까지 넣은 뒤 `{50,3}`을 찾는다.
>
> ```text
> Find({50,3}) -> "Enemy 3" / equality 비교=2
> ```
>
> **결과**
>
> 후보를 두 번 비교했지만 최종 결과는 정확히 `Enemy 3`이었다.
>
> **해석**
>
> ```text
> Hash Collision → 후보 증가 → 비교 비용 증가
> operator==     → 최종 Key 판정 → 결과 정확성 유지
> ```
>
> ### 4. 같은 데이터에서 Hash 입력만 개선한다
>
> **확인**
>
> `RunGoodHashSameZoneExperiment()`은 모든 `ZoneId`가 50인 조건을 유지하고 `HashCombineFast()`로 `EnemyId`까지 반영한다.
>
> ```text
> BadHash 같은 Zone  : 97.623 ms / equality=50,005,000
> GoodHash 같은 Zone :  0.013 ms / equality=11,808
>
> GoodHash {50,1}=2,654,439,028
> GoodHash {50,2}=2,654,439,029
> ```
>
> **결과**
>
> 적의 수와 Key는 그대로인데 Hash만 개선하자 equality 비교가 약 4,235분의 1로 줄었다. `SameZone.Checksum == GoodHash.Checksum`도 참이므로 검색 결과는 동일하다.
>
> **주의**
>
> > 측정 시간에는 계측 비용이 포함되므로 실제 게임에서 같은 향상 배율을 보장하지 않는다.
>
> 서로 다른 Hash도 제한된 Bucket 수 때문에 같은 Bucket으로 매핑될 수 있다. GoodHash의 equality가 정확히 10,000회가 아니라 11,808회인 이유도 여기에 있다.
>
> **검증된 범위**
>
> 기존에 무조건 같은 Hash가 되던 Key들이 분리되었고 전체 equality 횟수가 실제로 크게 감소했다.
>
> ### 5. 측정된 비용이 어느 스레드에 잡히는가
>
> **확인**
>
> ```text
> Game Thread에서 실행 중 = YES
> ```
>
> **결과**
>
> 현재 테스트의 Hash 계산과 5천만 번의 equality 비교 비용은 Game Thread의 CPU 시간에 포함된다.
>
> **영향**
>
> 실제 게임에서도 AI나 UI가 Game Thread에서 같은 조회를 반복한다면 프레임 시간에 직접 영향을 줄 수 있다.
>
> ### 6. Probing으로 바꾸면 Bad Hash가 해결되는가
>
> **확인**
>
> `RunBadLinearProbingExperiment()`과 `RunGoodLinearProbingExperiment()`은 Unreal `TMap` 구현이 아니라 Linear Probing의 개념만 재현한다. 8개 원소가 같은 시작 Slot을 갖는 경우와 서로 다른 시작 Slot에 분산되는 경우를 비교한다.
>
> ```text
> 같은 시작 Slot : total probes=36 / max probe=8
> 잘 분산됨      : total probes=8  / max probe=1
> ```
>
> **결과**
>
> 같은 Slot에서 시작하면 첫 원소는 1회, 다음 원소들은 2회부터 8회까지 Probe하여 총 36회가 된다. 잘 분산되면 모든 원소가 한 번에 자리를 찾아 총 8회로 끝난다.
>
> **결론**
>
> > 이 실험은 어느 충돌 방식이 항상 더 빠른지 비교하지 않는다. Chaining과 Probing은 Collision 처리 방식이며, 나쁜 Hash 분포 자체를 해결하지 않는다는 점을 확인한다.
>
> ### 코드와 로그가 연결한 최종 인과관계
>
> | 관찰 | 코드·로그에서 확인한 원인 |
> |---|---|
> | 같은 10,000명인데 특정 Zone만 느림 | 해당 Zone의 모든 `ZoneId`가 50 |
> | 서로 다른 Enemy가 같은 후보군에 집중 | BadHash가 `ZoneId`만 사용하여 모두 Hash 50 생성 |
> | 조회 시간이 급증 | equality가 11,808회에서 50,005,000회로 증가 |
> | Collision인데 결과는 정확 | `operator==`가 `ZoneId + EnemyId`를 최종 비교 |
> | Hash 개선 후 다시 빨라짐 | `HashCombineFast()` 적용 후 equality가 11,808회로 감소 |
> | Linear Probing 삽입에서도 Bad Hash가 탐색을 늘림 | 같은 시작 Slot에서 최대 8회 연속 Probe |
>
> 최종 흐름은 다음과 같다.
>
> ```text
> 특정 Zone에서만 Find가 느림
> → 적의 수가 아니라 ZoneId 분포 차이를 발견
> → GetTypeHash가 ZoneId만 사용
> → 서로 다른 10,000개 Key가 같은 Hash에 집중
> → equality 비교와 메모리 접근 증가
> → ZoneId와 EnemyId를 함께 Hash하여 분포 개선
> → 충돌 전략 변경은 Hash 품질을 고친 뒤 검토할 별도 선택
> ```
>
> **보충**
>
> 이 테스트는 Game Thread에서 실행됐으므로 측정된 조회 비용도 Game Thread의 CPU 시간에 포함된다.

## 복습

> [!abstract]- 30초 복습
> **한 문장으로:** Collision은 비교 비용을 늘렸고, `operator==`는 결과의 정확성을 지켰다.
>
> **원인:** `ZoneId`만 Hash해서 같은 Zone의 서로 다른 Key가 같은 후보군에 집중됐다.
>
> **해결:** `ZoneId + EnemyId`를 함께 Hash해 실제 입력을 더 고르게 분산시켰다.
>
> **숫자로 확인:** equality는 `11,808 → 50,005,000 → 11,808`로 변했다.
>
> **남는 점:** Chaining과 Probing은 Collision 처리 방식일 뿐, 나쁜 Hash 분포 자체를 고치지는 않는다.
