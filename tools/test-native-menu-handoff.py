from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
work = root.parents[1] / 'work'
source = (root / 'src/game/main.c').read_text()
start = source.index('#ifndef __EMSCRIPTEN__\nstatic PartyBoardNativeOnlineMenuHandler')
end = source.index('PARTYBOARD_BROWSER_API int PartyBoard_GameLifecyclePhase', start)
fixture = work / 'native-menu-handoff-test.cpp'
fixture.write_text('''#include <cassert>
#define PARTYBOARD_BROWSER_API
#include "port/native_menu_handoff.hpp"
static unsigned char partyBoardEnginePhase;
static int stops;
extern "C" int PartyBoard_GameLifecyclePhase(){return partyBoardEnginePhase;}
extern "C" void PartyBoard_GameRequestStop(){++stops;}
''' + source[start:end] + '''
int main(){
using partyboard::net::NativeMenuHandoff;
NativeMenuHandoff host, competitor;
assert(host.install());assert(!host.install());assert(!competitor.install());
partyBoardEnginePhase=3;
assert(!PartyBoard_OnlineMenuRequest(2));assert(stops==0);
assert(PartyBoard_OnlineMenuRequest(0));assert(stops==1);
assert(!PartyBoard_OnlineMenuRequest(1));assert(stops==1);
assert(!host.close());assert(!host.takeStoppedRequest());
partyBoardEnginePhase=2;
assert(!host.takeStoppedRequest());assert(host.close());
assert(host.takeStoppedRequest()==0);assert(!host.takeStoppedRequest());
assert(competitor.install());assert(!host.close());
partyBoardEnginePhase=3;assert(PartyBoard_OnlineMenuRequest(1));assert(stops==2);
partyBoardEnginePhase=2;assert(competitor.close());assert(competitor.takeStoppedRequest()==1);
assert(!PartyBoard_OnlineMenuRequest(0));
}
''')
subprocess.run(['cl', '/nologo', '/std:c++20', '/EHsc',
                '/I' + str(root / 'include'), str(fixture),
                '/Fe' + str(work / 'native-menu-handoff-test.exe'),
                '/Fo' + str(work / 'native-menu-handoff-test.obj')], check=True)
subprocess.run([str(work / 'native-menu-handoff-test.exe')], check=True)
print('PASS native Host/Join handoff: stop once, reject reentry, retain running owner, consume after stopped detach')
