#include <random>
#include "Tutorial04_Instancing.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ColorConversion.h"
#include "../../Common/src/TexturedCube.hpp"
#include "Prism.hpp"
#include "imgui.h"
#include <iostream>
#include <filesystem>
#include <windows.h>
#include "FirstPersonCamera.hpp"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial04_Instancing();
}

struct InstanceData
{
    float4x4 Matrix;
    float    TextureInd = 0;
};

struct LightData
{
    float3 lightPositionWorld;
    float3 eyePositionWorld;
    float4 ambientLight;
    float4 lightDirection;
    float4 spec;
    float  specPower;
};

void Tutorial04_Instancing::CreateShadowPSO()
{
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name = "Cube shadow PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 0;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = TEX_FORMAT_UNKNOWN;
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_ShadowMapFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    
     ShaderCreateInfo ShaderCI;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    RefCntAutoPtr<IShader> pShadowVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube Shadow VS";
        ShaderCI.FilePath        = "cube_shadow.vsh";
        m_pDevice->CreateShader(ShaderCI, &pShadowVS);
    }
    PSOCreateInfo.pVS = pShadowVS;
    PSOCreateInfo.pPS = nullptr;

    LayoutElement LayoutElems[] =
    {
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        LayoutElement{2, 0, 3, VT_FLOAT32, False},
        LayoutElement{1, 0, 2, VT_FLOAT32, False}
    };

    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    if (m_pDevice->GetDeviceInfo().Features.DepthClamp)
    {
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthClipEnable = False;
    }

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pCubeShadowPSO);
    m_pCubeShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pCubeShadowPSO->CreateShaderResourceBinding(&m_CubeShadowSRB, true);
}

void Tutorial04_Instancing::CreateShadowMapVisPSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name = "Shadow Map Vis PSO";

    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros      = {Macros, _countof(Macros)};

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    RefCntAutoPtr<IShader> pShadowMapVisVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Shadow Map Vis VS";
        ShaderCI.FilePath        = "shadow_map_vis.vsh";
        m_pDevice->CreateShader(ShaderCI, &pShadowMapVisVS);
    }

    RefCntAutoPtr<IShader> pShadowMapVisPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Shadow Map Vis PS";
        ShaderCI.FilePath        = "shadow_map_vis.psh";
        m_pDevice->CreateShader(ShaderCI, &pShadowMapVisPS);
    }

    PSOCreateInfo.pVS = pShadowMapVisVS;
    PSOCreateInfo.pPS = pShadowMapVisPS;

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    ShaderResourceVariableDesc Variables[] =
        {
            {SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE, SHADER_VARIABLE_FLAG_UNFILTERABLE_FLOAT_TEXTURE_WEBGPU},
        };
    PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Variables;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Variables);

    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc ImtblSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_ShadowMap", SamLinearClampDesc}
    };

    if (m_pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_GLES)
    {
        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
        PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);
    }

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pShadowMapVisPSO);
}

void Tutorial04_Instancing::CreatePipelineState()
{
    LayoutElement LayoutElems[] =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 2, VT_FLOAT32, False},
            LayoutElement{2, 0, 3, VT_FLOAT32, False},
            LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{6, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{7, 1, 1, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
    };

    LayoutElement LayoutElems2[] =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 4, VT_FLOAT32, False},
            LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}};

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    Prism::CreatePSOInfo PrismPsoCI;
    PrismPsoCI.pDevice              = m_pDevice;
    PrismPsoCI.RTVFormat            = m_pSwapChain->GetDesc().ColorBufferFormat;
    PrismPsoCI.DSVFormat            = m_pSwapChain->GetDesc().DepthBufferFormat;
    PrismPsoCI.pShaderSourceFactory = pShaderSourceFactory;
    TexturedCube::CreatePSOInfo CubePsoCI;
    CubePsoCI.pDevice              = m_pDevice;
    CubePsoCI.RTVFormat            = m_pSwapChain->GetDesc().ColorBufferFormat;
    CubePsoCI.DSVFormat            = m_pSwapChain->GetDesc().DepthBufferFormat;
    CubePsoCI.pShaderSourceFactory = pShaderSourceFactory;

#ifdef DILIGENT_USE_OPENGL
    CubePsoCI.VSFilePath = "cube_inst_glsl.vert";
    CubePsoCI.PSFilePath = "cube_inst_glsl.frag";
#else
    CubePsoCI.VSFilePath  = "cube_inst.vsh";
    CubePsoCI.PSFilePath  = "cube_inst.psh";
    PrismPsoCI.VSFilePath = "prism_inst.vsh";
    PrismPsoCI.PSFilePath = "prism_inst.psh";
#endif

    CubePsoCI.ExtraLayoutElements     = LayoutElems;
    CubePsoCI.NumExtraLayoutElements  = _countof(LayoutElems);
    PrismPsoCI.ExtraLayoutElements    = LayoutElems2;
    PrismPsoCI.NumExtraLayoutElements = _countof(LayoutElems2);

    m_pPSO      = TexturedCube::CreatePipelineState(CubePsoCI, m_ConvertPSOutputToGamma);
    m_pPSOPrism = Prism::CreatePipelineState(PrismPsoCI, m_ConvertPSOutputToGamma);

    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 3 + sizeof(float4), "VS constants CB", &m_VSConstants);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstantsPrism);
    m_pPSOPrism->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstantsPrism);
    m_pPSOPrism->CreateShaderResourceBinding(&m_SRBPrism, true);

    CreateUniformBuffer(m_pDevice, sizeof(LightData), "PS constants CB", &m_PSConstants);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Constants")->Set(m_PSConstants);
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);


}

void Tutorial04_Instancing::CreateInstanceBuffer()
{
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Instance data buffer";
    InstBuffDesc.Size      = sizeof(InstanceData) * NumInstances;
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);
    PopulateInstanceBuffer();
}

void Tutorial04_Instancing::CreatePrismInstanceBuffer()
{
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name      = "Sphere Instance data buffer";
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(float4x4) * NumPrismInstances;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_PrismInstanceBuffer);
    PopulateInstanceBufferPrism();
}

void Tutorial04_Instancing::CreateShadowMap()
{
    TextureDesc SMDesc;
    SMDesc.Name      = "Shadow map";
    SMDesc.Type      = RESOURCE_DIM_TEX_2D;
    SMDesc.Width     = 2048;
    SMDesc.Height    = 2048;
    SMDesc.Format    = m_ShadowMapFormat;
    SMDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
    RefCntAutoPtr<ITexture> ShadowMap;
    m_pDevice->CreateTexture(SMDesc, nullptr, &ShadowMap);
    m_ShadowMapSRV = ShadowMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    m_ShadowMapDSV = ShadowMap->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

    m_SRB.Release();
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(m_ShadowMapSRV);

    m_ShadowMapVisSRB.Release();
    m_pShadowMapVisPSO->CreateShaderResourceBinding(&m_ShadowMapVisSRB, true);
    m_ShadowMapVisSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")->Set(m_ShadowMapSRV);
}

void Tutorial04_Instancing::LoadTextures() {
    {
        std::vector<RefCntAutoPtr<ITextureLoader>> loaders(NumInstances);
        for (int i = 0; i < NumInstances; ++i)
        {
            std::stringstream FileNameSS;
            FileNameSS << "grassFlowers" << i << ".png";
            const std::string FileName = FileNameSS.str();
            TextureLoadInfo   LoadInfo;
            LoadInfo.IsSRGB = true;
            CreateTextureLoaderFromFile(FileName.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, LoadInfo, &loaders[i]);
        }
        TextureDesc TexDesc = loaders[0]->GetTextureDesc();
        TexDesc.ArraySize   = NumInstances;
        TexDesc.Type        = RESOURCE_DIM_TEX_2D_ARRAY;
        TexDesc.Usage       = USAGE_DEFAULT;
        TexDesc.BindFlags   = BIND_SHADER_RESOURCE;

        std::vector<TextureSubResData> SubresData(TexDesc.ArraySize * TexDesc.MipLevels);
        for (Uint32 slice = 0; slice < TexDesc.ArraySize; ++slice)
        {
            for (Uint32 mip = 0; mip < TexDesc.MipLevels; ++mip)
            {
                SubresData[slice * TexDesc.MipLevels + mip] = loaders[slice]->GetSubresourceData(mip, 0);
            }
        }
        TextureData InitData{SubresData.data(), TexDesc.MipLevels * TexDesc.ArraySize};

        RefCntAutoPtr<ITexture> pGrassFlowersArray;
        m_pDevice->CreateTexture(TexDesc, &InitData, &pGrassFlowersArray);
        m_TextureGrassFlowers = pGrassFlowersArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RTexture")->Set(m_TextureGrassFlowers);
        
    }
    {
        std::vector<RefCntAutoPtr<ITextureLoader>> loaders(NumInstances);
        for (int i = 0; i < NumInstances; ++i)
        {
            std::stringstream FileNameSS;
            FileNameSS << "grassy" << i << ".png";
            const std::string FileName = FileNameSS.str();
            TextureLoadInfo   LoadInfo;
            LoadInfo.IsSRGB = true;
            CreateTextureLoaderFromFile(FileName.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, LoadInfo, &loaders[i]);
        }
        TextureDesc TexDesc = loaders[0]->GetTextureDesc();
        TexDesc.ArraySize   = NumInstances;
        TexDesc.Type        = RESOURCE_DIM_TEX_2D_ARRAY;
        TexDesc.Usage       = USAGE_DEFAULT;
        TexDesc.BindFlags   = BIND_SHADER_RESOURCE;

        std::vector<TextureSubResData> SubresData(TexDesc.ArraySize * TexDesc.MipLevels);
        for (Uint32 slice = 0; slice < TexDesc.ArraySize; ++slice)
        {
            for (Uint32 mip = 0; mip < TexDesc.MipLevels; ++mip)
            {
                SubresData[slice * TexDesc.MipLevels + mip] = loaders[slice]->GetSubresourceData(mip, 0);
            }
        }
        TextureData InitData{SubresData.data(), TexDesc.MipLevels * TexDesc.ArraySize};

        RefCntAutoPtr<ITexture> pGrassyArray;
        m_pDevice->CreateTexture(TexDesc, &InitData, &pGrassyArray);
        // Guardar la vista de shader resource
        m_TextureGrassy = pGrassyArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GTexture")->Set(m_TextureGrassy);
    }

    {
        std::vector<RefCntAutoPtr<ITextureLoader>> loaders(NumInstances);
        for (int i = 0; i < NumInstances; ++i)
        {
            std::stringstream FileNameSS;
            FileNameSS << "mud" << i << ".png";
            const std::string FileName = FileNameSS.str();
            TextureLoadInfo   LoadInfo;
            LoadInfo.IsSRGB = true;
            CreateTextureLoaderFromFile(FileName.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, LoadInfo, &loaders[i]);
        }
        TextureDesc TexDesc = loaders[0]->GetTextureDesc();
        TexDesc.ArraySize   = NumInstances;
        TexDesc.Type        = RESOURCE_DIM_TEX_2D_ARRAY;
        TexDesc.Usage       = USAGE_DEFAULT;
        TexDesc.BindFlags   = BIND_SHADER_RESOURCE;

        std::vector<TextureSubResData> SubresData(TexDesc.ArraySize * TexDesc.MipLevels);
        for (Uint32 slice = 0; slice < TexDesc.ArraySize; ++slice)
        {
            for (Uint32 mip = 0; mip < TexDesc.MipLevels; ++mip)
            {
                SubresData[slice * TexDesc.MipLevels + mip] = loaders[slice]->GetSubresourceData(mip, 0);
            }
        }
        TextureData InitData{SubresData.data(), TexDesc.MipLevels * TexDesc.ArraySize};

        RefCntAutoPtr<ITexture> pMudArray;
        m_pDevice->CreateTexture(TexDesc, &InitData, &pMudArray);
        m_TextureMud = pMudArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_BTexture")->Set(m_TextureMud);
    }
    {
        std::vector<RefCntAutoPtr<ITextureLoader>> loaders(NumInstances);
        for (int i = 0; i < NumInstances; ++i)
        {
            std::stringstream FileNameSS;
            FileNameSS << "path" << i << ".png";
            const std::string FileName = FileNameSS.str();
            TextureLoadInfo   LoadInfo;
            LoadInfo.IsSRGB = true;
            CreateTextureLoaderFromFile(FileName.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, LoadInfo, &loaders[i]);
        }
        TextureDesc TexDesc = loaders[0]->GetTextureDesc();
        TexDesc.ArraySize   = NumInstances;
        TexDesc.Type        = RESOURCE_DIM_TEX_2D_ARRAY;
        TexDesc.Usage       = USAGE_DEFAULT;
        TexDesc.BindFlags   = BIND_SHADER_RESOURCE;

        std::vector<TextureSubResData> SubresData(TexDesc.ArraySize * TexDesc.MipLevels);
        for (Uint32 slice = 0; slice < TexDesc.ArraySize; ++slice)
        {
            for (Uint32 mip = 0; mip < TexDesc.MipLevels; ++mip)
            {
                SubresData[slice * TexDesc.MipLevels + mip] = loaders[slice]->GetSubresourceData(mip, 0);
            }
        }
        TextureData InitData{SubresData.data(), TexDesc.MipLevels * TexDesc.ArraySize};

        RefCntAutoPtr<ITexture> pPathArray;
        m_pDevice->CreateTexture(TexDesc, &InitData, &pPathArray);
        m_TexturePath = pPathArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_BackgroundTexture")->Set(m_TexturePath);

    }
        {
            std::vector<RefCntAutoPtr<ITextureLoader>> loaders(NumInstances);
            for (int i = 0; i < NumInstances; ++i)
            {
                std::stringstream FileNameSS;
                FileNameSS << "blendMap" << i << ".png";
                const std::string FileName = FileNameSS.str();
                TextureLoadInfo   LoadInfo;
                LoadInfo.IsSRGB = true;
                CreateTextureLoaderFromFile(FileName.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, LoadInfo, &loaders[i]);
            }
            TextureDesc TexDesc = loaders[0]->GetTextureDesc();
            TexDesc.ArraySize   = NumInstances;
            TexDesc.Type        = RESOURCE_DIM_TEX_2D_ARRAY;
            TexDesc.Usage       = USAGE_DEFAULT;
            TexDesc.BindFlags   = BIND_SHADER_RESOURCE;

            std::vector<TextureSubResData> SubresData(TexDesc.ArraySize * TexDesc.MipLevels);
            for (Uint32 slice = 0; slice < TexDesc.ArraySize; ++slice)
            {
                for (Uint32 mip = 0; mip < TexDesc.MipLevels; ++mip)
                {
                    SubresData[slice * TexDesc.MipLevels + mip] = loaders[slice]->GetSubresourceData(mip, 0);
                }
            }
            TextureData InitData{SubresData.data(), TexDesc.MipLevels * TexDesc.ArraySize};

            RefCntAutoPtr<ITexture> pBlendMapArray;
            m_pDevice->CreateTexture(TexDesc, &InitData, &pBlendMapArray);
            m_TextureBlendMap = pBlendMapArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
            m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_BlendMap")->Set(m_TextureBlendMap);
        }
    }

void Tutorial04_Instancing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
    CreatePipelineState();
    CreateShadowPSO();
    CreateShadowMapVisPSO();

    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);
    
    LoadTextures();


    CreateInstanceBuffer();

    m_VertexBufferPrism = Prism::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL);
    m_IndexBufferPrism  = Prism::CreateIndexBuffer(m_pDevice);
    CreatePrismInstanceBuffer();
    CreateShadowMap();

    m_ViewOption = 0;
    m_specPower         = 1.0;
    m_CameraMode        = 0; 
    m_CameraSpeedOption = 1;
    m_Camera.SetPos(float3(0.f, 0.f, 30.0f));
    m_Camera.SetLookAt(float3(0.f, 0.f, 0.f));
    m_Camera.SetProjAttribs(0.1f, 100.f, 1, 0.6f, SURFACE_TRANSFORM_IDENTITY, false);
}

float angleAxe1 = PI_F / 1.0;
float angleAxeD = PI_F / 1.0;

void  Tutorial04_Instancing::PopulateInstanceBuffer()
{
    std::vector<InstanceData> InstanceData(NumInstances);

    const float4x4 scaleHiloVertical = float4x4::Scale(0.01f, 1.0f, 0.01f);

    for (int i = 0; i < NumInstances; i++)
    {
        InstanceData[i].TextureInd = i;
    }
    // Se definen las transformaciones de traslación para las 5 instancias
    InstanceData[0].Matrix  = float4x4::Scale(1, 1, 1) * float4x4::Translation(0.0f, -5.0f, 0.0f) * float4x4::RotationY(angleAxe1);                        // C
    InstanceData[1].Matrix = scaleHiloVertical * float4x4::Translation(0.0f, -1.25f, 0.0f) * float4x4::RotationY(angleAxe1);                              // Hilo vertical 1
    InstanceData[2].Matrix = float4x4::Scale(1, 1, 1) * float4x4::Translation(0.0f, 0.0f, 0.0f);                                                          // A
    InstanceData[3].Matrix = float4x4::Scale(6.0f, 0.01f, 0.01f) * float4x4::Translation(0.0f, -2.25f, 0.0f) * float4x4::RotationY(angleAxe1);            // Hilo horizontal 1
    InstanceData[4].Matrix = scaleHiloVertical * float4x4::Translation(0.0f, -3.0f, 0.0f) * float4x4::RotationY(angleAxe1);                               // Hilo vertical que conecta a C
    InstanceData[5].Matrix = float4x4::Translation(6.0f, -5.0f, 0.0f) * float4x4::RotationY(angleAxe1);                                                   // D
    InstanceData[6].Matrix = scaleHiloVertical * float4x4::Translation(6.0f, -3.25f, 0.0f) * float4x4::RotationY(angleAxe1);                              // Hilo vertical que conecta a D
    InstanceData[8].Matrix = scaleHiloVertical * float4x4::Translation(6.0f, -6.5f, 0.0f) * float4x4::RotationY(angleAxe1);                               // Hilo vertical 2
    InstanceData[9].Matrix  = float4x4::Scale(3.0f, 0.01f, 0.01f) * float4x4::RotationY(angleAxeD) * float4x4::Translation(0, -2.5f, 0) * InstanceData[5].Matrix; // Hilo horizontal 2
    InstanceData[10].Matrix = scaleHiloVertical * float4x4::Translation(-6.0f, -3.25f, 0.0f) * float4x4::RotationY(angleAxe1);                             // Hilo vertical que conecta a B
    InstanceData[11].Matrix = scaleHiloVertical * float4x4::Translation(3, -1, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[9].Matrix;                   // Hilo vertical que conecta a G
    InstanceData[12].Matrix = scaleHiloVertical * float4x4::Translation(-3, -1, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[9].Matrix;           // Hilo vertical que conecta a H
    InstanceData[13].Matrix = float4x4::Translation(3, -2, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[9].Matrix;                                       // H
    InstanceData[7].Matrix  = float4x4::Translation(-3, -2, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[9].Matrix;                               // G
    InstanceData[14].Matrix = float4x4::Scale(3.0f, 0.01f, 0.01f) * float4x4::RotationY(angleAxeD) * float4x4::Translation(0, -2.5f, 0) *
        float4x4::Translation(-6.0f, -6.2f, 0.0f) * float4x4::RotationY(angleAxe1);                                                 // Hilo horizontal Prismas
    InstanceData[15].Matrix = scaleHiloVertical * float4x4::Translation(3, -1, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[14].Matrix; // Hilo vertical que conecta a Prisma izquierda
    InstanceData[16].Matrix = scaleHiloVertical * float4x4::Translation(-3, -1, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[14].Matrix; // Hilo vertical que conecta a Prisma derecha
    InstanceData[17].Matrix = float4x4::Scale(1, 2, 1) * scaleHiloVertical * float4x4::Translation(-6.0f, -6.75f, 0.0f) * float4x4::RotationY(angleAxe1);
    InstanceData[18].Matrix = float4x4::Scale(100, 1, 100) * float4x4::Translation(0.0f, -20.0f, 0.0f);

    angleAxe1 += 0.002f;
    angleAxeD += 0.004f;

    Uint32 DataSize = static_cast<Uint32>(sizeof(InstanceData[0]) * InstanceData.size());
    m_pImmediateContext->UpdateBuffer(m_InstanceBuffer, 0, DataSize, InstanceData.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void Tutorial04_Instancing::PopulateInstanceBufferPrism()
{
    std::vector<float4x4> InstanceData(NumPrismInstances);
    InstanceData[0] = float4x4::Scale(1, 1, 1) * float4x4::Translation(-6.0f, -6.0f, 0.0f) * float4x4::RotationY(angleAxe1);
    InstanceData[1] = float4x4::Scale(1, 1, 1) * float4x4::Translation(-3.0f, -6.0f, 0.0f) * float4x4::RotationY(angleAxeD) * InstanceData[0];
    InstanceData[2] = float4x4::Scale(1, 1, 1) * float4x4::Translation(3.0f, -6.0f, 0.0f) * float4x4::RotationY(angleAxeD) * InstanceData[0];

    Uint32 DataSize = static_cast<Uint32>(sizeof(InstanceData[0]) * InstanceData.size());
    m_pImmediateContext->UpdateBuffer(m_PrismInstanceBuffer, 0, DataSize, InstanceData.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void Tutorial04_Instancing::RenderShadowMap()
{
    float3 f3LightSpaceX, f3LightSpaceY, f3LightSpaceZ;
    f3LightSpaceZ = normalize(m_LightDirection);

    auto min_cmp = min(min(std::abs(m_LightDirection.x), std::abs(m_LightDirection.y)), std::abs(m_LightDirection.z));
    if (min_cmp == std::abs(m_LightDirection.x))
        f3LightSpaceX = float3(1, 0, 0);
    else if (min_cmp == std::abs(m_LightDirection.y))
        f3LightSpaceX = float3(0, 1, 0);
    else
        f3LightSpaceX = float3(0, 0, 1);

    f3LightSpaceY = cross(f3LightSpaceZ, f3LightSpaceX);
    f3LightSpaceX = cross(f3LightSpaceY, f3LightSpaceZ);
    f3LightSpaceX = normalize(f3LightSpaceX);
    f3LightSpaceY = normalize(f3LightSpaceY);

    float4x4 WorldToLightViewSpaceMatr = float4x4::ViewFromBasis(f3LightSpaceX, f3LightSpaceY, f3LightSpaceZ);

    float3 f3SceneCenter = float3(0, 0, 0);
    float  SceneRadius   = std::sqrt(3.f);
    float3 f3MinXYZ      = f3SceneCenter - float3(SceneRadius, SceneRadius, SceneRadius);
    float3 f3MaxXYZ      = f3SceneCenter + float3(SceneRadius, SceneRadius, SceneRadius * 5);
    float3 f3SceneExtent = f3MaxXYZ - f3MinXYZ;

    const auto& DevInfo = m_pDevice->GetDeviceInfo();
    const bool  IsGL    = DevInfo.IsGLDevice();
    float4      f4LightSpaceScale;
    f4LightSpaceScale.x = 2.f / f3SceneExtent.x;
    f4LightSpaceScale.y = 2.f / f3SceneExtent.y;
    f4LightSpaceScale.z = (IsGL ? 2.f : 1.f) / f3SceneExtent.z;
    float4 f4LightSpaceScaledBias;
    f4LightSpaceScaledBias.x = -f3MinXYZ.x * f4LightSpaceScale.x - 1.f;
    f4LightSpaceScaledBias.y = -f3MinXYZ.y * f4LightSpaceScale.y - 1.f;
    f4LightSpaceScaledBias.z = -f3MinXYZ.z * f4LightSpaceScale.z + (IsGL ? -1.f : 0.f);

    float4x4 ScaleMatrix      = float4x4::Scale(f4LightSpaceScale.x, f4LightSpaceScale.y, f4LightSpaceScale.z);
    float4x4 ScaledBiasMatrix = float4x4::Translation(f4LightSpaceScaledBias.x, f4LightSpaceScaledBias.y, f4LightSpaceScaledBias.z);
    float4x4 ShadowProjMatr = ScaleMatrix * ScaledBiasMatrix;
    float4x4 WorldToLightProjSpaceMatr = WorldToLightViewSpaceMatr * ShadowProjMatr;

    const auto& NDCAttribs    = DevInfo.GetNDCAttribs();
    float4x4    ProjToUVScale = float4x4::Scale(0.5f, NDCAttribs.YtoVScale, NDCAttribs.ZtoDepthScale);
    float4x4    ProjToUVBias  = float4x4::Translation(0.5f, 0.5f, NDCAttribs.GetZtoDepthBias());

    m_WorldToShadowMapUVDepthMatr = WorldToLightProjSpaceMatr * ProjToUVScale * ProjToUVBias;

    Render_Cube(WorldToLightProjSpaceMatr, true);
}

void Tutorial04_Instancing::RenderShadowMapVis()
{
    m_pImmediateContext->SetPipelineState(m_pShadowMapVisPSO);
    m_pImmediateContext->CommitShaderResources(m_ShadowMapVisSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    DrawAttribs DrawAttrs(4, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->Draw(DrawAttrs);
}

void Tutorial04_Instancing::Render_Cube(const float4x4& CameraViewProj, bool IsShadowPass)
{
    struct Constants
    {
        float4x4 CameraView;
        float4x4 RotationMatrix;
        float4   LightDirection;
        float4x4 WorldToShadowMapUVDepth;
    };
    {
        MapHelper<Constants>    CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->CameraView     = CameraViewProj;
        CBConstants->RotationMatrix = m_RotationMatrix;
        CBConstants->LightDirection = m_LightDirection;
        CBConstants->WorldToShadowMapUVDepth = m_WorldToShadowMapUVDepthMatr;
    }

    {
        MapHelper<LightData> CBConstantsPS(m_pImmediateContext, m_PSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstantsPS->lightPositionWorld = float3(0.f, 4.f, 0.f);
        CBConstantsPS->eyePositionWorld   = m_Camera.GetPos();
        CBConstantsPS->ambientLight       = float4(0.0f, 0.0f, 0.0f, 0.0f);
        CBConstantsPS->lightDirection     = m_LightDirection;
        CBConstantsPS->spec               = m_specMask;
        CBConstantsPS->specPower          = m_specPower;
    }

    const Uint64 offsets[] = {0, 0};
    IBuffer*     pBuffs[]  = {m_CubeVertexBuffer, m_InstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);


    if (IsShadowPass) {
        m_pImmediateContext->SetPipelineState(m_pCubeShadowPSO);
        m_pImmediateContext->CommitShaderResources(m_CubeShadowSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    else {
        m_pImmediateContext->SetPipelineState(m_pPSO);
        m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }


    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType    = VT_UINT32;
    DrawAttrs.NumIndices   = 36;
    DrawAttrs.NumInstances = NumInstances;
    DrawAttrs.Flags        = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);


}

void Tutorial04_Instancing::Render()
{
    auto*  pRTV       = m_pSwapChain->GetCurrentBackBufferRTV();
    auto*  pDSV       = m_pSwapChain->GetDepthBufferDSV();
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};

    PopulateInstanceBuffer();
    PopulateInstanceBufferPrism();

    if (m_ConvertPSOutputToGamma)
        ClearColor = LinearToSRGB(ClearColor);

    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);


    {
        MapHelper<float4x4> CBConstantsPrism(m_pImmediateContext, m_VSConstantsPrism, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstantsPrism[0] = m_ViewProjMatrix;
        CBConstantsPrism[1] = m_RotationMatrix;
    }
    
    Render_Cube(m_ViewProjMatrix, false);

    const Uint64 offsets2[]    = {0, 0};
    IBuffer*     pBuffsPrism[] = {m_VertexBufferPrism, m_PrismInstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffsPrism), pBuffsPrism, offsets2, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_IndexBufferPrism, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(m_pPSOPrism);
    m_pImmediateContext->CommitShaderResources(m_SRBPrism, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs PrismDrawAttrs;
    PrismDrawAttrs.IndexType    = VT_UINT32;
    PrismDrawAttrs.NumIndices   = 36;
    PrismDrawAttrs.NumInstances = NumPrismInstances;
    PrismDrawAttrs.Flags        = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(PrismDrawAttrs);


}

void Tutorial04_Instancing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    auto SrfPreTransform  = GetSurfacePretransformMatrix(float3{0, 0, 1});
    auto Proj             = GetAdjustedProjectionMatrix(m_FOV * PI_F / 180.0f, 0.1f, 100.f);
    auto SrfPreTransform2 = m_pSwapChain->GetDesc().PreTransform;
    m_Camera.SetMoveSpeed(10.0f);

    // Ventana principal para modo de cámara
    ImGui::SetNextWindowSize(ImVec2(500, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Vista del movil para bebes");
    {
        const char* cameraModes[] = {"Estatico", "Libre"};
        ImGui::Combo("Modo Camara", &m_CameraMode, cameraModes, IM_ARRAYSIZE(cameraModes));

        if (m_CameraMode == 0) // Modo estático
        {
            const char* views[] = {"Frente", "Arriba", "Abajo", "Izquierda", "Derecha"};
            ImGui::Combo("Vista", &m_ViewOption, views, IM_ARRAYSIZE(views));
        }
        else if (m_CameraMode == 1) // Modo libre
        {
            const char* speedOptions[] = {"Lento", "Normal", "Rapido"};
            ImGui::Combo("Velocidad Camara", &m_CameraSpeedOption, speedOptions, IM_ARRAYSIZE(speedOptions));
            ImGui::SliderFloat("FOV", &m_FOV, 30.0f, 120.0f, "FOV: %.1f deg");
        }
    }
    ImGui::End();

    // Nueva ventana para ajustar Specular
    ImGui::Begin("Specular Settings");
    {
        // Checkboxes para activar o desactivar cada canal
        static bool spec_R = true;
        static bool spec_G = true;
        static bool spec_B = true;
        ImGui::Checkbox("Specular Red", &spec_R);
        ImGui::Checkbox("Specular Green", &spec_G);
        ImGui::Checkbox("Specular Blue", &spec_B);

        // Actualizamos la máscara: cada canal vale 1 si está activado, 0 si no.
        m_specMask = float4(spec_R ? 1.0f : 0.0f,
                            spec_G ? 1.0f : 0.0f,
                            spec_B ? 1.0f : 0.0f,
                            1.0f);

        ImGui::SliderFloat("Specular Power", &m_specPower, 0.0f, 100.0f, "Power: %.1f");
    }
    ImGui::End();

    // Ajustar la velocidad de rotación según la opción seleccionada en ImGui
    float rotationSpeed = 0.00005f; // Valor por defecto
    switch (m_CameraSpeedOption)
    {
        case 0: rotationSpeed = 0.00005f; break; // Lento
        case 1: rotationSpeed = 0.0005f; break;  // Normal
        case 2: rotationSpeed = 0.001f; break;   // Rápido
        default: rotationSpeed = 0.00005f; break;
    }
    m_Camera.SetRotationSpeed(rotationSpeed);

    if (m_CameraMode == 1) // Modo cámara libre
    {
        m_Camera.SetProjAttribs(0.1f, 100.f, 1, m_FOV * PI_F / 180.0f, SURFACE_TRANSFORM_IDENTITY, false);
        m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));

        const auto& mouseState = m_InputController.GetMouseState();
        if (m_LastMouseState.PosX >= 0 &&
            m_LastMouseState.PosY >= 0 &&
            (m_LastMouseState.ButtonFlags & MouseState::BUTTON_FLAG_RIGHT) != 0)
        {
            float fYawDelta   = (mouseState.PosX - m_LastMouseState.PosX) * rotationSpeed;
            float fPitchDelta = (mouseState.PosY - m_LastMouseState.PosY) * rotationSpeed;
            m_Camera.SetRotation(fYawDelta, fPitchDelta);
        }
        m_LastMouseState = mouseState;

        m_ViewProjMatrix = m_Camera.GetViewMatrix() * SrfPreTransform * m_Camera.GetProjMatrix();
    }
    else
    {
        float4x4 View;
        switch (m_ViewOption)
        {
            case 0: View = float4x4::RotationX(0.f) * float4x4::Translation(0.f, 3.f, 30.0f); break;    // Frente
            case 1: View = float4x4::RotationX(-1.62f) * float4x4::Translation(0.f, 3.f, 30.0f); break; // Arriba
            case 2: View = float4x4::RotationX(1.62f) * float4x4::Translation(0.f, 3.f, 40.0f); break;  // Abajo
            case 3: View = float4x4::RotationY(-1.62f) * float4x4::Translation(0.f, 3.f, 30.0f); break; // Izquierda
            case 4: View = float4x4::RotationY(1.62f) * float4x4::Translation(0.f, 3.f, 30.0f); break;  // Derecha
            default: View = float4x4::RotationX(0.f) * float4x4::Translation(0.f, 3.f, 30.0f);
        }
        m_ViewProjMatrix = View * SrfPreTransform * Proj;
    }

    m_RotationMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 0.0f);
}






} // namespace Diligent