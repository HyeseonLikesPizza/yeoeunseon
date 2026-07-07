---
tags: [index, unreal-engine, cpp, study-note]
created: 2026-07-07
updated: 2026-07-07
---

# Unreal 정리 목차

> [!summary]
> Unreal 폴더는 엔진이 C++ 객체를 어떻게 관리하고 실행하는지 보는 곳이다.
> Actor 생명주기, Reflection, GC, Async는 따로 떨어진 개념이 아니라 UObject 시스템 위에서 서로 연결된다.

## 주제별 위치

| 주제 | 문서 | 핵심 |
| --- | --- | --- |
| Actor 생성 흐름 | [[Actor Spawn 생명주기]] | 생성자, Construction, 컴포넌트 초기화, BeginPlay |
| 엔진 메타데이터 | [[Reflection]] | UHT가 타입·프로퍼티·함수 정보를 엔진용으로 생성 |
| UObject 수명 | [[GC]] | Root Set과 참조 그래프로 UObject 정리 |
| 비동기 작업 | [[Async & ThreadPool]] | Worker Thread에서는 계산, UObject 반영은 GameThread |

---

## 연결해서 보기

```text
Actor Spawn
    ↓
UObject가 생성되고 컴포넌트가 준비됨
    ↓
Reflection이 타입과 프로퍼티 정보를 제공
    ↓
GC가 UObject 참조를 추적
    ↓
Async 작업은 UObject 접근 시 GameThread 규칙을 지켜야 함
```

| 궁금한 것 | 볼 문서 |
| --- | --- |
| 값은 언제 넣어야 하지? | [[Actor Spawn 생명주기]] |
| `UPROPERTY`가 왜 중요하지? | [[Reflection]], [[GC]] |
| 객체가 왜 갑자기 사라질 수 있지? | [[GC]] |
| 비동기에서 Actor를 직접 만져도 되나? | [[Async & ThreadPool]] |
| C# Reflection이랑 뭐가 다르지? | [[Reflection]], [[CSharp Reflection|C# Reflection]] |

---

## C++ 쪽으로 돌아갈 때

| Unreal 개념 | 연결되는 C++ 개념 |
| --- | --- |
| UObject 포인터와 유효성 | [[char 포인터와 배열]], [[가상 함수]] |
| `UCLASS`, `UPROPERTY`, `UFUNCTION` | [[Reflection]] |
| `TArray<T>`, `TMap<K, V>` | [[템플릿]], [[Cache Locality]] |
| Worker Thread와 GameThread | [[Race Condition]] |

---

[[정리집 목차]] · [[Unity와 Unreal 비교 기준]]
