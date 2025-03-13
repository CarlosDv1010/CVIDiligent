#include <random>
#include "MovilBebes.hpp"
#include "MapHelper.hpp"
#include "TextureView.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ColorConversion.h"
#include "../../Common/src/TexturedCube.hpp"
#include "Prism.hpp"
#include "imgui.h"
#include <iostream>
#include <filesystem>
#include <windows.h>
#include "SampleBase.hpp"
#include <utility> 
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

namespace Diligent
{



MovilBebes::MovilBebes()
{
}

float4x4 MovilBebes::GetAdjustedProjectionMatrix(float FOV, float NearPlane, float FarPlane) const
{
    const auto& SCDesc = m_pSwapChain->GetDesc();

    float AspectRatio = static_cast<float>(SCDesc.Width) / static_cast<float>(SCDesc.Height);
    float XScale, YScale;
    if (SCDesc.PreTransform == SURFACE_TRANSFORM_ROTATE_90 ||
        SCDesc.PreTransform == SURFACE_TRANSFORM_ROTATE_270 ||
        SCDesc.PreTransform == SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90 ||
        SCDesc.PreTransform == SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270)
    {
        // When the screen is rotated, vertical FOV becomes horizontal FOV
        XScale = 1.f / std::tan(FOV / 2.f);
        // Aspect ratio is inversed
        YScale = XScale * AspectRatio;
    }
    else
    {
        YScale = 1.f / std::tan(FOV / 2.f);
        XScale = YScale / AspectRatio;
    }

    float4x4 Proj;
    Proj._11 = XScale;
    Proj._22 = YScale;
    Proj.SetNearFarClipPlanes(NearPlane, FarPlane, m_pDevice->GetDeviceInfo().NDC.MinZ == -1);
    return Proj;
}

float4x4 MovilBebes::GetSurfacePretransformMatrix(const float3& f3CameraViewAxis) const
{
    const auto& SCDesc = m_pSwapChain->GetDesc();
    switch (SCDesc.PreTransform)
    {
        case SURFACE_TRANSFORM_ROTATE_90:
            // The image content is rotated 90 degrees clockwise.
            return float4x4::RotationArbitrary(f3CameraViewAxis, -PI_F / 2.f);

        case SURFACE_TRANSFORM_ROTATE_180:
            // The image content is rotated 180 degrees clockwise.
            return float4x4::RotationArbitrary(f3CameraViewAxis, -PI_F);

        case SURFACE_TRANSFORM_ROTATE_270:
            // The image content is rotated 270 degrees clockwise.
            return float4x4::RotationArbitrary(f3CameraViewAxis, -PI_F * 3.f / 2.f);

        case SURFACE_TRANSFORM_OPTIMAL:
            UNEXPECTED("SURFACE_TRANSFORM_OPTIMAL is only valid as parameter during swap chain initialization.");
            return float4x4::Identity();

        case SURFACE_TRANSFORM_HORIZONTAL_MIRROR:
        case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90:
        case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180:
        case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270:
            UNEXPECTED("Mirror transforms are not supported");
            return float4x4::Identity();

        default:
            return float4x4::Identity();
    }
}


void MovilBebes::SetAttributes(RefCntAutoPtr<IRenderDevice>  m_pDeviceExt,
                               RefCntAutoPtr<IDeviceContext> m_pImmediateContextExt,
                               RefCntAutoPtr<ISwapChain>     pSwapChainExt,
                               RefCntAutoPtr<IEngineFactory>   m_pEngineFactoryExt)
{
    m_pDevice           = m_pDeviceExt;
    m_pImmediateContext = m_pImmediateContextExt;
    m_pSwapChain        = pSwapChainExt;
    m_pEngineFactory    = m_pEngineFactoryExt;
}


void MovilBebes::SetInstanceBuffers(RefCntAutoPtr<IBuffer> InstanceBuffer,
                                    RefCntAutoPtr<IBuffer> PrismInstanceBuffer,
                                    RefCntAutoPtr<IRenderDevice> m_pDeviceE)
{
    m_InstanceBuffer      = InstanceBuffer;
    m_PrismInstanceBuffer = PrismInstanceBuffer;
    m_pDevice             = m_pDeviceE;
}


void MovilBebes::CreatePipelineState()
{
    LayoutElement LayoutElems[] =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 2, VT_FLOAT32, False},
            LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}};

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

    // Configuración del PSO usando el helper de TexturedCube y Prism
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
    // Para OpenGL se usan nombres de archivos de shader adecuados (por ejemplo, GLSL)
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

    m_pPSO      = TexturedCube::CreatePipelineState(CubePsoCI, false);
    m_pPSOPrism = Prism::CreatePipelineState(PrismPsoCI, false);

    // Creación del uniform buffer para las constantes del VS
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstants);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstantsPrism);
    m_pPSOPrism->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstantsPrism);
    m_pPSOPrism->CreateShaderResourceBinding(&m_SRBPrism, true);
}

void MovilBebes::CreateInstanceBuffer()
{
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name      = "Instance data buffer";
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(float4x4) * NumInstances;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);
    PopulateInstanceBuffer();
}

void MovilBebes::CreatePrismInstanceBuffer()
{
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name      = "Sphere Instance data buffer";
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(float4x4) * NumPrismInstances;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_PrismInstanceBuffer);
    PopulateInstanceBufferPrism();
}




void MovilBebes::Initialize()
{
    CreatePipelineState();

    // Creación de los buffers e inicialización de recursos del cubo
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_TEX);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);
    m_TextureSRV       = TexturedCube::LoadTexture(m_pDevice, "DGLogo.png")->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);


    // --- NUEVO: Creación de buffers para la esfera/prisma
    m_VertexBufferPrism = Prism::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_TEX);
    m_IndexBufferPrism  = Prism::CreateIndexBuffer(m_pDevice);
    // --- FIN NUEVO

    if (!m_InstanceBuffer)
        CreateInstanceBuffer();

    if (!m_PrismInstanceBuffer)
        CreatePrismInstanceBuffer();

    // Inicializamos la opción de vista (0: Frente, 1: Arriba, 2: Abajo, 3: Izquierda, 4: Derecha)
    m_ViewOption = 0;
}

float angleAxe1 = PI_F / 1.0;
float angleAxeD = PI_F / 1.0;
void  MovilBebes::PopulateInstanceBuffer()
{
    if (!m_InstanceBuffer)
    {
        // El buffer debería estar creado, si no es así, lanza un error o crea el buffer
        return;
    }
    std::vector<float4x4> InstanceData(NumInstances);

    const float4x4 scaleHiloVertical = float4x4::Scale(0.01f, 1.0f, 0.01f);

    // Se definen las transformaciones de traslación para las 5 instancias
    InstanceData[0]  = float4x4::Scale(1, 1, 1) * float4x4::Translation(0.0f, -5.0f, 0.0f) * float4x4::RotationY(angleAxe1);                        // C
    InstanceData[1]  = scaleHiloVertical * float4x4::Translation(0.0f, -1.25f, 0.0f) * float4x4::RotationY(angleAxe1);                              // Hilo vertical 1
    InstanceData[2]  = float4x4::Scale(2, 1, 1) * float4x4::Translation(0.0f, 0.0f, 0.0f);                                                          // A
    InstanceData[3]  = float4x4::Scale(6.0f, 0.01f, 0.01f) * float4x4::Translation(0.0f, -2.25f, 0.0f) * float4x4::RotationY(angleAxe1);            // Hilo horizontal 1
    InstanceData[4]  = scaleHiloVertical * float4x4::Translation(0.0f, -3.0f, 0.0f) * float4x4::RotationY(angleAxe1);                               // Hilo vertical que conecta a C
    InstanceData[5]  = float4x4::Translation(6.0f, -5.0f, 0.0f) * float4x4::RotationY(angleAxe1);                                                   // D
    InstanceData[6]  = scaleHiloVertical * float4x4::Translation(6.0f, -3.25f, 0.0f) * float4x4::RotationY(angleAxe1);                              // Hilo vertical que conecta a D
    InstanceData[8]  = scaleHiloVertical * float4x4::Translation(6.0f, -6.5f, 0.0f) * float4x4::RotationY(angleAxe1);                               // Hilo vertical 2
    InstanceData[9]  = float4x4::Scale(3.0f, 0.01f, 0.01f) * float4x4::RotationY(angleAxeD) * float4x4::Translation(0, -2.5f, 0) * InstanceData[5]; // Hilo horizontal 2
    InstanceData[10] = scaleHiloVertical * float4x4::Translation(-6.0f, -3.25f, 0.0f) * float4x4::RotationY(angleAxe1);                             // Hilo vertical que conecta a B
    InstanceData[11] = scaleHiloVertical * float4x4::Translation(3, -1, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[9];                   // Hilo vertical que conecta a G
    InstanceData[12] = scaleHiloVertical * float4x4::Translation(-3, -1, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[9];                  // Hilo vertical que conecta a H
    InstanceData[13] = float4x4::Translation(3, -2, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[9];                                       // H
    InstanceData[7]  = float4x4::Translation(-3, -2, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[9];                                      // G
    InstanceData[14] = float4x4::Scale(3.0f, 0.01f, 0.01f) * float4x4::RotationY(angleAxeD) * float4x4::Translation(0, -2.5f, 0) *
        float4x4::Translation(-6.0f, -6.2f, 0.0f) * float4x4::RotationY(angleAxe1);                                                 // Hilo horizontal Prismas
    InstanceData[15] = scaleHiloVertical * float4x4::Translation(3, -1, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[14];  // Hilo vertical que conecta a Prisma izquierda
    InstanceData[16] = scaleHiloVertical * float4x4::Translation(-3, -1, 0) * float4x4::Scale(0.333f, 100, 100) * InstanceData[14]; // Hilo vertical que conecta a Prisma derecha
    InstanceData[17] = float4x4::Scale(1, 2, 1) * scaleHiloVertical * float4x4::Translation(-6.0f, -6.75f, 0.0f) * float4x4::RotationY(angleAxe1);

    angleAxe1 += 0.01f;
    angleAxeD += 0.02f;

    Uint32 DataSize = static_cast<Uint32>(sizeof(InstanceData[0]) * InstanceData.size());
    m_pImmediateContext->UpdateBuffer(m_InstanceBuffer, 0, DataSize, InstanceData.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void MovilBebes::PopulateInstanceBufferPrism()
{
    if (!m_PrismInstanceBuffer)
    {
        return;
    }
    std::vector<float4x4> InstanceData(NumPrismInstances);
    InstanceData[0] = float4x4::Scale(1, 1, 1) * float4x4::Translation(-6.0f, -6.0f, 0.0f) * float4x4::RotationY(angleAxe1);
    InstanceData[1] = float4x4::Scale(1, 1, 1) * float4x4::Translation(-3.0f, -6.0f, 0.0f) * float4x4::RotationY(angleAxeD) * InstanceData[0];
    InstanceData[2] = float4x4::Scale(1, 1, 1) * float4x4::Translation(3.0f, -6.0f, 0.0f) * float4x4::RotationY(angleAxeD) * InstanceData[0];

    Uint32 DataSize = static_cast<Uint32>(sizeof(InstanceData[0]) * InstanceData.size());
    m_pImmediateContext->UpdateBuffer(m_PrismInstanceBuffer, 0, DataSize, InstanceData.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void MovilBebes::Render()
{

    // Obtiene los render target views y depth stencil views.
    auto* pRTV       = m_pSwapChain->GetCurrentBackBufferRTV();
    auto*  pDSV       = m_pSwapChain->GetDepthBufferDSV();
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};

    // Asegurarse de que el buffer de instancias se ha creado.
    if (!m_InstanceBuffer)
    {
        CreateInstanceBuffer();
        // Si sigue siendo nulo, se debería abortar o mostrar un error.
        if (!m_InstanceBuffer)
        {
            LOG_ERROR("m_InstanceBuffer es null en Render().");
            return;
        }
    }

    // Actualiza los buffers de instancia
    PopulateInstanceBuffer();
    PopulateInstanceBufferPrism();

    // Limpia los targets
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Actualiza los uniform buffers para el cubo y el prisma
    {
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0] = m_ViewProjMatrix;
        CBConstants[1] = m_RotationMatrix;
    }
    {
        MapHelper<float4x4> CBConstantsPrism(m_pImmediateContext, m_VSConstantsPrism, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstantsPrism[0] = m_ViewProjMatrix;
        CBConstantsPrism[1] = m_RotationMatrix;
    }

    // Para el cubo: pasar 2 buffers (el de vértices y el de instancias)
    const Uint64 offsets[] = {0, 0};
    IBuffer*     pBuffs[]  = {m_CubeVertexBuffer, m_InstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 2, pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(m_pPSO);
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType    = VT_UINT32;
    DrawAttrs.NumIndices   = 36;
    DrawAttrs.NumInstances = NumInstances;
    DrawAttrs.Flags        = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);

    // Para el prisma: similar, se pasan sus buffers correspondientes
    const Uint64 offsets2[]    = {0, 0};
    IBuffer*     pBuffsPrism[] = {m_VertexBufferPrism, m_PrismInstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 2, pBuffsPrism, offsets2, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
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




void MovilBebes::Update(double CurrTime, double ElapsedTime)
{
    float4x4 View;
    switch (m_ViewOption)
    {
        case 0: // Vista Frontal
            View = float4x4::RotationX(0.f) * float4x4::Translation(0.f, 3.f, 30.f);
            break;
        case 1: // Vista desde Arriba
            View = float4x4::RotationX(-1.62f) * float4x4::Translation(0.f, 3.f, 30.f);
            break;
        case 2: // Vista desde Abajo
            View = float4x4::RotationX(1.62f) * float4x4::Translation(0.f, 3.f, 40.f);
            break;
        case 3: // Vista Izquierda
            View = float4x4::RotationY(-1.62f) * float4x4::Translation(0.f, 3.f, 30.f);
            break;
        case 4: // Vista Derecha
            View = float4x4::RotationY(1.62f) * float4x4::Translation(0.f, 3.f, 30.f);
            break;
        default:
            View = float4x4::RotationX(0.f) * float4x4::Translation(0.f, 3.f, 30.f);
    }
    // Usa las funciones de SampleBase para obtener la matriz de pretransformación
    // y la proyección ajustada a la orientación de la pantalla.
    // Asegúrate de incluir el header correspondiente donde se definen estas funciones.
    float4x4 SrfPreTransfo = GetSurfacePretransformMatrix(float3{0.f, 0.f, 1.f});
    float4x4 Proj          = GetAdjustedProjectionMatrix(PI_F / 4.f, 0.1f, 100.f);

    // Calcula la matriz vista-proyección combinando las tres transformaciones.
    m_ViewProjMatrix = float4x4::Mul(Proj, float4x4::Mul(View, SrfPreTransfo));

    // Calcula una matriz de rotación animada (en este ejemplo se rota en Y)
    m_RotationMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 0.5f);
}





} // namespace Diligent
