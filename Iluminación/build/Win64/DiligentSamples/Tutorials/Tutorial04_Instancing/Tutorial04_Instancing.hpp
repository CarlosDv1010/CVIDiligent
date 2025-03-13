#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "FirstPersonCamera.hpp" // Para la navegación libre

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
    void CreateShadowPSO();
    void CreateShadowMapVisPSO();
    void CreatePipelineState();
    void CreateInstanceBuffer();
    void PopulateInstanceBuffer();
    void PopulateInstanceBufferPrism();
    void RenderShadowMap();
    void RenderShadowMapVis();
    void Render_Cube(const float4x4& CameraViewProj, bool IsShadowPass);
    void CreatePrismInstanceBuffer();

    void CreateShadowMap();

    void InitializeResourceBindings();

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
    RefCntAutoPtr<IBuffer> m_VSConstantsShadowMap;
    RefCntAutoPtr<IBuffer> m_PSConstants;
    RefCntAutoPtr<IBuffer> m_VSConstantsPrism;
    float4x4               m_ViewProjMatrix;
    float4x4               m_RotationMatrix;
    static constexpr int   NumInstances      = 19;
    static constexpr int   NumPrismInstances = 3;
    int                    m_ViewOption      = 0;

    RefCntAutoPtr<ISampler> m_pComparisonSampler;
    std::vector<RefCntAutoPtr<IShaderResourceBinding>> m_ShadowSRBs;
    RefCntAutoPtr<IBuffer>                             m_LightAttribsCB;
    std::vector<RefCntAutoPtr<IPipelineState>>         m_RenderMeshShadowPSO;
    RefCntAutoPtr<ISampler>                            m_pFilterableShadowMapSampler;
    TEXTURE_FORMAT                                     m_ShadowMapFormat = TEX_FORMAT_D16_UNORM;
    RefCntAutoPtr<IPipelineState>                      m_pCubeShadowPSO;
    RefCntAutoPtr<IShaderResourceBinding>              m_CubeShadowSRB;
    RefCntAutoPtr<IShaderResourceBinding>              m_ShadowMapVisSRB;
    RefCntAutoPtr<ITextureView>                        m_ShadowMapDSV;
    RefCntAutoPtr<ITextureView>                        m_ShadowMapSRV;
    RefCntAutoPtr<IPipelineState>                      m_pShadowMapVisPSO;
    float4x4                                           m_WorldToShadowMapUVDepthMatr;
    float3                                             m_LightDirection = normalize(float3(0.0, -4.00f, 0.00f));
    float4                                             m_specMask;
    float                                              m_specPower = 1.0f;




    // --- NUEVO: Variables para el modo de cámara libre
    int               m_CameraMode        = 0; // 0: Estático (usa m_ViewOption), 1: Libre (navegación con FirstPersonCamera)
    int               m_CameraSpeedOption = 1; // 0: Lento, 1: Normal, 2: Rápido
    float               m_FOV               = 60;
    FirstPersonCamera m_Camera;
    MouseState        m_LastMouseState;
    // --- FIN NUEVO
};

} // namespace Diligent