#include <vector>
#include "Sphere.hpp"
#include "BasicMath.hpp"
#include "TextureUtilities.h"
#include "GraphicsTypesX.hpp"
#include "GraphicsUtilities.h"

namespace Diligent
{
namespace Sphere
{

RefCntAutoPtr<IBuffer> CreateVertexBuffer(IRenderDevice*                  pDevice,
                                          GEOMETRY_PRIMITIVE_VERTEX_FLAGS Components,
                                          BIND_FLAGS                      BindFlags,
                                          BUFFER_MODE                     Mode)
{
    GeometryPrimitiveBuffersCreateInfo SphereBuffersCI;
    SphereBuffersCI.VertexBufferBindFlags = BindFlags;
    SphereBuffersCI.VertexBufferMode      = Mode;

    GeometryPrimitiveInfo  SpherePrimInfo;
    RefCntAutoPtr<IBuffer> pVertices;
    CreateGeometryPrimitiveBuffers(
        pDevice,
        SphereGeometryPrimitiveAttributes{2.f, Components},
        &SphereBuffersCI,
        &pVertices,
        nullptr,
        &SpherePrimInfo);
    return pVertices;
}

RefCntAutoPtr<IBuffer> CreateIndexBuffer(IRenderDevice* pDevice, BIND_FLAGS BindFlags, BUFFER_MODE Mode)
{
    GeometryPrimitiveBuffersCreateInfo SphereBuffersCI;
    SphereBuffersCI.IndexBufferBindFlags = BindFlags;
    SphereBuffersCI.IndexBufferMode      = Mode;

    GeometryPrimitiveInfo  SpherePrimInfo;
    RefCntAutoPtr<IBuffer> pIndices;
    CreateGeometryPrimitiveBuffers(
        pDevice,
        SphereGeometryPrimitiveAttributes{},
        &SphereBuffersCI,
        nullptr,
        &pIndices,
        &SpherePrimInfo);
    return pIndices;
}

RefCntAutoPtr<ITexture> LoadTexture(IRenderDevice* pDevice, const char* Path)
{
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    RefCntAutoPtr<ITexture> pTex;
    CreateTextureFromFile(Path, loadInfo, pDevice, &pTex);
    return pTex;
}

RefCntAutoPtr<IPipelineState> CreatePipelineState(const CreatePSOInfo& CreateInfo, bool ConvertPSOutputToGamma)
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PipelineStateDesc&              PSODesc          = PSOCreateInfo.PSODesc;
    PipelineResourceLayoutDesc&     ResourceLayout   = PSODesc.ResourceLayout;
    GraphicsPipelineDesc&           GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    PSODesc.Name         = "Sphere PSO";

    GraphicsPipeline.NumRenderTargets             = 1;
    GraphicsPipeline.RTVFormats[0]                = CreateInfo.RTVFormat;
    GraphicsPipeline.DSVFormat                    = CreateInfo.DSVFormat;
    GraphicsPipeline.SmplDesc.Count               = CreateInfo.SampleCount;
    GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    ShaderMacro Macros[]                = {{"CONVERT_PS_OUTPUT_TO_GAMMA", ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros                     = {Macros, _countof(Macros)};
    ShaderCI.pShaderSourceStreamFactory = CreateInfo.pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Sphere VS";
        ShaderCI.FilePath        = CreateInfo.VSFilePath;
        CreateInfo.pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Sphere PS";
        ShaderCI.FilePath        = CreateInfo.PSFilePath;
        CreateInfo.pDevice->CreateShader(ShaderCI, &pPS);
    }

    InputLayoutDescX InputLayout;
    Uint32           Attrib = 0;
    if (CreateInfo.Components & GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION)
        InputLayout.Add(Attrib++, 0u, 3u, VT_FLOAT32, False);
    if (CreateInfo.Components & GEOMETRY_PRIMITIVE_VERTEX_FLAG_NORMAL)
        InputLayout.Add(Attrib++, 0u, 3u, VT_FLOAT32, False);
    if (CreateInfo.Components & GEOMETRY_PRIMITIVE_VERTEX_FLAG_TEXCOORD)
        InputLayout.Add(Attrib++, 0u, 2u, VT_FLOAT32, False);

    for (Uint32 i = 0; i < CreateInfo.NumExtraLayoutElements; ++i)
        InputLayout.Add(CreateInfo.ExtraLayoutElements[i]);

    GraphicsPipeline.InputLayout = InputLayout;

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderResourceVariableDesc Vars[] =
        {
            {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    ResourceLayout.Variables    = Vars;
    ResourceLayout.NumVariables = _countof(Vars);

    SamplerDesc SamLinearClampDesc{
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};
    ImmutableSamplerDesc ImtblSamplers[] =
        {
            {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}};
    ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    RefCntAutoPtr<IPipelineState> pPSO;
    CreateInfo.pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    return pPSO;
}

} // namespace Sphere
} // namespace Diligent
