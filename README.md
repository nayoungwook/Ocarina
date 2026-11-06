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
`quit` 또는 EOF로 셸을 종료합니다.

최신 Chestnut 바이트코드는 `load file.cb`로 등록합니다. 여러 파일을 함께 지정할 수 있으며,
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
현재 Ocarina IR은 `register_ir()`로 컴파일러의 인자 타입·지역 변수·객체 배치 정보를 함께 전달합니다.
`register_bytes()`만 사용하면 구형 IR에서 인자가 있는 함수의 서명을 알 수 없습니다.

```cpp
CVM vm;
vm.register_bytecode("fibo.cb");
vm.load_files();
vm.run(std::cout);
```

Ocarina와 최신 Chestnut의 바이너리 형식은 `IRFormat::Ocarina`, `IRFormat::Chestnut`으로 구분합니다.
`register_bytes()`의 기본값은 Ocarina, `register_bytecode()`의 기본값은 Chestnut입니다.
서로 다른 형식을 한 CVM에 섞을 수 없습니다. 잘린 데이터, 잘못된 명령어·라벨·문자열 참조는 오류로 보고하며,
로딩에 실패하면 이전에 로딩한 데이터는 유지합니다.

## 언어와 메모리

언어 키워드는 `object`이며 함수, 생성자, 상속, 전역·지역·멤버 변수, 조건문과 반복문을 지원합니다.
`new Object(...)`, 재귀 호출, 숫자·문자열·bool·null, 배열과 벡터 연산을 사용할 수 있습니다.

```text
var values: array<int> = {1, 2, 3};
values.push(4);
values[0] = 5;
values.remove(1);
for (var i:int = 0; i < values.length; i++) {
    print(values[i], "\n");
}
```

배열은 중첩 타입과 인덱싱, 인자·반환값, `push`, `remove`, 읽기 전용 `length`를 지원합니다.
배열 원소의 숫자 타입은 선언한 타입과 일치해야 합니다. 배열 원소 대입은 문장으로 사용합니다.
`fibo.cn`은 메모이제이션으로 40개 값을 출력하며 마지막 값은 `102334155`입니다.

`ParserContext`, `TokenizerContext`, `IRContext`는 클래스입니다. AST 자식, 심볼, VM 객체 참조는 인덱스로 관리합니다.
AST·토큰·타입·스택·heap 데이터는 C++ 컨테이너가 값을 소유하며 별도의 수동 해제가 필요하지 않습니다.
함수 프레임은 `CVM`의 컨테이너에서 처리하므로 VM 재귀 호출이 C++ 재귀 호출로 이어지지 않습니다.
현재 호출 깊이는 512단계, 프레임 총 크기는 1 MiB, 피연산자 스택은 262,144개로 제한합니다.
배열 범위 초과, null 접근, 0으로 나누기와 스택 오류는 예외로 보고합니다.

`&&`와 `||`는 양쪽 식을 평가합니다. 전위·후위 증감은 AST에서 구분하지 않으므로 값으로 사용하면 갱신 후 값을 사용합니다.
모든 반환 경로 검사, 접근 제한자, `**`와 비트 연산자는 아직 지원하지 않습니다.

## 내장 함수와 엔진

| 함수 | 반환값 | 동작 |
| --- | --- | --- |
| `print(...)` | void | 인자 순서대로 출력 |
| `vector(...)` | vector | 숫자 성분을 최대 세 개 받아 벡터 생성 |
| `window(string, int, int)` | window | SDL 창 생성 |

내장 함수는 일반 함수 목록과 분리한 `syscall_smtb`에 등록합니다. `sys_gc`는 포함하지 않습니다.
`window()`로 만든 창은 `main` 실행 후 종료·닫기 이벤트를 기다립니다.
SDL 창을 사용하는 셸과 VM은 메인 스레드에서 실행합니다.

Tonic 엔진의 C++ API는 `tonic_context.h`의 `TonicContext`로 사용합니다.
창·프레임, 도형·텍스처·텍스트 그리기, 입력, 카메라와 오디오를 지원합니다.
엔진의 추가 렌더링·모델·조명·UI 기능은 `Ocarina/engine`에 있습니다.

## 파일 구성

| 파일 | 역할 |
| --- | --- |
| `token.h`, `lexer.cpp` | 토큰과 토큰화 |
| `ast.h` | 값으로 소유하는 AST 구조 |
| `parser.cpp`, `parser_array.cpp` | 구문 분석 |
| `source_files.cpp` | 소스 등록과 import 처리 |
| `type.cpp` | 타입 생성과 조회 |
| `semantics.cpp`, `semantics_array.cpp`, `syscall_semantics.cpp` | 심볼·타입·호출 검사 |
| `ir.cpp`, `ir_array.cpp` | IR 생성 |
| `code_data.cpp`, `ir_load.cpp` | VM 데이터와 실제 IR 로딩 |
| `vm.cpp`, `vm_execute.cpp`, `vm_instruction.cpp` | 등록과 실행 |
| `vm_memory.cpp`, `vm_numeric.cpp`, `vm_syscall.cpp` | 메모리·연산·내장 함수 |
| `util.cpp` | 파일 읽기·쓰기 |
| `main.cpp` | 명령 셸 |
