---
tags: [cpp, array, c-string, pointer, memory-layout, std-array, std-span]
created: 2026-07-07
updated: 2026-07-07
---

# 배열과 C 문자열

> [!summary]
> **배열은 같은 타입의 값을 연속된 메모리에 놓는 구조다.**
>
> ```text
> int Numbers[5]      = int 5개짜리 배열
> char Text[]         = char 배열
> char Text[]="Hello" = 마지막에 '\0'이 들어간 C 문자열 배열
> ```
>
> 배열과 포인터가 비슷해 보이는 이유는 배열을 함수에 넘길 때 첫 번째 원소를 가리키는 포인터처럼 변하기 때문이다.

---

## 먼저 이것만 알면 된다

| 표현 | 의미 |
| --- | --- |
| `T Array[N]` | `T` 원소 `N`개가 연속된 메모리에 놓인 배열 |
| `char Text[] = "Hello"` | `H e l l o \0`가 들어간 문자 배열 |
| `T* Pointer` | `T` 원소 하나를 가리키는 포인터 |
| `T[R][C]` | `T[C]` 배열이 `R`개 있는 진짜 2차원 배열 |

배열은 원소가 붙어 있다.

```cpp
int Numbers[5] = {1, 2, 3, 4, 5};
```

```text
Index      0   1   2   3   4
Numbers  [1] [2] [3] [4] [5]
```

`Numbers[0]`, `Numbers[1]`처럼 인덱스로 접근할 수 있는 이유는 원소들이 같은 크기로 연속 배치되어 있기 때문이다.

---

## 배열은 길이를 타입에 포함한다

```cpp
int Numbers[5] = {1, 2, 3, 4, 5};
```

`Numbers`의 타입은 단순히 `int*`가 아니라 `int[5]`다.

즉, 배열의 크기 `5`도 타입에 포함된다.

```cpp
#include <iterator>

std::size_t Count = std::size(Numbers); // 5
```

배열 객체를 직접 알고 있는 위치에서는 `std::size()`로 원소 개수를 알 수 있다.

---

## 그런데 함수에 넘기면 길이를 잃는다

```cpp
void PrintNumbers(const int* Numbers, std::size_t Count)
{
    for (std::size_t Index = 0; Index < Count; ++Index)
    {
        std::cout << Numbers[Index] << '\n';
    }
}
```

함수 안의 `Numbers`는 배열이 아니라 포인터다.

```text
원래 배열
[1][2][3][4][5]
 ^
 |
Numbers 포인터
```

포인터는 첫 번째 원소의 위치만 알 뿐, 원소가 몇 개인지는 모른다. 그래서 `Count`를 함께 넘긴다.

> [!important]
> 배열을 함수에 넘길 때 핵심은 이것이다.
>
> ```text
> 배열 자체를 넘기는 느낌이지만,
> 함수 매개변수에서는 보통 첫 원소를 가리키는 포인터로 다룬다.
> ```

---

## C 문자열은 `char` 배열 + `'\0'`

```cpp
char Text[] = "Hello";
```

실제 배열 크기는 5가 아니라 6이다.

```text
Index   0    1    2    3    4     5
Text  ['H']['e']['l']['l']['o']['\0']
```

`'\0'`은 문자열의 끝을 표시하는 종료 문자다.

```cpp
std::strlen(Text); // 5
std::size(Text);   // 6
```

둘의 의미가 다르다.

| 표현 | 의미 |
| --- | --- |
| `strlen(Text)` | `'\0'` 전까지의 문자 개수 |
| `std::size(Text)` | 배열 전체 원소 개수 |

`"Hello"`의 글자는 5개지만 배열에는 끝 표시 `'\0'`까지 들어가므로 원소는 6개다.

---

## 모든 `char` 배열이 C 문자열은 아니다

```cpp
char Buffer[5] = {'H', 'e', 'l', 'l', 'o'};
```

이 배열에는 `'\0'`이 없다.

```text
Buffer ['H']['e']['l']['l']['o']
```

따라서 C 문자열 함수에 넘기면 위험하다.

```cpp
std::printf("%s", Buffer); // 잘못된 사용
```

`printf("%s")`는 `'\0'`을 만날 때까지 계속 읽는다. 배열 안에 `'\0'`이 없으면 배열 밖까지 읽을 수 있다.

> [!caution]
> `char` 배열이라고 해서 자동으로 문자열인 것은 아니다. C 문자열로 쓰려면 읽을 수 있는 범위 안에 `'\0'`이 있어야 한다.

---

## 진짜 2차원 배열

```cpp
int Matrix[3][4] =
{
    {1,  2,  3,  4},
    {5,  6,  7,  8},
    {9, 10, 11, 12}
};
```

논리적으로는 표처럼 보인다.

```text
1   2   3   4
5   6   7   8
9  10  11  12
```

실제 메모리에는 한 줄로 연속 배치된다.

```text
[1][2][3][4][5][6][7][8][9][10][11][12]
```

`Matrix`의 타입은 `int[3][4]`다.

조금 더 풀면:

```text
int[4]짜리 배열이 3개 있다
```

즉, 바깥 배열의 원소 하나가 `int[4]` 배열이다.

```text
Matrix[0] -> int[4] {1, 2, 3, 4}
Matrix[1] -> int[4] {5, 6, 7, 8}
Matrix[2] -> int[4] {9, 10, 11, 12}
```

---

## `T[R][C]`는 `T**`가 아니다

이 부분이 가장 헷갈리기 쉽다.

```cpp
int Matrix[3][4];
```

`Matrix`를 함수에 넘길 때는 `int**`가 되지 않는다.

```cpp
void PrintMatrix(const int Matrix[][4], std::size_t RowCount);
```

위 선언은 사실상 아래와 같다.

```cpp
void PrintMatrix(const int (*Matrix)[4], std::size_t RowCount);
```

읽으면:

```text
Matrix는 int 4개짜리 배열을 가리키는 포인터
```

왜 `4`가 필요할까?

`Matrix[Row][Column]`의 주소를 계산하려면 한 행이 몇 칸인지 알아야 한다.

```text
Matrix[2][1]에 접근

한 행 크기 = 4
2행 1열 = 앞에서 2행 * 4칸 건너뛰고, 1칸 더 이동
```

그래서 열 개수 `4`는 타입에 남아 있어야 한다.

```cpp
// void Wrong(int** Matrix); // int[3][4]를 받을 수 없음
```

---

## `char[3][8]`과 `const char*[3]` 비교

둘 다 문자열 여러 개처럼 보이지만 구조가 다르다.

```cpp
char Rectangular[3][8] =
{
    "Alice",
    "Bob",
    "Chris"
};

const char* PointerArray[3] =
{
    "Alice",
    "Bob",
    "Chris"
};
```

### `char[3][8]`

```text
['A']['l']['i']['c']['e']['\0'][ ][ ]
['B']['o']['b']['\0'][ ][ ][ ][ ]
['C']['h']['r']['i']['s']['\0'][ ][ ]
```

각 행의 크기가 항상 8이다. 전체가 하나의 연속된 배열이다.

### `const char*[3]`

```text
PointerArray[0] -> "Alice\0"
PointerArray[1] -> "Bob\0"
PointerArray[2] -> "Chris\0"
```

배열 안에는 포인터 3개가 있고, 각 포인터가 문자열을 가리킨다.

| 구분 | `char[3][8]` | `const char*[3]` |
| --- | --- | --- |
| 구조 | 문자 배열 3개 | 문자 포인터 3개 |
| 각 문자열 위치 | 전체가 연속 | 서로 다른 위치일 수 있음 |
| 각 행 크기 | 항상 8 | 문자열마다 다름 |
| 함수 전달 시 | `char (*)[8]` | `const char**` |
| `char**`로 받을 수 있음 | 아니요 | 매개변수에서는 가능 |

---

## 현대 C++에서는 무엇을 쓸까

저수준 개념을 이해하는 것은 중요하지만, 직접 코드를 설계할 때는 목적이 드러나는 타입을 쓰는 편이 좋다.

| 목적 | 추천 타입 |
| --- | --- |
| 고정 크기 배열 | `std::array<T, N>` |
| 크기가 바뀌는 배열 | `std::vector<T>` |
| 배열 일부를 소유하지 않고 보기 | `std::span<T>` |
| 문자열 소유 | `std::string` |
| 문자열 읽기 전용 뷰 | `std::string_view` |

예를 들어 배열과 길이를 따로 넘기는 대신:

```cpp
void PrintNumbers(std::span<const int> Numbers)
{
    for (int Value : Numbers)
    {
        std::cout << Value << '\n';
    }
}
```

`std::span`은 데이터를 소유하지 않지만 시작 위치와 길이를 함께 들고 있다.

---

## 정리

1. 배열은 같은 타입 원소들이 연속된 메모리에 놓인 구조다.
2. `char Text[] = "Hello"`는 마지막에 `'\0'`까지 포함하므로 크기가 6이다.
3. 모든 `char` 배열이 C 문자열은 아니다. `'\0'`이 있어야 C 문자열이다.
4. 배열을 함수에 넘기면 보통 첫 원소를 가리키는 포인터처럼 다뤄져 길이 정보를 잃는다.
5. 진짜 2차원 배열 `T[R][C]`는 `T**`가 아니다.
6. `T[R][C]`를 함수에 넘기려면 열 크기 `C`를 알아야 한다.

---

[[char 포인터]]
