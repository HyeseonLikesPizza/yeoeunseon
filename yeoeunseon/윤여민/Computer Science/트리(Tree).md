---
tags: [computer-science, data-structure, tree, graph, traversal, cpp]
created: 2026-07-16
updated: 2026-07-16
---

# 트리(Tree)

> [!summary]
> **트리는 부모-자식 관계로 데이터를 계층적으로 표현하는 자료구조다.**
>
> 일반 트리는 계층 구조를 표현하고, BST는 정렬 규칙으로 검색 방향을 정한다.  
> 자주 찾는 데이터는 HashMap 인덱스를 함께 두기도 한다.

---

## 핵심 정리

| 구분 | 역할 | 검색 방식 |
| --- | --- | --- |
| 일반 트리 | 부모-자식 계층 구조 표현 | DFS/BFS 순회 |
| BST | 정렬 규칙 기반 검색 | 값 비교 |
| 균형 트리 | 치우친 BST의 검색 성능 보완 | 균형을 유지하며 값 비교 |
| HashMap | key로 빠르게 찾기 위한 별도 인덱스 | 해시 |

트리는 구조를 표현하고, HashMap은 빠른 검색을 맡는다.

## 기본 흐름

```text
계층 구조가 필요하다
→ 트리로 표현한다.

일반 트리에서 특정 노드를 찾아야 한다
→ 정렬 규칙이 없으면 DFS/BFS로 순회한다.

값 비교로 탐색 방향을 정할 수 있다
→ BST를 검토한다.

검색을 자주 해야 한다
→ 트리 구조와 별도로 HashMap 인덱스를 둘 수 있다.
```

---

## 트리란?

트리는 데이터를 부모-자식 관계로 표현한다.

예를 들어 행정구역은 자연스럽게 트리처럼 볼 수 있다.

```text
경기도
├── 수원시
│   ├── 영통구
│   │   ├── 매탄동
│   │   └── 원천동
│   └── 장안구
└── 성남시
    └── 분당구
```

이 구조에서:

| 용어 | 의미 | 예시 |
| --- | --- | --- |
| Root | 가장 위의 노드 | 경기도 |
| Parent | 부모 노드 | 수원시는 영통구의 부모 |
| Child | 자식 노드 | 영통구는 수원시의 자식 |
| Leaf | 자식이 없는 노드 | 매탄동, 원천동 |
| Depth | 루트에서 얼마나 떨어졌는가 | 경기도는 깊이 0 |
| Subtree | 어떤 노드를 루트로 하는 작은 트리 | 수원시 이하 전체 |

---

## C++에서 트리는 어떻게 구현하나?

C++ 표준 라이브러리에 `Tree`라는 직접적인 컨테이너는 없다.

보통 노드를 직접 만들고, 각 노드가 자식 목록을 가진다.

```cpp
struct Region
{
    std::string Name;
    std::vector<Region*> Children;
};
```

핵심은 이 부분이다.

```cpp
std::vector<Region*> Children;
```

즉, 노드 하나가 자기 자식들을 저장한다.

```text
Region
├─ Name
└─ Children
   ├─ child pointer
   ├─ child pointer
   └─ child pointer
```

> [!note]
> 위 예시는 구조를 이해하기 위한 단순 예시다. 실제 C++ 코드에서는 누가 노드를 소유하고 삭제할지 정해야 한다. 소유권이 필요하면 `std::unique_ptr<Region>` 같은 스마트 포인터를 검토한다.

예를 들어 소유권까지 표현하면 이런 형태가 될 수 있다.

```cpp
struct Region
{
    std::string Name;
    std::vector<std::unique_ptr<Region>> Children;
};
```

아래 DFS/BFS 예제는 구조를 보기 쉽게 하기 위해 `Region*` 기준으로 작성한다.  
`std::unique_ptr<Region>`로 자식을 소유한다면 순회할 때 `Child.get()`으로 포인터를 꺼내 사용할 수 있다.

---

## 일반 트리에서 노드 찾기

일반 트리는 보통 자식들 사이에 정렬 규칙이 없다.

```text
경기도
├── 수원시
├── 성남시
├── 용인시
└── 안양시
```

`"용인시"`를 찾는다고 해보자.

현재 `경기도` 노드에서 어느 자식으로 가야 하는지 알 수 있는 규칙이 없다.

```text
수원시로 가야 하나?
성남시로 가야 하나?
용인시로 가야 하나?
안양시로 가야 하나?
```

그래서 일반적으로는 노드를 하나씩 확인해야 한다.

대표적인 순회 방식이 DFS와 BFS다.

```text
DFS = 깊이 우선 탐색
BFS = 너비 우선 탐색
```

> [!important]
> 일반 트리의 검색 방법이 DFS/BFS “뿐”이라는 뜻은 아니다. 다만 정렬 규칙이 없는 계층 구조에서는 결국 확인할 후보를 순회해야 하며, DFS와 BFS가 가장 기본적인 순회 방식이다.

---

## DFS

DFS(Depth First Search)는 한 방향으로 깊게 내려간 뒤, 더 갈 곳이 없으면 돌아오면서 다른 자식을 탐색한다.

```text
경기도
 ↓
수원시
 ↓
영통구
 ↓
매탄동
```

재귀 함수로 표현하기 쉽다.

```cpp
Region* FindDFS(Region* Node, const std::string& TargetName)
{
    if (!Node)
    {
        return nullptr;
    }

    if (Node->Name == TargetName)
    {
        return Node;
    }

    for (Region* Child : Node->Children)
    {
        if (Region* Found = FindDFS(Child, TargetName))
        {
            return Found;
        }
    }

    return nullptr;
}
```

### 특징

| 특징 | 설명 |
| --- | --- |
| 탐색 방향 | 한 경로를 깊게 내려감 |
| 구현 | 재귀로 구현하기 쉬움 |
| 주의 | 트리가 매우 깊으면 재귀 호출이 깊어질 수 있음 |

---

## BFS

BFS(Breadth First Search)는 같은 깊이의 노드들을 먼저 탐색한다.

```text
경기도

수원시, 성남시

영통구, 장안구, 분당구
```

큐를 사용해 구현한다.

```cpp
Region* FindBFS(Region* Root, const std::string& TargetName)
{
    if (!Root)
    {
        return nullptr;
    }

    std::queue<Region*> Queue;
    Queue.push(Root);

    while (!Queue.empty())
    {
        Region* Current = Queue.front();
        Queue.pop();

        if (Current->Name == TargetName)
        {
            return Current;
        }

        for (Region* Child : Current->Children)
        {
            Queue.push(Child);
        }
    }

    return nullptr;
}
```

### 특징

| 특징 | 설명 |
| --- | --- |
| 탐색 방향 | 가까운 깊이부터 확인 |
| 구현 | 큐 사용 |
| 장점 | 루트에서 가까운 노드를 먼저 찾음 |

---

## 일반 트리의 검색 비용

정렬 규칙이 없는 일반 트리에서는 최악의 경우 모든 노드를 확인해야 한다.

```text
찾는 노드가 마지막에 있거나
아예 존재하지 않으면
전체 노드를 확인해야 한다.
```

따라서 일반적인 검색 비용은 최악의 경우 `O(N)`이다.

```text
N = 트리 전체 노드 수
```

---

## BST와 일반 트리 차이

BST(Binary Search Tree)는 이진 탐색 트리다.

각 노드는 최대 두 자식을 가진다.

```text
        50
      /    \
    30      80
   /  \    /  \
 20   40 70 100
```

규칙은 아래와 같다.

```text
왼쪽 자식 < 부모 < 오른쪽 자식
```

예를 들어 `70`을 찾는다면:

```text
70 > 50
→ 오른쪽으로 이동

70 < 80
→ 왼쪽으로 이동

70 발견
```

일반 트리와 달리 BST는 비교 결과로 탐색 방향을 정할 수 있다.

```text
작으면 왼쪽
크면 오른쪽
```

균형이 잘 잡힌 BST라면 평균적으로 `O(log N)`에 탐색할 수 있다.

하지만 한쪽으로 치우치면 리스트처럼 되어 `O(N)`이 될 수 있다.

```text
10
  \
   20
     \
      30
        \
         40
```

---

## 균형 트리

BST가 한쪽으로 치우치는 문제를 줄이기 위해 균형을 유지하는 트리들이 있다.

| 트리 | 목적 |
| --- | --- |
| AVL Tree | 균형을 엄격하게 유지 |
| Red-Black Tree | 균형을 느슨하게 유지하면서 삽입·삭제 비용 조절 |
| B-Tree | 디스크/DB 인덱스처럼 큰 블록 단위 저장에 적합 |

균형 트리는 탐색, 삽입, 삭제를 보통 `O(log N)`으로 유지하려고 설계된다.

C++의 `std::map`, `std::set`은 일반적으로 Red-Black Tree 같은 균형 이진 탐색 트리로 구현된다.

> [!note]
> 표준은 내부 구현을 특정 트리로 강제하지는 않지만, `std::map`과 `std::set`은 정렬 상태와 `O(log N)` 연산을 제공해야 한다.

---

## 트리와 HashMap을 같이 쓰는 경우

일반 트리는 계층 구조를 잘 표현한다.

```text
경기도
└── 수원시
    └── 영통구
        └── 매탄동
```

하지만 `"영통구"`를 이름으로 자주 찾아야 한다면 매번 DFS/BFS를 돌리는 것은 비효율적일 수 있다.

이럴 때 별도의 인덱스를 같이 둔다.

```cpp
std::unordered_map<std::string, Region*> RegionMap;
```

```text
"경기도" → 경기도 노드
"수원시" → 수원시 노드
"영통구" → 영통구 노드
"매탄동" → 매탄동 노드
```

검색은 HashMap이 담당한다.

```cpp
auto It = RegionMap.find("영통구");
Region* Found = (It != RegionMap.end()) ? It->second : nullptr;
```

`operator[]`는 key가 없을 때 새 항목을 만들 수 있으므로, 조회만 할 때는 `find()`가 더 안전하다.

역할을 나누면 이렇게 된다.

| 자료구조 | 담당 |
| --- | --- |
| 트리 | 부모-자식 계층 관계 표현 |
| HashMap | 이름이나 ID로 빠르게 검색 |

> [!caution]
> 트리와 HashMap을 함께 쓰면 노드를 추가·삭제·이름 변경할 때 두 자료구조를 같이 갱신해야 한다. 빠른 검색을 얻는 대신 동기화 관리 비용이 생긴다.

---

## 비교 정리

| 자료구조 | 목적 | 검색 방식 | 평균 시간복잡도 |
| --- | --- | --- | --- |
| 일반 트리 | 계층 구조 표현 | DFS / BFS 순회 | `O(N)` |
| BST | 정렬 규칙 기반 검색 | 값 비교 | 평균 `O(log N)`, 최악 `O(N)` |
| AVL Tree | 균형 유지 | 값 비교 | `O(log N)` |
| Red-Black Tree | 균형 유지 | 값 비교 | `O(log N)` |
| B-Tree | 디스크/DB 인덱스 | 값 비교 | `O(log N)` |
| HashMap | key 기반 검색 | 해시 | 평균 `O(1)` |

---

## 정리

1. 트리는 부모-자식 관계로 계층 구조를 표현한다.
2. 일반 트리는 정렬 규칙이 없어서 DFS/BFS로 순회하며 찾는 경우가 많다.
3. 일반 트리에서 특정 노드 검색은 최악의 경우 `O(N)`이다.
4. BST는 왼쪽 < 부모 < 오른쪽 규칙이 있어 탐색 방향을 정할 수 있다.
5. 균형 잡힌 BST는 평균적으로 `O(log N)` 탐색이 가능하다.
6. 실무에서는 계층 표현은 트리, 빠른 검색은 HashMap이 담당하게 함께 쓰는 경우가 많다.

---

[[정리집 목차]] · [[그래프 표현 방법과 컨테이너 선택]] · [[STL 컨테이너 선택 기준]]
