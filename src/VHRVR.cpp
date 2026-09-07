#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <mutex>
#include <cmath>
#include <cstdio>
#include "openvr_capi.h"
#include "MinHook.h"
#include <string>

extern "C" {
__declspec(dllimport) uint32_t __cdecl VR_InitInternal(EVRInitError*, EVRApplicationType);
__declspec(dllimport) void __cdecl VR_ShutdownInternal();
__declspec(dllimport) intptr_t __cdecl VR_GetGenericInterface(const char*, EVRInitError*);
}

// Private prototype: GameMaker renders two views; this bridge submits their backbuffer halves.
static std::mutex gate;
// Use the flat C ABI. MinGW and MSVC disagree on C++ virtual calls returning matrices.
static VR_IVRSystem_FnTable* systemVR=nullptr;
static VR_IVRCompositor_FnTable* compositorVR=nullptr;
static bool active=false, poseValid=false, theatre=true, screenRecenter=true;
static VR_IVROverlay_FnTable* overlayVR=nullptr;
static VROverlayHandle_t screenHandle=0;
static ID3D11Texture2D* screenTexture=nullptr;
extern "C" double vhr_poll();
static TrackedDevicePose_t poses[k_unMaxTrackedDeviceCount]{};
static float eyePose[2][12]{};
static float center[3]{}, yawCenter=0;
static bool recenter=true;
static ID3D11Texture2D* eyes[2]{};
static ID3D11Device* textureDevice=nullptr;
static UINT width=0,height=0;
static int errorCode=0;
using PresentFn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
static PresentFn originalPresent=nullptr;
static void* presentAddress=nullptr;
static void releaseTextures(){if(screenTexture){screenTexture->Release();screenTexture=nullptr;}for(auto& t:eyes){if(t)t->Release();t=nullptr;}if(textureDevice)textureDevice->Release();textureDevice=nullptr;width=height=0;}
static HRESULT STDMETHODCALLTYPE onPresent(IDXGISwapChain* swap,UINT interval,UINT flags){
    if(active && theatre && !(flags&DXGI_PRESENT_TEST)) vhr_poll();
    {
        std::lock_guard<std::mutex> lock(gate);
        if(active && poseValid && !(flags&DXGI_PRESENT_TEST)){
            ID3D11Texture2D* source=nullptr;
            ID3D11Device* device=nullptr;
            ID3D11DeviceContext* context=nullptr;
            if(SUCCEEDED(swap->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&source)) && SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device),(void**)&device))){
                D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);
                // Never submit a minimized or multisampled buffer with an incompatible format.
                if(desc.Width>=512 && desc.Height>=256 && desc.Width%2==0 && desc.SampleDesc.Count==1){
                    device->GetImmediateContext(&context);
                    auto sourceDesc=desc; UINT wantedWidth=theatre?1536:desc.Width/2,wantedHeight=theatre?1536:desc.Height;
                    if(width!=wantedWidth || height!=wantedHeight || device!=textureDevice){
                        releaseTextures();width=wantedWidth;height=wantedHeight;
                        textureDevice=device;textureDevice->AddRef();
                        desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
                        desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
                        desc.CPUAccessFlags=0;desc.MiscFlags=0;
                        for(auto& t:eyes) if(FAILED(device->CreateTexture2D(&desc,nullptr,&t)))errorCode=-20;
                    }
                    if(eyes[0] && eyes[1]){
                                                if(theatre && overlayVR && screenHandle) {
                            D3D11_TEXTURE2D_DESC sd{};if(screenTexture)screenTexture->GetDesc(&sd);
                            if(!screenTexture || sd.Width!=sourceDesc.Width || sd.Height!=sourceDesc.Height || sd.Format!=sourceDesc.Format){
                                if(screenTexture){screenTexture->Release();screenTexture=nullptr;}
                                sourceDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;sourceDesc.MiscFlags=0;sourceDesc.CPUAccessFlags=0;sourceDesc.Usage=D3D11_USAGE_DEFAULT;
                                device->CreateTexture2D(&sourceDesc,nullptr,&screenTexture);
                            }
                            if(screenTexture){context->CopyResource(screenTexture,source);Texture_t st{screenTexture,ETextureType_TextureType_DirectX,EColorSpace_ColorSpace_Gamma};context->Flush();overlayVR->SetOverlayTexture(screenHandle,&st);}
                            if(screenRecenter){
                                DXGI_SWAP_CHAIN_DESC gameWindow{};if(SUCCEEDED(swap->GetDesc(&gameWindow)))SetForegroundWindow(gameWindow.OutputWindow);
                                const auto& h=poses[0].mDeviceToAbsoluteTracking;
                                float yaw=atan2f(h.m[0][2],h.m[2][2]),c=cosf(yaw),s=sinf(yaw);
                                HmdMatrix34_t t{};t.m[0][0]=c;t.m[0][2]=s;t.m[1][1]=1;t.m[2][0]=-s;t.m[2][2]=c;
                                t.m[0][3]=h.m[0][3]-s*3;t.m[1][3]=h.m[1][3];t.m[2][3]=h.m[2][3]-c*3;
                                overlayVR->SetOverlayWidthInMeters(screenHandle,3.2f);
                                overlayVR->SetOverlayTransformAbsolute(screenHandle,ETrackingUniverseOrigin_TrackingUniverseStanding,&t);screenRecenter=false;
                            }
                            overlayVR->ShowOverlay(screenHandle);
                            const float black[4]={0,0,0,1};
                            for(auto eye:eyes){ID3D11RenderTargetView* rtv=nullptr;if(SUCCEEDED(device->CreateRenderTargetView(eye,nullptr,&rtv))){context->ClearRenderTargetView(rtv,black);rtv->Release();}}
                        } else {
                            for(UINT i=0;i<2;i++){
                                D3D11_BOX box{i*width,0,0,(i+1)*width,height,1};
                                context->CopySubresourceRegion(eyes[i],0,0,0,0,source,0,&box);
                            }
                        }
                        context->Flush();
                        for(int i=0;i<2;i++){
                            Texture_t t{eyes[i],ETextureType_TextureType_DirectX,EColorSpace_ColorSpace_Gamma};
                            auto result=compositorVR->Submit((EVREye)i,&t,nullptr,EVRSubmitFlags_Submit_Default);
                            if(result!=EVRCompositorError_VRCompositorError_None)errorCode=(int)result;
                        }
                        compositorVR->PostPresentHandoff();
                    }
                }
            }
            if(context)context->Release();
            if(device)device->Release();
            if(source)source->Release();
            poseValid=false; // Each pose frame may only be submitted once.
        }
    }
    return originalPresent(swap,interval,flags);
}
static bool installHook(){
    if(presentAddress)return true;
    HWND window=CreateWindowExW(0,L"STATIC",L"VHRVR hidden probe",WS_POPUP,0,0,16,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return false;
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=1;desc.BufferDesc.Width=16;desc.BufferDesc.Height=16;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow=window;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain* swap=nullptr;ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;
    HRESULT result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,nullptr,&context);
    bool ok=false;
    if(SUCCEEDED(result)){
        void* target=(*reinterpret_cast<void***>(swap))[8];
        auto mh=MH_Initialize();
        if((mh==MH_OK || mh==MH_ERROR_ALREADY_INITIALIZED) && MH_CreateHook(target,(void*)onPresent,(void**)&originalPresent)==MH_OK){
            if(MH_EnableHook(target)==MH_OK){presentAddress=target;ok=true;}
            else MH_RemoveHook(target);
        }
    }
    if(context)context->Release();
    if(device)device->Release();
    if(swap)swap->Release();
    DestroyWindow(window);
    return ok;
}
#define API extern "C" __declspec(dllexport) double
API vhr_init(){
    std::lock_guard<std::mutex> lock(gate);
    if(active)return 0;
    EVRInitError err=EVRInitError_VRInitError_None;
    VR_InitInternal(&err,EVRApplicationType_VRApplication_Scene);
    if(err!=EVRInitError_VRInitError_None){errorCode=(int)err;return errorCode;}
    systemVR=reinterpret_cast<VR_IVRSystem_FnTable*>(VR_GetGenericInterface((std::string("FnTable:")+IVRSystem_Version).c_str(),&err));
    compositorVR=reinterpret_cast<VR_IVRCompositor_FnTable*>(VR_GetGenericInterface((std::string("FnTable:")+IVRCompositor_Version).c_str(),&err));
    if(!systemVR || !compositorVR || err!=EVRInitError_VRInitError_None || !installHook()){
        VR_ShutdownInternal();systemVR=nullptr;compositorVR=nullptr;return errorCode=-10;
    }
    compositorVR->SetTrackingSpace(ETrackingUniverseOrigin_TrackingUniverseStanding);
        overlayVR=reinterpret_cast<VR_IVROverlay_FnTable*>(VR_GetGenericInterface((std::string("FnTable:")+IVROverlay_Version).c_str(),&err));
    if(overlayVR){char key[]="vhrvr.theatre",title[]="Victory Heat Rally";overlayVR->CreateOverlay(key,title,&screenHandle);}
    if(!overlayVR || !screenHandle){VR_ShutdownInternal();systemVR=nullptr;compositorVR=nullptr;overlayVR=nullptr;return errorCode=-11;}
    active=true;theatre=true;screenRecenter=true;recenter=true;errorCode=0;return 0;
}
API vhr_stop(){std::lock_guard<std::mutex> lock(gate);active=false;poseValid=false;if(overlayVR && screenHandle){overlayVR->HideOverlay(screenHandle);overlayVR->DestroyOverlay(screenHandle);}screenHandle=0;overlayVR=nullptr;releaseTextures();if(systemVR)VR_ShutdownInternal();systemVR=nullptr;compositorVR=nullptr;return 0;}
API vhr_recenter(){std::lock_guard<std::mutex> lock(gate);recenter=true;screenRecenter=true;return 0;}
API vhr_poll(){
    std::lock_guard<std::mutex> lock(gate);
    if(!active)return 0;
        // Runtime-origin changes must also reset our cached recenter origin.
    VREvent_t event{};
    if(systemVR->PollNextEvent) while(systemVR->PollNextEvent(&event,sizeof(event))) {
        if(event.eventType==EVREventType_VREvent_SeatedZeroPoseReset || event.eventType==EVREventType_VREvent_StandingZeroPoseReset || event.eventType==EVREventType_VREvent_ChaperoneUniverseHasChanged) {recenter=true;screenRecenter=true;}
    }
    auto e=compositorVR->WaitGetPoses(poses,k_unMaxTrackedDeviceCount,nullptr,0);
    if(e!=EVRCompositorError_VRCompositorError_None || !poses[0].bPoseIsValid){poseValid=false;return 0;}
    const auto& h=poses[0].mDeviceToAbsoluteTracking;
    if(recenter){for(int i=0;i<3;i++)center[i]=h.m[i][3];yawCenter=atan2f(h.m[0][2],h.m[2][2]);recenter=false;}
    float c=cosf(yawCenter),s=sinf(yawCenter);
    for(int eye=0;eye<2;eye++){
        auto offset=systemVR->GetEyeToHeadTransform((EVREye)eye);
        float p[3][4]{};
        for(int r=0;r<3;r++)for(int col=0;col<4;col++){
            for(int k=0;k<3;k++)p[r][col]+=h.m[r][k]*offset.m[k][col];
            if(col==3)p[r][col]+=h.m[r][3]-center[r];
        }
        for(int col=0;col<4;col++){
            eyePose[eye][col]=c*p[0][col]-s*p[2][col];
            eyePose[eye][4+col]=p[1][col];
            eyePose[eye][8+col]=s*p[0][col]+c*p[2][col];
        }
    }
    poseValid=true;return 1;
}
API vhr_value(double eye,double field){
    std::lock_guard<std::mutex> lock(gate);
    int i=(int)eye,k=(int)field;if(i<0||i>1)return 0;
    if(k>=0 && k<12)return eyePose[i][k];
    if(k==99)return errorCode;
    if(systemVR && k==30)return systemVR->GetEyeToHeadTransform((EVREye)i).m[0][3];
    if(systemVR && k>=20 && k<=23){float l,r,t,b;systemVR->GetProjectionRaw((EVREye)i,&l,&r,&t,&b);float v[]={l,r,t,b};return v[k-20];}
    return 0;
}
// DLL stays loaded until game exit; do not call external_free while the Present hook is installed.

API vhr_mode(double scene){
    std::lock_guard<std::mutex> lock(gate);theatre=scene<0.5;poseValid=false;screenRecenter=true;recenter=true;
    if(overlayVR && screenHandle && !theatre)overlayVR->HideOverlay(screenHandle);
    releaseTextures();return 0;
}
