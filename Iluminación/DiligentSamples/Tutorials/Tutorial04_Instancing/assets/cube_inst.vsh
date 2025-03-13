cbuffer Constants
{
    float4x4 g_ViewProj; // Matriz vista-proyección para la cámara
    float4x4 g_Rotation; // Transformación global (si se utiliza)
    float4 g_LightDirection; // Dirección de la luz (se deja para referencia)
    float4x4 g_WorldToShadow; // Matriz que transforma la posición en mundo al espacio del shadow map (debe incluir conversión a [0,1])
};

struct VSInput
{
    // Atributos del vértice (en espacio objeto)
    float3 Pos : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 UV : ATTRIB2;

    // Atributos de la instancia (cada instancia ya transforma a coordenadas globales)
    float4 MtrxRow0 : ATTRIB3;
    float4 MtrxRow1 : ATTRIB4;
    float4 MtrxRow2 : ATTRIB5;
    float4 MtrxRow3 : ATTRIB6;
    float TexArrInd : ATTRIB7;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEX_COORD;
    float TexIndex : TEX_ARRAY_INDEX;
    float3 WorldPos : POSITION0; // Usamos POSITION0 para WorldPos
    float3 Normal : NORMAL; // Usamos NORMAL para la normal en mundo
    float4 ShadowMapPos : TEXCOORD3; // Coordenadas para el shadow map
};

void main(in VSInput VSIn,
          out PSInput PSIn)
{
    // Construir la matriz de transformación de la instancia.
    float4x4 InstanceMatr = MatrixFromRows(VSIn.MtrxRow0, VSIn.MtrxRow1, VSIn.MtrxRow2, VSIn.MtrxRow3);

    // Transformar la posición del vértice a espacio mundial.
    float4 worldPos = mul(float4(VSIn.Pos, 1.0), InstanceMatr);
    PSIn.WorldPos = worldPos.xyz;

    // Calcular la posición en clip space para la cámara.
    PSIn.Pos = mul(worldPos, g_ViewProj);

    // Pasar las coordenadas de textura y el índice.
    PSIn.UV = VSIn.UV;
    PSIn.TexIndex = VSIn.TexArrInd;

    // Transformar la normal a espacio mundial.
    float3 worldNormal = mul(VSIn.Normal, (float3x3) InstanceMatr);
    PSIn.Normal = normalize(worldNormal);

    // Calcular las coordenadas del shadow map:
    // Se transforma la posición en mundo al espacio de sombra usando g_WorldToShadow.
    float4 shadowPos = mul(worldPos, g_WorldToShadow);
    PSIn.ShadowMapPos = shadowPos;
}
