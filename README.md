# Ocarina

Chestnut의 C 컴파일러와 VM을 C++로 포팅하고 Tonic 엔진을 통합한 프로젝트입니다.
소스 파싱, 의미 분석, IR 생성, code/data 로딩과 실행을 지원합니다.

## 빌드와 실행

Visual Studio에서 `Ocarina.slnx`를 열고 단일 `Ocarina` 프로젝트를 x64로 빌드합니다.
SDL3, SDL3_ttf, SDL3_mixer, GLEW, GLM 등의 의존성은 `Ocarina/dependencies`에 있습니다.
빌드 후 필요한 DLL과 셰이더를 실행 파일 옆으로 복사합니다.

소스 확장자는 `.oc`, Ocarina 바이트코드 확장자는 `.ob`입니다.
셸의 파일 경로는 현재 작업 폴더를 기준으로 합니다.
Visual Studio에서 디버그 실행할 때는 실행 파일 폴더(`x64/Debug` 또는 `x64/Release`)를 작업 폴더로 사용합니다.
터미널에서 실행할 때는 해당 터미널의 작업 폴더가 그대로 유지됩니다.

```text
compile test.oc test2.oc
register test.ob
run
compile fibo.oc
register fibo.ob
run
quit
```

`compile`은 지정한 소스와 import된 소스를 함께 검사하고, 각 소스 옆에 같은 이름의 `.ob`를 생성합니다.
`test.oc`가 `test2.oc`를 import하면 `compile test.oc`로 `test.ob`와 `test2.ob`가 생성됩니다.
생성된 바이트가 기존 파일과 같으면 덮어쓰지 않습니다. `test.oc`만 수정하면 변경 없는 `test2.ob`의 내용과 수정 시간은 유지됩니다.
`#import "test2.oc"`는 현재 소스 파일 기준으로 읽으며 중복 등록과 순환 import를 건너뜁니다.
컴파일은 VM에 자동 등록하지 않으며, 컴파일 오류가 발생하면 기존 `.ob`와 등록된 프로그램을 유지합니다.
`register`는 지정한 `.ob` 파일들로 현재 VM 프로그램을 교체합니다. 여러 파일을 함께 지정할 수 있습니다.
각 `.ob`는 자신의 import 의존성을 기록합니다. `register test.ob`는 `test2.ob`도 자동으로 읽으며,
의존 파일이 없으면 등록과 실행을 거부합니다. 의존 경로는 해당 `.ob`가 있는 폴더를 기준으로 해석합니다.
파일을 다시 컴파일한 후 `register`를 다시 호출해야 VM에 갱신된 바이트코드가 반영됩니다.
함수·객체·문자열 ID는 바이트코드에 기록된 값을 그대로 사용하며, 이름 기반 병합이나 ID 재배치는 하지 않습니다.
함께 로딩하는 파일의 ID가 충돌하면 오류로 보고합니다.
파일 읽기에 실패하면 이전 등록 내용을 유지합니다. `run`은 등록된 바이트코드를 로딩하고 전역 초기화 코드와 `main`을 실행합니다.
import 의존 파일은 `run`에서도 존재 여부를 다시 확인합니다. 함수·객체·변수·문자열 ID는 함께 컴파일할 때 공유합니다.
시작 시 별도 스레드가 셸 입력을 받고 명령 큐에 저장합니다. 메인 스레드는 명령 실행과 SDL 엔진 프레임을 처리하므로
창이 열린 동안에도 다음 명령을 입력할 수 있습니다. `stop`은 창을 닫고 셸을 유지합니다.
`register`와 `run`은 기존 창을 닫은 뒤 새 프로그램을 등록하거나 다시 실행합니다. `compile`은 실행 중인 창을 유지합니다.
창의 닫기 버튼은 창만 닫으며, `quit` 또는 EOF는 창을 정리하고 입력 스레드를 합류한 뒤 셸을 종료합니다.

셸 명령은 `compile`, `register`, `run`, `stop`, `quit`이며 `load` 명령은 제거했습니다.

## C++ API

```cpp
#include "vm.h"
#include <iostream>

ParserContext pc;
pc.register_file("test.oc");
pc.compile_files();

IRContext irc;
irc.gen_ir(std::move(pc));

OVM vm;
vm.register_ir("program", irc);
vm.load_files();
std::optional<VMOperand> result = vm.run(std::cout);
```

`OVM`이 등록·로딩·실행을 모두 담당합니다. `clear()`는 바이트, 메타데이터, 문자열 풀과 실행 상태를 초기화합니다.
기존 `std::runtime_error` 대신 `ovm_panic.h`의 `OVMPanic` 예외를 사용합니다. 컴파일 오류인 `FrontendError`도 이를 상속합니다.
셸은 이를 `OVM Panic:` 접두사와 함께 표시하며, VM 실행 오류에는 호출 추적이 포함됩니다.
인자 타입·지역 변수·객체 배치 정보는 모두 IR 바이트에 기록됩니다.
`register_ir()`와 `register_bytes()`는 같은 데이터를 등록하므로 파일로 저장한 후에도 보조 정보 없이 실행할 수 있습니다.

```cpp
OVM vm;
vm.register_bytecode("fibo.ob");
vm.load_files();
vm.run(std::cout);
```

IR은 Ocarina 단일 형식이며 API에서 형식을 선택하지 않습니다.
잘린 데이터, 잘못된 메타데이터·명령어·라벨·문자열 참조는 오류로 보고합니다.
로딩에 실패하면 이전에 로딩한 데이터는 유지합니다.
기존 형식의 `.cb` 파일은 소스에서 다시 컴파일해야 합니다.
## 언어와 메모리

언어 키워드는 `object`, `scene`이며 함수, 생성자, 상속, 전역·지역·멤버 변수, 조건문과 반복문을 지원합니다.
`new Object(...)`, 재귀 호출, 숫자·문자열·bool·null과 벡터 연산을 사용할 수 있습니다.

`scene`은 생성자·멤버·상속을 지원하며 기본 `update():void`, `render():void`를 가집니다.
`position`, `size`, `rotation`, `width`, `height` 같은 공간 속성을 자동으로 추가하지 않습니다.
모든 구체적인 씬을 공통 `scene` 타입의 변수·인자·반환값으로 사용할 수 있습니다.
씬은 씬만 상속할 수 있으며, `object`와 `scene` 사이의 대입·캐스트·상속은 허용하지 않습니다.
`set_scene(scene)`은 syscall 5번이며 VM 인스턴스마다 활성 씬 하나를 저장합니다.
활성 씬에만 고정 업데이트마다 `update()`를, 렌더 프레임마다 `render()`를 호출한 뒤 기존 일반 객체를 순회합니다.
씬 교체는 다음 콜백부터 반영되며 재실행·프로그램 초기화 시 활성 씬 참조를 지웁니다. `null` 등록은 실행 오류입니다.
VM 창은 스크립트의 렌더링 명령을 모으고 화면에 출력할 엔진 렌더 큐를 함께 준비합니다.

```text
scene MainScene {
    var ticks:int = 0;

    func update():void { ticks++; }
    func render():void {}
}

func main():void {
    window("Scene", 800, 600);
    var current:scene = new MainScene();
    set_scene(current);
}
```

벡터의 `x`, `y`, `z` 성분은 `float`이며 `v.x`, `object.position.y`처럼 읽을 수 있습니다.
변수에 저장된 벡터의 성분에는 `=`, 복합 대입, `++`, `--`를 문장으로 사용할 수 있습니다.
예: `v.x = 3; v.y += 2; object.position.z++;`
`vector(1, 2, 3).x`, `make_vector().y`처럼 임시 벡터의 성분을 읽을 수 있으며, 임시 벡터의 성분에는 대입할 수 없습니다.

`cast<Type>(expr)`는 명시적 타입 변환입니다. 원본 `is_castable()` 규칙을 양방향으로 검사하여
`int`·`float`·`double` 사이의 확대·축소 변환과 같은 상속 계층의 객체 업캐스트·다운캐스트를 허용합니다.
캐스트 식의 결과 타입만 바뀌며 원본 변수의 선언 타입은 유지됩니다. 숫자는 `OP_CONVERT`로 변환하고,
객체 참조는 같은 hierarchy 인덱스를 유지합니다. 객체의 실제 타입을 런타임에서 검사하지 않습니다.
서로 무관한 타입과 `void`로의 캐스트는 컴파일 오류입니다.

`this`는 현재 객체 인스턴스를 참조하는 예약어입니다. 메서드·생성자·멤버 초기화 식에서 사용할 수 있습니다.
`this.value`, `this.method()`, `return this`, 다른 함수에 `this`를 인자로 전달하는 표현을 지원합니다.
매개변수·지역 변수와 멤버 이름이 같을 때 `this.value`로 멤버를 지정할 수 있으며, `this` 자체에는 대입할 수 없습니다.

`super(...)`는 자식 생성자에서 같은 객체의 직계 부모 생성자를 실행합니다.
부모 생성자가 있으면 자식도 생성자를 선언하고 첫 문장에서 `super(...)`를 정확히 한 번 호출해야 합니다.
호출 위치와 인자 개수·타입은 컴파일 시 검사합니다. 부모 생성자가 없으면 `super()`를 호출할 수 없습니다.
객체 필드 초기화는 부모부터 자식까지 한 번씩 수행한 뒤 생성자 본문을 실행합니다.

`super.update()`, `super.render()`, `super.method(...)`는 같은 객체에서 부모 타입의 메서드를 직접 실행합니다.
함수를 작성한 타입의 직계 부모부터 찾으며 자식의 재정의 함수로 돌아가지 않습니다.
자식의 메서드와 생성자 안에서 사용할 수 있고, 일반 메서드의 인자 변환과 반환값도 지원합니다.

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
    var first:Item = cast<Item>(group.get_child(0));
}
```

객체는 `VMHierarchy`에 안정적인 인덱스로 저장됩니다. 새 객체는 루트에 연결되며 각 객체가 부모 인덱스와 순서 있는 자식 목록을 가집니다.
`obj.add_child(child)`는 서로 다른 타입의 객체도 자식으로 연결합니다. 객체 내부에서는 `add_child(child)`로 현재 객체에 연결할 수 있습니다.
다른 부모에 속한 자식은 새 부모로 이동하고, 같은 부모에 중복 추가하면 목록을 유지합니다. 자기 자신·조상·null은 자식으로 추가할 수 없습니다.
`obj.get_child(index)`는 현재 자식 목록의 0부터 시작하는 순서로 조회하며, 객체 내부에서는 `get_child(index)`로 호출할 수 있습니다.
인덱스는 숫자 하나를 받아 기존 숫자 변환 규칙에 따라 `int`로 변환합니다. 음수·범위 초과·null 수신 객체는 실행 오류입니다.
반환 타입 `object`는 공통 객체 참조입니다. 변수·매개변수·반환값으로 사용할 수 있고, 모든 구체적인 객체 타입을 담을 수 있습니다.
`object` 참조에서도 공통 멤버 `position`·`size`·`rotation`을 읽고 수정할 수 있습니다. 예: `obj.get_child(0).position.x += 1;`
공통 멤버는 구체적인 타입·부모 타입·`object` 참조에서 같은 저장 공간을 사용합니다.
사용자 정의 멤버에 접근하려면 `cast<Item>(obj.get_child(0))`처럼 캐스트합니다. `object` 참조에서는 `add_child`·`get_child`·`child_size`를 직접 호출할 수 있습니다.
GC, 개별 객체 해제와 자식 분리 API는 구현하지 않습니다. 전체 hierarchy는 실행 상태 초기화 시 정리합니다.
기존 배열 타입·리터럴·인덱싱과 `push`, `remove`, `length`는 제거했습니다. 이를 사용하던 소스는 객체 계층 구조로 변경해야 합니다.

엔진은 소유 VM을 직접 참조하며, 고정 업데이트마다 hierarchy의 모든 객체에 `update():void`를,
렌더 프레임마다 `render():void`를 호출합니다. 각 타입의 기본 구현은 빈 함수이며 재정의할 수 있습니다.
부모의 update·render도 실행하려면 자식의 함수에서 `super.update()`·`super.render()`를 명시적으로 호출합니다.
호출 순서는 객체 생성 순서이며, 한 순회 중 생성된 객체는 다음 순회부터 참여합니다.
Scene 없이도 두 함수가 실행됩니다. 창이 열려 있으면 `main`의 지역 객체를 포함한 VM 상태를 유지합니다.
`ReturnAfterMain`을 사용하는 C++ 호출자는 출력 스트림을 창 실행이 끝날 때까지 유지해야 합니다.

`ParserContext`, `TokenizerContext`, `IRContext`는 클래스입니다. AST 자식, 심볼, VM 객체 참조는 인덱스로 관리합니다.
AST·토큰·타입·스택·hierarchy 데이터는 C++ 컨테이너가 값을 소유합니다.
함수 프레임은 `OVM`의 컨테이너에서 처리하므로 VM 재귀 호출이 C++ 재귀 호출로 이어지지 않습니다.
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
캐스트 파서 검사는 `powershell -File tests/run_cast_parser.ps1`, 타입 검사·변환·실행 검사는 `powershell -File tests/run_cast.ps1`로 실행합니다.
부모 생성자 호출과 컴파일 시 사용 규칙 검사는 `powershell -File tests/run_super_constructor.ps1`로 실행합니다.
부모 메서드 호출과 다단계 상속·수신 객체·인자·반환값 검사는 `powershell -File tests/run_super_method.ps1`로 실행합니다.
객체 연결과 제거된 배열 문법 검사는 `powershell -File tests/run_hierarchy.ps1`로 실행합니다.
자식 조회·공통 객체 참조·캐스트·인덱스 검사는 `powershell -File tests/run_get_child.ps1`로 실행합니다.
`this` 참조·멤버 접근·상속·컴파일 시 사용 규칙 검사는 `powershell -File tests/run_this.ps1`로 실행합니다.
객체의 자동 update·render, 재정의·상속·생성·부모 변경·재실행 검사는 `powershell -File tests/run_hierarchy_frame.ps1`로 실행합니다.
창이 열린 동안의 셸 입력·재실행·종료 검사는 `powershell -File tests/run_shell_engine.ps1`로 실행합니다.
이 검사는 실제 SDL 창을 잠시 열었다가 닫습니다.
바이트코드 파일 생성·독립 실행·재등록과 셸 명령 검사는 `powershell -File tests/run_bytecode_shell.ps1`로 실행합니다.
파일별 출력·변경 없는 의존 파일 유지·중첩 import·의존 파일 누락 검사는 `powershell -File tests/run_split_compilation.ps1`로 실행합니다.

## 내장 함수와 엔진

| 함수 | 반환값 | 동작 |
| --- | --- | --- |
| `print(...)` | void | 인자 순서대로 출력 |
| `vector(...)` | vector | 숫자 성분을 최대 세 개 받아 벡터 생성 |
| `window(string, int, int)` | window | SDL 창 생성 |
| `set_scene(scene)` | void | 활성 씬 등록·교체 (syscall 5) |

내장 함수는 일반 함수 목록과 분리한 `syscall_smtb`에 등록합니다. `sys_gc`는 포함하지 않습니다.
`window()`로 만든 창은 스크립트의 `main` 실행 후 셸의 엔진 루프에서 갱신합니다.
SDL 창 생성·이벤트·렌더링과 VM 명령 실행은 메인 스레드에서 처리하며, 셸 입력은 별도 스레드에서 받습니다.
임베딩 API의 `OVM::run(out)`은 기존처럼 창이 닫힐 때까지 실행합니다.
`OVM::run(out, OVM::RunMode::ReturnAfterMain)`은 창을 유지한 채 반환하므로 호출자가 `engine.run_frame()`과 종료 처리를 담당합니다.

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
| `code_data.cpp`, `ir_load.cpp` | VM 데이터와 IR 로딩 |
| `vm.cpp`, `vm_execute.cpp`, `vm_instruction.cpp` | 등록과 실행 |
| `vm_memory.cpp`, `vm_numeric.cpp`, `vm_syscall.cpp` | 메모리·연산·내장 함수 |
| `vm_hierarchy.cpp` | 객체 저장과 부모·자식 연결 |
| `util.cpp` | 파일 읽기·쓰기 |
| `main.cpp`, `shell_commands.cpp` | 명령 셸과 컴파일·바이트코드 등록 |
