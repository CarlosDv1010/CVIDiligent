#include <vector>
#include "Prism.hpp"
#include "BasicMath.hpp"
#include "GraphicsTypesX.hpp"
#include "GraphicsUtilities.h"

namespace Diligent
{
namespace Prism
{

RefCntAutoPtr<IBuffer> CreateVertexBuffer(IRenderDevice*                  pDevice,
                                          GEOMETRY_PRIMITIVE_VERTEX_FLAGS Components,
                                          BIND_FLAGS                      BindFlags,
                                          BUFFER_MODE                     Mode)
{
    // Definición de la estructura de vértice.
    struct Vertex
    {
        float3 pos;
        float4 color;
    };

    // Definición de los vértices del prisma.
    constexpr Vertex PrismVerts[6] =
        {
            // Base inferior
            {float3(-1.0f, 0.0f, -1.0f), float4(1.0f, 0.0f, 0.0f, 1.0f)}, // v0
            {float3(1.0f, 0.0f, -1.0f), float4(0.0f, 1.0f, 0.0f, 1.0f)},  // v1
            {float3(0.0f, 0.0f, 1.0f), float4(0.0f, 0.0f, 1.0f, 1.0f)},   // v2

            // Base superior
            {float3(-1.0f, 2.0f, -1.0f), float4(1.0f, 0.0f, 0.0f, 1.0f)}, // v3 (sobre v0)
            {float3(1.0f, 2.0f, -1.0f), float4(0.0f, 1.0f, 0.0f, 1.0f)},  // v4 (sobre v1)
            {float3(0.0f, 2.0f, 1.0f), float4(0.0f, 0.0f, 1.0f, 1.0f)}    // v5 (sobre v2)
        };

    // Configuración de la descripción del buffer.
    BufferDesc VertBuffDesc;
    VertBuffDesc.Name      = "Prism vertex buffer";
    VertBuffDesc.Usage     = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BindFlags; // O puedes usar BIND_VERTEX_BUFFER directamente.
    VertBuffDesc.Size      = sizeof(PrismVerts);

    // Inicialización de los datos del buffer.
    BufferData VBData;
    VBData.pData    = PrismVerts;
    VBData.DataSize = sizeof(PrismVerts);

    // Creación del buffer a través del dispositivo.
    RefCntAutoPtr<IBuffer> pVertices;
    pDevice->CreateBuffer(VertBuffDesc, &VBData, &pVertices);
    return pVertices;
}



RefCntAutoPtr<IBuffer> CreateIndexBuffer(IRenderDevice* pDevice,
                                         BIND_FLAGS     BindFlags,
                                         BUFFER_MODE    Mode)
{
    constexpr Uint32 PrismIndices[] =
        {
            0, 1, 2,
            3, 5, 4, 

            0, 1, 4,
            0, 4, 3,

            1, 2, 5,
            1, 5, 4,

            2, 0, 3,
            2, 3, 5};



    BufferDesc IndBuffDesc;
    IndBuffDesc.Name      = "Prism index buffer";
    IndBuffDesc.Usage     = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BindFlags;
    IndBuffDesc.Size      = sizeof(PrismIndices);

    BufferData IBData;
    IBData.pData    = PrismIndices;
    IBData.DataSize = sizeof(PrismIndices);

    RefCntAutoPtr<IBuffer> pIndices;
    pDevice->CreateBuffer(IndBuffDesc, &IBData, &pIndices);
    return pIndices;
}


RefCntAutoPtr<IPipelineState> CreatePipelineState(const CreatePSOInfo& CreateInfo, bool ConvertPSOutputToGamma)
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PipelineStateDesc&              PSODesc          = PSOCreateInfo.PSODesc;
    PipelineResourceLayoutDesc&     ResourceLayout   = PSODesc.ResourceLayout;
    GraphicsPipelineDesc&           GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    PSODesc.Name         = "Prism PSO";

    GraphicsPipeline.NumRenderTargets             = 1;
    GraphicsPipeline.RTVFormats[0]                = CreateInfo.RTVFormat;
    GraphicsPipeline.DSVFormat                    = CreateInfo.DSVFormat;
    GraphicsPipeline.SmplDesc.Count               = CreateInfo.SampleCount;
    GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    // Desactivamos el uso de texturas
    ShaderCI.Desc.UseCombinedTextureSamplers = false;
    ShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    ShaderMacro Macros[]                = {{"CONVERT_PS_OUTPUT_TO_GAMMA", ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros                     = {Macros, _countof(Macros)};
    ShaderCI.pShaderSourceStreamFactory = CreateInfo.pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Prism VS";
        ShaderCI.FilePath        = CreateInfo.VSFilePath;
        CreateInfo.pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Prism PS";
        ShaderCI.FilePath        = CreateInfo.PSFilePath;
        CreateInfo.pDevice->CreateShader(ShaderCI, &pPS);
    }

    InputLayoutDescX InputLayout;

    for (Uint32 i = 0; i < CreateInfo.NumExtraLayoutElements; ++i)
        InputLayout.Add(CreateInfo.ExtraLayoutElements[i]);

    GraphicsPipeline.InputLayout = InputLayout;




    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    RefCntAutoPtr<IPipelineState> pPSO;
    CreateInfo.pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    return pPSO;
}

} 
} // namespace Diligent
