## 요약

**비동기(Async)** = 작업을 다른 스레드에서 수행하는 방식

**ThreadPool** = 그 작업을 실행하는 Worker Thread들의 집합

---

## 왜 사용하는가?

GameThread에서 무거운 계산을 수행하면

- FPS 감소
    
- 입력 지연
    
- Hitch 발생 (특정 프레임의 과도한 작업)


**계산 때문에 게임이 멈춰 보인다.

따라서

> 무거운 계산은 Worker Thread에 맡기고
> 
> GameThread는 게임 진행에 집중시킨다.


---

## ThreadPool

Worker Thread를 미리 여러 개 생성해두고 (비용 절감)

필요할 때 빌려 쓰는 구조.

```text
미리 생성

Worker1
Worker2
Worker3

↓

작업 요청

↓

빈 Worker가 처리
```

## Worker Thread

GameThread 대신 계산을 수행하는 보조 스레드.

ThreadPool에 포함되어 있다.

---

## 무거운 계산

```cpp
Async(EAsyncExecution::ThreadPool,
[]()
{
    // Heavy Calculation
});
```

---

## 계산 후 UObject 수정

```cpp
Async(EAsyncExecution::ThreadPool,
[]()
{
    auto Result = Calculate();

    AsyncTask(
        ENamedThreads::GameThread,
        [Result]()
        {
            Actor->SetActorLocation(Result);
        });
});
```

→ 계산은 Worker Thread

→ UObject 수정은 GameThread

---

## 주의

아래는 Worker Thread에서 하면 안 된다.

```cpp
SpawnActor(...);

Actor->SetActorLocation(...);

Widget->SetText(...);
```

왜?

UObject는 Thread Safe하지 않기 때문.

---

## 🔗 Related

### GameThread

언리얼 메인 스레드.

- Tick
    
- UObject 생성/삭제
    
- GC
    
- Rendering 명령 생성
    

---