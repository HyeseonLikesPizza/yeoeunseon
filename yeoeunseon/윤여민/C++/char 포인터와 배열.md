---
tags: [cpp, pointer, array, c-string, argv, memory]
created: 2026-07-06
updated: 2026-07-06
---

# `char*`, `char**`, 배열

> [!summary]
> - `char*`는 **문자열 자체가 아니라 `char`를 가리키는 포인터**다.
> - `char*`가 가리키는 메모리가 연속되어 있고 마지막에 `'\0'`이 있을 때 C 문자열로 사용할 수 있다.
> - `char**`는 **2차원 배열 자체가 아니라 `char*`를 가리키는 포인터**다.
> - 진짜 2차원 배열 `T[R][C]`은 연속된 메모리지만, 함수에 전달될 때 `T**`가 아니라 `T (*)[C]`로 변환된다.
> - 포인터는 길이를 저장하지 않으므로 개수, 종료 표식 또는 별도의 길이 정보가 필요하다.

---

## 먼저 바로잡을 표현

| 흔히 하는 표현 | 정확한 의미 |
| --- | --- |
| `char*`는 문자열이다 | `char*`는 문자 포인터다. 특정 조건을 만족할 때 C 문자열을 가리킨다. |
| 포인터의 포인터는 2차원 배열이다 | `T**`는 `T*`를 가리키는 포인터다. 2차원 배열 `T[R][C]`과 타입과 메모리 구조가 다르다. |
| 배열은 자기 길이를 알고 있다 | 배열의 크기는 타입에 포함된다. 하지만 배열 객체에 별도의 길이 값이 저장되는 것은 아니며, 포인터로 변환되면 크기 정보가 사라진다. |

---

## 1. `char*`는 무엇인가

```cpp
char* Pointer;
```

`char*`는 `char` 객체의 주소를 저장하는 포인터 타입이다.

### 문자 하나를 가리키는 경우

```cpp
char Character = 'A';
char* Pointer = &Character;
```

```text
Pointer
   │
   ▼
┌─────┐
│ 'A' │
└─────┘
```

이때 `Pointer`는 문자 하나를 가리킬 뿐이다. 뒤에 `'\0'`이 있다는 보장이 없으므로 C 문자열 함수에 전달하면 안 된다.

```cpp
std::printf("%s", Pointer); // 잘못된 사용: '\0'을 만날 때까지 범위를 넘어 읽을 수 있음
```

### C 문자열을 가리키는 경우

```cpp
char Text[] = "Hello";
char* Pointer = Text;
```

`Text`의 실제 배열 크기는 6이며 마지막에 널 문자 `'\0'`이 포함된다.

```text
인덱스     0     1     2     3     4      5
        ┌─────┬─────┬─────┬─────┬─────┬──────┐
Text    │ 'H' │ 'e' │ 'l' │ 'l' │ 'o' │ '\0' │
        └─────┴─────┴─────┴─────┴─────┴──────┘
          ▲
       Pointer
```

`Pointer` 자체에는 문자열 길이 5나 배열 크기 6이 저장되어 있지 않다. `std::strlen`, `std::strcmp`, `printf`의 `%s` 같은 C 문자열 기능은 `'\0'`을 만날 때까지 메모리를 읽는다.

### C 문자열로 사용할 수 있는 조건

1. 문자가 유효한 메모리에 연속해서 저장되어 있어야 한다.
2. 읽을 수 있는 범위 안에 종료 문자 `'\0'`이 있어야 한다.
3. 포인터가 해당 메모리의 유효한 위치를 가리켜야 한다.

> [!caution]
> `char*`라는 타입만 보고 C 문자열이라고 판단할 수 없다. 널 포인터일 수도 있고, 문자 하나나 널 종료되지 않은 버퍼를 가리킬 수도 있다.

### 문자열 리터럴과 `const char*`

```cpp
const char* Literal = "Hello";
char Buffer[] = "Hello";

// Literal[0] = 'Y'; // 오류: 문자열 리터럴은 수정하지 않음
Buffer[0] = 'Y';     // 가능: 수정 가능한 배열
```

C++에서 문자열 리터럴은 수정하면 안 되므로 `const char*`로 받는다. 수정 가능한 문자열 버퍼가 필요하면 `char[]` 또는 `std::string`을 사용한다.

---

## 2. 일반 배열과 크기

일반 배열은 `'\0'`으로 끝나는 구조가 아니다.

```cpp
int Numbers[5] = {1, 2, 3, 4, 5};
int Matrix[3][4] = {};
```

`Numbers`의 타입은 `int[5]`이며 배열 크기 5가 타입에 포함된다. 실제 배열을 선언한 범위에서는 `std::size()`로 원소 수를 구할 수 있다.

```cpp
#include <iterator>

const std::size_t Count = std::size(Numbers); // 5
```

하지만 배열이 함수 인자로 사용되면 대부분 첫 번째 원소를 가리키는 포인터로 변환된다.

```cpp
void PrintNumbers(const int* Numbers, std::size_t Count)
{
    for (std::size_t Index = 0; Index < Count; ++Index)
    {
        std::cout << Numbers[Index] << '\n';
    }
}
```

함수 안의 `Numbers`는 포인터이므로 원래 배열의 길이를 알 수 없다. 따라서 `Count`를 함께 전달한다.

> [!note]
> `char Text[] = "Hello"`에 `'\0'`이 들어가는 이유는 모든 배열이 널 문자로 끝나기 때문이 아니다. **문자열 리터럴로 초기화된 문자 배열**이기 때문이다.

---

## 3. 진짜 2차원 배열

```cpp
int Matrix[3][4] =
{
    {1,  2,  3,  4},
    {5,  6,  7,  8},
    {9, 10, 11, 12}
};
```

`Matrix`의 타입은 “원소가 `int[4]`인 배열 3개”, 즉 `int[3][4]`다.

```text
논리적인 모습

1   2   3   4
5   6   7   8
9  10  11  12

실제 메모리 배치

1  2  3  4  5  6  7  8  9  10  11  12
```

모든 `int` 원소는 행 우선 순서로 연속해서 배치된다.

### 함수에 전달되면 `int**`가 아니다

`Matrix`가 포인터로 변환될 때 첫 번째 행을 가리키므로 타입은 `int (*)[4]`, 즉 **원소 4개짜리 int 배열을 가리키는 포인터**가 된다.

```cpp
void PrintMatrix(const int Matrix[][4], std::size_t RowCount);

// 위 선언과 같은 매개변수 타입
void PrintMatrix(const int (*Matrix)[4], std::size_t RowCount);
```

```cpp
// void PrintMatrix(int** Matrix); // int[3][4]를 받을 수 없음
```

`Matrix + 1`은 `int` 하나가 아니라 한 행인 `int[4]`만큼 이동한다. 컴파일러가 `Matrix[Row][Column]`의 주소를 계산하려면 두 번째 크기인 4를 알아야 한다.

---

## 4. `char**`는 무엇인가

```cpp
char** PointerToPointer;
```

`char**`는 `char*` 객체의 주소를 저장한다.

문자열 여러 개를 다룰 때는 먼저 문자 포인터의 배열을 만들 수 있다.

```cpp
const char* Names[3] =
{
    "Alice",
    "Bob",
    "Chris"
};
```

```text
Names
┌─────────┐
│ pointer │ ──► "Alice\0"
├─────────┤
│ pointer │ ──► "Bob\0"
├─────────┤
│ pointer │ ──► "Chris\0"
└─────────┘
```

`Names` 자체는 포인터 3개가 연속된 배열이다. 그러나 각 포인터가 가리키는 문자열들은 서로 다른 위치에 있을 수 있으며, 문자열 길이도 달라도 된다.

### 배열이 함수 매개변수로 들어갈 때

함수 매개변수 선언에서 배열 형태는 포인터 형태로 조정된다.

```cpp
void PrintNames(const char* Names[], std::size_t Count);

// 매개변수 타입의 의미는 위와 같음
void PrintNames(const char** Names, std::size_t Count);
```

따라서 `char* Names[3]` 같은 **문자 포인터 배열**을 함수에 전달할 때 `char**`로 받을 수 있다. 이것이 `char**`를 2차원 배열처럼 느끼게 하는 주된 이유다.

그러나 다음 두 선언은 서로 다른 구조다.

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

| 구분 | `char[3][8]` | `const char*[3]` |
| --- | --- | --- |
| 구조 | 문자 배열 3개 | 문자 포인터 3개 |
| 전체 문자 메모리 | 한 덩어리로 연속 | 문자열마다 다른 위치일 수 있음 |
| 한 행의 용량 | 항상 8바이트 | 각 문자열 길이가 달라도 됨 |
| 포인터 변환 결과 | `char (*)[8]` | `const char**` |
| `char**`로 받을 수 있는가 | 아니요 | 함수 전달 시 가능 |

> [!important]
> **`T**`와 `T[R][C]`는 같은 타입이 아니다.**  
> `T**`는 포인터를 따라 한 번 더 이동하는 구조이고, `T[R][C]`는 고정된 크기의 행들이 한 메모리 블록에 연속 배치된 구조다.

---

## 5. `char**`는 끝을 어떻게 아는가

`char**`도 포인터이므로 원소 개수를 스스로 알지 못한다. 주로 두 가지 방식을 사용한다.

### 개수를 함께 전달

```cpp
void PrintNames(const char* const* Names, std::size_t Count)
{
    for (std::size_t Index = 0; Index < Count; ++Index)
    {
        std::cout << Names[Index] << '\n';
    }
}
```

```text
Count = 3

Names
● ──► "A\0"
● ──► "B\0"
● ──► "C\0"
```

### 끝에 `nullptr`을 배치

```cpp
const char* Names[] =
{
    "A",
    "B",
    "C",
    nullptr
};

for (std::size_t Index = 0; Names[Index] != nullptr; ++Index)
{
    std::cout << Names[Index] << '\n';
}
```

```text
● ──► "A\0"
● ──► "B\0"
● ──► "C\0"
nullptr
```

개수를 사용한다면 정확한 개수를 전달해야 하고, 종료 표식을 사용한다면 반드시 접근 가능한 범위 안에 `nullptr`이 있어야 한다.

---

## 6. `argc`와 `argv`

명령줄에서 다음과 같이 프로그램을 실행한다고 가정한다.

```text
program.exe input.txt 100 hello
```

`main`은 다음 두 형태 중 하나로 명령줄 인자를 받을 수 있다.

```cpp
int main(int argc, char* argv[])
```

```cpp
int main(int argc, char** argv)
```

함수 매개변수에서 `char* argv[]`가 `char** argv`로 조정되므로 두 선언은 같은 의미다.

```text
argc = 4

argv
┌─────────┐
│ pointer │ ──► "program.exe\0"
├─────────┤
│ pointer │ ──► "input.txt\0"
├─────────┤
│ pointer │ ──► "100\0"
├─────────┤
│ pointer │ ──► "hello\0"
├─────────┤
│ nullptr │  ← argv[argc]
└─────────┘
```

- `argc`: 전달된 문자열의 개수
- `argv`: 첫 번째 문자 포인터 원소를 가리키는 포인터
- `argv[0]`: 일반적으로 프로그램을 실행할 때 사용한 이름
- `argv[argc]`: 표준이 보장하는 널 포인터

따라서 `argv`는 `argc`로 개수를 알 수 있고, 끝의 `nullptr`도 보장된다. 일반적으로는 의미가 명확한 `argc`를 기준으로 순회한다.

---

## 7. `argv[2][1]`의 의미

```text
argv[2]     ──► "100\0"
                    ▲
                    │
                argv[2][1]
```

```cpp
argv[2]     // 세 번째 문자열을 가리키는 char*
argv[2][1]  // 세 번째 문자열의 두 번째 문자인 '0'
```

첨자는 왼쪽부터 한 단계씩 해석한다.

```cpp
argv[2][1]

// 같은 의미
*(*(argv + 2) + 1)
```

안전하게 접근하려면 문자열과 문자 인덱스가 모두 범위 안인지 확인해야 한다.

```cpp
if (argc > 2 && std::strlen(argv[2]) > 1)
{
    const char Value = argv[2][1];
}
```

---

## 핵심 타입 비교

| 타입 또는 표현 | 정확한 의미 | 길이·끝을 판단하는 방법 |
| --- | --- | --- |
| `char*` | `char`를 가리키는 포인터 | 포인터 자체는 모름. C 문자열이라면 `'\0'` |
| `const char*` | 수정하지 않을 `char`를 가리키는 포인터 | 포인터 자체는 모름. C 문자열이라면 `'\0'` |
| `char[N]` | 문자 N개의 배열 | 배열 타입의 `N`; C 문자열인지는 `'\0'` 포함 여부로 별도 판단 |
| `char[R][C]` | `char[C]` 행 R개의 2차원 배열 | 타입의 행·열 크기 |
| `char*[N]` | 문자 포인터 N개의 배열 | 배열 타입의 `N`; 각 문자열은 `'\0'` |
| `char**` | `char*`를 가리키는 포인터 | 포인터 자체는 모름. 개수 또는 `nullptr` 필요 |
| `argv` | `main`이 받는 문자 포인터 배열의 첫 원소를 가리키는 포인터 | `argc`; 추가로 `argv[argc] == nullptr` 보장 |

---

## 현대 C++에서의 대안

저수준 API나 C API를 다룰 때는 `char*`, `char**`를 이해해야 한다. 직접 설계하는 C++ 코드에서는 목적에 맞는 타입을 사용하면 길이와 소유권을 더 명확하게 표현할 수 있다.

| 목적 | 우선 고려할 타입 |
| --- | --- |
| 문자열을 소유하고 수정 | `std::string` |
| 문자열을 소유하지 않고 읽기 | `std::string_view` |
| 고정 크기 배열 | `std::array<T, N>` |
| 연속된 배열을 길이와 함께 전달 | `std::span<T>` |
| 여러 문자열을 소유 | `std::vector<std::string>` |

> [!caution]
> `std::string_view`는 길이를 알고 있지만 마지막에 `'\0'`이 있다고 보장하지 않는다. C 문자열 API에 넘길 때는 별도로 널 종료 여부를 확인해야 한다.

---

## 정리

1. `char*`는 문자열이 아니라 **문자 포인터**다.
2. `char*`가 유효한 연속 메모리와 마지막 `'\0'`을 가리킬 때 C 문자열로 사용할 수 있다.
3. 일반 배열은 `'\0'`으로 끝나지 않으며 크기가 배열 타입에 포함된다.
4. 배열이 포인터로 변환되면 원래 원소 개수 정보가 사라진다.
5. 진짜 2차원 배열 `T[R][C]`은 연속된 메모리이며 `T**`와 구조가 다르다.
6. `char**`는 `char*`를 가리키는 포인터이므로 개수나 `nullptr` 같은 종료 정보가 필요하다.
7. `argv`는 `argc`로 개수를 알며 `argv[argc] == nullptr`도 보장된다.

---

[[DLL과 공개 C API]]
