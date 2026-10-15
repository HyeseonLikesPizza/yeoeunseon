## 원소가 커지면 연속된 vector도 불리해질까?

**같은 50,000개에서 조회당 비교할 ID 수는 같았지만, 원소가 커진 vector는 약 12~13배 느려졌다. 이번에는 캐시 miss와 메모리 대기 증가까지 관찰했다.**

앞선 benchmark는 시간과 stride의 관계를 보여줬다. 여기서는 8B/256B 원소를 VTune으로 교대 수집해, 그 차이가 CPU의 어떤 지표와 연결되는지 확인했다.

---

## 비교 조건

- Intel Core i5-14600KF / VTune 2026.4 / MSVC `/O2 /Zi /EHsc /std:c++17`.
- 원소 50,000개, 같은 정수 ID·배열 순서·HP·512개 조회 목록, seed 1.
- 성공/실패 혼합 조회. 원소의 추가 payload만 바꿔 `sizeof(Enemy)`를 8B/256B로 비교했다.
- 초기화와 warm-up 뒤 약 5초간 반복. 완료한 조회 수로 시간·event를 정규화했다.
- 테스트 Thread만 logical CPU 0에 고정했다. 실행 파일 모듈의 Clockticks는 8개 결과 모두 P-core에서 집계됐고 E-core는 0이었다.
- 모든 capture의 checksum은 `141802`, mismatch는 0이었다.

```text
1차: vector 8B → map 8B → vector 256B → map 256B
2차: map 256B → vector 256B → map 8B → vector 8B
```

코어 고정과 별도 빌드가 포함된 새 실험이므로, 이전 benchmark의 시간과 섞어 평균내지 않는다.

```text
VectorMemoryProfile.exe profile vector 8   50000 5 0
VectorMemoryProfile.exe profile vector 256 50000 5 0
VectorMemoryProfile.exe profile map    8   50000 5 0
VectorMemoryProfile.exe profile map    256 50000 5 0
```

마지막 `0`은 이번 실행의 logical CPU index다. 다른 PC에 그대로 적용할 코어 번호는 아니다. 코드: [[VectorVsHashFactors.cpp]]의 `profile` 모드.

---

## 시간은 어떻게 달라졌나?

| 구현 | 8B · 1차 / 2차 | 256B · 1차 / 2차 |
|---|---:|---:|
| vector · μs/조회 | 8.58 / 8.64 | 104.77 / 110.62 |
| map · ns/조회 | 2.57 / 2.63 | 2.58 / 2.75 |

**vector는 원소 크기에 크게 영향을 받았고, 이번 map은 비슷한 수준을 유지했다.** 위 두 행은 단위가 다르다: `1μs = 1,000ns`.

둘 다 같은 ID를 찾지만 vector는 많은 앞 원소를 지나간다. map은 선택한 bucket의 후보만 확인한다. 이번 입력은 큰 N에서 선형 탐색보다 해시 탐색이 유리한 조건이지, 작은 vector가 더 빠른 조건의 실험은 아니다.

---

## 계산량이 늘어서 느려졌을까?

**비교할 ID는 그대로였다. 명령/조회도 약 1% 차이인데, cycle과 CPI는 약 12배로 늘었다.**

| vector | 8B · 1차 / 2차 | 256B · 1차 / 2차 |
|---|---:|---:|
| 명령/조회 | 약 187,118 / 187,177 | 약 189,408 / 188,728 |
| cycle/조회 | 약 41,928 / 41,942 | 약 514,285 / 523,493 |
| CPI | 0.224 / 0.224 | 2.715 / 2.774 |

모듈 평균 주파수는 전체 조건에서 약 5.26~5.31GHz였다. 이 작은 주파수 차이나 명령 수 차이로 몇 배의 시간 증가를 설명할 수는 없다.

STL `find`가 별도 함수로 표시되므로 map은 `MapLookup` 한 행만 읽지 않았다. 모든 표는 **우리 실행 파일 모듈 합계**로, STL 보조 함수와 소량의 초기화·출력을 포함한다.

---

## 캐시 계층에서는 무엇이 달라졌나?

**256B vector는 L1에서 찾지 못한 load가 크게 늘었고, 그중 많은 load가 L3에서 데이터를 가져왔다.**

| vector · event/조회 추정치 | 8B · 1차 / 2차 | 256B · 1차 / 2차 |
|---|---:|---:|
| L1 miss | 약 102 / 125 | 약 34,421 / 34,948 |
| L2 hit | 약 102 / 137 | 약 10,560 / 10,026 |
| L3 hit | 0 / 0 | 약 23,866 / 26,718 |
| L3 miss | 0 / 0 | 약 309 / 287 |

원본과 정확한 수치는 [[VTuneVectorMemory/VTuneMemorySummary.csv|정규화 CSV]]에 있다. 위 값은 retired load event를 완료한 조회 수로 나눈 것이며, miss rate나 전송된 byte 수가 아니다.

```text
8B 원소
→ 다음 ID까지 8B
→ 가까운 ID들을 가져온 cache line에서 계속 사용

256B 원소
→ 다음 ID까지 256B
→ 같은 수의 ID를 읽어도 더 넓은 주소 범위 접근
→ L1 miss와 L3 접근 증가
→ 메모리 대기와 CPI 증가
```

vector의 전체 객체는 여전히 연속이다. 하지만 **이번 루프가 읽는 ID의 간격**은 커졌다. “연속 저장이면 어떤 필드를 읽어도 같은 효율”은 아니었다.

배열 할당 크기는 약 0.4MB → 12.8MB다. 이것은 **실제로 모든 payload byte를 읽었다는 뜻이 아니다.** 사용하지 않는 필드를 통째로 load했다고 주장하지 않는다.

---

## 메모리 대기와 주소 변환도 늘었나?

256B vector에서는 다음 단서도 두 번 모두 나타났다.

- L1 miss 관련 stall cycle/조회: 약 40.7만 / 42.1만. 8B에서는 약 509 / 285이었다.
- L3 Bound: 약 65.6% / 66.3%, DRAM Bound: 약 10.0% / 9.4%.
- DTLB page walk가 진행된 cycle/조회: 약 77,483 / 78,062. 8B에서는 약 5.7 / 0이었다.

→ **캐시 계층과 주소 변환 비용이 모두 커지는 방향이었다.** 특히 L3 hit와 L3 Bound가 커졌으므로 “매번 RAM까지 가서 느리다”로 설명하면 틀린다. L3에서 찾더라도 더 가까운 캐시에서 처리하는 것보다 비용이 들 수 있다. [Intel — CPU Metrics Reference](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2024-2/cpu-metrics-reference.html)

DTLB walk cycle은 TLB miss 횟수가 아니다. 다른 stall과 겹칠 수 있어 위 cycle들을 더해 원인별 시간 비율로 만들지 않는다. TLB 자체의 기여만 분리한 실험도 아니다.

---

## map은 왜 같이 느려지지 않았나?

**50,000개 전체를 찾은 것이 아니라, 같은 512개 조회 목록을 계속 반복했기 때문이다.**

map은 전체 노드를 순회하지 않고, 목록이 지목한 키와 bucket의 후보를 반복해서 접근한다. 원소가 커져도 payload 전체를 읽지는 않는다. 이번 map의 L1 miss는 조회당 약 1.9~2.0, L3 hit는 0~약 0.018로 보고됐다.

→ 이 조건에서는 반복해서 찾는 후보의 재사용이 가능했다. **“map은 데이터가 커져도 캐시 비용이 없다”는 결론은 아니다.** 이후 [[VectorVsHash_VTune 조회 키 범위 실험|키 범위 실험]]에서 목록 길이를 고정하고 키 범위를 넓히자 L1 miss·L3 접근·대기와 시간이 증가했다. 두 실험의 입력은 다르므로 배수를 직접 합치지 않는다.

---

## 어디까지 확인했나?

이후 [[VectorVsHash_VTune ID 분리 실험|ID 분리 실험]]에서 같은 비교 횟수로 miss·대기와 시간을 다시 줄이는 것까지 확인했다. 전체 시스템의 배열 유지 비용을 검증한 것은 아니다.

**큰 stride에서 캐시 miss·L3 접근·메모리 대기가 실제로 증가했다. 배치와 실행 시간의 연결은 이제 추측만이 아니다.**

다만 약 12~13배의 차이를 cache와 TLB 각각 몇 %로 나누지는 않았다. DRAM bandwidth가 한계에 도달했는지도 측정하지 않았다.

5초의 multiplex sampling이며 summary MUX Reliability는 약 0.955~0.997이었다. 높은 값도 모든 지표의 정확성을 보장하지는 않는다. 0 event는 실제 발생이 전혀 없었다는 보장이 아니며, top-down 하위 비율을 단순 합산하지 않는다. [Intel — Multiplex Events](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-1/allow-multiple-runs-or-multiplex-events.html)

요약 CSV·수집 로그는 `VTuneVectorMemory/`, 상세 보고서·함수별 결과는 그 아래 `원본 보고서/`에 있다. 상세 보고서는 Git에서 제외한다. 원본 VTune 결과는 학습 폴더의 `tmp/vtune_memory_*`에 남겼다.

수집은 `재현/VTuneMemoryCaptures.cmd`, 해석은 `재현/ReadVTuneMemoryCaptures.ps1`로 수행했다. 한 결과에서 임시 DB I/O 오류가 났으나 원본을 지우지 않고 재-finalize해 복구했다. 재수집한 데이터로 대체하지 않았다.

[[../vector와 unordered_map 조회 비용 비교|전체 풀이]] · [[VectorVsHash_VTune 조회 순서 실험|작은 입력의 분기 실험]]
