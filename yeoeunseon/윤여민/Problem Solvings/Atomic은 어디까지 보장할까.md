## Atomic인데 왜 틀릴 수 있지?

**Atomic은 개별 연산을 보장한다. 여러 줄의 처리 전체나 주변 데이터까지 자동으로 보호하지는 않는다.**

- 숫자를 바꾸는 코드가 헷갈리면 → [[#값 하나를 바꿀 때]]
- Ready를 보고 다른 변수를 읽어도 되는지 궁금하면 → [[#다른 데이터를 전달할 때]]
- seq_cst와 relaxed가 헷갈리면 → [[#Memory order가 달라지면]]
- 배운 것을 직접 적용하려면 → [[#직접 판단하기 — 작업은 한 번만 실행했는데]]

예시는 C++ 학습용이다. 공유 객체는 Thread 시작 전에 초기화하며, 각 예시에서 명시한 접근 외에는 다른 변경이 없다고 가정한다.

---

## 값 하나를 바꿀 때

### 두 Thread가 증가시키면 번갈아 실행될까?

**번갈아 실행된다는 보장은 없다. 그래도 원자적으로 1,000번씩 증가시키고 모두 끝났다면 2,000이다.**

> [!note]- Thread를 만드는 코드부터 보기
> ```cpp
> #include <atomic>
> #include <iostream>
> #include <thread>
>
> int main()
> {
>     std::atomic<int> Count{0};
>
>     std::thread Worker([&Count]()
>     {
>         for (int i = 0; i < 1000; ++i)
>             Count.fetch_add(1);
>     });
>
>     for (int i = 0; i < 1000; ++i)
>         Count.fetch_add(1);
>
>     Worker.join();
>     std::cout << Count.load(); // 2000
> }
> ```
>
> Main이 Worker를 만들지만 람다 본문은 별도 Thread에서 실행된다. Main의 반복문과 실행 순서가 섞일 수 있다. 하드웨어 상황에 따라 실제로 동시에 실행될 수도 있다.
>
> `[&Count]`는 같은 Count를 참조한다는 뜻이다. `join()`은 Worker의 완료를 기다린다. 출력 시점에는 양쪽 반복문이 모두 끝났다.

### load하고 1을 더해 store하면 같은 증가 아닌가?

**아니다. 두 Thread가 모두 0을 읽고 1을 저장하면 최종값은 1이다.**

```cpp
int Value = Count.load();
Count.store(Value + 1); // 읽기와 저장이 별개

Count.fetch_add(1);    // 읽기·증가·저장이 하나의 원자적 연산
```

> [!note]- 각각 안전한데 왜 증가가 사라질까?
> ```text
> A: 0 읽기 ──────────→ 1 저장
> B:        0 읽기 ──────────→ 1 저장
> ```
>
> Count 접근 자체에는 Data Race가 없지만 증가 전체의 논리적 경쟁은 남는다.
>
> `Count.store(Count.load() + 1)`처럼 한 줄로 써도 두 연산이다. 기본 seq_cst도 이를 하나로 합치지 않는다.
>
> 반대로 두 Thread가 `fetch_add(1)`을 한 번씩 실행하면, 0에서 시작한 Count는 둘 다 완료한 뒤 2다. relaxed로 지정해도 증가가 유실되지는 않는다.

### exchange, fetch_add, ++는 뭐가 다를까?

**exchange는 교체, fetch_add는 현재 값에 더하기다.** 각각 10에서 시작한다면:

| 연산 | 변경 후 값 | 반환값 |
|---|---:|---:|
| `Count.exchange(2)` | 2 | 10 |
| `Count.fetch_add(2)` | 12 | 10 |
| `Count++` | 11 | 10 |
| `++Count` | 11 | 11 |

`std::atomic<int>`의 증가·감소는 원자적이다. 일반 `int`에 같은 문법을 쓴다고 그 보장이 생기지는 않는다.

> [!note]- Atomic은 int만 돼?
> bool, 포인터, 조건을 만족하는 구조체 등에도 사용할 수 있다. 타입에 따라 지원하는 연산은 다르다.
>
> atomic 포인터가 가리키는 객체까지 보호하지는 않는다. 구현이 항상 lock-free인 것도 아니다.
>
> [C++ atomic 연산 규정](https://eel.is/c++draft/atomics.types.operations)

### 함수나 변수를 인자로 넣으면 그것도 보호할까?

**인자를 계산하는 과정은 atomic 연산 밖이다.**

```cpp
Count.fetch_add(CalculateAmount());

// 계산과 증가를 구분하면:
int Amount = CalculateAmount();
Count.fetch_add(Amount);
```

지역 변수나 안전하게 공유된 읽기 전용 데이터로 계산한다면 별도 보호가 필요 없다. 함수 안에서 다른 Thread가 변경하는 공유 데이터를 읽는다면 그 접근은 따로 동기화해야 한다.

### compare_exchange는 같으면 바꾸는 거야?

**현재 값이 Expected와 같으면 교체한다. 다르면 교체하지 않고, Expected에 관찰한 값을 써준다.**

```cpp
int Expected = 10;
bool Changed = Count.compare_exchange_strong(Expected, 20);
```

| 비교 순간 Count | Count에 하는 일 | Expected | 반환값 |
|---|---|---:|---|
| 10 | 20으로 변경 | 10 유지 | true |
| 7 | 변경하지 않음 | 7로 변경 | false |

> [!note]- Expected까지 갱신한다고?
> Expected는 참조 인자여서 함수가 값을 수정할 수 있다. 보통 각 Thread의 지역 변수로 둔다.
>
> 실패 시 얻는 값은 **비교 순간에 관찰한 값**이다. 함수가 반환된 뒤에도 Count가 계속 그 값이라는 보장은 없다.
>
> `if (Count.load() == 10)` 다음에 `store(20)`을 하면 그 사이 다른 Thread가 바꿀 수 있다. CAS는 비교와 조건부 변경을 하나로 처리한다.
>
> 반환형이 bool인 것이지, 대상이 bool에 한정되는 것은 아니다.

### 자리가 남았을 때만 증가시키려면?

**검사와 증가를 따로 하면 정원을 넘을 수 있다. CAS로 “아직 예상한 값일 때만 증가”시킨다.**

```cpp
// Count가 9일 때 두 Thread가 모두 조건을 통과할 수 있다.
if (Count.load() < 10)
{
    Count.fetch_add(1); // 9 → 10 → 11
    AdmitOnePlayer();
}
```

> [!note]- CAS로 자리를 확보하는 과정과 재시도 이유
> **“9를 보고 왔는데, 지금도 9라면 10으로 바꾸겠다.”**
>
> ```text
> A와 B: 각각 Expected = 9
> A: 9인가? → 맞음 → 10으로 변경 → 성공
> B: 9인가? → 지금은 10 → 실패, Expected = 10
> B: 정원이 찼으므로 입장하지 않음
> ```
>
> **실패했다고 항상 정원이 찬 것은 아니다. 시작값이 8이라면:**
>
> ```text
> A와 B: 각각 Expected = 8
> A: 8 → 9 성공
> B: 8을 예상했지만 현재 9 → 실패, Expected = 9
> B: 아직 한 자리 남음 → 9 → 10 재시도 → 성공 가능
> ```
>
> 실패는 “예상과 달라졌다”는 뜻이다. 새 값으로 제한을 다시 확인한다.
>
> ```cpp
> int Expected = Count.load();
>
> while (Expected < 10)
> {
>     if (Count.compare_exchange_strong(Expected, Expected + 1))
>     {
>         AdmitOnePlayer(); // 확보한 Thread만 실행
>         break;
>     }
>     // 실패하면 Expected가 갱신된다. 제한부터 다시 확인한다.
> }
> ```
>
> `Expected + 1`은 먼저 계산되지만, CAS가 현재 값과 Expected를 다시 비교한다. 그 사이 값이 달라지면 낡은 예상으로 변경하는 데 실패한다.
>
> Count는 0~10에서 시작하고 모든 자리 확보 코드가 같은 규칙을 따라야 한다. 입장이 실패할 수 있다면 자리 반환도 필요하다. 이 CAS가 입장 함수 내부의 공유 데이터까지 보호하지는 않는다. Lock으로 검사와 변경을 함께 보호하는 방법도 있다.

---

## 다른 데이터를 전달할 때

### Ready만 atomic인데 Result는 왜 안전해질까?

**준비한 쪽의 release 신호를 acquire로 읽으면, 그전에 쓴 Result를 이후에 안전하게 읽을 수 있다.**

```cpp
int Result = 0;
std::atomic<bool> Ready{false};

// Worker — 한 번만 작성하고 이후 Result를 수정하지 않음
Result = 42;
Ready.store(true, std::memory_order_release);

// Main
if (Ready.load(std::memory_order_acquire))
{
    Use(Result);
}
```

> [!note]- 무엇이 무엇을 보장하는 걸까?
> Main이 **이 release가 쓴 true를 읽었을 때** 다음 연결이 생긴다.
>
> ```text
> Result = 42
>     ↓ Worker에서 먼저 수행
> Ready.store(true, release)
>     ↓ 이 값을 acquire로 읽음
> Ready.load(acquire) == true
>     ↓ Main에서 이후 수행
> Use(Result)
> ```
>
> 이 연결이 Result 쓰기와 읽기 사이에 happens-before 관계를 만든다. Result가 atomic으로 바뀐 것은 아니다. 충돌하는 접근을 동기화로 순서 지은 것이다.
>
> Acquire는 새 값이 올 때까지 기다리지 않는다. false를 읽었다면 아직 이 공개 신호를 받은 것이 아니다.
>
> 옵션을 생략한 기본 seq_cst store/load에도 각각 release/acquire 역할이 있어서 이 예시는 안전하다.

### 코드에서 Result를 먼저 썼으니 일반 bool도 되지 않을까?

**한 Thread 안의 코드 순서만으로 다른 Thread와의 동기화가 생기지는 않는다.**

> [!note]- 동기화가 필요한 이유
> 일반 Ready를 한쪽에서 쓰고 다른 쪽에서 동기화 없이 읽으면 Ready 자체부터 Data Race다. C++에서는 정의되지 않은 동작이다.
>
> 컴파일러는 반복문에서 일반 변수를 매번 다시 읽지 않도록 최적화할 수 있다. CPU의 저장 버퍼 등으로 다른 코어가 쓰기를 관찰하는 시점도 단순한 코드 순서만으로 판단할 수 없다.
>
> 이것들은 필요성을 이해하는 예시이지, Data Race가 있을 때 항상 특정 결과가 나온다는 설명은 아니다. 동기화 규칙을 지키면 컴파일러와 CPU가 필요한 보장을 구현한다.
>
> `std::this_thread::yield()`는 실행 기회 양보 요청일 뿐이다. Ready나 Result의 동기화를 대신하지 않는다.

### 한쪽만 relaxed면 안 돼?

**위 Result 전달에서는 안 된다. 다른 동기화가 없다면 공개와 수신 양쪽이 연결되어야 한다.**

| store | load | 해당 true를 읽은 뒤 Result 전달 |
|---|---|---|
| release | acquire | 보장 |
| seq_cst | seq_cst | 보장 |
| seq_cst | acquire | 보장 |
| release | seq_cst | 보장 |
| release | relaxed | 이 신호만으로 보장 안 됨 |
| relaxed | acquire | 이 신호만으로 보장 안 됨 |

release는 store에, acquire는 load에 사용하는 역할이다. load에 release를 지정하는 것은 올바른 사용이 아니다.

### Acquire는 언제까지 보호해? Lock 같은 거야?

**다른 Thread를 막는 기능이 아니다. 공개 후의 새 쓰기까지 보호하지 않는다.**

```cpp
Result = 42;
Ready.store(true, std::memory_order_release);
Result = 100; // Main의 읽기와 충돌할 수 있음
```

> [!note]- “Ready 이전 변수는 전부 안전하다”와는 다른 이유
> 마지막 쓰기는 release 이후라 그 신호로 Main의 읽기와 순서가 연결되지 않는다. 다른 동기화가 없다면 Data Race다. 단순히 42 또는 100 중 하나를 안전하게 읽는다는 뜻이 아니다.
>
> 반대로 공개 후 아무도 Result를 수정하지 않는다면, 신호를 받은 Main이 나중에 다시 읽어도 안전하다. 보장의 유효 시간이 정해져 있는 것이 아니다.
>
> Ready를 false로 되돌려도 이미 true를 읽고 Result를 사용하는 Main을 멈추지 못한다. 같은 데이터를 다시 쓰려면 사용 완료 확인이나 Lock 등 재사용 규칙이 필요하다.
>
> Atomic을 “작은 Lock”으로 떠올리는 것은 개별 연산의 원자성을 이해하는 비유일 뿐이다. Lock처럼 주변 코드의 보호 구간을 소유하는 것은 아니다.
>
> [C++ Data Race와 happens-before 규정](https://eel.is/c++draft/intro.races)

---

## Memory order가 달라지면

### Relaxed는 정확히 무엇을 남겨두는 거야?

**해당 atomic 연산의 원자성과, 그 객체 하나의 변경 순서는 유지한다. 다른 데이터 전달은 동기화하지 않는다.**

```cpp
TotalHits.fetch_add(1, std::memory_order_relaxed);
```

숫자 자체만 필요한 통계라면 후보가 된다. 그 숫자를 보고 다른 결과 데이터까지 읽는다면 요구사항이 달라진다.

> [!note]- Modification order가 뭐였지?
> 하나의 atomic 객체에 일어난 변경들의 순서다. 0에서 두 번 증가했다면 `0 → 1 → 2`로 변경된다. 누가 먼저 증가하는지는 정하지 않는다.
>
> 다른 atomic 변수까지 합친 하나의 공통 순서는 아니다. 읽는 Thread가 모든 중간값을 보거나, 즉시 최신값을 읽는다는 뜻도 아니다.

### Seq_cst는 그냥 release/acquire의 다른 이름이야?

**아니다. 데이터 전달에 필요한 역할에 더해, seq_cst 연산들 사이에 하나의 공통 순서를 요구한다.**

seq_cst는 **sequentially consistent**의 줄임말이다. 기본 store는 release 역할, 기본 load는 acquire 역할을 포함하지만, 누가 먼저 실행될지 정하거나 여러 연산을 하나로 묶지는 않는다.

> [!note]- 둘 다 0을 읽는 예시로 차이 보기
> X와 Y는 atomic int이며 초기값이 모두 0이다.
>
> ```cpp
> // Thread A
> X.store(1);
> int SeenY = Y.load();
>
> // Thread B
> Y.store(1);
> int SeenX = X.load();
> ```
>
> **네 연산이 모두 기본 seq_cst라면 둘 다 0일 수 없다.**
>
> A가 Y=0을 읽었다면 공통 순서는 다음 관계를 가져야 한다.
>
> ```text
> A: X=1 저장
> → A: Y 읽기 0
> → B: Y=1 저장
> → B: X 읽기 1
> ```
>
> 마지막 X는 1이어야 한다. 반대도 같다. `(SeenY, SeenX)`는 (0,1), (1,0), (1,1)이 가능하다.
>
> ---
>
> **store를 release, load를 acquire로 바꾸면 (0,0)도 허용된다.**
>
> 저장 버퍼를 이용해 이해하면:
>
> | 순서 | 코어 A | 코어 B |
> |---|---|---|
> | 1 | X=1을 저장 버퍼에 넣음 | Y=1을 저장 버퍼에 넣음 |
> | 2 | B의 쓰기가 아직 안 보여 Y=0 읽음 | A의 쓰기가 아직 안 보여 X=0 읽음 |
> | 3 | X=1이 다른 코어에도 보이게 됨 | Y=1이 다른 코어에도 보이게 됨 |
>
> 둘 다 초기값을 읽었으므로 상대 release와 연결되지 않는다. Release는 모든 코어가 값을 볼 때까지 기다리는 기능이 아니다.
>
> 이 표는 가능한 하드웨어 설명 모델이다. 모든 CPU의 정확한 구현을 뜻하지 않는다. C++의 차이는 release/acquire는 이 결과를 허용하고 seq_cst는 금지한다는 것이다.
>
> [C++ memory order 규정](https://eel.is/c++draft/atomics.order)

### 그러면 무조건 seq_cst를 쓰면 되지 않을까?

**이해하기 쉬운 기본값으로 시작해도 된다. 다만 강한 순서 보장이 잘못된 알고리즘까지 고쳐주지는 않는다.**

정원 검사와 증가가 분리된 코드는 seq_cst여도 잘못될 수 있다. 반대로 통계 숫자만 세는 데는 relaxed로 충분할 수 있다. 비용 차이는 CPU와 연산에 따라 달라서, relaxed가 무조건 빠르지는 않다.

---

> [!note]- Unreal에서는 왜 std::atomic을 쓰라고 할까?
> Epic은 TAtomic을 deprecated로 표시하고 유지보수하지 않으므로 새 코드에 std::atomic을 권고한다. 이것을 std::atomic이 항상 더 빠르다는 뜻으로 해석하지 않는다.
>
> [Epic TAtomic 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/TAtomic)

---

## 직접 판단하기 — 작업은 한 번만 실행했는데

두 Worker가 아래 함수를 각각 한 번 호출한다. Main은 그동안 완료를 확인한다. 초기값은 다음과 같다.

```cpp
std::atomic<int> Attempts{0};
std::atomic<bool> Claimed{false};
int Result = 0;
```

```cpp
void Worker()
{
    int Seen = Attempts.load();
    Attempts.store(Seen + 1);

    if (!Claimed.exchange(true))
    {
        Result = Calculate();
    }
}

void CheckFromMain()
{
    if (Claimed.load())
    {
        Use(Result);
    }
}
```

Calculate는 공유 데이터에 접근하지 않고 값 하나를 계산하며 실패하지 않는다. 각 Worker는 한 번만 실행하고, 같은 작업을 재사용하지 않는다.

**Main은 Worker가 끝날 때까지 기다리지 않고 `CheckFromMain()`을 호출한다. 따라서 Worker가 아직 계산 중일 수도 있다.**

원하는 동작은 세 가지다.

- 두 Worker가 모두 끝나면 Attempts는 2다.
- Calculate는 한 Worker만 실행한다.
- Main은 계산이 완료된 Result만 안전하게 사용한다.

**현재 코드는 이 요구사항을 만족할까? 잘못된 부분만 수정하고, 왜 충분한지 설명해보자.**

> [!question]- 내 판단과 비교하기
> **한 번만 실행하는 부분은 맞지만, 시도 횟수와 결과 전달은 잘못되어 있다.**
>
> Attempts는 두 Worker가 같은 0을 읽으면 최종 1이 될 수 있다. 증가 전체를 `fetch_add`로 바꾼다.
>
> Claimed의 exchange는 처음 실행한 Worker에만 이전 값 false를 돌려준다. 다른 Worker는 true를 받으므로 Calculate는 한 번만 실행된다. 이 부분은 유지할 수 있다.
>
> ```text
> exchange(true) 실행 → 이전 값 반환 → ! 적용 → if 진입 여부 결정
>
> 첫 Worker:  false 반환 → !false → 진입
> 다음 Worker: true 반환 → !true  → 진입하지 않음
> ```
>
> if 본문에 들어가지 않아도 exchange 자체는 실행된다.
>
> 하지만 Claimed는 **작업을 맡았다는 표시**다. true가 된 다음에 계산하므로 완료 신호로 사용할 수 없다. 기본 seq_cst라도 뒤에서 실행될 Result 쓰기를 미리 공개해주지는 않는다.
>
> ```text
> Worker: Claimed = true → 계산 중… → Result 기록
> Main:           true 읽음 → Result 사용 시도
> ```
>
> Main이 일반 Result를 읽는 것과 Worker가 쓰는 것 사이에 동기화가 없어 Data Race가 생길 수 있다.
>
> **계산 중에 CheckFromMain()을 호출하는 것은 괜찮다. 준비되지 않은 Result를 읽는 것이 문제다.**
>
> ---
>
> **작업 확보와 완료를 서로 다른 상태로 둔다.**
>
> ```cpp
> std::atomic<bool> Ready{false}; // Thread 시작 전에 초기화
>
> void Worker()
> {
>     Attempts.fetch_add(1);
>
>     if (!Claimed.exchange(true))
>     {
>         Result = Calculate();
>         Ready.store(true);
>     }
> }
>
> void CheckFromMain()
> {
>     if (Ready.load())
>     {
>         Use(Result);
>     }
> }
> ```
>
> 원래 문제와 마찬가지로 모든 atomic 연산은 기본 seq_cst를 사용한다. Attempts의 최종값 2는 두 Worker 완료를 join 등으로 확인한 뒤에 검사한다.
>
> Ready에 true를 저장하기 전에 Result를 작성했다. Main이 그 true를 기본 seq_cst load로 읽으면 앞선 Result 쓰기와 동기화된다. 이후 Result를 다시 쓰지 않으므로 안전하게 읽을 수 있다.
>
> 보장을 명시적으로 낮추는 별도 선택도 가능하다. 이 예시에서는 통계용 Attempts의 증가에 relaxed, Ready의 store/load에 release/acquire를 사용할 수 있다. 원래 오류를 고치기 위해 필요한 변경은 아니며, 더 빨라진다고 단정할 수도 없다.
>
> ```text
> Claimed → 내가 맡았다 → 중복 계산 방지
> Ready   → 다 만들었다 → 결과 읽기 허용
> ```

---

## 정리

| 이름 | 기억할 단서 |
|---|---|
| [[#load하고 1을 더해 store하면 같은 증가 아닌가?\|load / store]] | **각각** 원자적 · 둘을 묶지는 않음 |
| [[#load하고 1을 더해 store하면 같은 증가 아닌가?\|fetch_add]] | 읽기 + 더하기 + 저장을 **한 번에** |
| [[#exchange, fetch_add, ++는 뭐가 다를까?\|exchange]] | 교체하고 **이전 값** 반환 |
| [[#compare_exchange는 같으면 바꾸는 거야?\|CAS]] | **같으면 교체** · 실패하면 Expected 갱신 |
| [[#자리가 남았을 때만 증가시키려면?\|CAS 재시도]] | 실패 ≠ 정원 초과 · **새 값으로 재판단** |
| [[#Relaxed는 정확히 무엇을 남겨두는 거야?\|relaxed]] | **원자성 유지** · 다른 데이터 동기화 없음 |
| [[#Ready만 atomic인데 Result는 왜 안전해질까?\|release / acquire]] | 신호 전 작성 → **그 신호 수신** → 사용 |
| [[#Seq_cst는 그냥 release/acquire의 다른 이름이야?\|seq_cst]] | 기본값 · seq_cst끼리 **공통 순서** |
| [[#Relaxed는 정확히 무엇을 남겨두는 거야?\|modification order]] | **atomic 하나**의 변경 순서 |
| [[#Acquire는 언제까지 보호해? Lock 같은 거야?\|공개 후 수정]] | acquire ≠ Lock · **새 쓰기는 별도 동기화** |
| [[#직접 판단하기 — 작업은 한 번만 실행했는데\|Claimed / Ready]] | **맡았다 ≠ 끝났다** |
