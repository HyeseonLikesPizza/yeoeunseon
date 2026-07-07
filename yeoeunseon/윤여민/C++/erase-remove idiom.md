---
tags: [cpp, stl, vector, algorithm, erase-remove-idiom, container]
created: 2026-07-07
updated: 2026-07-07
---

# Erase-Remove Idiom(`remove`/`remove_if` + `erase`)

> [!summary]
> **erase-remove idiom은 `std::remove_if`로 남길 원소를 앞쪽으로 모으고, `erase()`로 남은 뒤쪽 구간을 실제로 잘라내는 패턴이다.**
>
> ```text
> remove/remove_if  = 실제 삭제 X, 남길 원소를 앞쪽으로 모으고 새 끝 위치 반환
> erase             = 반환된 새 끝 위치부터 진짜 끝까지 잘라내서 size 감소
> ```
>
> 이름 때문에 헷갈리지만, `std::remove_if`만 호출하면 컨테이너의 `size()`는 줄어들지 않는다.

---

## 먼저 이것만 알면 된다

```cpp
v.erase(
    std::remove_if(v.begin(), v.end(), Predicate),
    v.end()
);
```

위 코드는 한 줄처럼 보이지만 실제로는 아래 흐름이다.

```cpp
auto newEnd = std::remove_if(v.begin(), v.end(), Predicate);
v.erase(newEnd, v.end());
```

이 코드의 의미는 아래와 같다.

```text
1. remove_if
   조건에 맞는 원소를 실제로 삭제하지 않는다.
   남길 원소들을 앞쪽으로 모은다.
   새 끝 위치(newEnd)를 반환한다.

2. erase
   remove_if가 반환한 newEnd를 받아서
   newEnd부터 기존 end까지의 남은 구간을 실제로 지운다.
   컨테이너 size가 줄어든다.
```

예를 들어 짝수를 지운다고 하자.

```text
처음
[1][2][3][4][5]    size = 5

remove_if 후
[1][3][5][?][?]    size = 5
          ^
          newEnd

erase 후
[1][3][5]          size = 3
```

`?` 부분은 더 이상 사용할 의미가 없는 구간이다. 컨테이너 안에 남아 있기는 하지만, 우리가 원하는 결과에는 포함되지 않는다.

---

## 왜 `remove_if`만으로 삭제하지 못할까?

`std::remove_if`는 `<algorithm>`에 있는 범용 알고리즘이다.

```cpp
std::remove_if(v.begin(), v.end(), Predicate);
```

이 함수는 컨테이너 자체를 받지 않는다. 오직 반복자 범위만 받는다.

```text
remove_if가 아는 것
→ begin 위치
→ end 위치
→ 어떤 원소를 지울지 판단하는 조건

remove_if가 모르는 것
→ 이 반복자가 vector에서 왔는지
→ 이 반복자가 deque에서 왔는지
→ 컨테이너 size를 어떻게 줄여야 하는지
```

그래서 `std::remove_if`는 컨테이너의 크기를 줄일 수 없다.

컨테이너의 크기를 줄이는 일은 컨테이너 멤버 함수인 `erase()`가 해야 한다.

---

## `remove_if`가 실제로 하는 일과 반환값

`remove_if`는 지울 원소를 하나씩 제거하는 함수가 아니다.

정확히는 **남길 원소를 앞쪽으로 덮어써서 모으는 함수**에 가깝다.

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};

auto newEnd = std::remove_if(
    v.begin(),
    v.end(),
    [](int x) { return x % 2 == 0; }
);
```

조건이 `true`인 원소가 제거 대상이다.

```text
조건: x % 2 == 0

1 → false → 남김
2 → true  → 제거 대상
3 → false → 남김
4 → true  → 제거 대상
5 → false → 남김
```

결과는 대략 이렇게 된다.

```text
[1][3][5][?][?]
          ^
          newEnd
```

여기서 중요한 점은 `v.size()`가 아직 5라는 것이다.

```cpp
std::cout << v.size(); // 아직 5
```

`newEnd`는 “여기부터는 결과에 포함하지 마”라는 경계선이다.

```text
begin                 newEnd      old end
  |                     |           |
  v                     v           v
[1] [3] [5]            [?] [?]
  └──── 결과 구간 ────┘ └ 버릴 구간 ┘
```

그래서 `remove_if`의 반환값을 바로 `erase`에 넘긴다.

```cpp
v.erase(newEnd, v.end());
```

---

## `erase`가 하는 일

`erase()`는 컨테이너의 멤버 함수다.

```cpp
v.erase(newEnd, v.end());
```

이 코드는 `newEnd`부터 기존 `end()`까지를 실제로 제거한다.

```text
[1][3][5][?][?]
          ^  ^
          |  |
       newEnd oldEnd

erase(newEnd, oldEnd)
→ [?][?] 구간 제거
→ size 감소
```

최종 결과:

```text
[1][3][5]
```

---

## 전체 코드

```cpp
#include <algorithm>
#include <vector>

void RemoveEvenNumbers(std::vector<int>& v)
{
    v.erase(
        std::remove_if(
            v.begin(),
            v.end(),
            [](int x) { return x % 2 == 0; }
        ),
        v.end()
    );
}
```

조금 풀어 쓰면 더 잘 보인다.

```cpp
void RemoveEvenNumbers(std::vector<int>& v)
{
    auto newEnd = std::remove_if(
        v.begin(),
        v.end(),
        [](int x) { return x % 2 == 0; }
    );

    v.erase(newEnd, v.end());
}
```

풀어 쓴 버전으로 보면 `remove_if`가 반환한 위치를 `erase`가 실제 삭제 구간으로 사용하는 흐름이 잘 보인다.

---

## `remove`와 `remove_if` 차이

| 함수 | 언제 사용 |
| --- | --- |
| `std::remove` | 특정 값과 같은 원소를 지울 때 |
| `std::remove_if` | 조건에 맞는 원소를 지울 때 |

둘 다 실제 삭제를 하지 않고, 남길 원소를 앞쪽으로 재배치한 뒤 **새 끝 위치를 반환한다.**

```text
std::remove     → 값으로 판단
std::remove_if  → 조건 함수로 판단
```

### 특정 값 제거

```cpp
std::vector<int> v = {1, 2, 3, 2, 4};

auto newEnd = std::remove(v.begin(), v.end(), 2);

v.erase(newEnd, v.end());
```

결과:

```text
[1][3][4]
```

중간 상태는 이런 느낌이다.

```text
처음
[1][2][3][2][4]

remove 후
[1][3][4][?][?]
          ^
          newEnd

erase 후
[1][3][4]
```

### 조건으로 제거

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};

v.erase(
    std::remove_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; }),
    v.end()
);
```

결과:

```text
[1][3][5]
```

---

## 핵심은 한 번에 훑고 한 번에 줄이는 것

반복문에서 `erase`해도 동작은 만들 수 있다. 하지만 `std::vector`에서는 여러 개를 지울 때 비효율적일 수 있다.

```cpp
for (auto it = v.begin(); it != v.end(); )
{
    if (*it % 2 == 0)
    {
        it = v.erase(it);
    }
    else
    {
        ++it;
    }
}
```

`vector`는 원소가 연속된 메모리에 있어야 한다.

중간 원소를 하나 지우면 뒤의 원소들을 앞으로 당겨야 한다.

```text
[1][2][3][4][5]
    ^
    2 삭제

[1][3][4][5]
   <- 뒤 원소들을 앞으로 이동
```

이 작업을 삭제할 때마다 반복하면 비용이 커진다.

```text
erase 한 번
→ 뒤 원소 이동

또 erase
→ 또 뒤 원소 이동

또 erase
→ 또 뒤 원소 이동
```

반면 erase-remove idiom은 전체를 한 번 훑으면서 남길 원소를 앞으로 모으고, 마지막에 뒤쪽을 한 번에 잘라낸다.

```text
remove_if
→ 한 번 훑으면서 남길 원소 정리

erase
→ 뒤쪽 구간 한 번에 제거
```

즉 핵심은 이것이다.

```text
지울 때마다 resize/shift 반복
→ 비용이 계속 발생

한 번 훑어서 남길 것 정리
→ 마지막에 한 번만 size 감소
```

그래서 `vector`에서 여러 원소를 조건으로 지울 때 자주 사용한다.

---

## `remove_if` 뒤쪽 구간은 어떤 상태인가?

```text
[1][3][5][?][?]
          ^
          newEnd
```

`newEnd`부터 기존 `end()`까지의 원소는 아직 컨테이너 안에 있다.

하지만 그 값이 무엇인지 의미 있게 기대하면 안 된다.

정확히 말하면:

```text
객체로서는 유효할 수 있다.
하지만 결과로 사용할 값은 아니다.
```

즉, 이 구간은 이렇게 이해하면 된다.

```text
remove_if 뒤쪽 구간
= 아직 vector 안에는 있음
= 하지만 어떤 값인지 믿으면 안 됨
= 결과 데이터로 쓰면 안 됨
= erase로 제거할 구간
```

그래서 이 구간을 읽어서 뭔가 판단하려고 하면 안 된다.

바로 `erase(newEnd, v.end())`로 잘라내는 것이 erase-remove idiom의 완성이다.

> [!caution]
> 이 구간을 “정의되지 않은 값”이라고 표현하면 너무 강하다. 보통은 **유효하지만 값의 의미를 믿지 않는 구간**으로 이해하면 된다.

---

## C++20부터는 `std::erase_if`

C++20부터는 더 간단한 함수가 추가되었다.

```cpp
#include <vector>

void RemoveEvenNumbers(std::vector<int>& v)
{
    std::erase_if(v, [](int x) { return x % 2 == 0; });
}
```

이 코드가 더 읽기 쉽다.

```text
조건에 맞는 원소를 v에서 지워라
```

가능한 환경이라면 C++20의 `std::erase_if`를 우선 고려해도 된다.

다만 erase-remove idiom을 알고 있어야 기존 코드와 C++17 이하 코드를 읽을 수 있다.

---

## `list`에서는 다르게 생각한다

`std::list`는 `vector`처럼 원소가 한 줄로 연속된 메모리에 있는 구조가 아니다.

각 원소가 노드로 따로 있고, 노드끼리 포인터로 연결되어 있다.

```text
[A] <-> [B] <-> [C]
```

그래서 `list`에는 멤버 함수 `remove_if`가 있다.

```cpp
std::list<int> values = {1, 2, 3, 4, 5};

values.remove_if([](int x) { return x % 2 == 0; });
```

`std::list::remove_if`는 조건에 맞는 노드를 실제로 제거한다.

```text
[1] <-> [2] <-> [3] <-> [4] <-> [5]
        제거          제거

[1] <-> [3] <-> [5]
```

`list`에서는 보통 멤버 함수 `list::remove_if`를 쓰는 편이 자연스럽다.

---

## `map`, `set` 같은 연관 컨테이너

`std::map`, `std::set`, `std::unordered_map` 같은 컨테이너에는 erase-remove idiom을 그대로 쓰지 않는다.

이유는 `remove_if` 방식과 컨테이너 구조가 맞지 않기 때문이다.

`remove_if`는 남길 원소를 앞쪽으로 덮어쓰면서 재배치한다.

```text
vector
[1][2][3][4][5]
→ [1][3][5][?][?]
```

그런데 `map`과 `set`은 이런 식으로 “앞쪽으로 덮어쓰기”를 하면 안 된다.

```text
map
key 1 -> "A"
key 2 -> "B"
key 3 -> "C"
```

`map`은 key 순서와 트리/해시 구조를 유지해야 한다. 원소를 임의로 앞으로 덮어쓰면 key의 정렬 규칙과 내부 구조가 깨진다.

`set`도 원소 자체가 key 역할을 하므로, 값을 마음대로 덮어써서 재배치할 수 없다.

그래서 `map`, `set`에서는 “남길 것 앞으로 모으기”가 아니라 **조건에 맞는 원소를 찾아서 그 자리에서 erase**하는 식으로 생각한다.

C++20 이상이면 `std::erase_if`를 사용할 수 있다.

```cpp
std::map<int, int> myMap =
{
    {1, 10},
    {2, 0},
    {3, 30}
};

std::erase_if(myMap, [](const auto& pair)
{
    return pair.second == 0;
});
```

결과:

```text
{1, 10}
{3, 30}
```

C++17 이하에서는 반복자를 직접 관리하면서 지운다.

여기서 **반복자를 직접 관리한다**는 말은, 반복자가 다음에 어디를 가리킬지 `for`문에 자동으로 맡기지 않고 코드에서 직접 정한다는 뜻이다.

보통 반복문은 끝날 때마다 자동으로 `++it`가 실행된다.

```cpp
for (auto it = myMap.begin(); it != myMap.end(); ++it)
```

하지만 삭제가 들어가는 반복문에서는 마지막에 `++it`를 두지 않는다.

```cpp
for (auto it = myMap.begin(); it != myMap.end(); )
```

왜냐하면 원소를 지웠는지 아닌지에 따라 다음 위치로 가는 방법이 달라지기 때문이다.

이 패턴의 핵심은 두 갈래다.

```text
지우지 않음
→ 현재 반복자가 그대로 유효함
→ 내가 직접 ++it로 다음 원소로 이동

지움
→ 현재 반복자가 가리키던 원소가 사라짐
→ erase(it)가 다음 원소 위치를 반환
→ 그 반환값을 it에 저장
```

```cpp
for (auto it = myMap.begin(); it != myMap.end(); )
{
    if (it->second == 0)
    {
        it = myMap.erase(it);
    }
    else
    {
        ++it;
    }
}
```

예를 들어 `myMap`이 이런 상태라고 하자.

```text
{1, 10}
{2, 0}
{3, 30}
```

흐름은 아래와 같다.

```text
it -> {1, 10}
조건 false
→ ++it

it -> {2, 0}
조건 true
→ erase(it)
→ 지운 다음 위치를 반환
→ it는 {3, 30}을 가리킴

it -> {3, 30}
조건 false
→ ++it
```

여기서 지운 뒤에 `++it`를 또 하면 안 된다.

```text
it = erase(it)
→ 이미 다음 원소로 이동한 상태

여기서 또 ++it
→ 다음 원소를 건너뛸 수 있음
```

그래서 코드가 이렇게 생긴다.

```cpp
if (it->second == 0)
{
    it = myMap.erase(it); // 지웠으므로 erase가 돌려준 다음 위치 사용
}
else
{
    ++it; // 안 지웠으므로 직접 다음으로 이동
}
```

한 줄로 정리하면:

```text
erase한 경우에는 erase가 다음 위치를 돌려주고,
erase하지 않은 경우에는 내가 ++it로 다음 위치로 간다.
```

---

## 반복자 무효화 주의

`erase()`를 호출하면 일부 반복자가 더 이상 유효하지 않게 된다.

특히 `std::vector`에서 원소를 지우면, 지운 위치 이후의 반복자는 대부분 다시 쓰면 안 된다.

이유는 `vector`가 연속된 배열 구조이기 때문이다.

```text
삭제 전
Index: 0   1   2   3   4
Value: A   B   C   D   E
              ^
              it는 C를 가리킴

C 삭제 후
Index: 0   1   2   3
Value: A   B   D   E
              ^
              원래 C 자리에는 D가 당겨져 옴
```

삭제 이후에는 뒤쪽 원소들이 앞으로 이동한다.

그러면 기존 반복자가 가리키던 위치의 의미가 바뀐다.

```text
삭제 전 it
→ C를 가리키는 반복자

삭제 후 같은 위치
→ 이제 D가 있음
→ 기존 it가 계속 안전하다고 볼 수 없음
```

주소 공간 자체가 그대로여도, 지운 위치 이후의 원소들은 이동 대입되면서 다른 값이 그 자리로 당겨져 온다. 그래서 지운 위치 이후의 반복자, 포인터, 참조는 다시 쓰지 않는다고 생각해야 한다.

```cpp
auto it = v.begin() + 2;
v.erase(it);

// it를 그대로 다시 사용하면 위험
```

반복문에서 직접 `erase`를 써야 한다면 반환값을 받아야 한다.

```cpp
it = v.erase(it);
```

erase-remove idiom에서는 `remove_if`가 반환한 `newEnd`를 바로 `erase`에 넘기므로 이 실수를 줄일 수 있다.

---

## 언제 무엇을 쓸까?

| 상황 | 추천 |
| --- | --- |
| `vector`에서 값 하나 제거 | `erase(std::remove(...), end)` |
| `vector`에서 조건 제거 | `erase(std::remove_if(...), end)` |
| C++20 이상에서 조건 제거 | `std::erase_if` |
| `list`에서 조건 제거 | `list.remove_if(...)` |
| `map`, `set`에서 조건 제거 | C++20 `std::erase_if`, 아니면 반복자 직접 관리 |

---

## 정리

1. `std::remove_if`는 실제 삭제가 아니라 남길 원소를 앞쪽으로 모으는 함수다.
2. `std::remove_if`만 호출하면 컨테이너의 `size()`는 줄어들지 않는다.
3. 실제 크기를 줄이려면 `erase(newEnd, end)`가 필요하다.
4. `vector`에서 반복문으로 여러 번 `erase`하면 원소 이동이 반복되어 비효율적일 수 있다.
5. C++20 이상에서는 `std::erase_if`가 더 간단하다.
6. `list`는 `list::remove_if`를 쓰는 편이 자연스럽다.
7. `map`, `set` 같은 연관 컨테이너에는 erase-remove idiom을 그대로 쓰지 않는다.

---

[[C++ 정리 목차]] · [[Cache Locality]] · [[템플릿]]
