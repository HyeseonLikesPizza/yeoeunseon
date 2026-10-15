## 같은 조회인데 순서만 바꾸면 왜 느려졌을까?

**작은 입력에서는 캐시만 볼 일이 아니었다. 조회 순서를 섞자 분기 실패가 늘고, 같은 명령을 처리하는 데 더 많은 cycle이 들었다.**

이전 `patterns` 실험에서 8개 원소와 같은 조회 목록을 유지해도 실행 시간이 달라졌다. 당시에는 분기 예측과 locality를 구분할 지표가 없었다. 이번에는 VTune 하드웨어 수집으로 그 조건을 다시 확인했다.

---

## 비교 조건

- Intel Core i5-14600KF, 물리 14코어 / 논리 20개, VTune 2026.4.
- MSVC `/O2 /Zi /EHsc /std:c++17`. 전용 `VectorOrderProfile.exe` 빌드, 단일 작업 Thread.
- Enemy 8개, 원소 8B, ID 1~8, seed 1. 성공/실패 조회를 절반씩 포함.
- 65,536개 조회 목록: 512개 패턴을 128번 반복한 목록과 그 목록을 섞은 경우.
- 컨테이너·조회 키별 빈도·결과·비교 횟수는 같다. 섞기는 측정 밖이다.
- 실행 전 두 구현의 결과를 확인하고 약 3초간 선택한 구현을 반복했다. 모든 capture의 checksum은 `3391616`, mismatch는 0이었다.

```text
1차: vector 반복 → map 반복 → vector 섞음 → map 섞음
2차: map 섞음 → vector 섞음 → map 반복 → vector 반복
```

`uarch-exploration`을 총 8회 수집했다. 함수 이름이 표시되도록 실행 파일과 일치하는 PDB 경로를 보정했다. 기존 benchmark의 절대 시간과 합쳐 평균내지 않는다.

실행 코드: [[VectorVsHashFactors.cpp]]의 `profile-order` 모드.

```text
VectorOrderProfile.exe profile-order vector periodic 3
VectorOrderProfile.exe profile-order map periodic 3
VectorOrderProfile.exe profile-order vector shuffled 3
VectorOrderProfile.exe profile-order map shuffled 3
```

---

## 시간 차이가 다시 나타났나?

**반복 순서에서는 map이 빨랐지만, 같은 목록을 섞으면 vector가 빨랐다.**

| 구현·순서 | 1차 · ns/조회 | 2차 · ns/조회 |
|---|---:|---:|
| vector · 반복 | 2.195 | 2.215 |
| vector · 섞음 | 5.370 | 5.398 |
| map · 반복 | 1.268 | 1.261 |
| map · 섞음 | 6.439 | 6.422 |

계산식은 `실제 경과 시간 / (완료한 pass 수 × 65,536)`이다. 같은 3초 동안 빠른 구현은 더 많은 조회를 끝낸다. 따라서 CPU Time이나 event 총량만 보고 비교하면 안 된다.

→ “8개면 어느 컨테이너가 빠르다”라는 규칙은 없다. **동일한 키를 찾더라도 실제 조회 순서가 승자를 바꿀 수 있었다.** 이 결론은 이번 작은 혼합 조회 조건에 한정한다.

---

## 일을 더 많이 해서 느려진 걸까?

**조회 1회당 명령 수는 거의 같았지만, cycle과 CPI는 크게 늘었다.**

| 구현·순서 | 명령/조회 · 1차 / 2차 | cycle/조회 · 1차 / 2차 | CPI · 1차 / 2차 |
|---|---:|---:|---:|
| vector · 반복 | 39.50 / 39.50 | 10.61 / 10.66 | 0.269 / 0.270 |
| vector · 섞음 | 39.51 / 39.49 | 26.09 / 26.14 | 0.660 / 0.662 |
| map · 반복 | 34.52 / 34.51 | 6.00 / 6.01 | 0.174 / 0.174 |
| map · 섞음 | 34.58 / 34.59 | 31.50 / 31.36 | 0.911 / 0.907 |

→ 비교 횟수나 명령 개수 증가로 설명할 차이가 아니다. **거의 같은 양의 명령을 처리하는 효율이 달라졌다.** 모듈 평균 CPU 주파수도 약 5.04~5.07GHz로, 몇 배의 시간 차이를 설명할 만큼 변하지 않았다.

### map은 왜 모듈 범위로 집계했나?

이 빌드에서는 `MapLookup`이 STL `unordered_map::find`를 호출했고, 보조 함수가 별도로 표시됐다. 반복 순서 1차에서 `MapLookup` 자체의 CPU Time은 약 0.98초, STL `find`는 약 1.83초였다.

`MapLookup` 한 행만 보면 조회 비용을 빠뜨린다. 표는 **우리 실행 파일 모듈의 합계**를 사용해 STL 보조 함수를 포함했다. 초기화·출력도 일부 포함되지만, 약 3초간 반복한 조회가 대부분을 차지한다. cycle·명령·miss 추정치는 완료한 조회 수로 나눴다.

---

## 분기 실패가 실제로 늘었나?

**두 번의 수집 모두, 섞은 입력에서 분기 실패가 크게 늘었다.**

| 구현·순서 | 분기 실패/조회 · 1차 / 2차 | P-core Branch Mispredict · 1차 / 2차 |
|---|---:|---:|
| vector · 반복 | 0.0023 / 0 | 0.0% / 0.0% |
| vector · 섞음 | 0.745 / 0.645 | 29.5% / 28.1% |
| map · 반복 | 0 / 0 | 0.5% / 0.0% |
| map · 섞음 | 0.448 / 0.426 | 47.4% / 48.2% |

왼쪽은 `BR_MISP_RETIRED.ALL_BRANCHES / 조회 수`다. 오른쪽은 VTune의 P-core top-down 지표이며 **전체 분기 중 틀린 비율과는 다른 지표**다. 두 숫자를 같은 백분율로 읽지 않는다.

E-core의 Branch Mispredict 지표도 섞은 조건에서 높았다: vector 약 40~46%, map 약 56~60%. P/E-core 지표는 서로 합산하지 않는다.

→ 조회 순서 변경 → 분기 실패 증가 → cycle/조회와 CPI 증가 → 처리량 감소가 함께 나타났다. **분기 예측은 이제 추측만이 아니라, 이 시간 차이와 연결되는 측정 근거가 있다.** 다만 분기 실패가 전체 시간 차이의 정확히 몇 %를 만들었는지는 계산하지 않았다.

---

## 그럼 캐시 영향은 없었나?

**이번 결과로 캐시 영향을 없다고 할 수는 없다. 그렇다고 느려진 이유를 캐시 miss라고 확정할 수도 없다.**

이번 모듈 집계의 L1 miss 추정치는 조회당 0~약 0.012였고, L3 miss는 모두 0으로 보고됐다. 섞은 조건이 두 실행 모두 더 큰 miss 증가를 보이지는 않았다.

이것은 원소 8개를 반복해서 찾는 warm workload다. 큰 원소·넓은 키 범위의 메모리 접근을 검증한 실험이 아니다. **샘플링에서 0은 해당 event가 정확히 한 번도 발생하지 않았다는 보장이 아니다.** L1 hit에서도 의존적 load 등의 비용은 남을 수 있다.

VTune은 여러 event를 multiplex하고 추정치를 전체 실행 시간으로 확장한다. 이번 3초 수집의 summary MUX Reliability는 약 0.79~0.99였다. 작은 miss 수치와 비율의 소수점은 확정값으로 쓰지 않고, 두 실행에서 반복된 큰 차이를 중심으로 읽었다. [Intel — Multiplex Events](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-1/allow-multiple-runs-or-multiplex-events.html)

---

## 여기서 가져갈 것

```text
같은 원소 수 · 같은 조회 키와 횟수
→ 순서만 변경
→ 명령/조회는 거의 동일
→ 분기 실패와 CPI 증가
→ 시간 증가, 컨테이너 간 승자도 바뀜
```

**이번 작은 입력의 차이는 “vector가 연속 메모리라서”만으로 설명되지 않는다. 분기 예측까지 확인해야 했다.**

큰 원소의 stride는 이후 [[VectorVsHash_VTune 큰 원소 실험|별도 8회 수집]]에서 cache miss·메모리 대기 증가를 확인했고, [[VectorVsHash_VTune ID 분리 실험|ID 분리]]로 줄어드는 것도 확인했다. 많은 키를 접근하는 map의 cache/TLB 기여는 아직 별도 수집 대상이다. 작은 입력의 결과를 그 조건에 그대로 적용하지 않는다.

요약 CSV·수집 로그: `VTuneVectorOrder/`. 상세 보고서는 그 아래 `원본 보고서/`에 로컬 보관하며 Git에서 제외한다.  
[[VTuneVectorOrder/VTuneOrderSummary.csv|정규화 수치]] · [[VTuneVectorOrder/VTuneOrderCaptures.log|8회 수집 로그]]

원본 VTune 결과는 저장소 루트의 `tmp/vtune_order_*` 폴더에 있다. 재현 스크립트는 `재현/VTuneOrderCaptures.cmd`, 함수 보정·보고서 추출은 `재현/ReadVTuneOrderCaptures.ps1`와 `재현/ReadVTuneOrderModules.ps1`이다.
