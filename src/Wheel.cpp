#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cstdio>
static IDirectInput8W* wi=nullptr;
static IDirectInputDevice8W* wd=nullptr;
static IDirectInputEffect* we=nullptr; static IDirectInputEffect* spring=nullptr;
static DIJOYSTATE2 ws{};
static HWND wh=nullptr;
static bool wvalid=false; static DWORD oldCenter=0; static bool haveCenter=false;
static void wheelLog(const wchar_t* what,long code=0){
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(GetModuleHandleW(L"VHRVR.dll"),path,MAX_PATH);
    wchar_t* end=wcsrchr(path,L'\\');if(!end)return;wcscpy(end+1,L"VHRVR-wheel.log");
    FILE* f=_wfopen(path,L"a");if(f){fwprintf(f,L"%llu %ls [%ld / 0x%08lx]\n",GetTickCount64(),what,code,(unsigned long)code);fclose(f);}
}
static BOOL CALLBACK chooseWheel(const DIDEVICEINSTANCEW* d,void*) {
        wheelLog(d->tszProductName,(long)d->guidProduct.Data1);
    bool usbG29=LOWORD(d->guidProduct.Data1)==0x046d && (HIWORD(d->guidProduct.Data1)==0xc24f || HIWORD(d->guidProduct.Data1)==0xc260);
    if(!usbG29 && !wcsstr(d->tszProductName,L"G29") && !wcsstr(d->tszInstanceName,L"G29"))return DIENUM_CONTINUE;
    if(SUCCEEDED(wi->CreateDevice(d->guidInstance,&wd,nullptr)))return DIENUM_STOP;
    return DIENUM_CONTINUE;
}
#define WAPI extern "C" __declspec(dllexport) double
WAPI vhr_wheel_stop(){
    if(spring){spring->Stop();spring->Release();spring=nullptr;}
    if(we){we->Stop();we->Release();we=nullptr;}
    if(wd){wd->SendForceFeedbackCommand(DISFFC_STOPALL);wd->Unacquire();if(haveCenter){DIPROPDWORD p{};p.diph.dwSize=sizeof(p);p.diph.dwHeaderSize=sizeof(DIPROPHEADER);p.diph.dwHow=DIPH_DEVICE;p.dwData=oldCenter;wd->SetProperty(DIPROP_AUTOCENTER,&p.diph);}wd->Release();wd=nullptr;haveCenter=false;}
    if(wi){wi->Release();wi=nullptr;}wvalid=false;return 0;
}
extern "C" double vhr_menu_poll(double);
WAPI vhr_wheel_init(){
    vhr_menu_poll(0);
    vhr_wheel_stop();wh=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(wh,&pid);
    if(pid!=GetCurrentProcessId()){wheelLog(L"Game is not foreground",-1);return -1;}
    if(FAILED(DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,(void**)&wi,nullptr)))return -2;
    wi->EnumDevices(DI8DEVCLASS_GAMECTRL,chooseWheel,nullptr,DIEDFL_ATTACHEDONLY);
    if(!wd){wheelLog(L"No matching attached G29",-3);vhr_wheel_stop();return -3;}
    if(FAILED(wd->SetDataFormat(&c_dfDIJoystick2)) || FAILED(wd->SetCooperativeLevel(wh,DISCL_EXCLUSIVE|DISCL_FOREGROUND))){vhr_wheel_stop();return -4;}
    DIPROPRANGE range{};range.diph.dwSize=sizeof(range);range.diph.dwHeaderSize=sizeof(DIPROPHEADER);range.diph.dwHow=DIPH_DEVICE;range.lMin=0;range.lMax=65535;
    wd->SetProperty(DIPROP_RANGE,&range.diph);
    DIPROPDWORD ac{};ac.diph.dwSize=sizeof(ac);ac.diph.dwHeaderSize=sizeof(DIPROPHEADER);ac.diph.dwHow=DIPH_DEVICE;ac.dwData=0;
    haveCenter=SUCCEEDED(wd->GetProperty(DIPROP_AUTOCENTER,&ac.diph));oldCenter=ac.dwData; // Preserve the driver centering until custom effects are requested.
        HRESULT acquired=wd->Acquire();
    if(FAILED(acquired)) {
        wheelLog(L"Exclusive acquisition failed; trying shared input",acquired);
        wd->Unacquire();
        if(FAILED(wd->SetCooperativeLevel(wh,DISCL_NONEXCLUSIVE|DISCL_FOREGROUND)) || FAILED(wd->Acquire())){wheelLog(L"Shared acquisition also failed",-5);vhr_wheel_stop();return -5;}
        wheelLog(L"Shared input enabled; force feedback unavailable");return 0;
    }
    DWORD axis=DIJOFS_X;LONG direction=0;DICONSTANTFORCE force{};
    DIEFFECT e{};e.dwSize=sizeof(e);e.dwFlags=DIEFF_CARTESIAN|DIEFF_OBJECTOFFSETS;e.dwDuration=150000;e.dwGain=DI_FFNOMINALMAX;e.dwTriggerButton=DIEB_NOTRIGGER;e.cAxes=1;e.rgdwAxes=&axis;e.rglDirection=&direction;e.cbTypeSpecificParams=sizeof(force);e.lpvTypeSpecificParams=&force;
    HRESULT fx=wd->CreateEffect(GUID_ConstantForce,&e,&we,nullptr);wheelLog(L"Input acquired; force effect result",fx);
        DICONDITION condition{};condition.lPositiveCoefficient=4000;condition.lNegativeCoefficient=4000;
    condition.dwPositiveSaturation=3000;condition.dwNegativeSaturation=3000;condition.lDeadBand=50;
    e.cbTypeSpecificParams=sizeof(condition);e.lpvTypeSpecificParams=&condition;
    HRESULT sh=wd->CreateEffect(GUID_Spring,&e,&spring,nullptr);wheelLog(L"Centering spring creation",sh);
    // Input can work even if the driver does not expose force feedback.
    return 0;
}
WAPI vhr_wheel_poll(){
    wvalid=false;if(!wd)return 0;
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    if(pid!=GetCurrentProcessId()){if(we)we->Stop();if(spring)spring->Stop();wd->Unacquire();return 0;}
    HRESULT hr=wd->Poll();if(FAILED(hr)){if(FAILED(wd->Acquire()))return 0;wd->Poll();}
    if(FAILED(wd->GetDeviceState(sizeof(ws),&ws)))return 0;
    wvalid=true;return 1;
}
WAPI vhr_wheel_value(double field){
    if(field==99)return (we && spring)?1:0;
    if(!wvalid)return 0;
    switch((int)field){
        case 0:return std::clamp((ws.lX-32767.5)/32767.5,-1.0,1.0);
        case 1:return std::clamp((65535.0-ws.lY)/65535.0,0.0,1.0);
        case 2:return std::clamp((65535.0-ws.lRz)/65535.0,0.0,1.0);
        case 3:return (ws.rgbButtons[2]&0x80)?1:0;
        case 4:return (ws.rgbButtons[3]&0x80)?1:0;
    }return 0;
}
WAPI vhr_wheel_force(double steerForce,double vibration,double enabled){
    if(!we)return -1;
    if(!wvalid || enabled<0.5){we->Stop();if(spring)spring->Stop();return 0;}
    if(!spring){wheelLog(L"Custom feedback requires centering spring",-6);return -6;}
    DIEFFECT hold{};hold.dwSize=sizeof(hold);hold.dwDuration=150000;spring->SetParameters(&hold,DIEP_DURATION|DIEP_START);
    // 30% spring plus up to 65% arcade detail force; both expire on stalled gameplay.
    double wave=sin(GetTickCount64()*0.001*6.28318530718*22.0);
    DICONSTANTFORCE force{(LONG)(std::clamp(steerForce+std::clamp(vibration,0.0,0.60)*wave,-0.65,0.65)*DI_FFNOMINALMAX)};
    DIEFFECT e{};e.dwSize=sizeof(e);e.dwDuration=150000;e.cbTypeSpecificParams=sizeof(force);e.lpvTypeSpecificParams=&force;
    return FAILED(we->SetParameters(&e,DIEP_TYPESPECIFICPARAMS|DIEP_DURATION|DIEP_START))?-2:0;
}

static IDirectInput8W* menuDI=nullptr;
static IDirectInputDevice8W* menuWheel=nullptr;
static unsigned menuNow=0,menuPrevious=0;
static BOOL CALLBACK chooseMenuWheel(const DIDEVICEINSTANCEW* d,void*){
    if(!wcsstr(d->tszProductName,L"G29") && !wcsstr(d->tszInstanceName,L"G29"))return DIENUM_CONTINUE;
    return SUCCEEDED(menuDI->CreateDevice(d->guidInstance,&menuWheel,nullptr))?DIENUM_STOP:DIENUM_CONTINUE;
}
WAPI vhr_menu_poll(double enabled){
    menuPrevious=menuNow;menuNow=0;
    if(enabled<0.5){if(menuWheel)menuWheel->Unacquire();return 0;}
    DWORD pid=0;HWND window=GetForegroundWindow();GetWindowThreadProcessId(window,&pid);if(pid!=GetCurrentProcessId())return 0;
    static ULONGLONG retry=0;
    if(!menuWheel){
        if(GetTickCount64()<retry)return 0;
        retry=GetTickCount64()+3000;
        if(!menuDI && FAILED(DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,(void**)&menuDI,nullptr)))return 0;
        menuDI->EnumDevices(DI8DEVCLASS_GAMECTRL,chooseMenuWheel,nullptr,DIEDFL_ATTACHEDONLY);
        if(!menuWheel)return 0;
        if(FAILED(menuWheel->SetDataFormat(&c_dfDIJoystick2)) || FAILED(menuWheel->SetCooperativeLevel(window,DISCL_NONEXCLUSIVE|DISCL_FOREGROUND))){menuWheel->Release();menuWheel=nullptr;return 0;}
    }
    menuWheel->Acquire();menuWheel->Poll();DIJOYSTATE2 state{};
    if(FAILED(menuWheel->GetDeviceState(sizeof(state),&state)))return 0;
    if(LOWORD(state.rgdwPOV[0])!=0xffff){
        unsigned a=((state.rgdwPOV[0]+2250)/4500)%8;
        if(a==7||a==0||a==1)menuNow|=1;
        if(a>=3&&a<=5)menuNow|=2;
        if(a>=5&&a<=7)menuNow|=4;
        if(a>=1&&a<=3)menuNow|=8;
    }
    return 1;
}
WAPI vhr_menu_key(double direction,double edge){
    if(direction<0 || direction>3)return 0;
    unsigned mask=1u<<(unsigned)direction;
    if(edge==1)return (menuNow&mask) && !(menuPrevious&mask);
    if(edge==2)return !(menuNow&mask) && (menuPrevious&mask);
    return (menuNow&mask)?1:0;
}


