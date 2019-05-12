// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
#include <vector>
#include <wrl/client.h>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

<<<<<<< HEAD
namespace DX11
{
#define SAFE_RELEASE(x)                                                                            \
  {                                                                                                \
    if (x)                                                                                         \
      (x)->Release();                                                                              \
    (x) = nullptr;                                                                                 \
  }
#define SAFE_DELETE(x)                                                                             \
  {                                                                                                \
    delete (x);                                                                                    \
    (x) = nullptr;                                                                                 \
  }
#define SAFE_DELETE_ARRAY(x)                                                                       \
  {                                                                                                \
    delete[](x);                                                                                   \
    (x) = nullptr;                                                                                 \
  }
#ifdef _MSC_VER
=======
>>>>>>> 1d5dd5db914d94f3f612c13c6c5e1d5e711b49b5
#define CHECK(cond, Message, ...)                                                                  \
  if (!(cond))                                                                                     \
  {                                                                                                \
    PanicAlert("%s failed in %s at line %d: " Message, __func__, __FILE__, __LINE__, __VA_ARGS__); \
  }
<<<<<<< HEAD
#else
#define CHECK(cond, Message, ...)                                                                  \
  if (!(cond))                                                                                     \
  {                                                                                                \
    PanicAlert("%s failed in %s at line %d: " Message, __func__, __FILE__, __LINE__,               \
               ##__VA_ARGS__);                                                                     \
  }
#endif
class D3DTexture2D;

namespace D3D
{
HRESULT LoadDXGI();
HRESULT LoadD3D();
HRESULT LoadD3DCompiler();
void UnloadDXGI();
void UnloadD3D();
void UnloadD3DCompiler();

D3D_FEATURE_LEVEL GetFeatureLevel(IDXGIAdapter* adapter);
std::vector<DXGI_SAMPLE_DESC> EnumAAModes(IDXGIAdapter* adapter);

HRESULT Create(HWND wnd);
void Close();

extern ID3D11Device* device;
extern ID3D11Device1* device1;
extern ID3D11DeviceContext* context;
extern IDXGISwapChain1* swapchain;
extern D3D_FEATURE_LEVEL featlevel;
=======

namespace DX11
{
using Microsoft::WRL::ComPtr;
class SwapChain;

namespace D3D
{
extern ComPtr<IDXGIFactory2> dxgi_factory;
extern ComPtr<ID3D11Device> device;
extern ComPtr<ID3D11Device1> device1;
extern ComPtr<ID3D11DeviceContext> context;
extern D3D_FEATURE_LEVEL feature_level;
>>>>>>> 1d5dd5db914d94f3f612c13c6c5e1d5e711b49b5

bool Create(u32 adapter_index, bool enable_debug_layer);
void Destroy();

// Returns a list of supported AA modes for the current device.
std::vector<u32> GetAAModes(u32 adapter_index);

// Checks for support of the given texture format.
bool SupportsTextureFormat(DXGI_FORMAT format);

}  // namespace D3D

}  // namespace DX11
