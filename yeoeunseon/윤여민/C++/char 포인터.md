---
tags: [cpp, pointer, char-pointer, c-string, argv, memory]
created: 2026-07-07
updated: 2026-07-07
---

# `char*`와 `char**`

> [!summary]
> **이 글의 핵심은 `char*`와 `char**`를 문자열이나 2차원 배열로 외우지 않는 것이다.**
>
> ```text
> char*   = char를 가리키는 포인터
> char**  = char*를 가리키는 포인터
> ```
>
> `char*`가 C 문자열처럼 보이는 이유는 포인터가 가리키는 메모리에 문자들이 연속으로 있고, 끝에 `'\0'`이 있기 때문이다.

---

## 먼저 이것만 알면 된다

| 표현 | 정확한 의미 |
| --- | --- |
| `char*` | `char` 하나를 가리킬 수 있는 포인터 |
| `const char*` | 수정하지 않을 `char`를 가리키는 포인터 |
| `char**` | `char*`를 가리키는 포인터 |
| `char* argv[]` | 함수 매개변수에서는 `char** argv`와 같은 의미 |

`char*`는 문자열 그 자체가 아니다.

```cpp
char Character = 'A';
char* Pointer = &Character;
```

이 경우 `Pointer`는 문자 하나를 가리킨다.

```text
Pointer
  |
  v
'A'
```

이 포인터를 문자열 함수에 넘기면 위험하다.

```cpp
std::printf("%s", Pointer); // 잘못된 사용
```

`%s`는 `'\0'`을 만날 때까지 계속 읽는다. 그런데 위 코드는 `'A'` 다음에 `'\0'`이 있다고 보장하지 않는다.

---

## `char*`가 문자열처럼 쓰이는 경우

```cpp
char Text[] = "Hello";
char* Pointer = Text;
```

`"Hello"`로 초기화된 문자 배열은 마지막에 종료 문자 `'\0'`이 들어간다.

```text
Index    0    1    2    3    4     5
Text   ['H']['e']['l']['l']['o']['\0']
         ^
         |
      Pointer
```

이때 `Pointer`는 첫 번째 문자 `'H'`를 가리킨다.

중요한 점은 `Pointer` 안에 문자열 길이 5가 저장되어 있지 않다는 것이다. `strlen`, `strcmp`, `printf("%s")` 같은 C 문자열 함수는 `'\0'`을 만날 때까지 메모리를 읽는다.

> [!important]
> `char*`가 C 문자열로 쓰이려면 조건이 필요하다.
>
> 1. 유효한 문자 메모리를 가리켜야 한다.
> 2. 문자들이 연속으로 있어야 한다.
> 3. 읽을 수 있는 범위 안에 `'\0'`이 있어야 한다.

---

## `const char*`와 문자열 리터럴

```cpp
const char* Literal = "Hello";
char Buffer[] = "Hello";
```

둘은 비슷해 보이지만 다르다.

| 코드 | 의미 |
| --- | --- |
| `const char* Literal = "Hello";` | 수정하지 않을 문자열 리터럴을 가리킨다 |
| `char Buffer[] = "Hello";` | 수정 가능한 문자 배열을 만든다 |

```cpp
// Literal[0] = 'Y'; // 오류 또는 위험한 코드
Buffer[0] = 'Y';     // 가능
```

문자열 리터럴은 수정하면 안 된다. 그래서 C++에서는 보통 `const char*`로 받는다.

수정 가능한 문자열이 필요하면 `char[]` 또는 `std::string`을 사용한다.

---

## `char**`는 무엇인가

`char**`는 `char*`를 가리키는 포인터다.

```cpp
char** PointerToPointer;
```

읽는 방향은 오른쪽에서 왼쪽으로 생각하면 편하다.

```text
char** Pointer

Pointer
  |
  v
char*
  |
  v
char
```

자주 만나는 형태는 문자열 여러 개를 다룰 때다.

```cpp
const char* Names[] =
{
    "Alice",
    "Bob",
    "Chris"
};
```

메모리 느낌은 이렇다.

```text
Names[0]  -> "Alice\0"
Names[1]  -> "Bob\0"
Names[2]  -> "Chris\0"
```

`Names`는 문자열 자체를 한 덩어리로 들고 있는 2차원 배열이 아니다. `const char*` 포인터 3개를 가진 배열이다.

---

## `char**`가 2차원 배열처럼 느껴지는 이유

함수 매개변수에서 배열은 포인터처럼 조정된다.

```cpp
void PrintNames(const char* Names[], std::size_t Count);
```

위 선언은 매개변수 위치에서는 사실상 아래와 같다.

```cpp
void PrintNames(const char** Names, std::size_t Count);
```

그래서 `Names[Index]`로 문자열 하나를 얻고, `Names[Index][CharIndex]`로 그 문자열 안의 문자 하나를 얻을 수 있다.

```cpp
const char* Names[] = { "Alice", "Bob", "Chris" };

Names[1];     // "Bob"을 가리키는 const char*
Names[1][0];  // 'B'
Names[1][1];  // 'o'
```

하지만 이것을 진짜 2차원 배열과 같다고 생각하면 안 된다.

```text
const char* Names[3]

Names 배열 안에는 포인터 3개가 있다.
각 포인터가 가리키는 문자열은 서로 다른 위치에 있을 수 있다.
```

---

## `char**`는 끝을 모른다

`char**`도 포인터다. 따라서 원소가 몇 개인지 스스로 알지 못한다.

보통 두 가지 방법 중 하나를 쓴다.

### 1. 개수를 함께 전달

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
Names -> "Alice"
      -> "Bob"
      -> "Chris"

Count = 3
```

### 2. 마지막에 `nullptr` 배치

```cpp
const char* Names[] =
{
    "Alice",
    "Bob",
    "Chris",
    nullptr
};

for (std::size_t Index = 0; Names[Index] != nullptr; ++Index)
{
    std::cout << Names[Index] << '\n';
}
```

```text
Names -> "Alice"
      -> "Bob"
      -> "Chris"
      -> nullptr
```

개수를 쓰든 종료 표식을 쓰든, 포인터 밖에서 길이 정보를 정해야 한다.

---

## `argc`와 `argv`

명령줄에서 프로그램을 이렇게 실행했다고 하자.

```text
program.exe input.txt 100 hello
```

`main`은 보통 이렇게 받을 수 있다.

```cpp
int main(int argc, char* argv[])
```

또는 이렇게 받을 수 있다.

```cpp
int main(int argc, char** argv)
```

함수 매개변수에서는 `char* argv[]`가 `char** argv`로 조정되므로 두 선언은 같은 의미다.

```text
argc = 4

argv[0] -> "program.exe\0"
argv[1] -> "input.txt\0"
argv[2] -> "100\0"
argv[3] -> "hello\0"
argv[4] -> nullptr
```

| 표현 | 의미 |
| --- | --- |
| `argc` | 전달된 문자열 개수 |
| `argv` | 첫 번째 `char*`를 가리키는 포인터 |
| `argv[0]` | 보통 실행 파일 이름 |
| `argv[argc]` | 표준상 `nullptr` |

---

## `argv[2][1]` 읽는 법

```cpp
argv[2][1]
```

왼쪽부터 차례대로 읽으면 된다.

```text
argv[2]     -> 세 번째 문자열
argv[2][1]  -> 세 번째 문자열의 두 번째 문자
```

예를 들어 다음과 같다면:

```text
argv[2] -> "100\0"
```

`argv[2][1]`은 `'0'`이다.

포인터 식으로 풀면 아래와 같다.

```cpp
argv[2][1]
*(*(argv + 2) + 1)
```

실제 코드에서는 범위를 먼저 확인해야 한다.

```cpp
if (argc > 2 && std::strlen(argv[2]) > 1)
{
    const char Value = argv[2][1];
}
```

---

## 실무에서 더 자주 쓰는 타입

저수준 API나 C API를 다룰 때는 `char*`, `char**`를 이해해야 한다. 하지만 직접 설계하는 C++ 코드에서는 더 명확한 타입을 쓰는 편이 좋다.

| 목적 | 추천 타입 |
| --- | --- |
| 문자열을 소유하고 수정 | `std::string` |
| 문자열을 소유하지 않고 읽기 | `std::string_view` |
| 여러 문자열을 소유 | `std::vector<std::string>` |
| C API에 문자열 전달 | `const char*` |
| C API에서 문자열 목록 전달 | `const char* const*`와 개수 |

> [!caution]
> `std::string_view`는 길이를 알고 있지만 마지막에 `'\0'`이 있다고 보장하지 않는다. C 문자열 API에 넘길 때는 별도로 주의해야 한다.

---

## 정리

1. `char*`는 문자열이 아니라 `char`를 가리키는 포인터다.
2. `char*`가 C 문자열로 쓰이려면 끝에 `'\0'`이 있어야 한다.
3. `const char*`는 수정하지 않을 문자를 가리킬 때 사용한다.
4. `char**`는 `char*`를 가리키는 포인터다.
5. `char**`는 원소 개수를 모르므로 개수나 `nullptr` 같은 종료 정보가 필요하다.
6. `argv`는 대표적인 `char**` 예시다.

---

[[C++ 정리 목차]] · [[배열과 C 문자열]]
