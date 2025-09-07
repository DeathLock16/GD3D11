#pragma once
#include <string>

void DX11_ApplyPendingShaderIfAny();
bool DX11Shader_HasPSOverride();
const std::string& DX11Shader_GetPSOverride();
void DX11Shader_ClearOverride();

void ApplyShaderByName(const std::wstring& name);

#ifdef __cplusplus
extern "C" {
#endif
void DX11_InitRemoteControl();
#ifdef __cplusplus
}
#endif
