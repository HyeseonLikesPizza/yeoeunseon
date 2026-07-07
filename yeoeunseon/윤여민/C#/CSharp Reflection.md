---
tags: [csharp, dotnet, reflection, unity]
created: 2026-07-07
updated: 2026-07-07
---

# C# Reflection

> [!summary]
> C# Reflection은 실행 중인 코드에서 타입, 필드, 프로퍼티, 메서드, Attribute 정보를 조회하는 .NET 기능이다.
> Unity를 공부할 때도 Attribute, 직렬화, 에디터 도구, 런타임 타입 처리와 자주 연결된다.

## 기본 흐름

```text
C# 타입 또는 객체
        ↓
Type 객체
        ↓
필드·프로퍼티·메서드·Attribute 정보 조회
        ↓
필요하면 값 읽기, 값 쓰기, 메서드 호출
```

```csharp
Type type = typeof(Player);

PropertyInfo? hpProperty = type.GetProperty("Hp");
MethodInfo? attackMethod = type.GetMethod("Attack");

object? hpValue = hpProperty?.GetValue(player);
attackMethod?.Invoke(player, null);
```

핵심은 `Type`이다.  
`typeof(Player)`나 `player.GetType()`으로 타입 정보를 얻고, 그 안에서 멤버 정보를 꺼내 쓴다.

---

## 자주 쓰는 정보

| 정보 | 예시 API | 의미 |
| --- | --- | --- |
| 타입 정보 | `typeof(Player)`, `obj.GetType()` | 어떤 클래스·구조체인지 확인 |
| 필드 | `GetField()` | 멤버 변수 조회 |
| 프로퍼티 | `GetProperty()` | C# 프로퍼티 조회 |
| 메서드 | `GetMethod()` | 함수 정보 조회와 동적 호출 |
| Attribute | `GetCustomAttributes()` | `[Serializable]` 같은 추가 정보 확인 |

---

## Unreal Reflection과 다른 점

| 구분 | C# Reflection | [[Reflection|Unreal Reflection]] |
| --- | --- | --- |
| 기반 | .NET 런타임 메타데이터 | UHT가 생성한 Unreal 메타데이터 |
| 표시 방법 | Attribute | `UCLASS`, `UPROPERTY`, `UFUNCTION` |
| 기본 대상 | C# 타입 대부분 | Unreal 매크로가 붙은 타입과 멤버 |
| 주된 용도 | 타입 조회, Attribute 확인, 동적 호출 | 에디터, 블루프린트, 직렬화, 리플리케이션, GC |

C# Reflection은 언어와 런타임이 제공하는 기능이다.

Unreal Reflection은 C++ 언어 자체의 기능이라기보다, Unreal Engine이 C++ 위에 얹은 엔진용 메타데이터 시스템이다.

---

## Unity에서 볼 때

Unity는 C#을 사용하므로 Reflection과 Attribute 개념이 자연스럽게 자주 나온다.

예를 들어 `[SerializeField]`, `[Range]`, `[Header]` 같은 Attribute는 필드에 추가 정보를 붙인다. Unity 에디터와 직렬화 시스템은 이런 정보를 읽어서 Inspector 표시나 저장 방식을 결정한다.

다만 Unity의 직렬화 규칙은 "C# Reflection으로 보이면 전부 저장된다"가 아니다. Unity가 지원하는 타입과 접근 규칙이 따로 있다.

---

## 주의할 점

- Reflection은 편하지만 일반 코드 호출보다 비용이 크다.
- 문자열로 멤버 이름을 찾으면 오타를 컴파일러가 잡아주지 못한다.
- Unity의 IL2CPP나 코드 스트리핑 환경에서는 동적 접근이 예상과 다르게 동작할 수 있다.
- 자주 반복되는 런타임 로직보다는 도구 코드, 에디터 확장, 초기화 단계에서 쓰는 편이 안전하다.

---

## 정리

- C# Reflection은 `Type`을 중심으로 타입과 멤버 정보를 조회하는 기능이다.
- Attribute는 코드에 추가 정보를 붙이는 방식이고, Reflection으로 읽을 수 있다.
- Unity에서는 Attribute, Inspector, 직렬화, 에디터 도구를 이해할 때 중요하다.
- Unreal Reflection과 이름은 비슷하지만 구현 방식과 목적이 다르다.

---

[[Reflection|Unreal Reflection]]
