## 생성된 코드에서 확인한 것

MSVC 19.44.35228, x64 /O2 /std:c++17 /FAcs. [[VectorVsHashFactors.cpp|이번 실행의 소스]]에서 생성한 Assembly를 확인했다.

아래는 확인한 루프의 핵심 명령을 발췌한 것이다. loop/matched는 읽기 위해 바꾼 label이며, 생략한 초기화·반환 코드가 있다. 다른 빌드의 생성 코드를 보장하지 않는다.

### vector — 8B 원소

```asm
loop:
    cmp DWORD PTR [rax], ecx
    je  matched
    add rax, 8
    cmp rax, rdx
    jne loop
```

ID를 하나 비교하고 다음 원소로 8B 이동한다. 확인한 비교 루프는 scalar다.

### vector — 256B 원소

```asm
loop:
    cmp DWORD PTR [rax], ecx
    je  matched
    add rax, 256
    cmp rax, rdx
    jne loop
```

검색 방식과 비교 명령은 같지만, 다음 ID 주소까지 256B 이동한다. SIMD가 아니라 접근 간격이 달라진 것이다.

### ID만 분리한 배열

```asm
loop:
    cmp DWORD PTR [rax], ecx
    je  matched
    inc rdx
    add rax, 4
    cmp rdx, r8
    jb  loop
```

KeyOnlyLookup의 전체 검색 루프에서도 packed SIMD는 없었다. ID를 4B 간격으로 비교하고, 찾으면 HP 배열에서 해당 Index를 읽었다.

→ 256B AoS와 4B ID 배열의 차이를 **SoA만 SIMD가 되어 빨라진 것**으로 설명할 수 없다. 다만 두 루프의 제어 방식도 조금 다르므로 모든 cycle 차이를 stride 하나로 환산하지 않는다.

### map — 기본 std::hash<int>

이번 MSVC의 기본 정수 hash는 byte별 xor와 곱셈으로 구성되어 있었다. “정수 hash는 항상 값 자체를 반환하므로 공짜”라고 가정하면 안 된다.

```asm
; hash 계산의 일부
xor  rax, r14
imul rax, rbp
xor  rdx, rax
imul rdx, rbp
; 이후 다른 byte 처리도 이어짐
```

이후 bucket에서 노드를 찾고 후보 키를 비교한다.

```asm
and rdx, rdi
add rdx, rdx
mov rax, QWORD PTR [rsi+rdx*8+8] ; bucket 후보 노드
cmp r8d, DWORD PTR [rax+16]     ; Key 비교

; 후보가 다르고 bucket 탐색이 끝나지 않았을 때
mov rax, QWORD PTR [rax+8]      ; 이전 후보 노드의 링크
cmp r8d, DWORD PTR [rax+16]
```

확인한 MSVC 헤더 xhash의 _Find_last 구현은 이 경로에서 _Where->_Prev를 따라간다. 특정 bucket 식과 링크 방향은 **이번 표준 라이브러리 구현**에 한정된다.

→ 충돌이 늘면 링크를 통해 다음 후보를 찾는 작업도 늘어난다. Assembly만으로 cache miss가 실제 몇 번 생겼는지는 알 수 없다.

### 전체 HP 합산

VectorScan<8>의 전체 합산 루프:

```asm
loop:
    mov edx, DWORD PTR [rcx+4]
    add rcx, 8
    add rax, rdx
    cmp rcx, r8
    jne loop
```

VectorScan<256>에서도 같은 scalar 합산이며 add rcx,256이었다. 이번 순회 차이를 vector의 자동 SIMD화로 설명하지 않는다.

---

| 확인 | 아직 확인하지 못한 것 |
|---|---|
| scalar 비교·합산 | 다른 compiler/빌드의 SIMD 여부 |
| ID stride 8/256/4B | Cache Miss와 TLB Miss 횟수 |
| hash 연산·bucket·노드 링크 | 각 구간의 cycle 기여율 |
| 배치 변경 후 시간 차이 | cache hierarchy/bandwidth 원인 분리 |

[[VectorVsHashFactors_실험 기록|측정 조건과 후속 실험]]
