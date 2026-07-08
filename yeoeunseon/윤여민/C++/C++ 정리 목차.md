---
tags: [index, cpp, study-note]
created: 2026-07-07
updated: 2026-07-08
---

# C++ 정리 목차

> [!summary]
> C++ 폴더는 Unreal과 시스템 프로그래밍을 이해하기 위한 기반 개념을 모아둔 곳이다.
> 포인터·배열·메모리 배치처럼 헷갈리기 쉬운 부분은 먼저 구조를 잡고, 가상 함수·템플릿·동시성은 필요할 때 깊게 들어간다.

## 주제별 위치

| 주제 | 문서 | 핵심 |
| --- | --- | --- |
| 포인터와 문자열 입구 | [[char 포인터와 배열]] | `char*`, `char**`, 배열 문서로 들어가는 입구 |
| 배열과 C 문자열 | [[배열과 C 문자열]] | 배열 길이, `'\0'`, 2차원 배열, `T[R][C]` |
| `char*`와 `char**` | [[char 포인터]] | `char*`는 문자열 자체가 아니고, `char**`는 `char*`를 가리키는 포인터 |
| 다형성 | [[가상 함수]] | 실제 객체 타입 기준 함수 호출 |
| 제네릭 프로그래밍 | [[템플릿]] | 타입이나 컴파일 타임 값을 받는 코드의 틀 |
| 메모리 성능 | [[Cache Locality]] | 연속 메모리와 캐시 효율 |
| 컨테이너 원소 삭제 | [[erase-remove idiom]] | `remove_if`로 모으고 `erase`로 실제 크기 줄이기 |
| `static` 키워드 | [[static 키워드]] | 위치에 따라 수명, 링크 범위, 공유 여부, `this` 유무가 달라짐 |
| 동시성 | [[Race Condition]] | 공유 데이터, mutex, atomic, happens-before |
| DLL 경계 | [[DLL과 공개 C API]] | `extern "C"`, export/import, handle, 버퍼 소유권 |

---

## 읽는 흐름

```text
배열과 포인터
→ C 문자열과 char**
→ 가상 함수와 템플릿
→ 캐시 지역성과 컨테이너 선택
→ erase-remove idiom
→ static 키워드
→ Race Condition과 비동기
→ DLL 공개 API
```

Unreal 문서에서 막히면 보통 아래처럼 돌아오면 된다.

| Unreal에서 막힌 부분 | 돌아볼 C++ 문서 |
| --- | --- |
| UObject 포인터, 약한 참조, 배열 전달 | [[char 포인터와 배열]], [[배열과 C 문자열]] |
| `virtual`, 인터페이스, 소멸자 | [[가상 함수]] |
| `TArray<T>`, `TMap<K, V>`, 타입별 코드 | [[템플릿]] |
| 프레임 성능, 컨테이너 순회 | [[Cache Locality]], [[erase-remove idiom]] |
| Worker Thread, 공유 데이터 | [[Race Condition]] |
| 외부 모듈과 API 경계 | [[DLL과 공개 C API]] |

---

[[정리집 목차]]
