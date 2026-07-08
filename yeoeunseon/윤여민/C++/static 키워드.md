---
title: static 키워드
tags: [cpp, static, storage-duration, linkage, class-member, memory]
created: 2026-07-08
updated: 2026-07-08
---

# `static` 키워드

> [!summary]
> **C++의 `static`은 붙는 위치에 따라 의미가 달라진다.**
>
> ```text
> 전역 static        = 이 .cpp 파일 안에서만 보이게 함
> 함수 안 static     = 함수가 끝나도 값이 유지됨
> 클래스 static 변수  = 객체마다가 아니라 클래스에 1개만 존재
> 클래스 static 함수  = 객체 없이 호출하며 this가 없음
> ```
>
> `static`을 볼 때는 먼저 “어디에 붙었는가?”를 봐야 한다.

---

## 핵심 정리

```text
1. static은 위치에 따라 의미가 다르다.
2. 전역 변수/함수 앞 static은 현재 .cpp 파일 안으로 이름을 숨긴다.
3. 함수 안 static 지역 변수는 함수가 끝나도 값이 유지된다.
4. 클래스 static 멤버 변수는 객체마다 생기지 않고 클래스에 1개만 존재한다.
5. 클래스 static 멤버 함수는 this가 없어서 일반 멤버에 바로 접근할 수 없다.
6. C++17부터 inline static 멤버 변수는 헤더에서 정의할 수 있다.
7. static 지역 변수는 처음 실행될 때 한 번 초기화되며, C++11부터 초기화가 thread-safe다.
```

기준:

```text
static은 “오래 산다”만 뜻하는 게 아니라, 위치에 따라 수명·공유·링크 범위를 바꾼다.
```

---

## 위치별 의미

| 위치 | 예시 | 핵심 의미 |
| --- | --- | --- |
| 전역 변수 | `static int GValue;` | 이 `.cpp` 파일 안에서만 보임 |
| 전역 함수 | `static void Helper();` | 이 `.cpp` 파일 안에서만 호출 가능 |
| 함수 안 지역 변수 | `static int Count;` | 함수가 끝나도 값 유지 |
| 클래스 멤버 변수 | `static int Count;` | 객체마다가 아니라 클래스에 1개 |
| 클래스 멤버 함수 | `static void Print();` | 객체 없이 호출, `this` 없음 |

같은 `static`이어도 “전역에 붙었는지”, “함수 안에 붙었는지”, “클래스 안에 붙었는지”에 따라 봐야 할 포인트가 다르다.

---

## 1. 전역 `static`: 이 파일 안에서만 보이게 한다

전역 변수나 전역 함수 앞에 `static`을 붙이면 **현재 소스 파일 내부에서만 보이는 이름**이 된다.

```cpp
// Enemy.cpp
static int GSpawnCount = 0;

static void PrintDebugLog()
{
}
```

이 `GSpawnCount`와 `PrintDebugLog`는 `Enemy.cpp` 안에서만 사용할 수 있다.

다른 `.cpp` 파일에서 같은 이름을 써도 서로 다른 이름처럼 취급된다.

```cpp
// Player.cpp
static int GSpawnCount = 0; // Enemy.cpp의 GSpawnCount와 충돌하지 않음
```

이것을 **내부 링크(Internal Linkage)**라고 한다.

```text
링크 범위가 현재 번역 단위(.cpp) 안으로 제한된다.
```

> [!note]
> 현대 C++에서는 전역 `static` 함수/변수 대신 익명 네임스페이스를 쓰는 경우도 많다.

```cpp
namespace
{
    int GSpawnCount = 0;

    void PrintDebugLog()
    {
    }
}
```

의도는 같다.

```text
이 파일 안에서만 쓰는 내부 구현이다.
```

---

## 2. 함수 안 `static`: 함수가 끝나도 값이 유지된다

일반 지역 변수는 함수가 끝나면 사라진다.

```cpp
void Count()
{
    int Value = 0;
    ++Value;
    std::cout << Value << '\n';
}
```

이 함수는 호출할 때마다 `Value`가 다시 0에서 시작한다.

```text
Count() → 1
Count() → 1
Count() → 1
```

하지만 지역 변수에 `static`을 붙이면 함수가 끝나도 값이 유지된다.

```cpp
void Count()
{
    static int Value = 0;
    ++Value;
    std::cout << Value << '\n';
}
```

```text
Count() → 1
Count() → 2
Count() → 3
```

이때 `Value`는 함수 안에서만 이름을 사용할 수 있지만, 수명은 프로그램 종료까지 이어진다.

```text
접근 범위: Count 함수 안
수명: 프로그램 종료까지
초기화: 처음 실행될 때 한 번
```

> [!important]
> C++11부터 함수 안 `static` 지역 변수의 초기화는 thread-safe로 보장된다. 여러 스레드가 동시에 처음 호출해도 객체가 한 번만 초기화되도록 보장한다.
>
> 다만 이것은 **초기화가 안전하다**는 뜻이다. 초기화 이후 그 객체를 여러 스레드가 동시에 읽고 쓰는 것까지 자동으로 안전해지는 것은 아니다. 공유 상태를 수정한다면 [[Race Condition]]을 고려해야 한다.

---

## 3. 클래스 `static` 멤버 변수: 객체마다가 아니라 클래스에 1개

일반 멤버 변수는 객체마다 따로 있다.

```cpp
class Player
{
public:
    int Hp = 100;
};

Player A;
Player B;
```

```text
A.Hp와 B.Hp는 서로 다른 변수
```

하지만 `static` 멤버 변수는 객체마다 생기지 않는다.

```cpp
class Player
{
public:
    int Hp = 100;
    static int PlayerCount;
};
```

```text
Player::PlayerCount는 클래스에 1개만 존재
A와 B가 공유
```

예시:

```cpp
class Player
{
public:
    static int PlayerCount;

    Player()
    {
        ++PlayerCount;
    }
};

int Player::PlayerCount = 0;
```

```cpp
Player A;
Player B;

std::cout << Player::PlayerCount; // 2
```

### 왜 클래스 밖에서 정의하나?

클래스 안의 이 줄은 보통 “선언”이다.

```cpp
static int PlayerCount;
```

실제 저장 공간은 한 곳에만 있어야 하므로 `.cpp`에서 따로 정의한다.

```cpp
int Player::PlayerCount = 0;
```

```text
클래스 안 선언
→ 이런 static 멤버가 있다

.cpp 정의
→ 실제 메모리 공간을 만든다
```

### C++17부터 `inline static`

C++17부터는 `inline static`을 사용하면 헤더 안에서 바로 정의할 수 있다.

```cpp
class Player
{
public:
    inline static int PlayerCount = 0;
};
```

이 경우 별도의 `.cpp` 정의가 필요 없다.

---

## 4. 클래스 `static` 멤버 함수: `this`가 없다

일반 멤버 함수는 특정 객체를 대상으로 호출된다.

```cpp
Player A;
A.PrintHp();
```

이때 함수 안에는 “나를 호출한 객체”를 가리키는 `this`가 있다.

```text
this → A 객체
```

하지만 `static` 멤버 함수는 객체 없이 호출할 수 있다.

```cpp
class Player
{
public:
    int Hp = 100;
    inline static int PlayerCount = 0;

    static void PrintPlayerCount()
    {
        std::cout << PlayerCount << '\n';
    }
};

Player::PrintPlayerCount();
```

객체 없이 호출할 수 있으므로 `this`가 없다.

그래서 일반 멤버 변수에 바로 접근할 수 없다.

```cpp
class Player
{
public:
    int Hp = 100;

    static void Print()
    {
        // std::cout << Hp; // 오류: 어떤 객체의 Hp인지 알 수 없음
    }
};
```

접근하려면 객체를 직접 받아야 한다.

```cpp
class Player
{
public:
    int Hp = 100;

    static void PrintHp(const Player& Target)
    {
        std::cout << Target.Hp << '\n';
    }
};
```

정리:

```text
static 멤버 함수는 클래스 소속 함수지만, 특정 객체 소속 함수가 아니다.
```

---

## 메모리와 수명 관점

`static` 변수는 일반적으로 **정적 저장 기간(static storage duration)**을 가진다.

프로그램이 실행되는 동안 오래 살아 있는 저장 공간을 가진다고 보면 된다.

| 변수 | 이름을 쓸 수 있는 범위 | 값이 살아 있는 기간 |
| --- | --- | --- |
| 일반 지역 변수 | 함수 안 | 함수 실행 중 |
| 함수 안 `static` 지역 변수 | 함수 안 | 프로그램 종료까지 |
| 전역 `static` 변수 | 현재 `.cpp` 파일 안 | 프로그램 종료까지 |
| 클래스 `static` 멤버 변수 | 클래스 접근 규칙에 따름 | 프로그램 종료까지 |

다만 초기화 시점은 종류에 따라 다르게 봐야 한다.

```text
전역/static 멤버 변수
→ 프로그램 시작 전후 초기화

함수 안 static 지역 변수
→ 해당 코드가 처음 실행될 때 초기화
```

---

## 지역 `static`과 싱글톤

함수 안 `static` 지역 변수는 처음 호출될 때 한 번만 만들어지고 이후 계속 유지된다.

이 특성을 이용해 싱글톤을 만들 수 있다.

```cpp
class GameManager
{
public:
    static GameManager& Get()
    {
        static GameManager Instance;
        return Instance;
    }

private:
    GameManager() = default;
};
```

```text
첫 Get 호출
→ Instance 생성

그 이후 Get 호출
→ 이미 만들어진 Instance 반환
```

이 방식을 흔히 **Meyers Singleton**이라고 부른다.

---

## 정적 초기화 순서 문제

전역 객체나 `static` 멤버 객체는 프로그램 시작 시점에 초기화될 수 있다.

문제는 서로 다른 `.cpp` 파일에 있는 전역 객체들의 초기화 순서가 기대와 다를 수 있다는 점이다.

```cpp
// A.cpp
Logger GLogger;

// B.cpp
Config GConfig;
```

`GConfig` 생성자에서 `GLogger`를 사용한다고 해보자.

```text
GLogger가 먼저 초기화된다고 보장되지 않음
→ 아직 준비되지 않은 객체를 사용할 수 있음
```

이런 문제를 **정적 초기화 순서 문제(SIOF, Static Initialization Order Fiasco)**라고 한다.

피하는 대표적인 방법 중 하나는 함수 안 `static` 지역 변수로 지연 초기화하는 것이다.

```cpp
Logger& GetLogger()
{
    static Logger Instance;
    return Instance;
}
```

```text
프로그램 시작 때 무조건 만들지 않음
→ 처음 필요할 때 생성
→ 초기화 순서 문제를 줄일 수 있음
```

---

## `static_cast`와는 다른 이야기

`static_cast`에도 `static`이라는 단어가 들어가지만, 이 페이지의 `static` 키워드 용법과는 다른 문법이다.

```cpp
Engine* EnginePtr = static_cast<Engine*>(Handle);
```

`static_cast`는 컴파일 타임에 타입 변환을 명시하는 캐스트 문법이다.

```text
static 키워드
→ 수명, 링크 범위, 클래스 공유, this 여부와 관련

static_cast
→ 타입 변환 문법
```

---

## 언제 사용하나?

| 상황 | 사용 |
| --- | --- |
| 이 `.cpp` 파일 안에서만 쓸 helper 함수 | 전역 `static` 함수 또는 익명 네임스페이스 |
| 함수 호출 횟수처럼 값이 계속 유지되어야 함 | 함수 안 `static` 지역 변수 |
| 모든 객체가 공유하는 값이 필요함 | 클래스 `static` 멤버 변수 |
| 객체 없이 호출하는 유틸리티성 클래스 함수 | 클래스 `static` 멤버 함수 |
| 싱글톤 인스턴스를 지연 생성하고 싶음 | 함수 안 `static` 지역 변수 |

---

## 정리

1. `static`은 위치에 따라 의미가 달라진다.
2. 전역 `static`은 이름을 현재 `.cpp` 파일 안으로 숨긴다.
3. 함수 안 `static` 지역 변수는 함수가 끝나도 값이 유지된다.
4. 클래스 `static` 멤버 변수는 객체마다가 아니라 클래스에 1개만 존재한다.
5. 클래스 `static` 멤버 함수는 `this`가 없어서 일반 멤버에 바로 접근할 수 없다.
6. C++17부터 `inline static` 멤버 변수는 헤더에서 정의할 수 있다.
7. 전역 static 객체는 초기화 순서 문제를 조심해야 한다.

---

[[C++ 정리 목차]] · [[DLL과 공개 C API]] · [[Race Condition]]
