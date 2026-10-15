## ID를 모으면 캐시 비용도 다시 줄어들까?

**비교 횟수는 그대로였지만, ID를 분리하자 검색 시간이 약 99μs → 7.9μs로 줄었다. L1 miss·L3 접근·메모리 대기도 함께 감소했다.**

큰 원소 실험에서는 stride가 커질 때 miss와 대기가 늘었다. 이번에는 그 가설에서 제안한 개선을 적용해, 증가했던 비용이 다시 줄어드는지 확인했다.

---

## 무엇을 바꿨나?

```text
기존 검색
256B Enemy 배열 → 각 Enemy의 ID 비교 → 일치하면 HP 사용

분리 검색
4B ID 배열     → 각 ID 비교 → 일치하면 같은 index의 HP 사용
```

이번 구현은 ID와 HP를 각각 `vector<int>`에 보관한다. ID를 찾은 뒤 `HP[index]`를 읽으므로 결과는 기존 검색과 같다. ID만 반환하도록 기능을 줄인 비교가 아니다.

- Enemy 50,000개, 같은 ID·순서·HP, seed 1, 같은 512개 성공/실패 혼합 조회.
- 조회당 평균 ID 비교 **37,360.287109375회**, checksum **141802**로 모든 capture가 같았다.
- 기존 256B 배열과 분리한 ID/HP 배열을 양쪽 실행에 모두 만들어 두었다. 분리 배열 생성·유지 비용은 검색 측정에서 제외했다.
- 테스트 Thread는 logical CPU 0에 고정했다. 모듈 Clockticks는 모두 P-core였고 E-core는 0이었다.
- Intel Core i5-14600KF / VTune 2026.4 / MSVC `/O2 /Zi /EHsc /std:c++17`.

```text
5초씩: 기존 → ID 분리 → ID 분리 → 기존
```

2026-10-11 수집. 두 구현은 별도 process에서 실행했다. 시간과 hardware event는 완료한 조회 수로 나눴다. 이전 날의 시간값을 기준값으로 재사용하지 않고, 이번 실행의 기존 검색과 비교했다.

---

## 결과가 같으면서 빨라졌나?

| 검색 | 1차 · μs/조회 | 2차 · μs/조회 |
|---|---:|---:|
| 256B Enemy 순회 | 99.33 | 98.96 |
| 4B ID 순회 | 7.923 | 7.925 |

**약 12.5배 차이가 두 실행에서 반복됐고 mismatch는 모두 0이었다.**

→ 알고리즘은 여전히 선형 탐색이다. 덜 비교한 것도 아니다. **같은 비교를 수행할 때 읽는 데이터의 배치**를 바꿔 비용을 낮췄다.

---

## 캐시·CPU 지표도 개선됐나?

| 조회당 추정치 | 기존 · 1차 / 2차 | ID 분리 · 1차 / 2차 |
|---|---:|---:|
| 실행 명령 수 | 약 191,285 / 191,285 | 약 224,579 / 224,574 |
| cycle | 약 492,171 / 500,167 | 약 39,490 / 39,623 |
| L1 miss | 약 33,204 / 34,896 | 약 105 / 94 |
| L3 hit | 약 18,363 / 17,191 | 약 47 / 47 |
| L1 miss 관련 stall cycle | 약 359,376 / 382,814 | 약 680 / 0 |
| DTLB walk active cycle | 약 75,328 / 74,872 | 0 / 0 |
| CPI | 2.573 / 2.615 | 0.176 / 0.176 |

**명령은 오히려 더 실행했는데 cycle은 줄었다.** 평균 주파수도 약 5.29~5.31GHz로 비슷했다.

→ “연산을 덜 해서 빨라졌다”보다 **메모리 접근과 대기가 줄어 같은 일을 효율적으로 처리했다**는 설명에 맞는 결과다. page walk 지표도 감소했지만 cache와 TLB의 기여를 각각 분리한 것은 아니다.

숫자는 multiplex sampling의 추정치다. 0은 실제로 한 번도 발생하지 않았다는 보장이 아니며, 여러 stall cycle을 더해 원인별 시간을 계산하지 않는다. summary MUX Reliability는 약 0.946~0.986이었다. [Intel — Multiplex Events](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-1/allow-multiple-runs-or-multiplex-events.html)

---

## SIMD로 바뀐 결과는 아닐까?

이번 실행 파일을 만들 때 생성한 Assembly의 두 검색 루프를 확인했다. **둘 다 scalar ID 비교이며, 다음 ID까지의 주소 간격이 달랐다.**

```asm
; VectorLookup<256>의 내부 루프
cmp DWORD PTR [rax], ecx
je  found
add rax, 256
cmp rax, rdx
jne loop
```

```asm
; KeyOnlyLookup의 내부 루프
cmp DWORD PTR [rax], ecx
je  found
inc rdx
add rax, 4
cmp rdx, r8
jb  loop
```

→ ID 분리만 packed SIMD로 처리돼 빨라진 결과는 아니었다. index 관리 때문에 명령 수가 늘 수 있어도, 더 촘촘한 데이터 접근이 큰 이득을 줬다.

---

## 설계에 그대로 적용해도 될까?

**검색용 데이터 분리는 검증된 후보지만, 전체 시스템 비용까지 측정한 것은 아니다.**

ID 배열의 검색 범위는 약 200KB다. HP 배열도 별도로 있고, 이 실험에서는 원래 12.8MB Enemy 배열도 그대로 남겼다. 전체 메모리 사용량을 200KB로 줄였다는 뜻이 아니다.

실제 적용에서는 ID와 HP의 index 대응을 유지해야 한다. 추가·삭제·순서 변경·HP 수정 시 분리 배열을 함께 관리하는 비용과 복잡성이 생긴다. 이 비용은 이번 검색 시간에 포함되지 않았다.

```text
큰 원소에서 ID 간격이 넓음
→ miss와 메모리 대기 증가
→ ID를 촘촘하게 분리
→ 같은 비교 횟수·같은 결과
→ miss와 대기 감소, 조회 시간 감소
```

**원인을 의심하는 데서 끝내지 않고, 그 원인을 겨냥한 개선 뒤 지표가 함께 좋아지는지 확인했다.** 캐시/TLB별 정확한 기여율이나 DRAM bandwidth 한계까지 증명한 것은 아니다.

---

코드: [[VectorVsHashFactors.cpp]]의 `profile-split` 모드.

```text
VectorSplitProfile.exe profile-split aos256 5 0
VectorSplitProfile.exe profile-split ids4   5 0
```

요약 CSV·수집 로그: `VTuneVectorSplit/`. 상세 지표·함수별 보고서는 그 아래 `원본 보고서/`에 로컬 보관하며 Git에서 제외한다.  
[[VTuneVectorSplit/VTuneSplitSummary.csv|정규화 수치]] · [[VTuneVectorSplit/VTuneSplitCaptures.log|4회 수집 로그]]

원본 VTune 결과는 루트의 `tmp/vtune_split_*`, 전체 생성 Assembly는 `tmp/VectorSplitProfile.asm`에 남겼다. 수집 스크립트는 `재현/VTuneSplitCaptures.cmd`, 해석·정규화는 `재현/ReadVTuneSplitCaptures.ps1`, `재현/SummarizeVTuneSplit.ps1`이다.

[[VectorVsHash_VTune 큰 원소 실험|먼저 확인한 큰 원소 비용]] · [[../vector와 unordered_map 조회 비용 비교|전체 풀이]]
