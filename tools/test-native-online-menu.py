from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
work=root.parents[1]/'work'
source=(root/'src/game/main.c').read_text()
start=source.index('#ifndef __EMSCRIPTEN__\nstatic PartyBoardNativeOnlineMenuHandler')
end=source.index('PARTYBOARD_BROWSER_API int PartyBoard_GameLifecyclePhase',start)
fixture=work/'native-online-menu-test.cpp'
fixture.write_text('''#include <cassert>
#include <cstddef>
#include <iostream>
#define PARTYBOARD_BROWSER_API
typedef int (*PartyBoardNativeOnlineMenuHandler)(void*,int);
static unsigned char partyBoardEnginePhase;
'''+source[start:end]+'''
static int calls, result, action;
int handler(void* context,int value){assert(context==&calls);calls++;action=value;return result;}
int main(){assert(!PartyBoard_OnlineMenuRequest(0));assert(PartyBoardNativeSetOnlineMenuHandler(handler,&calls));assert(!PartyBoardNativeSetOnlineMenuHandler(handler,&calls));
assert(!PartyBoard_OnlineMenuRequest(-1)&&!PartyBoard_OnlineMenuRequest(2)&&calls==0);
partyBoardEnginePhase=3;assert(!PartyBoardNativeSetOnlineMenuHandler(nullptr,nullptr));assert(!PartyBoardNativeClearOwnedOnlineMenuHandler(handler,&calls));
assert(!PartyBoard_OnlineMenuRequest(0)&&calls==1&&action==0);result=1;assert(PartyBoard_OnlineMenuRequest(1)&&calls==2&&action==1);
result=2;assert(!PartyBoard_OnlineMenuRequest(0));partyBoardEnginePhase=2;assert(PartyBoardNativeSetOnlineMenuHandler(nullptr,nullptr));assert(!PartyBoard_OnlineMenuRequest(1));
assert(PartyBoardNativeSetOnlineMenuHandler(handler,&calls));
assert(!PartyBoardNativeClearOwnedOnlineMenuHandler(nullptr,&calls));
assert(!PartyBoardNativeClearOwnedOnlineMenuHandler(handler,&result));
result=1;assert(PartyBoard_OnlineMenuRequest(0));
assert(PartyBoardNativeClearOwnedOnlineMenuHandler(handler,&calls));
assert(!PartyBoardNativeClearOwnedOnlineMenuHandler(handler,&calls));assert(!PartyBoard_OnlineMenuRequest(0));
std::cout<<"PASS actual native menu dispatch: Host/Join, explicit acceptance, invalid actions, lifecycle and owned cleanup\\n";}
''')
subprocess.run(['cl','/nologo','/std:c++20','/EHsc',str(fixture),'/Fe'+str(work/'native-online-menu-test.exe'),'/Fo'+str(work/'native-online-menu-test.obj')],check=True)
subprocess.run([str(work/'native-online-menu-test.exe')],check=True)
