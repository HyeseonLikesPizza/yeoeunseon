## 증상

```text
Cannot enable Hardware Event-Based Sampling or Hardware Tracing.
Please check the Sampling Driver / administrator privileges.
```

드라이버가 없다는 확정 메시지가 아니다. 드라이버 상태와 실행 권한을 따로 확인해야 한다.

---

## 실제로 확인하고 해결한 것

2026-10-10, VTune 2026.4 / Intel Core i5-14600KF 환경에서 확인했다.

- `sepdrv5`, `sepdal` 드라이버는 이미 `RUNNING`이었다.
- 일반 실행의 Windows 관리자 권한은 `False`였다. 도구의 샌드박스 밖 실행도 Windows 관리자 실행과는 달랐다.
- UAC를 거쳐 관리자 권한으로 실행하자 하드웨어 수집이 성공했다.
- 드라이버 재설치, 보안 기능 해제, 재부팅은 하지 않았다.

```text
Administrator=True
Collection started
Collector Type: Event-based sampling driver
ExitCode=0
```

**다음에는 VTune을 우클릭 → 관리자 권한으로 실행한 뒤 수집한다.**

Intel 공식 문서에도 Windows sampling driver 확인은 관리자 명령 프롬프트에서 수행하도록 안내되어 있다. [Sampling Drivers for Windows Targets](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/install-sampling-drivers-for-windows-targets.html)

---

## 수집과 함수 표시까지 확인

`VectorVsHashFactors.exe profile vector 8 50000 3`을 대상으로 약 3초간 `uarch-exploration`을 수집했다. 이는 수집 기능 확인용이며 vector/map 성능 비교가 아니다.

처음에는 한글 경로의 실행 파일을 찾지 못한다는 경고와 함수 주소만 표시되는 문제가 있었다. 결과를 다시 finalize할 때 실행 파일과 일치하는 PDB가 있는 `tmp` 폴더를 search directory로 지정한 뒤 함수 이름이 표시됐다.

| 확인한 함수 | CPU Time | Clockticks | Instructions Retired | CPI |
|---|---:|---:|---:|---:|
| `VectorLookup<8>` | 2.839 s | 14,339,376,000 | 66,515,278,000 | 0.216 |

OS/외부 모듈의 PDB 경고가 남아 있어도 우리 함수의 hardware event 수집 실패를 뜻하지는 않는다. 우리 실행 파일의 함수 이름이 표시되는지 별도로 확인한다.

실행용 스크립트: `재현/VTuneHardwareCheck.cmd`  
원본 로그: `tmp/VTuneHardwareCheck.log`  
결과 폴더: `tmp/vtune_hardware_verified_01`

**수집 문제는 해결됐다.** 위 수집 확인용 숫자만으로 캐시 원인을 확정하지 않는다. 이후 [[VectorVsHash_VTune 조회 순서 실험|작은 입력의 순서 변경 실험]]에서 분기 실패와 CPI 증가를, [[VectorVsHash_VTune 큰 원소 실험|큰 원소 실험]]에서 cache miss·메모리 대기 증가를 확인했다.
