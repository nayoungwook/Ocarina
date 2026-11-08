# Ocarina

Chestnut의 C 컴파일러와 VM을 C++로 포팅하고 Tonic 엔진을 통합한 프로젝트입니다.
소스 파싱, 의미 분석, IR 생성, code/data 로딩과 실행을 지원합니다.

## 빌드와 실행

Visual Studio에서 `Ocarina.slnx`를 열고 단일 `Ocarina` 프로젝트를 x64로 빌드합니다.
SDL3, SDL3_ttf, SDL3_mixer, GLEW, GLM 등의 의존성은 `Ocarina/dependencies`에 있습니다.
빌드 후 필요한 DLL과 셰이더를 실행 파일 옆으로 복사합니다.

소스 파일을 실행 파일 옆에 두고 다음 명령을 입력합니다.

```text
register test.cn test2.cn
run
register fibo.cn
run
quit
```

`register`는 기존 VM 데이터를 초기화한 뒤 지정한 파일들을 하나의 컴파일 단위로 처리합니다.
`#import "test2.cn"`는 현재 소스 파일 기준으로 읽으며 중복 등록과 순환 import를 건너뜁니다.
등록에 실패하면 이전 프로그램도 남지 않습니다. `run`은 전역 초기화 코드와 `main`을 실행합니다.
시작 시 별도 스레드가 셸 입력을 받고 명령 큐에 저장합니다. 메인 스레드는 명령 실행과 SDL 엔진 프레임을 처리하므로
창이 열린 동안에도 다음 명령을 입력할 수 있습니다. `stop`은 창을 닫고 셸을 유지합니다.
`register`와 `run`은 기존 창을 닫은 뒤 새 프로그램을 등록하거나 다시 실행합니다.
창의 닫기 버튼은 창만 닫으며, `quit` 또는 EOF는 창을 정리하고 입력 스레드를 합류한 뒤 셸을 종료합니다.

Ocarina 바이트코드는 `load file.cb`로 등록합니다. 여러 파일을 함께 지정할 수 있으며,
`load`의 상대 경로는 현재 작업 폴더를 기준으로 합니다.

## C++ API

```cpp
#include "vm.h"
#include <iostream>

ParserContext pc;
pc.register_file("test.cn");
pc.compile_files();

IRContext irc;
irc.gen_ir(std::move(pc));

CVM vm;
vm.register_ir("program", irc);
vm.load_files();
std::optional<VMOperand> result = vm.run(std::cout);
```

`CVM`이 등록·로딩·실행을 모두 담당합니다. `clear()`는 바이트, 메타데이터, 문자열 풀과 실행 상태를 초기화합니다.
인자 타입·지역 변수·객체 배치 정보는 모두 IR 바이트에 기록됩니다.
`register_ir()`와 `register_bytes()`는 같은 데이터를 등록하므로 파일로 저장한 후에도 보조 정보 없이 실행할 수 있습니다.

```cpp
CVM vm;
vm.register_bytecode("fibo.cb");
vm.load_files();
vm.run(std::cout);
```

IR은 Ocarina 단일 형식이며 API에서 형식을 선택하지 않습니다.
잘린 데이터, 잘못된 메타데이터·명령어·라벨·문자열 참조는 오류로 보고합니다.
로딩에 실패하면 이전에 로딩한 데이터는 유지합니다.
기존 형식의 `.cb` 파일은 소스에서 다시 컴파일해야 합니다.
## 언어와 메모리

언어 키워드는 `object`이며 함수, 생성자, 상속, 전역·지역·멤버 변수, 조건문과 반복문을 지원합니다.
`new Object(...)`, 재귀 호출, 숫자·문자열·bool·null과 벡터 연산을 사용할 수 있습니다.

`super(...)`는 자식 생성자에서 같은 객체의 직계 부모 생성자를 실행합니다.
부모 생성자가 있으면 자식도 생성자를 선언하고 첫 문장에서 `super(...)`를 정확히 한 번 호출해야 합니다.
호출 위치와 인자 개수·타입은 컴파일 시 검사합니다. 부모 생성자가 없으면 `super()`를 호출할 수 없습니다.
객체 필드 초기화는 부모부터 자식까지 한 번씩 수행한 뒤 생성자 본문을 실행합니다.

`+=`, `-=`, `*=`, `/=`, `%=` 복합 대입은 숫자·벡터 변수와 객체 멤버를 갱신합니다.
`%`는 곱셈·나눗셈과 같은 우선순위이며, 대입은 오른쪽부터 결합합니다.
복합 대입의 수신 객체는 한 번만 평가합니다. 멤버 대입은 문장으로 사용합니다.

```text
object Group {}
object Item { var value:int = 0; }

func main():void {
    var group:Group = new Group();
    var item:Item = new Item();
    group.add_child(item);
}
```

객체는 `VMHierarchy`에 안정적인 인덱스로 저장됩니다. 새 객체는 루트에 연결되며 각 객체가 부모 인덱스와 순서 있는 자식 목록을 가집니다.
`obj.add_child(child)`는 서로 다른 타입의 객체도 자식으로 연결합니다. 객체 내부에서는 `add_child(child)`로 현재 객체에 연결할 수 있습니다.
다른 부모에 속한 자식은 새 부모로 이동하고, 같은 부모에 중복 추가하면 목록을 유지합니다. 자기 자신·조상·null은 자식으로 추가할 수 없습니다.
GC, 개별 객체 해제와 자식 조회·분리 API는 구현하지 않습니다. 전체 hierarchy는 실행 상태 초기화 시 정리합니다.
기존 배열 타입·리터럴·인덱싱과 `push`, `remove`, `length`는 제거했습니다. 이를 사용하던 소스는 객체 계층 구조로 변경해야 합니다.

`ParserContext`, `TokenizerContext`, `IRContext`는 클래스입니다. AST 자식, 심볼, VM 객체 참조는 인덱스로 관리합니다.
AST·토큰·타입·스택·hierarchy 데이터는 C++ 컨테이너가 값을 소유합니다.
함수 프레임은 `CVM`의 컨테이너에서 처리하므로 VM 재귀 호출이 C++ 재귀 호출로 이어지지 않습니다.
현재 호출 깊이는 512단계, 프레임 총 크기는 1 MiB, 피연산자 스택은 262,144개로 제한합니다.
null 접근, hierarchy 순환 관계, 0으로 나누기와 스택 오류는 예외로 보고합니다.

지역·전역·객체 변수는 연속 바이트 버퍼에 저장하며, load/save는 바이트 offset으로 직접 접근해 `memcpy`합니다.
타입 태그는 offset으로 인덱싱하는 별도 배열에 저장합니다. 메모리 접근 시 map 검색, 타입·배치·초기화 검사와 암시적 변환을 하지 않습니다.
값의 타입 호환성은 컴파일 단계에서 검사하고, 필요한 숫자 변환은 컴파일러가 별도 `OP_CONVERT` 명령으로 생성합니다.
외부 바이트코드도 같은 컴파일러의 타입·메모리 배치 규약을 따라야 합니다.
load/save의 암시적 변환에 의존하던 기존 `.cb` 파일은 소스에서 다시 컴파일해야 합니다.

`&&`와 `||`는 양쪽 식을 평가합니다. 전위·후위 증감은 AST에서 구분하지 않으므로 값으로 사용하면 갱신 후 값을 사용합니다.
모든 반환 경로 검사, 접근 제한자, `**`와 비트 연산자는 아직 지원하지 않습니다.

복합 대입 회귀 검사는 x64 Debug 빌드 후 `powershell -File tests/run_compound_assignment.ps1`로 실행합니다.
메모리 배치·변환·컴파일 시 타입 검사 회귀 검사는 `powershell -File tests/run_memory_storage.ps1`로 실행합니다.
부모 생성자 호출과 컴파일 시 사용 규칙 검사는 `powershell -File tests/run_super_constructor.ps1`로 실행합니다.
객체 연결과 제거된 배열 문법 검사는 `powershell -File tests/run_hierarchy.ps1`로 실행합니다.
창이 열린 동안의 셸 입력·재실행·종료 검사는 `powershell -File tests/run_shell_engine.ps1`로 실행합니다.
이 검사는 실제 SDL 창을 잠시 열었다가 닫습니다.

## 내장 함수와 엔진

| 함수 | 반환값 | 동작 |
| --- | --- | --- |
| `print(...)` | void | 인자 순서대로 출력 |
| `vector(...)` | vector | 숫자 성분을 최대 세 개 받아 벡터 생성 |
| `window(string, int, int)` | window | SDL 창 생성 |

내장 함수는 일반 함수 목록과 분리한 `syscall_smtb`에 등록합니다. `sys_gc`는 포함하지 않습니다.
`window()`로 만든 창은 스크립트의 `main` 실행 후 셸의 엔진 루프에서 갱신합니다.
SDL 창 생성·이벤트·렌더링과 VM 명령 실행은 메인 스레드에서 처리하며, 셸 입력은 별도 스레드에서 받습니다.
임베딩 API의 `CVM::run(out)`은 기존처럼 창이 닫힐 때까지 실행합니다.
`CVM::run(out, CVM::RunMode::ReturnAfterMain)`은 창을 유지한 채 반환하므로 호출자가 `engine.run_frame()`과 종료 처리를 담당합니다.

Tonic 엔진의 C++ API는 `tonic_context.h`의 `TonicContext`로 사용합니다.
창·프레임, 도형·텍스처·텍스트 그리기, 입력, 카메라와 오디오를 지원합니다.
엔진의 추가 렌더링·모델·조명·UI 기능은 `Ocarina/engine`에 있습니다.

## 파일 구성

| 파일 | 역할 |
| --- | --- |
| `token.h`, `lexer.cpp` | 토큰과 토큰화 |
| `ast.h` | 값으로 소유하는 AST 구조 |
| `parser.cpp` | 구문 분석 |
| `source_files.cpp` | 소스 등록과 import 처리 |
| `type.cpp` | 타입 생성과 조회 |
| `semantics.cpp`, `syscall_semantics.cpp` | 심볼·타입·호출 검사 |
| `ir.cpp` | IR 생성 |
| `code_data.cpp`, `ir_load.cpp` | VM 데이터와 실제 IR 로딩 |
| `vm.cpp`, `vm_execute.cpp`, `vm_instruction.cpp` | 등록과 실행 |
| `vm_memory.cpp`, `vm_numeric.cpp`, `vm_syscall.cpp` | 메모리·연산·내장 함수 |
| `vm_hierarchy.cpp` | 객체 저장과 부모·자식 연결 |
| `util.cpp` | 파일 읽기·쓰기 |
| `main.cpp` | 명령 셸 |
