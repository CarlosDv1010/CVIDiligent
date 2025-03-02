#pragma once
#include "../../Common/src/TexturedCube.hpp"
#include "Prism.hpp"
#include "imgui.h"
#include "SampleBase.hpp"

namespace Diligent
{

class MovilBebes
{
public:
    void SetAttributes(RefCntAutoPtr<IRenderDevice> m_pDevice, RefCntAutoPtr<IDeviceContext> m_pImmediateContext, RefCntAutoPtr<ISwapChain> pSwapChain, RefCntAutoPtr<IEngineFactory> m_pEngineFactory);
    void SetInstanceBuffers(RefCntAutoPtr<IBuffer> InstanceBuffer, RefCntAutoPtr<IBuffer> PrismInstanceBuffer, RefCntAutoPtr<IRenderDevice> m_pDeviceE);
    MovilBebes();
    virtual void        Initialize();
    virtual void        Render();
    virtual void        Update(double CurrTime, double ElapsedTime);
    virtual const Char* GetSampleName() const { return "Tutorial04: Instancing"; }
    // Returns projection matrix adjusted to the current screen orientation
    float4x4 GetAdjustedProjectionMatrix(float FOV, float NearPlane, float FarPlane) const;

    // Returns pretransform matrix that matches the current screen rotation
    float4x4 GetSurfacePretransformMatrix(const float3& f3CameraViewAxis) const;

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
    RefCntAutoPtr<ITextureView>           m_TextureSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB;

    // Recursos para la esfera
    RefCntAutoPtr<IPipelineState>         m_pPSOPrism;
    RefCntAutoPtr<IBuffer>                m_VertexBufferPrism;
    RefCntAutoPtr<IBuffer>                m_IndexBufferPrism;
    RefCntAutoPtr<IBuffer>                m_PrismInstanceBuffer;
    RefCntAutoPtr<ITextureView>           m_TextureSRVPrism;
    RefCntAutoPtr<IShaderResourceBinding> m_SRBPrism;

    RefCntAutoPtr<IEngineFactory> m_pEngineFactory;
    RefCntAutoPtr<IDeviceContext> m_pImmediateContext;
    RefCntAutoPtr<IRenderDevice>  m_pDevice;
    RefCntAutoPtr<IBuffer> m_VSConstants;
    RefCntAutoPtr<IBuffer> m_VSConstantsPrism;
    RefCntAutoPtr<ISwapChain>     m_pSwapChain;
    float4x4               m_ViewProjMatrix;
    float4x4               m_RotationMatrix;
    int   NumInstances = 18;
    int   NumPrismInstances = 3;
    int                    m_ViewOption      = 0;
};

} // namespace Diligent