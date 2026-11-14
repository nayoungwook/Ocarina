# VM 최적화 기록

측정: 2026-11-13, Intel Core Ultra 7 255H, Windows 11, MSVC x64 Release.

`before`는 이번 작업 시작 시점의 소스입니다. 사용자가 변경한 고정 용량 피연산자 스택과 커밋되지 않은 코드도 포함합니다.
`after`는 아래 결과를 낸 최적화 소스와 실행파일입니다. 피연산자 스택은 고정 용량을 유지합니다.

## 전후 실행 시간

단위는 ms, 각 버전·작업별 27개 표본의 중앙값입니다.

| 작업 | before | after | 속도 향상 |
| --- | ---: | ---: | ---: |
| 정수 연산 20만 회 | 38.014 | 10.008 | 3.80배 |
| double 연산 20만 회 | 37.920 | 10.029 | 3.78배 |
| 재귀 Fibonacci(22) | 11.964 | 2.466 | 4.85배 |
| 객체 메서드 20만 회 | 65.778 | 16.557 | 3.97배 |

같은 논리 CPU(affinity mask 1)에 고정하고 순서를 바꿔가며 3라운드를 측정했습니다. 라운드마다 워밍업 후 9개 표본을 수집했습니다.
소스 컴파일·바이트코드 로딩·프로세스 시작·콘솔 출력·GPU 렌더링은 제외하고, VM 실행 상태 준비와 정리는 포함합니다.
전후 체크섬은 모두 같습니다. 이 결과는 네 CPU 작업의 측정값이며 게임 전체 FPS의 개선율은 아닙니다.

원시 표본·환경·실행파일 해시: [comparison.json](comparison.json). 다른 언어와의 최신 비교: [벤치마크 보고서](../benchmarks/README.md).

## 변경 내용

- 호출 프레임과 지역 저장 버퍼를 재사용하고, 일반 함수·메서드 호출의 임시 인자 vector를 제거했습니다.
- 현재 프레임의 피연산자 스택 시작 위치를 캐시하고, 자주 실행하는 명령을 인라인 처리합니다.
- 같은 타입의 int/float/double 연산은 직접 계산합니다. 혼합 타입과 벡터는 기존 범용 경로를 사용합니다.
- 로더가 변수 메타데이터를 보고 숫자 지역·암시적 객체 멤버 load/save를 고정 크기 명령으로 바꿉니다. `.ob` 파일 형식은 유지합니다.
- 호출 위치별로 실제 수신 객체 타입과 대상 함수를 캐시합니다. 객체 타입이 바뀌면 다시 찾습니다.
- 컴파일러에서 중복 SP 증감과 반환 직전의 공통 라벨 이동을 제거했습니다. 기존 SP 명령은 계속 지원합니다.
- float/double 증감도 타입에 맞게 처리합니다. int 증감과 산술은 기존 32비트 wrap 동작을 유지합니다.

호출 깊이 512단계, 활성 지역 공간 1 MiB, 피연산자 262,144개 제한과 오류 검사를 유지합니다. 재사용 버퍼는 초기화하고, 비활성 프레임 캐시를 정리해 실행 종료 시 4 MiB 이하로 유지합니다.
JIT는 추가하지 않았습니다.

## 검증

- 빠른 숫자 경로와 기존 범용 경로 142,800개 차등 비교: 통과.
- 생성자·상속·super·다형 호출·중첩 생성·재귀·조기 반환·혼합 인자·벡터·scene 등록: 전후 통과.
- 상속받은 float/double 멤버와 bool 뒤의 비정렬 int 멤버, float/double 증감: 통과.
- 0으로 나누기·호출 깊이 초과·반환값 누락: 전후 모두 예상 오류.
- before 복원 후 after 재복원: 전체 소스 목록과 SHA-256 일치 여부를 [restoration.json](restoration.json)에 기록합니다.

검증 결과: [verification.json](verification.json). 검사 소스: [checks](checks).

## 다시 비교하거나 복원하기

프로젝트 루트에서 실행합니다.

```powershell
python bench.py compare
python bench.py verify
python bench.py restore --version before
python bench.py restore --version after
python bench.py
```

`compare`는 보관한 before 실행파일과 현재 소스를 빌드한 실행파일을 비교합니다. `verify`도 현재 소스를 먼저 빌드합니다. 이미 빌드했다면 `--no-build`로 생략할 수 있습니다.
`restore`는 소스만 복원하며, 직전 작업을 `restore-save-*`에 자동 백업합니다. 복원 후 빌드는 `python bench.py` 또는 Visual Studio에서 수행합니다.
프로젝트 C++/헤더/셰이더/프로젝트 파일과 루트 README·솔루션이 복원 대상입니다. 의존성, 게임 애셋, `.oc` 게임 코드와 벤치마크 도구는 유지합니다.

다음 최적화의 기준을 새 이름으로 보관할 수도 있습니다.

```powershell
python bench.py snapshot --version my-baseline
python bench.py compare --version my-baseline
python bench.py restore --version my-baseline
```

기존 백업 이름은 덮어쓰지 않습니다. `stage3`는 숫자 메모리 접근을 특수화하기 전 중간 버전입니다.
