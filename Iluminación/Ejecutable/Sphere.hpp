#pragma once

#include <array>
#include "RenderDevice.h"
#include "Buffer.h"
#include "RefCntAutoPtr.hpp"
#include "BasicMath.hpp"
#include "GeometryPrimitives.h"

namespace Diligent
{
namespace Sphere
{

RefCntAutoPtr<IBuffer> CreateVertexBuffer(IRenderDevice*                  pDevice,
                                          GEOMETRY_PRIMITIVE_VERTEX_FLAGS Components,
                                          BIND_FLAGS                      BindFlags = BIND_VERTEX_BUFFER,
                                          BUFFER_MODE                     Mode      = BUFFER_MODE_UNDEFINED);

RefCntAutoPtr<IBuffer> CreateIndexBuffer(IRenderDevice* pDevice,
                                         BIND_FLAGS     BindFlags = BIND_INDEX_BUFFER,
                                         BUFFER_MODE    Mode      = BUFFER_MODE_UNDEFINED);

RefCntAutoPtr<ITexture> LoadTexture(IRenderDevice* pDevice, const char* Path);

struct CreatePSOInfo
{
    IRenderDevice*                   pDevice                = nullptr;
    TEXTURE_FORMAT                   RTVFormat              = TEX_FORMAT_UNKNOWN;
    TEXTURE_FORMAT                   DSVFormat              = TEX_FORMAT_UNKNOWN;
    IShaderSourceInputStreamFactory* pShaderSourceFactory   = nullptr;
    const char*                      VSFilePath             = nullptr;
    const char*                      PSFilePath             = nullptr;
    GEOMETRY_PRIMITIVE_VERTEX_FLAGS  Components             = GEOMETRY_PRIMITIVE_VERTEX_FLAG_NONE;
    LayoutElement*                   ExtraLayoutElements    = nullptr;
    Uint32                           NumExtraLayoutElements = 0;
    Uint8                            SampleCount            = 1;
};

RefCntAutoPtr<IPipelineState> CreatePipelineState(const CreatePSOInfo& CreateInfo, bool ConvertPSOutputToGamma = false);

} // namespace Sphere
} // namespace Diligent
