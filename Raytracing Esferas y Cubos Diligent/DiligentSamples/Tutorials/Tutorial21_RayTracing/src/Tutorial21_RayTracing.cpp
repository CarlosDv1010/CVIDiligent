/*
 *  Copyright 2019-2024 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http: *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include "Tutorial21_RayTracing.hpp"
#include "MapHelper.hpp"
#include "GraphicsTypesX.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ShaderMacroHelper.hpp"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "AdvancedMath.hpp"
#include "PlatformMisc.hpp"
#include <random>




namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial21_RayTracing();
}

void Tutorial21_RayTracing::CreateGraphicsPSO()
{

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Image blit PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;
    ShaderCI.CompileFlags   = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Image blit VS";
        ShaderCI.FilePath        = "ImageBlit.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
        VERIFY_EXPR(pVS != nullptr);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Image blit PS";
        ShaderCI.FilePath        = "ImageBlit.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
        VERIFY_EXPR(pPS != nullptr);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pImageBlitPSO);
    VERIFY_EXPR(m_pImageBlitPSO != nullptr);

    m_pImageBlitPSO->CreateShaderResourceBinding(&m_pImageBlitSRB, true);
    VERIFY_EXPR(m_pImageBlitSRB != nullptr);
}

void Tutorial21_RayTracing::CreateRayTracingPSO()
{
    m_MaxRecursionDepth = std::min(m_MaxRecursionDepth, m_pDevice->GetAdapterInfo().RayTracing.MaxRecursionDepth);

    RayTracingPipelineStateCreateInfoX PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Ray tracing PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_RAY_TRACING;

    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("NUM_TEXTURES", NumTextures);

    ShaderCreateInfo ShaderCI;
    ShaderCI.Desc.UseCombinedTextureSamplers = false;

    ShaderCI.Macros = Macros;

    ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;

    ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    ShaderCI.HLSLVersion    = {6, 3};
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pRayGen;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
        ShaderCI.Desc.Name       = "Ray tracing RG";
        ShaderCI.FilePath        = "RayTrace.rgen";
        ShaderCI.EntryPoint      = "main";
        m_pDevice->CreateShader(ShaderCI, &pRayGen);
        VERIFY_EXPR(pRayGen != nullptr);
    }

    RefCntAutoPtr<IShader> pPrimaryMiss, pShadowMiss;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
        ShaderCI.Desc.Name       = "Primary ray miss shader";
        ShaderCI.FilePath        = "PrimaryMiss.rmiss";
        ShaderCI.EntryPoint      = "main";
        m_pDevice->CreateShader(ShaderCI, &pPrimaryMiss);
        VERIFY_EXPR(pPrimaryMiss != nullptr);

        ShaderCI.Desc.Name  = "Shadow ray miss shader";
        ShaderCI.FilePath   = "ShadowMiss.rmiss";
        ShaderCI.EntryPoint = "main";
        m_pDevice->CreateShader(ShaderCI, &pShadowMiss);
        VERIFY_EXPR(pShadowMiss != nullptr);
    }

    RefCntAutoPtr<IShader> pCubePrimaryHit, pGroundHit, pGlassPrimaryHit, pSpherePrimaryHit;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
        ShaderCI.Desc.Name       = "Cube primary ray closest hit shader";
        ShaderCI.FilePath        = "CubePrimaryHit.rchit";
        ShaderCI.EntryPoint      = "main";
        m_pDevice->CreateShader(ShaderCI, &pCubePrimaryHit);
        VERIFY_EXPR(pCubePrimaryHit != nullptr);

        ShaderCI.Desc.Name  = "Ground primary ray closest hit shader";
        ShaderCI.FilePath   = "Ground.rchit";
        ShaderCI.EntryPoint = "main";
        m_pDevice->CreateShader(ShaderCI, &pGroundHit);
        VERIFY_EXPR(pGroundHit != nullptr);

        ShaderCI.Desc.Name  = "Glass primary ray closest hit shader";
        ShaderCI.FilePath   = "GlassPrimaryHit.rchit";
        ShaderCI.EntryPoint = "main";
        m_pDevice->CreateShader(ShaderCI, &pGlassPrimaryHit);
        VERIFY_EXPR(pGlassPrimaryHit != nullptr);

        ShaderCI.Desc.Name  = "Sphere primary ray closest hit shader";
        ShaderCI.FilePath   = "SpherePrimaryHit.rchit";
        ShaderCI.EntryPoint = "main";
        m_pDevice->CreateShader(ShaderCI, &pSpherePrimaryHit);
        VERIFY_EXPR(pSpherePrimaryHit != nullptr);
    }

    RefCntAutoPtr<IShader> pSphereIntersection;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_RAY_INTERSECTION;
        ShaderCI.Desc.Name       = "Sphere intersection shader";
        ShaderCI.FilePath        = "SphereIntersection.rint";
        ShaderCI.EntryPoint      = "main";
        m_pDevice->CreateShader(ShaderCI, &pSphereIntersection);
        VERIFY_EXPR(pSphereIntersection != nullptr);
    }


    PSOCreateInfo.AddGeneralShader("Main", pRayGen);
    PSOCreateInfo.AddGeneralShader("PrimaryMiss", pPrimaryMiss);
    PSOCreateInfo.AddGeneralShader("ShadowMiss", pShadowMiss);

    PSOCreateInfo.AddTriangleHitShader("CubePrimaryHit", pCubePrimaryHit);
    PSOCreateInfo.AddTriangleHitShader("GroundHit", pGroundHit);
    PSOCreateInfo.AddTriangleHitShader("GlassPrimaryHit", pGlassPrimaryHit);

    PSOCreateInfo.AddProceduralHitShader("SpherePrimaryHit", pSphereIntersection, pSpherePrimaryHit);
    PSOCreateInfo.AddProceduralHitShader("SphereShadowHit", pSphereIntersection);

    PSOCreateInfo.RayTracingPipeline.MaxRecursionDepth = static_cast<Uint8>(m_MaxRecursionDepth);

    PSOCreateInfo.RayTracingPipeline.ShaderRecordSize = 0;

    PSOCreateInfo.MaxAttributeSize = std::max<Uint32>(sizeof(/*BuiltInTriangleIntersectionAttributes*/ float2), sizeof(HLSL::ProceduralGeomIntersectionAttribs));
    PSOCreateInfo.MaxPayloadSize   = std::max<Uint32>(sizeof(HLSL::PrimaryRayPayload), sizeof(HLSL::ShadowRayPayload));

    SamplerDesc SamLinearWrapDesc{
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP};

    PipelineResourceLayoutDescX ResourceLayout;
    ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    ResourceLayout.AddImmutableSampler(SHADER_TYPE_RAY_CLOSEST_HIT, "g_SamLinearWrap", SamLinearWrapDesc);
    ResourceLayout
        .AddVariable(SHADER_TYPE_RAY_GEN | SHADER_TYPE_RAY_MISS | SHADER_TYPE_RAY_CLOSEST_HIT, "g_ConstantsCB", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
        .AddVariable(SHADER_TYPE_RAY_GEN, "g_ColorBuffer", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
    ResourceLayout.AddVariable(SHADER_TYPE_RAY_CLOSEST_HIT, "g_SphereMaterials", SHADER_RESOURCE_VARIABLE_TYPE_STATIC);
    ResourceLayout.AddVariable(SHADER_TYPE_RAY_CLOSEST_HIT, "g_CubeMaterials", SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

    PSOCreateInfo.PSODesc.ResourceLayout = ResourceLayout;

    m_pDevice->CreateRayTracingPipelineState(PSOCreateInfo, &m_pRayTracingPSO);
    VERIFY_EXPR(m_pRayTracingPSO != nullptr);

    m_pRayTracingPSO->GetStaticVariableByName(SHADER_TYPE_RAY_GEN, "g_ConstantsCB")->Set(m_ConstantsCB);
    m_pRayTracingPSO->GetStaticVariableByName(SHADER_TYPE_RAY_MISS, "g_ConstantsCB")->Set(m_ConstantsCB);
    m_pRayTracingPSO->GetStaticVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_ConstantsCB")->Set(m_ConstantsCB);
    m_pRayTracingPSO->GetStaticVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_SphereMaterials")->Set(m_SphereMaterialsBufferSRV);
    m_pRayTracingPSO->GetStaticVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_CubeMaterials")->Set(m_CubeMaterialsBufferSRV);

    m_pRayTracingPSO->CreateShaderResourceBinding(&m_pRayTracingSRB, true);
    VERIFY_EXPR(m_pRayTracingSRB != nullptr);
}

void Tutorial21_RayTracing::LoadTextures()
{
    IDeviceObject*          pTexSRVs[NumTextures] = {};
    RefCntAutoPtr<ITexture> pTex[NumTextures];
    StateTransitionDesc     Barriers[NumTextures];
    for (int tex = 0; tex < NumTextures; ++tex)
    {
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;

        std::stringstream FileNameSS;
        FileNameSS << "DGLogo" << tex << ".png";
        auto FileName = FileNameSS.str();
        CreateTextureFromFile(FileName.c_str(), loadInfo, m_pDevice, &pTex[tex]);

        auto* pTextureSRV = pTex[tex]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        pTexSRVs[tex]     = pTextureSRV;
        Barriers[tex]     = StateTransitionDesc{pTex[tex], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
    }
    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);


    m_pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_CubeTextures")->SetArray(pTexSRVs, 0, NumTextures);

    RefCntAutoPtr<ITexture> pGroundTex;
    CreateTextureFromFile("Ground.jpg", TextureLoadInfo{}, m_pDevice, &pGroundTex);

    m_pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_GroundTexture")->Set(pGroundTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
}

void Tutorial21_RayTracing::CreateCubeBLAS()
{
    RefCntAutoPtr<IDataBlob> pCubeVerts;
    RefCntAutoPtr<IDataBlob> pCubeIndices;
    GeometryPrimitiveInfo    CubeGeoInfo;
    constexpr float          CubeSize = 2.f;
    CreateGeometryPrimitive(CubeGeometryPrimitiveAttributes{CubeSize, GEOMETRY_PRIMITIVE_VERTEX_FLAG_ALL}, &pCubeVerts, &pCubeIndices, &CubeGeoInfo);

    struct CubeVertex
    {
        float3 Pos;
        float3 Normal;
        float2 UV;
    };
    VERIFY_EXPR(CubeGeoInfo.VertexSize == sizeof(CubeVertex));
    const CubeVertex* pVerts   = pCubeVerts->GetConstDataPtr<CubeVertex>();
    const Uint32*     pIndices = pCubeIndices->GetConstDataPtr<Uint32>();

    {
        HLSL::CubeAttribs Attribs;
        for (Uint32 v = 0; v < CubeGeoInfo.NumVertices; ++v)
        {
            Attribs.UVs[v]     = {pVerts[v].UV, 0, 0};
            Attribs.Normals[v] = pVerts[v].Normal;
        }

        for (Uint32 i = 0; i < CubeGeoInfo.NumIndices; i += 3)
        {
            const Uint32* TriIdx{&pIndices[i]};
            Attribs.Primitives[i / 3] = uint4{TriIdx[0], TriIdx[1], TriIdx[2], 0};
        }

        BufferDesc BuffDesc;
        BuffDesc.Name      = "Cube Attribs";
        BuffDesc.Usage     = USAGE_IMMUTABLE;
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        BuffDesc.Size      = sizeof(Attribs);

        BufferData BufData = {&Attribs, BuffDesc.Size};

        m_pDevice->CreateBuffer(BuffDesc, &BufData, &m_CubeAttribsCB);
        VERIFY_EXPR(m_CubeAttribsCB != nullptr);

        m_pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_CubeAttribsCB")->Set(m_CubeAttribsCB);
    }

    RefCntAutoPtr<IBuffer>             pCubeVertexBuffer;
    RefCntAutoPtr<IBuffer>             pCubeIndexBuffer;
    GeometryPrimitiveBuffersCreateInfo CubeBuffersCI;
    CubeBuffersCI.VertexBufferBindFlags = BIND_RAY_TRACING;
    CubeBuffersCI.IndexBufferBindFlags  = BIND_RAY_TRACING;
    CreateGeometryPrimitiveBuffers(m_pDevice, CubeGeometryPrimitiveAttributes{CubeSize, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION},
                                   &CubeBuffersCI, &pCubeVertexBuffer, &pCubeIndexBuffer);

    {
        BLASTriangleDesc Triangles;
        {
            Triangles.GeometryName         = "Cube";
            Triangles.MaxVertexCount       = CubeGeoInfo.NumVertices;
            Triangles.VertexValueType      = VT_FLOAT32;
            Triangles.VertexComponentCount = 3;
            Triangles.MaxPrimitiveCount    = CubeGeoInfo.NumIndices / 3;
            Triangles.IndexType            = VT_UINT32;

            BottomLevelASDesc ASDesc;
            ASDesc.Name          = "Cube BLAS";
            ASDesc.Flags         = RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
            ASDesc.pTriangles    = &Triangles;
            ASDesc.TriangleCount = 1;

            m_pDevice->CreateBLAS(ASDesc, &m_pCubeBLAS);
            VERIFY_EXPR(m_pCubeBLAS != nullptr);
        }

        RefCntAutoPtr<IBuffer> pScratchBuffer;
        {
            BufferDesc BuffDesc;
            BuffDesc.Name      = "BLAS Scratch Buffer";
            BuffDesc.Usage     = USAGE_DEFAULT;
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = m_pCubeBLAS->GetScratchBufferSizes().Build;

            m_pDevice->CreateBuffer(BuffDesc, nullptr, &pScratchBuffer);
            VERIFY_EXPR(pScratchBuffer != nullptr);
        }

        BLASBuildTriangleData TriangleData;
        TriangleData.GeometryName         = Triangles.GeometryName;
        TriangleData.pVertexBuffer        = pCubeVertexBuffer;
        TriangleData.VertexStride         = sizeof(float3);
        TriangleData.VertexCount          = Triangles.MaxVertexCount;
        TriangleData.VertexValueType      = Triangles.VertexValueType;
        TriangleData.VertexComponentCount = Triangles.VertexComponentCount;
        TriangleData.pIndexBuffer         = pCubeIndexBuffer;
        TriangleData.PrimitiveCount       = Triangles.MaxPrimitiveCount;
        TriangleData.IndexType            = Triangles.IndexType;
        TriangleData.Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        BuildBLASAttribs Attribs;
        Attribs.pBLAS             = m_pCubeBLAS;
        Attribs.pTriangleData     = &TriangleData;
        Attribs.TriangleDataCount = 1;

        Attribs.pScratchBuffer = pScratchBuffer;

        Attribs.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Attribs.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        m_pImmediateContext->BuildBLAS(Attribs);
    }
}

void Tutorial21_RayTracing::CreateProceduralBLAS()
{
    static_assert(sizeof(HLSL::BoxAttribs) % 16 == 0, "BoxAttribs must be aligned by 16 bytes");

    std::vector<HLSL::BoxAttribs> BoxAttributes(m_NumSpheres);
    const HLSL::BoxAttribs        SingleBoxDimensions{-2.5f, -2.5f, -2.5f, 2.5f, 2.5f, 2.5f};
    for (Uint32 i = 0; i < m_NumSpheres; ++i)
    {
        float radius     = m_SphereMaterials[i].Radius;
        BoxAttributes[i] = HLSL::BoxAttribs{-radius, -radius, -radius, +radius, +radius, +radius};
    }

    {
        BufferData BufData = {BoxAttributes.data(), sizeof(HLSL::BoxAttribs) * m_NumSpheres};
        BufferDesc BuffDesc;
        BuffDesc.Name              = "AABB Buffer";
        BuffDesc.Usage             = USAGE_IMMUTABLE;
        BuffDesc.BindFlags         = BIND_RAY_TRACING | BIND_SHADER_RESOURCE;
        BuffDesc.Size              = sizeof(HLSL::BoxAttribs) * m_NumSpheres;
        BuffDesc.ElementByteStride = sizeof(HLSL::BoxAttribs);
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;

        if (m_BoxAttribsCB) m_BoxAttribsCB.Release();
        m_pDevice->CreateBuffer(BuffDesc, &BufData, &m_BoxAttribsCB);
        VERIFY_EXPR(m_BoxAttribsCB != nullptr);

        if (m_pRayTracingSRB)
        {
            m_pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_INTERSECTION, "g_BoxAttribs")->Set(m_BoxAttribsCB->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
        }
    }

    {
        BLASBoundingBoxDesc BoxInfo;
        {
            BoxInfo.GeometryName = "ProceduralSpheres";
            BoxInfo.MaxBoxCount  = m_NumSpheres;

            BottomLevelASDesc ASDesc;
            ASDesc.Name     = "Procedural BLAS";
            ASDesc.Flags    = RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
            ASDesc.pBoxes   = &BoxInfo;
            ASDesc.BoxCount = 1;
            if (m_pProceduralBLAS) m_pProceduralBLAS.Release();
            m_pDevice->CreateBLAS(ASDesc, &m_pProceduralBLAS);
            VERIFY_EXPR(m_pProceduralBLAS != nullptr);
        }

        RefCntAutoPtr<IBuffer> pScratchBuffer;
        {
            BufferDesc BuffDesc;
            BuffDesc.Name      = "BLAS Scratch Buffer";
            BuffDesc.Usage     = USAGE_DEFAULT;
            BuffDesc.BindFlags = BIND_RAY_TRACING;
            BuffDesc.Size      = m_pProceduralBLAS->GetScratchBufferSizes().Build;

            m_pDevice->CreateBuffer(BuffDesc, nullptr, &pScratchBuffer);
            VERIFY_EXPR(pScratchBuffer != nullptr);
        }

        BLASBuildBoundingBoxData BoxData;
        BoxData.GeometryName = BoxInfo.GeometryName;
        BoxData.BoxCount     = m_NumSpheres;
        BoxData.BoxStride    = sizeof(HLSL::BoxAttribs);
        BoxData.pBoxBuffer   = m_BoxAttribsCB;
        BoxData.BoxOffset    = 0;

        BuildBLASAttribs Attribs;
        Attribs.pBLAS                       = m_pProceduralBLAS;
        Attribs.pBoxData                    = &BoxData;
        Attribs.BoxDataCount                = 1;
        Attribs.pScratchBuffer              = pScratchBuffer;
        Attribs.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Attribs.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        m_pImmediateContext->BuildBLAS(Attribs);
    }
}

void Tutorial21_RayTracing::UpdateTLAS()
{
    const Uint32 TotalInstances = 1 + m_NumSpheres + m_NumCubes;
    bool         NeedUpdate     = true;

    if (!m_pTLAS)
    {
        TopLevelASDesc TLASDesc;
        TLASDesc.Name             = "TLAS";
        TLASDesc.MaxInstanceCount = TotalInstances;
        TLASDesc.Flags            = RAYTRACING_BUILD_AS_ALLOW_UPDATE | RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
        m_pDevice->CreateTLAS(TLASDesc, &m_pTLAS);
        VERIFY_EXPR(m_pTLAS != nullptr);
        NeedUpdate = false;
        if (m_pRayTracingSRB)
        {
            if (auto* pVar = m_pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_TLAS")) pVar->Set(m_pTLAS);
            if (auto* pVar = m_pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_TLAS")) pVar->Set(m_pTLAS);
        }
    }
    if (!m_ScratchBuffer)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name      = "TLAS Scratch Buffer";
        BuffDesc.Usage     = USAGE_DEFAULT;
        BuffDesc.BindFlags = BIND_RAY_TRACING;
        BuffDesc.Size      = std::max(m_pTLAS->GetScratchBufferSizes().Build, m_pTLAS->GetScratchBufferSizes().Update);
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_ScratchBuffer);
        VERIFY_EXPR(m_ScratchBuffer != nullptr);
    }
    if (!m_InstanceBuffer || m_InstanceBuffer->GetDesc().Size < TLAS_INSTANCE_DATA_SIZE * TotalInstances)
    {
        if (m_InstanceBuffer) m_InstanceBuffer.Release();
        BufferDesc BuffDesc;
        BuffDesc.Name      = "TLAS Instance Buffer";
        BuffDesc.Usage     = USAGE_DEFAULT;
        BuffDesc.BindFlags = BIND_RAY_TRACING;
        BuffDesc.Size      = TLAS_INSTANCE_DATA_SIZE * TotalInstances;
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_InstanceBuffer);
        VERIFY_EXPR(m_InstanceBuffer != nullptr);
    }

    std::vector<TLASBuildInstanceData> Instances(TotalInstances);

    const auto AnimateOpaqueSphere = [&](TLASBuildInstanceData& Dst) {
        if (!m_EnableSpheres[Dst.CustomId])
            Dst.Mask = 0;
    };

    const auto AnimateOpaqueCube = [&](TLASBuildInstanceData& Dst) {
        if (!m_EnableCubes[Dst.CustomId])
            Dst.Mask = 0;
    };

    Instances[0].InstanceName = "Ground Instance";
    Instances[0].pBLAS        = m_pCubeBLAS;
    Instances[0].Mask         = OPAQUE_GEOM_MASK;
    Instances[0].Transform.SetRotation(float3x3::Scale(100.0f, 0.1f, 100.0f).Data());
    Instances[0].Transform.SetTranslation(0.0f, -6.0f, 0.0f);
    Instances[0].CustomId = 0;

    const float                           fixedSphereY = -2.0f;
    std::vector<std::pair<float3, float>> placedSpheres;
    std::vector<std::pair<float3, float>> placedCubes;
    placedCubes.reserve(m_NumCubes);
    placedSpheres.reserve(m_NumSpheres);
    std::vector<std::string> sphereInstanceNames(m_NumSpheres);
    std::vector<std::string> cubeInstanceNames(m_NumCubes);

    if (m_NumSpheres >= 1)
    {
        int    instanceIndex0                  = 1;
        float  radius0                         = m_SphereMaterials[0].Radius;
        float3 pos0                            = float3(0.0f, fixedSphereY, -8.0f);
        sphereInstanceNames[0]                 = "Sphere Instance 0";
        Instances[instanceIndex0].InstanceName = sphereInstanceNames[0].c_str();
        Instances[instanceIndex0].pBLAS        = m_pProceduralBLAS;
        Instances[instanceIndex0].Mask         = OPAQUE_GEOM_MASK;
        Instances[instanceIndex0].Transform.SetTranslation(pos0.x, pos0.y, pos0.z);
        Instances[instanceIndex0].CustomId = 0;
        placedSpheres.push_back({pos0, radius0});
    }

    if (m_NumCubes >= 1)
    {
        int    instanceIndex0                  = 1 + m_NumSpheres;
        float3 pos0                            = float3(0.0f, fixedSphereY, 0.0f);
        cubeInstanceNames[0]                   = "Cube Instance 0";
        Instances[instanceIndex0].InstanceName = cubeInstanceNames[0].c_str();
        Instances[instanceIndex0].pBLAS        = m_pCubeBLAS;
        Instances[instanceIndex0].Mask         = OPAQUE_GEOM_MASK;
        Instances[instanceIndex0].Transform.SetTranslation(pos0.x, pos0.y, pos0.z);
        Instances[instanceIndex0].CustomId = 0;
        placedCubes.push_back({pos0, 2.5f});
    }

    if (m_NumSpheres >= 2)
    {
        int    instanceIndex1                  = 2;
        float  radius1                         = m_SphereMaterials[1].Radius;
        float3 pos1                            = float3(0.0f, fixedSphereY, 8.0f);
        sphereInstanceNames[1]                 = "Sphere Instance 1";
        Instances[instanceIndex1].InstanceName = sphereInstanceNames[1].c_str();
        Instances[instanceIndex1].pBLAS        = m_pProceduralBLAS;
        Instances[instanceIndex1].Mask         = OPAQUE_GEOM_MASK;
        Instances[instanceIndex1].Transform.SetTranslation(pos1.x, pos1.y, pos1.z);
        Instances[instanceIndex1].CustomId = 1;
        placedSpheres.push_back({pos1, radius1});
    }

    if (m_NumCubes >= 2)
    {
        int    instanceIndex1                  = 2 + m_NumSpheres;
        float3 pos1                            = float3(0.0f, fixedSphereY, 12.0f);
        cubeInstanceNames[1]                   = "Cube Instance 1";
        Instances[instanceIndex1].InstanceName = cubeInstanceNames[1].c_str();
        Instances[instanceIndex1].pBLAS        = m_pCubeBLAS;
        Instances[instanceIndex1].Mask         = OPAQUE_GEOM_MASK;
        Instances[instanceIndex1].Transform.SetTranslation(pos1.x, pos1.y, pos1.z);
        Instances[instanceIndex1].CustomId = 1;
        placedCubes.push_back({pos1, 2.5f});
    }

    if (m_NumSpheres > 2)
    {
        std::mt19937                          pos_gen(67890);
        const float                           placementAreaSizeX = 40.0f;
        const float                           placementAreaSizeZ = 40.0f;
        std::uniform_real_distribution<float> distX(-placementAreaSizeX / 2.0f, placementAreaSizeX / 2.0f);
        std::uniform_real_distribution<float> distZ(-placementAreaSizeZ / 2.0f, placementAreaSizeZ / 2.0f);
        const int                             MaxPlacementTries = 100;
        const float                           DesiredGap        = 0.2f;

        for (Uint32 i = 2; i < m_NumSpheres; ++i)
        {
            int instanceIndex      = i + 1; // Corrige el índice de la instancia en el TLAS
            sphereInstanceNames[i] = "Sphere Instance " + std::to_string(i);
            float currentRadius    = m_SphereMaterials[i].Radius;
            bool  placed           = false;

            for (int tries = 0; tries < MaxPlacementTries; ++tries)
            {
                float3 candidatePos = float3(distX(pos_gen), fixedSphereY, distZ(pos_gen));
                bool   overlaps     = false;
                for (const auto& placedSphere : placedSpheres)
                {
                    float3 otherPos    = placedSphere.first;
                    float  otherRadius = placedSphere.second;
                    float  minDist     = currentRadius + otherRadius + DesiredGap;
                    float3 diffVector  = candidatePos - otherPos;
                    float  distSq      = Diligent::dot(diffVector, diffVector);

                    if (distSq < minDist * minDist)
                    {
                        overlaps = true;
                        break;
                    }
                }

                if (!overlaps)
                {
                    Instances[instanceIndex].InstanceName = sphereInstanceNames[i].c_str();
                    Instances[instanceIndex].pBLAS        = m_pProceduralBLAS;
                    Instances[instanceIndex].Mask         = OPAQUE_GEOM_MASK;
                    Instances[instanceIndex].Transform.SetTranslation(candidatePos.x, candidatePos.y, candidatePos.z);
                    Instances[instanceIndex].CustomId = i;
                    placedSpheres.push_back({candidatePos, currentRadius});
                    placed = true;
                    break;
                }
            }

            if (!placed)
            {
                Instances[instanceIndex].InstanceName = sphereInstanceNames[i].c_str();
                Instances[instanceIndex].pBLAS        = m_pProceduralBLAS;
                Instances[instanceIndex].Mask         = OPAQUE_GEOM_MASK;
                Instances[instanceIndex].Transform.SetTranslation(0.0f, fixedSphereY, 0.0f);
                Instances[instanceIndex].CustomId = i;
                placedSpheres.push_back({{0.0f, fixedSphereY, 0.0f}, currentRadius});
            }
            // Usa el índice correcto
            AnimateOpaqueSphere(Instances[instanceIndex]);
        }
    }

    if (m_NumCubes > 2)
    {
        std::mt19937                          pos_gen(67890);
        const float                           placementAreaSizeX = 40.0f;
        const float                           placementAreaSizeZ = 40.0f;
        std::uniform_real_distribution<float> distX(-placementAreaSizeX / 2.0f, placementAreaSizeX / 2.0f);
        std::uniform_real_distribution<float> distZ(-placementAreaSizeZ / 2.0f, placementAreaSizeZ / 2.0f);
        const int                             MaxPlacementTries = 100;
        const float                           DesiredGap        = 0.2f;
        for (Uint32 i = 2; i < m_NumCubes; ++i)
        {
            int instanceIndex    = i + 1 + m_NumSpheres; // Corrige el índice para cubos
            cubeInstanceNames[i] = "Cube Instance " + std::to_string(i);
            bool placed          = false;
            for (int tries = 0; tries < MaxPlacementTries; ++tries)
            {
                float3 candidatePos = float3(distX(pos_gen), fixedSphereY, distZ(pos_gen));
                bool   overlaps     = false;
                for (const auto& placedCube : placedCubes)
                {
                    float3 otherPos    = placedCube.first;
                    float  otherRadius = placedCube.second;
                    float  minDist     = 2.5f + otherRadius + DesiredGap;
                    float3 diffVector  = candidatePos - otherPos;
                    float  distSq      = Diligent::dot(diffVector, diffVector);
                    if (distSq < minDist * minDist)
                    {
                        overlaps = true;
                        break;
                    }
                }
                if (!overlaps)
                {
                    Instances[instanceIndex].InstanceName = cubeInstanceNames[i].c_str();
                    Instances[instanceIndex].pBLAS        = m_pCubeBLAS;
                    Instances[instanceIndex].Mask         = OPAQUE_GEOM_MASK;
                    Instances[instanceIndex].Transform.SetTranslation(candidatePos.x, candidatePos.y, candidatePos.z);
                    Instances[instanceIndex].CustomId = i;
                    placedCubes.push_back({candidatePos, 2.5f});
                    placed = true;
                    break;
                }
            }
            if (!placed)
            {
                Instances[instanceIndex].InstanceName = cubeInstanceNames[i].c_str();
                Instances[instanceIndex].pBLAS        = m_pCubeBLAS;
                Instances[instanceIndex].Mask         = OPAQUE_GEOM_MASK;
                Instances[instanceIndex].Transform.SetTranslation(0.0f, fixedSphereY, 0.0f);
                Instances[instanceIndex].CustomId = i;
                placedCubes.push_back({{0.0f, fixedSphereY, 0.0f}, 2.5f});
            }
            // Usa el índice correcto
            AnimateOpaqueCube(Instances[instanceIndex]);
        }
    }



    BuildTLASAttribs Attribs;
    Attribs.pTLAS                        = m_pTLAS;
    Attribs.Update                       = NeedUpdate;
    Attribs.pScratchBuffer               = m_ScratchBuffer;
    Attribs.pInstanceBuffer              = m_InstanceBuffer;
    Attribs.pInstances                   = Instances.data();
    Attribs.InstanceCount                = TotalInstances;
    Attribs.BindingMode                  = HIT_GROUP_BINDING_MODE_PER_INSTANCE;
    Attribs.HitGroupStride               = HIT_GROUP_STRIDE;
    Attribs.TLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.BLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.InstanceBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.ScratchBufferTransitionMode  = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    m_pImmediateContext->BuildTLAS(Attribs);
}

void Tutorial21_RayTracing::CreateSBT()
{

    ShaderBindingTableDesc SBTDesc;
    SBTDesc.Name = "SBT";
    SBTDesc.pPSO = m_pRayTracingPSO;

    m_pDevice->CreateSBT(SBTDesc, &m_pSBT);
    VERIFY_EXPR(m_pSBT != nullptr);

    m_pSBT->BindRayGenShader("Main");

    m_pSBT->BindMissShader("PrimaryMiss", PRIMARY_RAY_INDEX);
    m_pSBT->BindMissShader("ShadowMiss", SHADOW_RAY_INDEX);


    m_pSBT->BindHitGroupForInstance(m_pTLAS, "Ground Instance", PRIMARY_RAY_INDEX, "GroundHit");

    for (Uint32 i = 0; i < m_NumSpheres; ++i)
    {
        std::string sphereName = "Sphere Instance " + std::to_string(i);

        m_pSBT->BindHitGroupForInstance(m_pTLAS, sphereName.c_str(), PRIMARY_RAY_INDEX, "SpherePrimaryHit");
        m_pSBT->BindHitGroupForInstance(m_pTLAS, sphereName.c_str(), SHADOW_RAY_INDEX, "SphereShadowHit");
    }

    for (Uint32 i = 0; i < m_NumCubes; ++i)
    {
        std::string cubeName = "Cube Instance " + std::to_string(i);
        m_pSBT->BindHitGroupForInstance(m_pTLAS, cubeName.c_str(), PRIMARY_RAY_INDEX, "GlassPrimaryHit");
        m_pSBT->BindHitGroupForInstance(m_pTLAS, cubeName.c_str(), SHADOW_RAY_INDEX, "GlassPrimaryHit");
    }

    m_pSBT->BindHitGroupForTLAS(m_pTLAS, SHADOW_RAY_INDEX, nullptr);

    m_pImmediateContext->UpdateSBT(m_pSBT);
}

void Tutorial21_RayTracing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    for (int i = 0; i < m_NumSpheres; ++i)
    {
        m_EnableSpheres[i] = true;
    }

    for (int i = 0; i < m_NumCubes; ++i)
    {
        m_EnableCubes[i] = true;
    }

    if ((m_pDevice->GetAdapterInfo().RayTracing.CapFlags & RAY_TRACING_CAP_FLAG_STANDALONE_SHADERS) == 0)
    {
        UNSUPPORTED("Ray tracing shaders are not supported by device");
        return;
    }

    BufferDesc CBDesc;
    CBDesc.Name      = "Constant buffer";
    CBDesc.Size      = sizeof(m_Constants);
    CBDesc.Usage     = USAGE_DEFAULT;
    CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
    m_pDevice->CreateBuffer(CBDesc, nullptr, &m_ConstantsCB);
    VERIFY_EXPR(m_ConstantsCB != nullptr);


    m_SphereMaterials.resize(m_NumSpheres);
    std::mt19937                          gen(12345);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> ior_dist(1.01f, 2.0f);
    const float                           LargeSphereRadius = 3.0f;
    const float                           SmallSphereRadius = 0.5f;
    for (Uint32 i = 0; i < m_NumSpheres; ++i)
    {
        m_SphereMaterials[i].BaseColor = float4(dist(gen), dist(gen), dist(gen), 1.0f);
        m_SphereMaterials[i].Roughness = 0.05f + 0.9f * dist(gen);
        m_SphereMaterials[i].Metallic  = (dist(gen) > 0.5f) ? (0.7f + 0.3f * dist(gen)) : (0.0f + 0.2f * dist(gen));

        if (i % 3 == 0)
            m_SphereMaterials[i].IndexOfRefraction = 1.01f + 0.1f * dist(gen);
        else if (i % 3 == 1)
            m_SphereMaterials[i].IndexOfRefraction = 1.4f + 0.3f * dist(gen);
        else
            m_SphereMaterials[i].IndexOfRefraction = 1.7f + 0.3f * dist(gen);
        if (i < 2)
        {
            m_SphereMaterials[i].Radius = LargeSphereRadius;
        }
        else
        {
            m_SphereMaterials[i].Radius = SmallSphereRadius;
        }
    }
    m_CubeMaterials.resize(m_NumCubes);

    for (Uint32 i = 0; i < m_NumCubes; i++) {
        m_CubeMaterials[i].BaseColor         = float4(dist(gen), dist(gen), dist(gen), 1.0f);
        m_CubeMaterials[i].Roughness         = 0.05f + 0.9f * dist(gen);
        m_CubeMaterials[i].Metallic          = (dist(gen) > 0.5f) ? (0.7f + 0.3f * dist(gen)) : (0.0f + 0.2f * dist(gen));
        m_CubeMaterials[i].IndexOfRefraction = 1.01f + 0.1f * dist(gen);
    }

    BufferDesc CubeMatBuffDesc;
    CubeMatBuffDesc.Name = "Cube Materials Buffer";
    CubeMatBuffDesc.Size = sizeof(CubeMaterialData) * m_NumCubes;
    CubeMatBuffDesc.Usage = USAGE_DEFAULT;
    CubeMatBuffDesc.BindFlags = BIND_SHADER_RESOURCE;
    CubeMatBuffDesc.Mode      = BUFFER_MODE_STRUCTURED;
    CubeMatBuffDesc.ElementByteStride = sizeof(CubeMaterialData);

    BufferData CubeMatData;
    CubeMatData.pData = m_CubeMaterials.data();
    CubeMatData.DataSize = CubeMatBuffDesc.Size;

    m_pDevice->CreateBuffer(CubeMatBuffDesc, &CubeMatData, &m_CubeMaterialsBuffer);
    VERIFY_EXPR(m_CubeMaterialsBuffer != nullptr);

    m_CubeMaterialsBufferSRV = m_CubeMaterialsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
    VERIFY_EXPR(m_CubeMaterialsBufferSRV != nullptr);

    BufferDesc SphereMatBuffDesc;
    SphereMatBuffDesc.Name              = "Sphere Materials Buffer";
    SphereMatBuffDesc.Size              = sizeof(SphereMaterialData) * m_NumSpheres;
    SphereMatBuffDesc.Usage             = USAGE_DEFAULT;
    SphereMatBuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
    SphereMatBuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    SphereMatBuffDesc.ElementByteStride = sizeof(SphereMaterialData);

    BufferData SphereMatData;
    SphereMatData.pData    = m_SphereMaterials.data();
    SphereMatData.DataSize = SphereMatBuffDesc.Size;


    m_pDevice->CreateBuffer(SphereMatBuffDesc, &SphereMatData, &m_SphereMaterialsBuffer);
    VERIFY_EXPR(m_SphereMaterialsBuffer != nullptr);

    m_SphereMaterialsBufferSRV = m_SphereMaterialsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
    VERIFY_EXPR(m_SphereMaterialsBufferSRV != nullptr);



    CreateGraphicsPSO();
    CreateRayTracingPSO();
    LoadTextures();
    CreateCubeBLAS();
    CreateProceduralBLAS();
    UpdateTLAS();
    CreateSBT();

    m_Camera.SetPos(float3(7.f, -0.5f, -16.5f));
    m_Camera.SetRotation(0.48f, -0.145f);
    m_Camera.SetRotationSpeed(0.005f);
    m_Camera.SetMoveSpeed(5.f);
    m_Camera.SetSpeedUpScales(5.f, 10.f);

    {
        m_Constants.ClipPlanes               = float2{0.1f, 100.0f};
        m_Constants.ShadowPCF                = 1;
        m_Constants.MaxRecursion             = std::min(Uint32{6}, m_MaxRecursionDepth);
        m_Constants.GlassReflectionColorMask = {0.22f, 0.83f, 0.93f};
        m_Constants.GlassAbsorption          = 0.5f;
        m_Constants.GlassMaterialColor       = {0.33f, 0.93f, 0.29f};
        m_Constants.GlassIndexOfRefraction   = {1.5f, 1.02f};
        m_Constants.GlassEnableDispersion    = 0;

        m_Constants.DispersionSamples[0]  = {0.140000f, 0.000000f, 0.266667f, 0.53f};
        m_Constants.DispersionSamples[1]  = {0.130031f, 0.037556f, 0.612267f, 0.25f};
        m_Constants.DispersionSamples[2]  = {0.100123f, 0.213556f, 0.785067f, 0.16f};
        m_Constants.DispersionSamples[3]  = {0.050277f, 0.533556f, 0.785067f, 0.00f};
        m_Constants.DispersionSamples[4]  = {0.000000f, 0.843297f, 0.619682f, 0.13f};
        m_Constants.DispersionSamples[5]  = {0.000000f, 0.927410f, 0.431834f, 0.38f};
        m_Constants.DispersionSamples[6]  = {0.000000f, 0.972325f, 0.270893f, 0.27f};
        m_Constants.DispersionSamples[7]  = {0.000000f, 0.978042f, 0.136858f, 0.19f};
        m_Constants.DispersionSamples[8]  = {0.324000f, 0.944560f, 0.029730f, 0.47f};
        m_Constants.DispersionSamples[9]  = {0.777600f, 0.871879f, 0.000000f, 0.64f};
        m_Constants.DispersionSamples[10] = {0.972000f, 0.762222f, 0.000000f, 0.77f};
        m_Constants.DispersionSamples[11] = {0.971835f, 0.482222f, 0.000000f, 0.62f};
        m_Constants.DispersionSamples[12] = {0.886744f, 0.202222f, 0.000000f, 0.73f};
        m_Constants.DispersionSamples[13] = {0.715967f, 0.000000f, 0.000000f, 0.68f};
        m_Constants.DispersionSamples[14] = {0.459920f, 0.000000f, 0.000000f, 0.91f};
        m_Constants.DispersionSamples[15] = {0.218000f, 0.000000f, 0.000000f, 0.99f};
        m_Constants.DispersionSampleCount = 4;

        m_Constants.AmbientColor  = float4(1.f, 1.f, 1.f, 0.f) * 0.015f;
        m_Constants.LightPos[0]   = {8.00f, +8.0f, +0.00f, 0.f};
        m_Constants.LightColor[0] = {1.00f, +0.8f, +0.80f, 0.f};
        m_Constants.LightPos[1]   = {0.00f, +4.0f, -5.00f, 0.f};
        m_Constants.LightColor[1] = {0.85f, +1.0f, +0.85f, 0.f};

        m_Constants.DiscPoints[0] = {+0.0f, +0.0f, +0.9f, -0.9f};
        m_Constants.DiscPoints[1] = {-0.8f, +1.0f, -1.1f, -0.8f};
        m_Constants.DiscPoints[2] = {+1.5f, +1.2f, -2.1f, +0.7f};
        m_Constants.DiscPoints[3] = {+0.1f, -2.2f, -0.2f, +2.4f};
        m_Constants.DiscPoints[4] = {+2.4f, -0.3f, -3.0f, +2.8f};
        m_Constants.DiscPoints[5] = {+2.0f, -2.6f, +0.7f, +3.5f};
        m_Constants.DiscPoints[6] = {-3.2f, -1.6f, +3.4f, +2.2f};
        m_Constants.DiscPoints[7] = {-1.8f, -3.2f, -1.1f, +3.6f};
    }
    static_assert((sizeof(HLSL::Constants) % 16) == 0, "HLSL::Constants size must be a multiple of 16 bytes");
}

void Tutorial21_RayTracing::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    Attribs.EngineCI.Features.RayTracing = DEVICE_FEATURE_STATE_ENABLED;
}

void Tutorial21_RayTracing::Render()
{
    UpdateTLAS();

    {
        float3 CameraWorldPos = float3::MakeVector(m_Camera.GetWorldMatrix()[3]);
        auto   CameraViewProj = m_Camera.GetViewMatrix() * m_Camera.GetProjMatrix();

        m_Constants.CameraPos   = float4{CameraWorldPos, 1.0f};
        m_Constants.InvViewProj = CameraViewProj.Inverse();

        m_pImmediateContext->UpdateBuffer(m_ConstantsCB, 0, sizeof(m_Constants), &m_Constants, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    {
        m_pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_ColorBuffer")->Set(m_pColorRT->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

        m_pImmediateContext->SetPipelineState(m_pRayTracingPSO);
        m_pImmediateContext->CommitShaderResources(m_pRayTracingSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        TraceRaysAttribs Attribs;
        Attribs.DimensionX = m_pColorRT->GetDesc().Width;
        Attribs.DimensionY = m_pColorRT->GetDesc().Height;
        Attribs.pSBT       = m_pSBT;

        m_pImmediateContext->TraceRays(Attribs);
    }

    {
        m_pImageBlitSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_pColorRT->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

        auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetPipelineState(m_pImageBlitPSO);
        m_pImmediateContext->CommitShaderResources(m_pImageBlitSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->Draw(DrawAttribs{3, DRAW_FLAG_VERIFY_ALL});
    }
}

void Tutorial21_RayTracing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    if (m_Animate)
    {
        m_AnimationTime += static_cast<float>(std::min(m_MaxAnimationTimeDelta, ElapsedTime));
    }

    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));

    auto oldPos = m_Camera.GetPos();
    if (oldPos.y < -5.7f)
    {
        oldPos.y = -5.7f;
        m_Camera.SetPos(oldPos);
        m_Camera.Update(m_InputController, 0.f);
    }
}

void Tutorial21_RayTracing::WindowResize(Uint32 Width, Uint32 Height)
{
    if (Width == 0 || Height == 0)
        return;

    float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    m_Camera.SetProjAttribs(m_Constants.ClipPlanes.x, m_Constants.ClipPlanes.y, AspectRatio, PI_F / 4.f,
                            m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceInfo().NDC.MinZ == -1);

    if (m_pColorRT != nullptr &&
        m_pColorRT->GetDesc().Width == Width &&
        m_pColorRT->GetDesc().Height == Height)
        return;

    m_pColorRT = nullptr;

    TextureDesc RTDesc       = {};
    RTDesc.Name              = "Color buffer";
    RTDesc.Type              = RESOURCE_DIM_TEX_2D;
    RTDesc.Width             = Width;
    RTDesc.Height            = Height;
    RTDesc.BindFlags         = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    RTDesc.ClearValue.Format = m_ColorBufferFormat;
    RTDesc.Format            = m_ColorBufferFormat;

    m_pDevice->CreateTexture(RTDesc, nullptr, &m_pColorRT);
}

void Tutorial21_RayTracing::UpdateUI()
{
    const float MaxIndexOfRefraction = 2.0f;
    const float MaxDispersion        = 0.5f;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Checkbox("Animate", &m_Animate);
        ImGui::Text("Use WASD to move camera");
        ImGui::SliderInt("Shadow blur", &m_Constants.ShadowPCF, 0, 16);
        ImGui::SliderInt("Max recursion", &m_Constants.MaxRecursion, 0, m_MaxRecursionDepth);

        static int activeSphereCount = 0;

        // Slider que controla cuántas esferas están activadas
        ImGui::SliderInt("Active Spheres", &activeSphereCount, 0, static_cast<int>(m_NumSpheres));

        // Actualiza automáticamente el estado de las esferas
        for (int i = 0; i < static_cast<int>(m_NumSpheres); ++i)
        {
            m_EnableSpheres[i] = (i < activeSphereCount);
        }

        // Slider que controla cuántos cubos están activados
        static int activeCubeCount = 0;
        ImGui::SliderInt("Active Cubes", &activeCubeCount, 0, static_cast<int>(m_NumCubes));
        // Actualiza automáticamente el estado de los cubos
        for (int i = 0; i < static_cast<int>(m_NumCubes); ++i)
        {
            m_EnableCubes[i] = (i < activeCubeCount);
        }
    }
    ImGui::End();
}

} // namespace Diligent