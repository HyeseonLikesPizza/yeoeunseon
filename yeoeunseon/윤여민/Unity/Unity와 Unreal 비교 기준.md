---
tags: [unity, unreal-engine, csharp, comparison, study-note]
created: 2026-07-07
updated: 2026-07-07
---

# Unity와 Unreal 비교 기준

> [!summary]
> Unity와 Unreal을 비교할 때는 “어느 엔진이 좋다”보다 같은 문제를 각 엔진이 어떤 방식으로 해결하는지 보는 것이 중요하다.
> 특히 C# Reflection과 Unreal Reflection, Unity 직렬화와 Unreal GC/UPROPERTY는 나중에 자주 비교하게 될 축이다.

## 비교 축

| 관점 | Unity | Unreal |
| --- | --- | --- |
| 주 언어 | C# | C++ 중심, Blueprint 병행 |
| 기본 객체 단위 | `GameObject`, `Component`, `MonoBehaviour` | `UObject`, `AActor`, `UActorComponent` |
| 타입 정보 | .NET 메타데이터와 [[CSharp Reflection|C# Reflection]] | UHT 기반 [[Reflection]] |
| 에디터 노출 | C# 필드, Attribute, Unity 직렬화 규칙 | `UPROPERTY`, 지정자, Details Panel |
| 객체 수명 | Unity 엔진 객체와 C# GC가 함께 관여 | [[GC]]와 UObject 참조 그래프 |
| 게임 로직 시작 | `Awake`, `OnEnable`, `Start`, `Update` | 생성자, Construction, `BeginPlay`, `Tick` |
| 비동기 주의점 | Unity API는 보통 Main Thread 기준 | UObject 접근은 GameThread 기준, [[Async & ThreadPool]] 참고 |

---

## Reflection 비교

Unity 쪽 C# Reflection은 .NET 런타임이 가진 타입 정보를 조회하는 기능이다.  
Unreal Reflection은 C++ 헤더에 붙은 매크로를 UHT가 분석해서 엔진용 메타데이터를 만드는 시스템이다.

```text
Unity / C#
Type, FieldInfo, Attribute

Unreal / C++
UCLASS, UPROPERTY, UFUNCTION, UHT
```

이 차이 때문에 같은 “Reflection”이라는 단어를 써도 느낌이 다르다.

- Unity/C#: 런타임 타입 정보 조회
- Unreal: 엔진 시스템에 타입과 멤버 등록

---

## 나중에 비교하면 좋은 주제

| 주제 | Unity 쪽에서 볼 것 | Unreal 쪽에서 볼 것 |
| --- | --- | --- |
| 오브젝트 생명주기 | `Awake`, `Start`, `OnDestroy` | [[Actor Spawn 생명주기]] |
| 직렬화와 에디터 노출 | `[SerializeField]`, Inspector | [[Reflection]], `UPROPERTY` |
| 메모리 관리 | C# GC, Unity Object lifetime | [[GC]] |
| 비동기 | Coroutine, Task, Job System | [[Async & ThreadPool]] |
| 타입 정보 | [[CSharp Reflection|C# Reflection]] | [[Reflection]] |

---

## 정리

- Unity는 C#과 .NET 생태계 위에서 엔진 기능이 연결된다.
- Unreal은 C++ 위에 UObject, UHT, Reflection, GC 시스템을 얹어 엔진 기능을 만든다.
- 두 엔진을 비교할 때는 문법보다 객체 수명, 에디터 노출, 타입 정보, 스레드 규칙을 중심으로 보면 좋다.

---

[[정리집 목차]] · [[CSharp Reflection|C# Reflection]] · [[Reflection]]
