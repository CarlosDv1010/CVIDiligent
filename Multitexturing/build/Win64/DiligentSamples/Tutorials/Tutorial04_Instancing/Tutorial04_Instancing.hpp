#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "FirstPersonCamera.hpp" // Para la navegación libre
#include "../../../DiligentFX/Components/interface/ShadowMapManager.hpp"

namespace Diligent
{

class Tutorial04_Instancing final : public SampleBase
{
public:
    void                LoadTextures();
    virtual void        Initialize(const SampleInitInfo& InitInfo) override final;
    virtual void        Render() override final;
    virtual void        Update(double CurrTime, double ElapsedTime) override final;
    virtual const Char* GetSampleName() const override final { return "Tutorial04: Instancing"; }

private:
    // Funciones para configurar el pipeline y buffers de instancias
    void CreatePipelineState();
    void CreateInstanceBuffer();
    void PopulateInstanceBuffer();
    void PopulateInstanceBufferPrism();
    // --- NUEVO: Creación del buffer de instancia para la esfera
    void CreatePrismInstanceBuffer();
    // --- FIN NUEVO

    // Recursos para los cubos
    RefCntAutoPtr<IPipelineState>         m_pPSO;
    RefCntAutoPtr<IBuffer>                m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>                m_InstanceBuffer;
    RefCntAutoPtr<ITextureView>           m_TextureGrassFlowers;
    RefCntAutoPtr<ITextureView>           m_TextureGrassy;
    RefCntAutoPtr<ITextureView>           m_TextureMud;
    RefCntAutoPtr<ITextureView>           m_TexturePath;
    RefCntAutoPtr<ITextureView>           m_TextureBlendMap;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB;

    // Recursos para la esfera
    RefCntAutoPtr<IPipelineState>         m_pPSOPrism;
    RefCntAutoPtr<IBuffer>                m_VertexBufferPrism;
    RefCntAutoPtr<IBuffer>                m_IndexBufferPrism;
    RefCntAutoPtr<IBuffer>                m_PrismInstanceBuffer;
    RefCntAutoPtr<ITextureView>           m_TextureSRVPrism;
    RefCntAutoPtr<IShaderResourceBinding> m_SRBPrism;

    RefCntAutoPtr<IBuffer> m_VSConstants;
    RefCntAutoPtr<IBuffer> m_VSConstantsPrism;
    float4x4               m_ViewProjMatrix;
    float4x4               m_RotationMatrix;
    static constexpr int   NumInstances      = 18;
    static constexpr int   NumPrismInstances = 3;
    int                    m_ViewOption      = 0;

    // --- NUEVO: Variables para el modo de cámara libre
    int               m_CameraMode        = 0; // 0: Estático (usa m_ViewOption), 1: Libre (navegación con FirstPersonCamera)
    int               m_CameraSpeedOption = 1; // 0: Lento, 1: Normal, 2: Rápido
    float               m_FOV               = 60;
    FirstPersonCamera m_Camera;
    MouseState        m_LastMouseState;
    // --- FIN NUEVO
};

} // namespace Diligent