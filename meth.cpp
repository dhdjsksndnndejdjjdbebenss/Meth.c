#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <link.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <string>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <cmath>
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "backends/imgui_impl_android.h"
#include "dobby.h"

#define TAG "Meth"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

// --- MOD TOGGLES ---
bool g_MenuOpen = true;
bool g_LongArms = false;
bool g_Cosmetics = false;
bool g_Fly = false;
bool g_Speed = false;
bool g_PullMod = false;
bool g_GiantSelf = false;
bool g_AntiKick = true;
bool g_ForceLeave = false;

void* il2cpp_handle = nullptr;
void* localPlayer = nullptr;

// --- UNIVERSAL IL2CPP API ---
typedef void* (*il2cpp_domain_get_t)();
typedef void* (*il2cpp_domain_get_assemblies_t)(void*, size_t*);
typedef void* (*il2cpp_assembly_get_image_t)(void*);
typedef void* (*il2cpp_class_from_name_t)(void*, const char*, const char*);
typedef void* (*il2cpp_class_get_method_from_name_t)(void*, const char*, int);
typedef void* (*il2cpp_class_get_field_from_name_t)(void*, const char*);
typedef size_t (*il2cpp_field_get_offset_t)(void*);

il2cpp_domain_get_t domain_get;
il2cpp_domain_get_assemblies_t domain_get_assemblies;
il2cpp_assembly_get_image_t assembly_get_image;
il2cpp_class_from_name_t class_from_name;
il2cpp_class_get_method_from_name_t method_from_name;
il2cpp_class_get_field_from_name_t class_get_field_from_name;
il2cpp_field_get_offset_t field_get_offset;

void InitIl2Cpp() {
    if (il2cpp_handle) return;
    il2cpp_handle = dlopen("libil2cpp.so", RTLD_LAZY);
    if (!il2cpp_handle) return;

    domain_get = (il2cpp_domain_get_t)dlsym(il2cpp_handle, "il2cpp_domain_get");
    domain_get_assemblies = (il2cpp_domain_get_assemblies_t)dlsym(il2cpp_handle, "il2cpp_domain_get_assemblies");
    assembly_get_image = (il2cpp_assembly_get_image_t)dlsym(il2cpp_handle, "il2cpp_assembly_get_image");
    class_from_name = (il2cpp_class_from_name_t)dlsym(il2cpp_handle, "il2cpp_class_from_name");
    method_from_name = (il2cpp_class_get_method_from_name_t)dlsym(il2cpp_handle, "il2cpp_class_get_method_from_name");
    class_get_field_from_name = (il2cpp_class_get_field_from_name_t)dlsym(il2cpp_handle, "il2cpp_class_get_field_from_name");
    field_get_offset = (il2cpp_field_get_offset_t)dlsym(il2cpp_handle, "il2cpp_field_get_offset");
}

void* FindMethod(const char* className, const char* methodName, int argsCount) {
    InitIl2Cpp();
    if (!domain_get) return nullptr;
    size_t count = 0;
    void** assemblies = (void**)domain_get_assemblies(domain_get(), &count);
    for (size_t i = 0; i < count; i++) {
        void* image = assembly_get_image(assemblies[i]);
        void* klass = class_from_name(image, "", className);
        if (klass) {
            void* method = method_from_name(klass, methodName, argsCount);
            if (method) return *(void**)method;
        }
    }
    return nullptr;
}

int GetFieldOffset(const char* className, const char* fieldName) {
    InitIl2Cpp();
    if (!domain_get) return 0;
    size_t count = 0;
    void** assemblies = (void**)domain_get_assemblies(domain_get(), &count);
    for (size_t i = 0; i < count; i++) {
        void* image = assembly_get_image(assemblies[i]);
        void* klass = class_from_name(image, "", className);
        if (klass) {
            void* field = class_get_field_from_name(klass, fieldName);
            if (field) return field_get_offset(field);
        }
    }
    return 0;
}

// --- GAMEPLAY HOOKS ---
typedef void (*Player_LateUpdate_t)(void* instance);
Player_LateUpdate_t orig_Player_LateUpdate = nullptr;

void hook_Player_LateUpdate(void* instance) {
    localPlayer = instance;
    orig_Player_LateUpdate(instance);

    if (g_Fly) {
        int velOffset = GetFieldOffset("GorillaLocomotion.Player", "velocity");
        if (velOffset) {
            float* velocity = (float*)((uintptr_t)instance + velOffset);
            velocity[1] = 5.0f; // Fly Up
        }
    }
    if (g_Speed) {
        int speedOffset = GetFieldOffset("GorillaLocomotion.Player", "maxJumpSpeed");
        if (speedOffset) {
            float* speed = (float*)((uintptr_t)instance + speedOffset);
            *speed = 20.0f;
        }
    }
}

typedef void (*VRRig_LateUpdate_t)(void* instance);
VRRig_LateUpdate_t orig_VRRig_LateUpdate = nullptr;

void hook_VRRig_LateUpdate(void* instance) {
    orig_VRRig_LateUpdate(instance);

    if (g_LongArms) {
        int armOffset = GetFieldOffset("VRRig", "leftHand");
        if (armOffset) {
            void* leftArm = (void*)((uintptr_t)instance + armOffset);
            float* s = (float*)((uintptr_t)leftArm + 0x40);
            if(s) { s[0]=5; s[1]=5; s[2]=5; } 
        }
    }
    if (g_GiantSelf) {
        void* transform = (void*)((uintptr_t)instance + 0x10);
        float* s = (float*)((uintptr_t)transform + 0x40);
        if(s) { s[0]=2; s[1]=2; s[2]=2; }
    }
    if (g_PullMod && instance != localPlayer && localPlayer) {
        void* myTransform = (void*)((uintptr_t)localPlayer + 0x10); 
        void* theirTransform = (void*)((uintptr_t)instance + 0x10);
        if (myTransform && theirTransform) {
            float* myPos = (float*)((uintptr_t)myTransform + 0x38);
            float* theirPos = (float*)((uintptr_t)theirTransform + 0x38);
            if (myPos && theirPos) {
                theirPos[0] = myPos[0];
                theirPos[1] = myPos[1];
                theirPos[2] = myPos[2];
            }
        }
    }
}

typedef bool (*HasCosmetic_t)(void* instance, void* id);
HasCosmetic_t orig_HasCosmetic = nullptr;
bool hook_HasCosmetic(void* instance, void* id) {
    if (g_Cosmetics) return true;
    return orig_HasCosmetic(instance, id);
}

// --- CONTROLLER INPUT (X BUTTON) ---
typedef bool (*OVRInput_GetDown_t)(int buttonMask);
OVRInput_GetDown_t orig_GetDown = nullptr;
bool hook_OVRInput_GetDown(int buttonMask) {
    if (buttonMask == 4) g_MenuOpen = !g_MenuOpen; // X Button
    return orig_GetDown(buttonMask);
}

// --- BRUTE FORCE ANTI-KICK ---
typedef void (*Disconnect_t)(void* instance);
Disconnect_t orig_Disconnect = nullptr;
void hook_Disconnect(void* instance) { 
    if (g_ForceLeave) { g_ForceLeave = false; orig_Disconnect(instance); }
    else { LOGI("Blocked server Disconnect"); }
}

typedef void (*LeaveRoom_t)(void* instance);
LeaveRoom_t orig_LeaveRoom = nullptr;
void hook_LeaveRoom(void* instance) { 
    if (g_ForceLeave) { g_ForceLeave = false; orig_LeaveRoom(instance); }
    else { LOGI("Blocked server LeaveRoom"); }
}

// --- ATTESTATION & MOTHERSHIP BYPASS ---
void patch_packet(char* buf, size_t len) {
    std::string data(buf, len);
    bool changed = false;
    auto rep = [&](const std::string& from, const std::string& to) {
        if (from.length() != to.length()) return;
        size_t pos = 0;
        while ((pos = data.find(from, pos)) != std::string::npos) {
            data.replace(pos, from.length(), to);
            pos += to.length();
            changed = true;
        }
    };
    rep("\"is_banned\":true ", "\"is_banned\":false");
    rep("\"banned\":true ", "\"banned\":false");
    rep("\"status\":\"deny\"", "\"status\":\"pass\"");
    rep("\"kick\":true", "\"kick\":false");
    rep("\"device_integrity_state\":\"NotTrusted\"", "\"device_integrity_state\":\"Advanced\"  ");
    if (changed) memcpy(buf, data.data(), len);
}

typedef int (*SSL_read_t)(void*, void*, int);
static SSL_read_t orig_SSL_read = nullptr;
int hook_SSL_read(void* ssl, void* buf, int num) {
    int n = orig_SSL_read(ssl, buf, num);
    if (n > 0) patch_packet((char*)buf, n);
    return n;
}
typedef int (*SSL_write_t)(void*, const void*, int);
static SSL_write_t orig_SSL_write = nullptr;
int hook_SSL_write(void* ssl, const void* buf, int num) {
    std::string data((char*)buf, num);
    if (data.find("cheat") != std::string::npos || data.find("mothership") != std::string::npos) {
        return num; 
    }
    patch_packet((char*)buf, num);
    return orig_SSL_write(ssl, buf, num);
}

// --- IMGUI MENU ---
void SetupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]       = ImVec4(0.05f, 0.02f, 0.05f, 0.95f);
    colors[ImGuiCol_TitleBg]        = ImVec4(0.40f, 0.00f, 0.25f, 1.00f);
    colors[ImGuiCol_TitleBgActive]  = ImVec4(0.80f, 0.00f, 0.50f, 1.00f);
    colors[ImGuiCol_CheckMark]      = ImVec4(1.00f, 0.20f, 0.60f, 1.00f);
    style.WindowRounding = 8.0f;
    style.FramePadding = ImVec2(25, 25); // Huge touch targets
}

void RenderMenu() {
    if (!g_MenuOpen) return;
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Meth | Universal", &g_MenuOpen, ImGuiWindowFlags_NoCollapse);
    
    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.6f, 1.0f), "X to Toggle Menu");
    ImGui::Separator();
    
    ImGui::Checkbox("Fly", &g_Fly);
    ImGui::Checkbox("Speed (Max Jump)", &g_Speed);
    ImGui::Checkbox("Long Arms", &g_LongArms);
    ImGui::Checkbox("Pull Mod", &g_PullMod);
    ImGui::Checkbox("Giant Self", &g_GiantSelf);
    ImGui::Checkbox("Unlock Cosmetics", &g_Cosmetics);
    ImGui::Checkbox("Anti-Kick (Brute)", &g_AntiKick);
    
    ImGui::Separator();
    
    if (ImGui::Button("FORCE LEAVE ROOM", ImVec2(500, 50))) {
        g_ForceLeave = true;
        void* pLeave = FindMethod("PhotonNetwork", "LeaveRoom", 1);
        if (pLeave) {
            typedef void (*Leave_t)(void*, bool);
            ((Leave_t)pLeave)(nullptr, true);
        }
    }
    
    ImGui::End();
}

typedef EGLBoolean (*eglSwapBuffers_t)(EGLDisplay dpy, EGLSurface surface);
static eglSwapBuffers_t orig_eglSwapBuffers = nullptr;
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    static bool init = false;
    if (!init) {
        IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::GetIO().IniFilename = NULL;
        ImGui_ImplOpenGL3_Init("#version 100"); ImGui_ImplAndroid_Init(NULL); SetupStyle(); init = true;
    }
    ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplAndroid_NewFrame(); ImGui::NewFrame();
    RenderMenu();
    ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return orig_eglSwapBuffers(dpy, surface);
}

// --- ENTRY POINT ---
__attribute__((constructor))
void init() {
    void* p = dlsym(RTLD_DEFAULT, "SSL_read");
    if (p) DobbyHook(p, (void*)hook_SSL_read, (void**)&orig_SSL_read);
    p = dlsym(RTLD_DEFAULT, "SSL_write");
    if (p) DobbyHook(p, (void*)hook_SSL_write, (void**)&orig_SSL_write);

    void* pDisconnect = FindMethod("PhotonNetwork", "Disconnect", 0);
    if (pDisconnect) DobbyHook(pDisconnect, (void*)hook_Disconnect, (void**)&orig_Disconnect);
    void* pLeave = FindMethod("PhotonNetwork", "LeaveRoom", 1);
    if (pLeave) DobbyHook(pLeave, (void*)hook_LeaveRoom, (void**)&orig_LeaveRoom);

    void* addrPlayer = FindMethod("GorillaLocomotion.Player", "LateUpdate", 0);
    if (addrPlayer) DobbyHook(addrPlayer, (void*)hook_Player_LateUpdate, (void**)&orig_Player_LateUpdate);

    void* addrVRRig = FindMethod("VRRig", "LateUpdate", 0);
    if (addrVRRig) DobbyHook(addrVRRig, (void*)hook_VRRig_LateUpdate, (void**)&orig_VRRig_LateUpdate);
    
    void* addrCosmetic = FindMethod("CosmeticsController", "HasCosmetic", 1);
    if (addrCosmetic) DobbyHook(addrCosmetic, (void*)hook_HasCosmetic, (void**)&orig_HasCosmetic);

    void* pGetDown = FindMethod("OVRInput", "GetDown", 1);
    if (pGetDown) DobbyHook(pGetDown, (void*)hook_OVRInput_GetDown, (void**)&orig_GetDown);

    void* egl = dlopen("libEGL.so", RTLD_LAZY);
    if (egl) {
        void* swap = dlsym(egl, "eglSwapBuffers");
        if (swap) DobbyHook(swap, (void*)hook_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
    }
}