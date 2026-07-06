---
tags: [cpp, concurrency, multithreading, race-condition, data-race, mutex, atomic, lock-free]
created: 2026-07-01
updated: 2026-07-06
---

# 경쟁 상태(Race Condition)

> [!summary]
> **Race Condition**은 스레드의 실행 순서에 따라 결과가 달라지는 문제다.
>
> ```text
> 공유하지 않기                  가장 좋은 해결
> 여러 변수·여러 코드 줄 보호     mutex
> 단순한 값 하나의 연산           atomic
> mutex가 측정된 병목             검증된 Lock-Free 구현 검토
> ```
>
> 일반적인 실무에서는 **mutex가 기본 선택**이다.

## Race Condition과 Data Race

| 용어 | 쉬운 의미 |
| --- | --- |
| **Race Condition** | 실행 순서가 프로그램의 올바른 결과를 바꾸는 문제 |
| **Data Race** | 여러 스레드가 같은 메모리를 보호 없이 동시에 사용하며 하나 이상이 쓰는 상태 |

C++에서 Data Race는 **정의되지 않은 동작(Undefined Behavior, UB)**이다.

전문적으로는 같은 메모리에 충돌하는 접근이 겹치고, 하나 이상이 비원자적이며, 접근 사이에 Happens-Before 관계가 없을 때 Data Race가 발생한다.

```text
읽기 + 읽기     다른 쓰기가 없다면 안전
읽기 + 쓰기     Data Race
쓰기 + 쓰기     Data Race
```

Data Race와 Race Condition은 같은 말이 아니다. atomic을 사용해 Data Race를 없애도 여러 연산의 논리적 순서가 잘못되면 Race Condition은 남을 수 있다.

---

## 왜 Counter++가 위험할까?

```cpp
int Counter = 0;

void Increase()
{
    ++Counter;
}
```

`++Counter`는 한 줄이지만 실제로는 다음 과정이다.

```text
값 읽기 → 1 증가 → 결과 쓰기
```

두 스레드의 작업이 겹치면:

```text
Thread A: 0 읽기 → 1 계산 ───────→ 1 쓰기
Thread B:     0 읽기 → 1 계산 ───→ 1 쓰기

기대: 2
가능한 결과의 예: 1
```

한 번의 변경이 덮이는 현상을 **Lost Update(갱신 손실)**라고 한다.

> [!caution]
> 이 코드는 Data Race이므로 실제 결과가 반드시 1이라는 뜻은 아니다. C++에서는 프로그램의 동작 자체가 UB다.

---

## Mutex와 Atomic

> **Mutex는 코드 구역을 보호하고, Atomic은 변수 하나의 개별 연산을 보호한다.**

| 구분 | `std::mutex` | `std::atomic` |
| --- | --- | --- |
| 보호 단위 | 여러 변수와 여러 코드 줄 | atomic 객체 하나의 개별 연산 |
| 사용 예 | 계좌 이체, 여러 상태 동시 변경 | 카운터 증가, 플래그 변경 |
| 충돌 시 | 다른 스레드가 락을 기다림 | 원자 연산을 수행하며 구현에 따라 재시도 가능 |
| 난도 | 비교적 단순 | 복합 로직에서는 어려움 |

### Mutex

```cpp
std::mutex AccountMutex;
int PlayerGold = 100;
int ShopGold = 0;

void BuyItem()
{
    std::lock_guard<std::mutex> Lock(AccountMutex);

    PlayerGold -= 10;
    ShopGold += 10;
}
```

골드 차감과 증가는 함께 완료되어야 한다. mutex는 한 스레드가 임계 구역을 실행하는 동안 다른 스레드가 중간에 들어오지 못하게 한다.

`std::lock_guard`는 범위를 벗어날 때 자동으로 락을 해제한다. 이런 수명 기반 관리를 **RAII**라고 한다.

> 같은 데이터를 읽고 쓰는 모든 스레드가 **동일한 mutex**를 사용해야 한다.

### Atomic

```cpp
std::atomic<int> Counter{0};

void Increase()
{
    Counter.fetch_add(1);
}
```

`fetch_add(1)`은 읽기·증가·쓰기를 하나의 원자적 연산으로 처리한다.

atomic은 해당 객체의 개별 연산만 보호한다. 여러 atomic 변수나 앞뒤 코드 전체가 자동으로 하나의 작업이 되는 것은 아니다.

> [!important]
> **Atomic과 Lock-Free는 같은 말이 아니다.** `std::atomic`은 원자성을 제공하지만 플랫폼과 타입에 따라 내부 잠금을 사용할 수 있다.

```cpp
const bool bLockFree = Counter.is_lock_free();
```

---

## Mutex와 Lock-Free의 차이

둘 다 공유 데이터를 안전하게 다룰 수 있다. 차이는 **전체 진행이 특정 스레드에게 의존하는가**다.

```text
Mutex
Thread A가 락을 가진 채 멈춤
→ 같은 락이 필요한 B와 C도 대기

Lock-Free
Thread A가 연산 도중 멈춤
→ 락 소유자가 없음
→ Thread B가 원자 연산에 성공해 완료 가능
```

> **Lock-Free는 잠금을 기다리느라 전체가 멈추지 않고, 경쟁 중 적어도 하나가 계속 완료되는 구조다.**

mutex도 락 소유자가 정상 실행되면 작업 하나를 완료한다. 차이는 락 소유자가 멈췄을 때 다른 스레드의 진행까지 막히는가에 있다.

이때 같은 자원을 얻으려는 상황은 Race Condition이 아니라 **경합(Contention)**이라고 한다.

### CAS 흐름

Lock-Free 자료구조는 주로 **CAS(Compare-And-Swap)** 같은 원자 연산을 사용한다.

```text
현재 값 읽기
→ 새 값 계산
→ CAS 시도
   ├─ 성공: 완료
   └─ 실패: 다른 스레드가 먼저 변경함 → 다시 시도
```

모든 스레드가 바로 성공하는 것은 아니다. 특정 스레드는 계속 실패하는 **Starvation**을 겪을 수 있다.

### 언제 사용하는가?

- 락 소유자의 중단 때문에 시스템 전체 진행이 막히면 안 될 때
- mutex 대기가 실제 프로파일링에서 병목으로 확인됐을 때
- 검증된 Lock-Free Queue 같은 구현을 사용할 수 있을 때

Lock-Free는 높은 경합에서 항상 빠르지 않으며 직접 구현과 검증이 매우 어렵다. 일반 코드는 mutex부터 사용한다.

---

## Happens-Before

> **Happens-Before는 함수나 문법이 아니라 C++ 메모리 모델의 순서 관계다.**

### 왜 필요한가?

한 스레드가 먼저 값을 썼다고 해서 다른 스레드가 그 값을 같은 순서로 본다고 자동 보장되지는 않기 때문이다.

순서를 바꾸거나 관찰 결과에 영향을 줄 수 있는 주체는 컴파일러만이 아니다.

| 주체 | 관련 동작 |
| --- | --- |
| 컴파일러 | 명령어 재배치, 레지스터 사용, 불필요한 읽기 제거 |
| CPU | Out-of-Order Execution, Store Buffer 등 |
| 메모리 시스템 | 코어별 캐시와 캐시 일관성 처리 |

CPU와 캐시는 단일 스레드 결과가 유지되도록 동작하지만, 이것만으로 C++ 스레드 사이의 순서와 값 전달이 보장되지는 않는다.

Happens-Before는 두 가지를 보장한다.

1. **순서**: 앞 작업을 뒤 작업보다 먼저 처리된 것으로 본다.
2. **메모리 가시성**: 앞 스레드가 쓴 값이 뒤 스레드의 읽기에 반영된다.

여기서 가시성은 코드가 보기 좋다는 뜻이 아니라, **실행 중 한 스레드의 메모리 변경을 다른 스레드가 관찰할 수 있다는 뜻**이다.

같은 mutex를 올바르게 사용하면 Happens-Before 관계가 만들어진다.

```text
Thread A                              Thread B

mutex 잠금
Data = 42
mutex 해제  ── Happens-Before ──→    같은 mutex 잠금
                                      Data 읽기: 42
```

Happens-Before를 직접 호출하지 않는다. mutex, `std::thread::join()`, 올바른 atomic 메모리 순서 같은 동기화 기능을 사용하면 그 결과로 관계가 생긴다.

처음에는 동일한 mutex로 공유 데이터를 보호하고 atomic의 기본 메모리 순서를 사용하면 충분하다.

---

## 캐시 라인 경합

여러 코어가 같은 atomic을 계속 수정하면 **Cache Line Bouncing**이 발생할 수 있다.

```text
Core A가 캐시 라인의 쓰기 권한 획득
→ Core B가 수정하려고 권한을 가져감
→ Core A가 다시 권한을 되찾음
```

CPU는 캐시를 캐시 라인 단위로 관리한다. 한 코어가 쓰려면 다른 코어가 가진 같은 캐시 라인 사본을 무효화하고 쓰기 가능한 상태를 가져와야 한다. 여러 코어가 번갈아 쓰면 권한과 데이터가 계속 이동해 비용이 커진다.

이 과정은 Data Race를 막기 위한 것이 아니라 **Cache Coherence(캐시 일관성)**를 유지하기 위한 것이다. C++ 코드의 Data Race는 mutex나 atomic으로 별도 해결해야 한다.

---

## volatile

> **일반적인 C++·Unreal 멀티스레드 코드에서는 `volatile`을 동기화에 사용하지 않는다.**

`volatile`은 메모리 접근 자체가 중요하다고 컴파일러에 알리는 한정자다. 구현 환경에 따라 하드웨어 레지스터나 특수한 신호 처리에서 사용한다.

### 왜 접근 자체가 중요할까?

일반 변수는 프로그램 코드가 값을 바꾸지 않으면 컴파일러가 “값이 그대로일 것”이라고 판단할 수 있다. 그래서 반복되는 읽기를 한 번으로 줄이거나 필요 없다고 판단한 쓰기를 제거할 수 있다.

하지만 하드웨어 상태 레지스터는 프로그램 밖의 장치가 값을 바꿀 수 있다.

```cpp
// 구현 환경이 제공한 하드웨어 상태 레지스터라고 가정
volatile std::uint32_t* StatusRegister = GetStatusRegister();

while ((*StatusRegister & ReadyBit) == 0)
{
    // 장치가 ReadyBit를 설정할 때까지 상태를 다시 읽는다.
}
```

프로그램 코드에는 `StatusRegister`의 값을 바꾸는 부분이 없지만 실제 하드웨어가 값을 변경한다. 따라서 각 읽기를 없애지 말고 실제 접근으로 남겨야 한다.

쓰기 역시 장치에 명령을 전달하는 동작일 수 있어서, 최종 변수값만 같다는 이유로 제거하면 안 된다. 이런 경우에는 **값의 계산 결과뿐 아니라 읽기·쓰기 행위 자체가 외부 장치에 의미**가 있다.

> [!note]
> 이것은 구현 환경에서 하드웨어 레지스터를 다룰 때의 용도다. `volatile`은 스레드 사이의 원자성이나 값 전달 순서를 보장하지 않는다.

다음 기능은 제공하지 않는다.

- 원자성
- 상호 배제
- Happens-Before

```text
게임의 공유 변수·플래그 → atomic 또는 mutex
하드웨어 레지스터       → 환경에 따라 volatile 검토
```

---

## Unreal에서의 기본 원칙

가장 좋은 방법은 Worker Thread와 GameThread가 같은 UObject 상태를 직접 공유하지 않게 하는 것이다.

```text
GameThread에서 필요한 값 복사
→ Worker Thread에서 순수 데이터로 계산
→ GameThread에서 객체 유효성 확인 후 결과 반영
```

자세한 패턴은 [[Async & ThreadPool]] 참고.

---

## 최종 정리

```text
공유 제거 가능              → 공유하지 않기
여러 변수·복합 작업         → mutex
단순 카운터·플래그          → atomic
mutex가 실제 병목           → 검증된 Lock-Free 구현 검토
```

- Mutex: 구역을 잠그고 한 스레드만 실행
- Atomic: 변수 하나의 개별 연산을 원자적으로 처리
- Lock-Free: 특정 락 소유자 없이 전체 중 적어도 하나가 계속 완료
- Happens-Before: 스레드 사이의 순서와 메모리 가시성 관계
- Atomic이라고 항상 Lock-Free는 아니며, Lock-Free라고 항상 빠른 것도 아님

---

## 참고

- [C++ Working Draft: Data Races](https://eel.is/c++draft/intro.races)
- [C++ Working Draft: Lock-free Property](https://eel.is/c++draft/atomics.lockfree)
- [C++ Working Draft: Progress Guarantee](https://eel.is/c++draft/basic.exec)
