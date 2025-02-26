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
#include "FirstPersonCamera.hpp" // Se incluye para la navegación libre

namespace Diligent
{

// Función de fábrica para crear la muestra
SampleBase* CreateSample()
{
    return new Tutorial04_Instancing();
}

void Tutorial04_Instancing::CreatePipelineState()
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

    m_pPSO      = TexturedCube::CreatePipelineState(CubePsoCI, m_ConvertPSOutputToGamma);
    m_pPSOPrism = Prism::CreatePipelineState(PrismPsoCI, m_ConvertPSOutputToGamma);

    // Creación del uniform buffer para las constantes del VS
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstants);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstantsPrism);
    m_pPSOPrism->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstantsPrism);
    m_pPSOPrism->CreateShaderResourceBinding(&m_SRBPrism, true);
}

void Tutorial04_Instancing::CreateInstanceBuffer()
{
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name      = "Instance data buffer";
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(float4x4) * NumInstances;
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
// --- FIN NUEVO

void Tutorial04_Instancing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
    CreatePipelineState();

    // Creación de los buffers e inicialización de recursos del cubo
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_TEX);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);
    m_TextureSRV       = TexturedCube::LoadTexture(m_pDevice, "DGLogo.png")->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);

    CreateInstanceBuffer();

    // --- NUEVO: Creación de buffers para la esfera/prisma
    m_VertexBufferPrism = Prism::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_TEX);
    m_IndexBufferPrism  = Prism::CreateIndexBuffer(m_pDevice);
    CreatePrismInstanceBuffer();
    // --- FIN NUEVO

    // Inicializamos la opción de vista (0: Frente, 1: Arriba, 2: Abajo, 3: Izquierda, 4: Derecha)
    m_ViewOption = 0;

    // --- NUEVO: Inicializamos la cámara libre
    m_CameraMode        = 0; // 0: Estático, 1: Libre
    m_CameraSpeedOption = 1; // 0: Lento, 1: Normal, 2: Rápido
    m_Camera.SetPos(float3(0.f, 0.f, 30.0f));
    m_Camera.SetLookAt(float3(0.f, 0.f, 0.f));
    m_Camera.SetProjAttribs(0.1f, 100.f, 1, 0.6f, SURFACE_TRANSFORM_IDENTITY, false);
}

float angleAxe1 = PI_F / 1.0;
float angleAxeD = PI_F / 1.0;
void  Tutorial04_Instancing::PopulateInstanceBuffer()
{
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
        // Actualización del uniform buffer para el cubo
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0] = m_ViewProjMatrix;
        CBConstants[1] = m_RotationMatrix;
    }
    {
        // Actualización del uniform buffer para el prisma
        MapHelper<float4x4> CBConstantsPrism(m_pImmediateContext, m_VSConstantsPrism, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstantsPrism[0] = m_ViewProjMatrix;
        CBConstantsPrism[1] = m_RotationMatrix;
    }

    const Uint64 offsets[] = {0, 0};
    IBuffer*     pBuffs[]  = {m_CubeVertexBuffer, m_InstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(m_pPSO);
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType    = VT_UINT32;
    DrawAttrs.NumIndices   = 36;
    DrawAttrs.NumInstances = NumInstances;
    DrawAttrs.Flags        = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);

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


// Función Update corregida con la velocidad de rotación de la cámara basada en ImGui
void Tutorial04_Instancing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    // Se usa m_FOV para la proyección (convertido a radianes)
    auto SrfPreTransform  = GetSurfacePretransformMatrix(float3{0, 0, 1});
    auto Proj             = GetAdjustedProjectionMatrix(m_FOV * PI_F / 180.0f, 0.1f, 100.f);
    auto SrfPreTransform2 = m_pSwapChain->GetDesc().PreTransform;
    m_Camera.SetMoveSpeed(10.0f);
    // --- NUEVO: Ventana de UI condicional
    ImGui::Begin("Vista del movil para bebes");
    {
        // Siempre se muestra el combo para elegir el modo de cámara
        const char* cameraModes[] = {"Estatico", "Libre"};
        ImGui::Combo("Modo Camara", &m_CameraMode, cameraModes, IM_ARRAYSIZE(cameraModes));

        if (m_CameraMode == 0) // Modo estático: sólo se muestra la vista
        {
            const char* views[] = {"Frente", "Arriba", "Abajo", "Izquierda", "Derecha"};
            ImGui::Combo("Vista", &m_ViewOption, views, IM_ARRAYSIZE(views));
        }
        else if (m_CameraMode == 1) // Modo libre: se muestran velocidad y FOV
        {
            const char* speedOptions[] = {"Lento", "Normal", "Rapido"};
            ImGui::Combo("Velocidad Camara", &m_CameraSpeedOption, speedOptions, IM_ARRAYSIZE(speedOptions));
            ImGui::SliderFloat("FOV", &m_FOV, 30.0f, 120.0f, "FOV: %.1f deg");
        }
    }
    ImGui::End();
    // --- FIN NUEVO

    // Ajustar la velocidad de rotación según la opción seleccionada en ImGui
    float rotationSpeed = 0.00005f; // Valor por defecto
    switch (m_CameraSpeedOption)
    {
        case 0: rotationSpeed = 0.00005f; break; // Lento
        case 1: rotationSpeed = 0.0005f; break;  // Normal
        case 2: rotationSpeed = 0.001f; break;   // Rapido
        default: rotationSpeed = 0.00005f; break;
    }

    m_Camera.SetRotationSpeed(rotationSpeed);

    if (m_CameraMode == 1) // Modo cámara libre
    {
        m_Camera.SetProjAttribs(0.1f, 100.f, 1 , m_FOV * PI_F / 180.0f, SURFACE_TRANSFORM_IDENTITY, false);
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
    else // Modo cámara estática con vistas predefinidas
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