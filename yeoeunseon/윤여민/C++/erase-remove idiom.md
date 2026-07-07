---
tags: [cpp, stl, vector, algorithm, erase-remove-idiom, container]
created: 2026-07-07
updated: 2026-07-07
---

# Erase-Remove Idiom

> [!summary]
> **Erase-Remove Idiom은 `std::remove` 또는 `std::remove_if`로 남길 원소를 앞쪽에 모은 뒤, `erase()`로 뒤쪽 구간을 실제로 잘라내는 패턴이다.**
>
> ```text
> remove/remove_if  = 실제 삭제 X, 새 끝 위치 반환
> erase             = 새 끝 위치부터 기존 끝까지 실제 삭제
> ```
>
> 이름 때문에 헷갈리지만, `std::remove_if`만 호출하면 컨테이너의 `size()`는 줄어들지 않는다.

---

## 핵심 정리

```text
1. std::remove / std::remove_if는 실제 삭제가 아니다.
2. 남길 원소를 앞으로 재배치하고 newEnd를 반환한다.
3. 실제 size를 줄이려면 erase(newEnd, end)가 필요하다.
4. vector에서 여러 원소를 지울 때 erase를 반복하면 비효율적일 수 있다.
5. C++20이면 std::erase_if를 쓰면 더 간단하다.
6. map/set/list는 vector와 구조가 달라서 지우는 방식이 다르다.
7. erase 중 반복자는 무효화될 수 있으니 반환값으로 다음 위치를 받아야 한다.
```

이 주제의 중심은 아래 한 줄이다.

```text
한 번 훑어서 남길 원소를 모으고, 마지막에 한 번만 실제 크기를 줄인다.
```

---

## 기본 형태

```cpp
v.erase(
    std::remove_if(v.begin(), v.end(), Predicate),
    v.end()
);
```

풀어 쓰면 아래와 같다.

```cpp
auto newEnd = std::remove_if(v.begin(), v.end(), Predicate);
v.erase(newEnd, v.end());
```

흐름은 두 단계다.

```text
1. remove_if
   조건에 맞는 원소를 실제로 지우지 않는다.
   남길 원소를 앞쪽으로 모은다.
   새 끝 위치(newEnd)를 반환한다.

2. erase
   newEnd부터 기존 end까지의 구간을 실제로 제거한다.
   컨테이너 size가 줄어든다.
```

예를 들어 짝수를 지운다면:

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

---

## 왜 `remove_if`만으로 삭제되지 않을까?

`std::remove_if`는 `<algorithm>`에 있는 범용 알고리즘이다.

컨테이너 자체가 아니라 반복자 범위만 받는다.

```cpp
std::remove_if(v.begin(), v.end(), Predicate);
```

`remove_if`가 아는 것은 아래 정도다.

```text
begin 위치
end 위치
제거할 조건
```

반대로 아래 정보는 모른다.

```text
이 반복자가 vector에서 왔는지
deque에서 왔는지
컨테이너 size를 어떻게 줄여야 하는지
```

그래서 `remove_if`는 원소를 앞으로 재배치할 수는 있어도 컨테이너의 크기를 줄일 수 없다.

크기를 줄이는 일은 컨테이너 멤버 함수인 `erase()`가 맡는다.

---

## `remove`와 `remove_if`

| 함수 | 기준 |
| --- | --- |
| `std::remove` | 특정 값과 같은 원소 제거 |
| `std::remove_if` | 조건을 만족하는 원소 제거 |

둘 다 실제 삭제를 하지 않고, 남길 원소를 앞쪽으로 모은 뒤 새 끝 위치를 반환한다.

### 값으로 제거

```cpp
std::vector<int> v = {1, 2, 3, 2, 4};

auto newEnd = std::remove(v.begin(), v.end(), 2);
v.erase(newEnd, v.end());
```

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
    std::remove_if(v.begin(), v.end(), [](int x)
    {
        return x % 2 == 0;
    }),
    v.end()
);
```

```text
결과
[1][3][5]
```

> [!important]
> Predicate가 `true`를 반환한 원소가 제거 대상이다.

---

## 왜 `vector`에서 자주 쓰나

`vector`는 원소가 연속된 메모리에 놓인다.

중간 원소를 하나 지우면 뒤쪽 원소들을 앞으로 당겨야 한다.

```text
[1][2][3][4][5]
    ^
    2 삭제

[1][3][4][5]
   <- 뒤 원소 이동
```

반복문에서 조건에 맞는 원소를 하나씩 `erase`하면 이 이동이 여러 번 발생할 수 있다.

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

Erase-Remove Idiom은 전체 범위를 한 번 훑으면서 남길 원소를 정리하고, 마지막에 뒤쪽 구간을 한 번에 잘라낸다.

```text
remove_if
→ 남길 원소를 앞으로 모음

erase
→ 남은 뒤쪽 구간을 한 번에 제거
```

이런 이유로 `vector`에서 여러 원소를 조건으로 지울 때 자주 사용한다.

연속 메모리와 원소 이동 비용은 [[Cache Locality]]와도 연결된다.

---

## 뒤쪽 구간의 상태

`remove_if` 이후 `newEnd`부터 기존 `end()`까지의 구간은 아직 컨테이너 안에 있다.

```text
[1][3][5][?][?]
          ^
          newEnd
```

하지만 이 구간의 값은 결과 데이터로 사용하면 안 된다.

```text
newEnd 앞쪽   = 남길 결과 구간
newEnd 뒤쪽   = erase로 제거할 구간
```

> [!caution]
> 뒤쪽 구간을 “정의되지 않은 값”이라고 표현하면 너무 강하다.
> 객체로서는 유효할 수 있지만, 값의 의미를 기대하면 안 되는 구간으로 이해하면 된다.

---

## C++20부터는 `std::erase`와 `std::erase_if`

C++20부터는 더 직접적인 함수가 있다.

값으로 지울 때:

```cpp
std::vector<int> v = {1, 2, 3, 2, 4};

std::erase(v, 2);
```

조건으로 지울 때:

```cpp
std::vector<int> v = {1, 2, 3, 4, 5};

std::erase_if(v, [](int x)
{
    return x % 2 == 0;
});
```

가능한 환경이라면 C++20의 `std::erase` 또는 `std::erase_if`를 먼저 고려해도 된다.

그래도 Erase-Remove Idiom은 알아두는 것이 좋다.

- C++17 이하 코드에서 자주 보인다.
- 기존 STL 코드를 읽을 때 필요하다.
- `remove_if`가 실제 삭제를 하지 않는다는 사실을 이해하는 데 도움이 된다.

---

## 컨테이너별 선택

| 컨테이너 | 권장 방식 |
| --- | --- |
| `vector`, `deque`, `string` | Erase-Remove Idiom 또는 C++20 `std::erase_if` |
| `list` | `list.remove_if(...)` |
| `map`, `set`, `unordered_map`, `unordered_set` | C++20 `std::erase_if`, 아니면 반복자를 직접 관리하며 `erase` |

### `list`

`std::list`는 노드 기반 컨테이너다.

```text
[A] <-> [B] <-> [C]
```

`vector`처럼 뒤쪽 원소를 앞으로 당기는 구조가 아니므로 멤버 함수 `remove_if`를 쓰는 편이 자연스럽다.

```cpp
std::list<int> values = {1, 2, 3, 4, 5};

values.remove_if([](int x)
{
    return x % 2 == 0;
});
```

### `map`, `set`

`map`, `set` 같은 연관 컨테이너에는 Erase-Remove Idiom을 그대로 쓰지 않는다.

`remove_if`는 남길 원소를 앞쪽으로 덮어쓰며 재배치하는 방식인데, 연관 컨테이너는 key 순서와 내부 구조를 유지해야 한다.

특히 `set`의 원소나 `map`의 key는 컨테이너 안에서 마음대로 덮어써서 옮기는 대상이 아니다.

C++20 이상이면:

```cpp
std::erase_if(myMap, [](const auto& pair)
{
    return pair.second == 0;
});
```

C++17 이하에서는 반복자를 직접 관리한다.

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

반복자 이동 기준은 아래와 같다.

```text
지웠다      → erase가 반환한 다음 위치 사용
안 지웠다   → 직접 ++it로 다음 위치 이동
```

---

## 반복자 무효화

`erase()`를 호출하면 일부 반복자, 포인터, 참조가 더 이상 유효하지 않게 된다.

특히 `vector`에서 원소를 지우면 지운 위치 이후의 원소들이 앞으로 이동한다.

```text
삭제 전
Index: 0   1   2   3   4
Value: A   B   C   D   E
              ^
              it

C 삭제 후
Index: 0   1   2   3
Value: A   B   D   E
```

기존 반복자를 계속 쓰면 위험하다.

```cpp
auto it = v.begin() + 2;
v.erase(it);

// it를 그대로 다시 사용하면 안 된다.
```

반복문에서 직접 `erase`를 써야 한다면 반환값을 받아야 한다.

```cpp
it = v.erase(it);
```

Erase-Remove Idiom은 `remove_if`가 반환한 `newEnd`를 바로 `erase`에 넘기므로, 반복 중 직접 삭제할 때 생기는 실수를 줄이는 데도 도움이 된다.

---

## 언제 무엇을 쓸까?

| 상황 | 먼저 고려할 방식 |
| --- | --- |
| C++20 이상에서 값 제거 | `std::erase(container, value)` |
| C++20 이상에서 조건 제거 | `std::erase_if(container, predicate)` |
| C++17 이하 `vector`에서 값 제거 | `erase(std::remove(...), end)` |
| C++17 이하 `vector`에서 조건 제거 | `erase(std::remove_if(...), end)` |
| `list`에서 조건 제거 | `list.remove_if(...)` |
| `map`, `set` 계열에서 조건 제거 | C++20 `std::erase_if`, 아니면 반복자 직접 관리 |

---

## 정리

1. `std::remove`와 `std::remove_if`는 실제 삭제를 하지 않는다.
2. 둘은 남길 원소를 앞쪽으로 모으고 새 끝 위치를 반환한다.
3. 컨테이너 크기를 실제로 줄이는 것은 `erase()`다.
4. `vector`에서 여러 원소를 지울 때 Erase-Remove Idiom이 자주 쓰인다.
5. C++20 이상에서는 `std::erase`, `std::erase_if`가 더 간단하다.
6. `list`는 멤버 함수 `remove_if`가 자연스럽다.
7. `map`, `set` 같은 연관 컨테이너에는 Erase-Remove Idiom을 그대로 쓰지 않는다.

---

[[C++ 정리 목차]] · [[Cache Locality]] · [[템플릿]]
