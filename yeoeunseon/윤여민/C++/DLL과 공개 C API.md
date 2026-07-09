---
tags: [cpp, dll, api, abi, windows]
created: 2026-06-30
updated: 2026-07-09
---

# DLL & 공개 C API 핵심 정리

> [!summary]
> - **DLL**은 EXE가 불러 사용하는 컴파일된 기능 라이브러리다.
> - `export`/`import`는 DLL 경계를 넘는 함수를 표시한다.
> - `extern "C"`는 C++ 이름 맹글링을 피해서 공개 함수 이름을 단순하게 유지한다.
> - `void*` Handle은 DLL 내부 C++ 타입을 외부에 노출하지 않는 불투명 포인터다.
> - 결과 버퍼를 호출자가 만들면 메모리 소유권도 호출자에게 남는다.

```text
EXE
→ 공개 C API 호출
→ DLL 내부 Handle을 실제 C++ 객체로 복원
→ 작업 수행
→ 호출자가 준비한 결과 메모리에 기록
```

---

## 1. DLL

DLL은 **EXE가 실행 중(Runtime)에 불러 사용하는 컴파일된 기능 라이브러리**이다.

| EXE | DLL |
| --- | --- |
| `main()`이 있다 | `main()`이 없다 |
| 스스로 실행한다 | EXE가 호출해야 실행된다 |
| 프로그램이다 | 기능을 모아 둔 라이브러리이다 |

### 왜 DLL로 분리할까?

- 기능 분리
- 재사용
- 내부 구현 숨김
- DLL만 교체하여 업데이트 가능

---

## 2. EXE ↔ DLL 통신

```text
EXE
 ↓
공개 함수 호출
 ↓
DLL
 ↓
실제 기능 수행
 ↓
결과 반환
```

> **EXE는 요청하고, DLL이 실제 작업을 수행한다.**

---

## 3. `export` / `import`

```cpp
#define INSP_API __declspec(dllexport)
```

→ DLL에서 함수를 공개한다.

```cpp
#define INSP_API __declspec(dllimport)
```

→ EXE에서 DLL의 함수를 사용한다.

> **핵심:** 컴파일 전에 컴파일러에게 “이 함수는 DLL에서 공개되거나 사용된다”라고 알려 주는 표시이다.

---

## 4. `extern "C"`

### 일반 C++

```text
Insp_Run
   ↓
?Insp_Run@@...
```

### `extern "C"` 사용

```text
Insp_Run
   ↓
Insp_Run
```

> **핵심:** 함수 이름 변경(Name Mangling)을 막아 DLL 밖에서도 같은 이름으로 찾을 수 있게 한다.

---

## 5. Handle — `typedef void* InspHandle;`

```cpp
typedef void* InspHandle;
```

즉, `InspHandle`은 `void*`이다.

```text
Engine*
   ↓
void* (InspHandle)
   ↓
Engine*
```

### 핵심

- `void*`는 어떤 객체의 주소든 저장할 수 있다.
- 주소는 그대로 유지된다.
- 타입 정보만 숨긴다.
- DLL 내부에서 다시 `Engine*`로 복원한다.

### 왜 사용할까?

`Engine` 클래스를 외부에 공개하지 않기 위해서이다.

---

## 6. 왜 다시 `Engine*`로 복원할까?

```cpp
void* handle;

handle->Run();  // ❌ 불가능
```

```cpp
Engine* engine = static_cast<Engine*>(handle);

engine->Run();  // ✅ 가능
```

> **핵심:** 컴파일러가 객체 타입을 알아야 멤버 함수와 멤버 변수에 접근할 수 있기 때문이다.

여기서 `static_cast<Engine*>`는 "이 Handle은 원래 `Engine*`였고, API 규약상 다시 `Engine*`로 복원해도 된다"는 전제가 있을 때 사용하는 변환이다. 캐스팅 자체가 Handle의 실제 타입을 검사해주지는 않는다.

관련 캐스팅 개념은 [[C++ 캐스팅 4종]]에서 따로 정리한다.

---

## 7. `&result`

```cpp
InspResult result;

Insp_GetResult(handle, &result);
```

```text
EXE
메모리 생성
   ↓
DLL
값만 채움
   ↓
EXE
사용 및 해제
```

> **핵심:** 메모리의 소유권은 호출자(EXE)가 가진다. DLL은 전달받은 메모리에 값만 작성한다.

---

## 정리

1. DLL은 EXE가 실행 중에 사용하는 기능 라이브러리이다.
2. DLL로 나누는 이유는 기능 분리, 재사용, 캡슐화, 유지보수 때문이다.
3. EXE와 DLL은 **공개 함수(C API)**로 통신한다.
4. `export` / `import`는 컴파일러에게 DLL 공개·사용 여부를 알려 주는 표시이다.
5. `extern "C"`는 함수 이름 변경(Name Mangling)을 막는다.
6. `InspHandle`은 `Engine*`를 숨기기 위한 `void*`이며, 주소는 그대로 두고 타입만 숨긴다.
7. `&result`는 메모리 소유권을 EXE가 유지하고 DLL은 값만 채우기 위한 방식이다.

---

[[C++ 정리 목차]] · [[C++ 캐스팅 4종]] · [[char 포인터]] · [[배열과 C 문자열]]
