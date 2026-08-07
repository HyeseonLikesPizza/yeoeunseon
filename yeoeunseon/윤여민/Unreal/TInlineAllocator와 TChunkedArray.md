---
tags: [unreal-engine, cpp, memory, allocator, tarray, performance]
created: 2026-08-08
updated: 2026-08-08
---

# TInlineAllocator와 TChunkedArray

> [!summary]
> **`TInlineAllocator<N>`은 대부분 작은 배열의 외부 할당을 줄이고, `TChunkedArray`는 매우 큰 배열이 성장할 때 거대한 연속 메모리와 전체 재배치를 피한다.**
>
> 둘은 기본 `TArray`보다 무조건 빠른 상위 컨테이너가 아니라, 서로 다른 메모리 문제를 해결하는 도구다.

## 핵심 정리

```text
Reserve(N)
→ 컨테이너 밖에 N칸을 미리 마련

TInlineAllocator<N>
→ 컨테이너 몸체 안에 N칸을 내장

TChunkedArray
→ Chunk 관리 테이블 + 서로 떨어질 수 있는 Chunk들

크기를 예상할 수 있는 일반 배열
→ TArray + Reserve

대부분 N개 이하인 작고 자주 만들어지는 배열
→ TArray<T, TInlineAllocator<N>> 검토

매우 크고 계속 성장하며 큰 연속 할당이 부담인 배열
→ TChunkedArray<T> 검토
```

| 용어 | 의미 |
| --- | --- |
| Malloc | 동적으로 사용할 메모리 공간을 할당자에게 요청하는 것 |
| `FMemory` | Unreal이 제공하는 저수준 메모리 함수 창구 |
| `TInlineAllocator<N>` | 컨테이너 객체 안에 N개 원소용 저장 공간을 포함하는 정책 |
| `TChunkedArray` | 청크를 관리하는 컬렉션을 두고 데이터를 여러 연속 청크에 나눠 저장하는 컨테이너 |

---

## Malloc과 FMemory

### Malloc이란

`malloc`은 간단히 말해 **실행 중 필요한 크기의 메모리를 동적으로 요청하는 함수**다.

```cpp
void* Memory = malloc(1024);
free(Memory);
```

게임 코드에서는 보통 직접 `malloc`과 `free`를 호출하기보다 `TArray` 같은 컨테이너가 저장 공간을 관리하게 한다.

### Stack과 Heap

```text
Stack
→ 함수 호출과 지역 변수에 주로 사용
→ 함수가 끝나면 자동으로 정리

Heap
→ 실행 중 크기와 수명이 달라지는 동적 메모리에 사용
→ 할당자가 공간을 찾고 할당·반환 상태를 관리
```

힙 할당은 사용할 빈 공간을 찾고 관리 정보를 갱신해야 하므로, 작은 할당을 매우 자주 반복하면 비용이 될 수 있다.

### Unreal의 FMemory

`FMemory`는 Unreal Core가 제공하는 저수준 메모리 인터페이스다.

```cpp
void* Memory = FMemory::Malloc(1024);
Memory = FMemory::Realloc(Memory, 2048);
FMemory::Free(Memory);
```

```text
게임 코드
→ FMemory::Malloc / Realloc / Free
→ 현재 선택된 전역 FMalloc 구현
→ 실제 메모리 관리
```

`FMemory` 자체가 하나의 고정된 힙 알고리즘이라는 뜻은 아니다. Unreal의 현재 메모리 할당자에 요청을 전달하는 공통 창구라고 이해하면 된다.

실제로 연결되는 `FMalloc` 구현은 플랫폼과 빌드 설정에 따라 달라질 수 있다. `Binned2`나 `Binned3` 같은 특정 구현을 모든 UE5 프로젝트의 고정 규칙으로 외우지 않는다.

> [!note]
> 생성자와 소멸자가 필요한 C++ 객체에 저수준 `Malloc`을 바로 사용하는 것은 별도의 생명주기 처리가 필요하다. 일반 게임 코드에서는 컨테이너와 RAII 타입을 먼저 사용한다.

---

## TArray는 언제 재할당할까

`TArray`는 원소를 하나의 연속된 메모리에 저장한다.

```text
[A][B][C][D][E]
```

연속 배치는 순회와 인덱스 접근에 유리하지만, 현재 저장 공간이 가득 찬 뒤 원소를 더 추가하려면 더 큰 연속 공간을 확보하고 기존 원소를 옮길 수 있다.

```text
기존 공간 [A][B][C][D]
             ↓ 용량 부족
새 공간   [A][B][C][D][ ][ ][ ][ ]
```

### Add할 때마다 재할당하지는 않는다

```text
Num = 실제 원소 수
Max = 현재 확보한 공간에 담을 수 있는 원소 수
```

`Num < Max`라면 남은 공간에 바로 추가한다. 재할당은 확보한 공간이 부족할 때 발생한다.

최종 크기를 대략 안다면 다른 컨테이너로 바꾸기 전에 `Reserve()`를 먼저 검토한다.

```cpp
TArray<FEnemyData> Enemies;
Enemies.Reserve(ExpectedEnemyCount);
```

`Reserve`는 연속 메모리와 좋은 순회 성능을 유지하면서 반복 재할당을 줄이는 가장 단순한 방법이다.

---

## TInlineAllocator가 해결하는 문제

다음 배열이 매번 몇 개의 원소만 담지만 자주 만들어진다고 생각해 보자.

```cpp
void FindNearbyEnemies()
{
    TArray<AActor*> NearbyEnemies;
}
```

외부 저장 공간의 할당과 반환이 반복되어 실제 병목이 된다면 인라인 저장 공간을 둘 수 있다.

```cpp
TArray<AActor*, TInlineAllocator<8>> NearbyEnemies;
```

의미는 다음과 같다.

```text
원소 8개까지
→ 컨테이너 객체 내부의 Inline Storage 사용
→ 외부 할당이 필요하지 않음

8개 초과
→ Secondary Allocator로 외부 공간 확보
→ 기존 원소를 외부 공간으로 이동
```

## `Reserve(4)`와 `TInlineAllocator<4>` 차이

둘 다 원소 4개를 담을 **공간**을 준비할 수 있지만, 그 공간이 놓이는 위치와 할당 과정이 다르다.

```cpp
TArray<int32> A;
A.Reserve(4);

TArray<int32, TInlineAllocator<4>> B;
```

`A.Reserve(4)`는 컨테이너 밖에 별도의 저장 공간을 할당한다.

```text
A 객체                         별도로 할당된 저장 공간
┌────────────────┐            ┌─────────────────┐
│ Data ──────────┼───────────→│ [ ][ ][ ][ ]   │
│ Num = 0        │            └─────────────────┘
│ Max >= 4       │
└────────────────┘
```

`TInlineAllocator<4>`는 `B` 객체 자체에 4개 분량의 저장 공간을 포함한다.

```text
B 객체
┌──────────────────────────┐
│ Num = 0 / Max            │
│ Inline Storage           │
│ [ ][ ][ ][ ]             │
└──────────────────────────┘
```

두 경우 모두 아직 원소를 만든 것은 아니다.

```cpp
A.Num(); // 0
B.Num(); // 0
```

`Reserve()`와 인라인 공간은 **원소 개수(`Num`)를 늘리는 기능이 아니라, 앞으로 원소를 넣을 저장 공간을 준비하는 기능**이다.

| 구분 | `Reserve(4)` | `TInlineAllocator<4>` |
| --- | --- | --- |
| 공간 위치 | 컨테이너 밖의 별도 메모리 | 컨테이너 객체 내부 |
| 외부 할당 | `Reserve()` 호출 시 발생 | 4개 이하라면 발생하지 않음 |
| 컨테이너 자체 크기 | `Reserve` 수치 때문에 커지지 않음 | 인라인 용량만큼 커짐 |
| `Num` | 그대로 0 | 그대로 0 |
| 용량이 부족하면 | 더 큰 외부 공간으로 재할당 | 외부 공간을 할당하고 기존 원소를 이동 |

즉, `Reserve(4)`는 **외부 저장 공간을 미리 빌려 두는 것**이고, `TInlineAllocator<4>`는 **컨테이너가 처음부터 4칸을 몸체 안에 가지고 있는 것**이다.

> [!important]
> `Reserve()`는 여러 번의 재할당을 줄여 주지만, 최초 외부 할당 자체를 없애지는 않는다.  
> `TInlineAllocator<N>`은 원소 수가 `N` 이하인 동안 그 외부 할당을 피하는 것이 핵심이다.

## 인라인 용량을 넘으면

```cpp
TArray<int32, TInlineAllocator<4>> Values;

Values.Add(10);
Values.Add(20);
Values.Add(30);
Values.Add(40); // 아직 Inline Storage 사용
Values.Add(50); // 인라인 용량 초과
```

5번째 원소를 넣으면 인라인 공간 자체를 늘릴 수 없으므로, 더 큰 외부 저장 공간을 할당하고 기존 원소를 이동한다.

```text
이전: 컨테이너 내부
[10][20][30][40]

          5번째 원소 추가
                 ↓

이후: 별도 외부 저장 공간
[10][20][30][40][50][ ][ ][ ]
```

따라서 `TInlineAllocator<4>`는 "4개까지만 저장할 수 있다"는 뜻이 아니다.

```text
1~4개    → 컨테이너 내부 공간 사용
5개 이상 → 외부 Allocator로 전환하고 기존 원소 이동
```

## 성능 차이가 생기는 이유

일반 `TArray`의 원소 저장 공간은 컨테이너와 별도로 할당된다.

```text
일반 TArray 객체
┌─────────────┐
│ Data ───────┼──→ 외부 저장 공간 [A][B][C]
│ Num / Max   │
└─────────────┘

TInlineAllocator<4>를 사용한 TArray 객체
┌──────────────────┐
│ Num / Max        │
│ Inline Storage   │
│ [A][B][C][ ]     │
└──────────────────┘
```

반면 인라인 저장 공간은 컨테이너와 같은 객체 안에 이미 포함되어 있다. 작은 임시 배열이라면 별도의 메모리 할당과 해제를 피할 수 있다.

```cpp
void FindNearbyEnemies()
{
    TArray<AActor*, TInlineAllocator<8>> Enemies;
    // 대부분 8개 이하라면 외부 할당 없이 사용 가능
}
```

핵심 이점은 단순히 "스택이라 빠르다"가 아니라 **작은 배열에서 별도의 동적 메모리 할당을 피한다**는 것이다.

## 스택 Allocator는 아니다

`TInlineAllocator`는 **컨테이너 객체 내부**에 저장 공간을 둔다.

- 컨테이너가 함수의 지역 변수라면 객체와 인라인 버퍼가 보통 Stack에 놓인다.
- 컨테이너가 Heap에 생성된 객체의 멤버라면 인라인 버퍼도 그 객체가 있는 Heap 쪽에 놓인다.

따라서 `TInlineAllocator = Stack Allocator`라고 외우면 안 된다.

또한 **인라인 용량 초과**와 **Stack Overflow**는 전혀 다른 말이다.

```text
인라인 용량 초과
→ N칸이 부족하여 외부 저장 공간으로 전환

Stack Overflow
→ 지역 변수나 함수 호출 등이 실제 스택 메모리 한계를 초과
```

## 인라인 개수 N을 고르는 기준

`N`은 가능한 최대 개수보다 **평소 자주 나타나는 작은 개수의 상한**에 맞춘다.

`N`이 너무 크면 원소가 적거나 비어 있어도 컨테이너 객체 자체가 커진다. 지역 변수로 큰 인라인 배열을 만들면 Stack 사용량도 커지고, 지나치면 Stack Overflow 위험에도 영향을 줄 수 있다. 반대로 `N`을 자주 초과하면 외부 저장 공간으로 이동하는 비용이 반복될 수 있다.

> [!important]
> Inline Storage는 외부 할당을 줄일 수 있지만 L1 Cache Hit나 전체 성능을 보장하지 않는다. 원소 크기, 접근 패턴, 컨테이너가 놓인 위치로 결과가 달라진다.

---

## TChunkedArray가 해결하는 문제

`TArray`가 매우 커지면 더 큰 **하나의 연속 공간**을 확보하기 어려울 수 있고, 성장 과정에서 기존 원소를 모두 옮기는 비용도 커질 수 있다.

`TChunkedArray`는 데이터를 여러 allocation으로 나눈다.

```cpp
TChunkedArray<FNavigationNode> Nodes;
```

```text
Chunk 관리 컬렉션
[Chunk 0 주소] [Chunk 1 주소] [Chunk 2 주소]
      │              │              │
      ▼              ▼              ▼
 [A][B][C][D]   [E][F][G][H]   [I][J][K][L]
    내부 연속        내부 연속        내부 연속

Chunk끼리는 서로 떨어진 주소에 있을 수 있다.
```

즉, `TChunkedArray`를 다음과 같은 연결 리스트로 이해하면 안 된다.

```text
Chunk 0 → Chunk 1 → Chunk 2  // 이런 노드 연결 구조가 핵심이 아님
```

더 가까운 모습은 **Chunk 주소를 관리하는 컬렉션과, 각각 따로 할당된 연속 메모리 덩어리**다.

인덱스 접근은 개념적으로 전체 인덱스를 Chunk 번호와 Chunk 내부 위치로 나누어 찾는다.

```text
Chunk당 원소 4개, 전체 인덱스 6에 접근

Chunk 번호 = 6 / 4 = 1
내부 위치  = 6 % 4 = 2

Chunks[1] → [E][F][G][H]
                    ↑
                    G
```

이 그림은 이해를 위한 개념 모델이다. 정확한 내부 관리 방식과 Chunk 크기 계산은 엔진 버전과 템플릿 설정에 따라 달라질 수 있다.

공간이 더 필요하면 새 청크를 추가한다.

```text
Chunk 0  [A][B][C][D]  ← 그대로
Chunk 1  [E][F][G][H]  ← 그대로
Chunk 2  [I][J][K][L]  ← 새로 추가
```

핵심은 **성장할 때 기존 모든 원소를 더 큰 연속 공간으로 옮길 필요가 없다는 것**이다. “모든 Add와 재할당 비용이 무조건 `O(1)`”이라고 외우지는 않는다. 새 청크 할당과 청크 관리 비용은 여전히 존재한다.

## TChunkedArray 장점

- 매우 큰 단일 연속 메모리 요청을 여러 작은 요청으로 나눌 수 있다.
- 성장할 때 기존 청크 안의 원소를 전부 재배치하지 않는다.
- 새 청크 추가만으로 성장하는 동안 기존 원소 주소를 유지하는 데 유리하다.

## TChunkedArray 단점

- 전체 원소가 하나의 연속 메모리에 있지 않다.
- 인덱스에서 청크와 청크 내부 위치를 계산하는 과정이 필요하다.
- 순회 중 청크 경계를 넘으므로 `TArray`보다 캐시 지역성이 불리할 수 있다.
- Unreal API와 알고리즘 중 연속 메모리를 요구하는 기능에는 바로 사용할 수 없다.

대규모라는 이유만으로 자동 선택하지 않는다. 생성 후 매 프레임 전체 순회하는 작업이라면 큰 `TArray`가 여전히 더 빠를 수 있다.

---

## Heap Fragmentation

메모리를 할당하고 반환하다 보면 빈 공간이 여러 위치로 나뉠 수 있다.

```text
처음       [A][B][C][D][E]
일부 반환  [A][ ][C][ ][E]
```

빈 공간의 총합은 충분해도 하나의 큰 연속 공간이 없으면 큰 allocation 요청을 만족하기 어려울 수 있다. 이처럼 사용 가능한 공간이 조각난 상태가 **Heap Fragmentation**이다.

`TChunkedArray`는 하나의 거대한 연속 allocation 대신 여러 청크를 사용하므로, 파편화된 환경에서 큰 단일 allocation이 실패할 가능성을 줄이는 데 도움이 된다. 파편화 자체를 완전히 제거하는 것은 아니다.

---

## 둘의 차이

| 구분 | `TInlineAllocator<N>` | `TChunkedArray` |
| --- | --- | --- |
| 해결하려는 문제 | 작은 컨테이너의 외부 할당 반복 | 큰 연속 allocation과 전체 재배치 |
| 저장 방식 | N개까지 컨테이너 내부 | 여러 청크에 분산 |
| 주된 크기 | 작고 상한을 예상할 수 있음 | 매우 크고 계속 성장할 수 있음 |
| N/청크 초과 시 | 외부 저장 공간으로 이동 | 새 청크 추가 |
| 연속 메모리 | 외부 전환 전까지 한 저장 구간 | 전체가 하나로 연속되지 않음 |
| 주의점 | 객체 크기와 Stack 사용량 증가 가능 | 인덱싱·순회 지역성·호환성 비용 |

```text
TInlineAllocator
"대부분 아주 작으니 외부 allocation을 피하자."

TChunkedArray
"매우 크니 하나의 연속 allocation으로 키우지 말자."
```

---

## 선택 순서

1. 기본은 `TArray`로 시작한다.
2. 예상 크기가 있다면 `Reserve()`로 재할당을 줄인다.
3. 작은 임시 배열의 allocation이 실제 병목이면 `TInlineAllocator<N>`를 검토한다.
4. 큰 연속 allocation이나 대규모 relocation이 실제 문제면 `TChunkedArray`를 검토한다.
5. Unreal Insights의 Timing·Memory Insights와 목표 플랫폼의 프로파일러로 변경 전후를 측정한다.

> [!caution]
> `TInlineAllocator`는 항상 Stack에 있다, L1 캐시 성능이 최상이다, 힙 파편화를 완전히 없앤다, `TChunkedArray`의 모든 추가가 아무 비용 없이 `O(1)`이라는 표현은 정확하지 않다.

---

## 정리

```text
Malloc
= 동적 메모리 요청

FMemory
= Unreal의 저수준 메모리 할당 인터페이스

TArray
= 연속 메모리, Capacity 부족 시 재할당과 원소 이동 가능

Reserve(N)
= 컨테이너 밖에 N개 분량의 저장 공간을 미리 할당
= Num은 늘어나지 않으며 최초 외부 할당은 발생

TInlineAllocator<N>
= 컨테이너 내부에 N개 저장 공간 보유
= N을 넘으면 외부 Allocator로 전환하고 기존 원소 이동
= 객체 위치에 따라 인라인 공간도 Stack 또는 Heap 등에 놓일 수 있음

TChunkedArray
= Chunk 주소를 관리하며 큰 배열을 여러 청크로 나눠 저장
= Chunk 내부만 연속이고 Chunk끼리는 떨어져 있을 수 있음
= Linked List가 아님
= 전체 재배치와 큰 연속 할당 부담 완화
```

## 검증 자료

- [Epic Games: TArray](https://dev.epicgames.com/documentation/en-us/unreal-engine/array-containers-in-unreal-engine)
- [Epic Games: FMemory](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FMemory)
- [Epic Games: TInlineAllocator](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/Containers/TInlineAllocator)
- [Epic Games: TChunkedArray](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/Containers/TChunkedArray)

---

[[Unreal 정리 목차]] · [[GC]] · [[Cache Locality]]
