#include "structures.fxh"
#include "RayUtils.fxh"

ConstantBuffer<CubeAttribs>  g_CubeAttribsCB;

Texture2D    g_CubeTextures[NUM_TEXTURES];
SamplerState g_SamLinearWrap;

[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Calcular coordenadas baricéntricas
    float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    // Obtener índices de los vértices del triángulo
    uint3 primitive = g_CubeAttribsCB.Primitives[PrimitiveIndex()].xyz;

    // Calcular coordenadas de textura
    float2 uv = g_CubeAttribsCB.UVs[primitive.x].xy * barycentrics.x +
                g_CubeAttribsCB.UVs[primitive.y].xy * barycentrics.y +
                g_CubeAttribsCB.UVs[primitive.z].xy * barycentrics.z;

    // Calcular y transformar la normal del triángulo
    float3 normal = g_CubeAttribsCB.Normals[primitive.x].xyz * barycentrics.x +
                    g_CubeAttribsCB.Normals[primitive.y].xyz * barycentrics.y +
                    g_CubeAttribsCB.Normals[primitive.z].xyz * barycentrics.z;
    normal = normalize(mul((float3x3) ObjectToWorld3x4(), normal));

    // Aplicar la textura pero reduciendo su influencia al mínimo
    float3 textureColor = g_CubeTextures[NonUniformResourceIndex(InstanceID())].SampleLevel(g_SamLinearWrap, uv, 0).rgb * 0.0001;

    // Color base influenciado por la iluminación ambiental
    float3 baseColor = float3(0.3, 0.3, 0.4); // Color base similar al de la imagen
    float3 ambientLight = float3(0.5, 0.5, 0.6); // Luz ambiental

    // Combinar el color base con la textura casi anulada
    payload.Color = baseColor * ambientLight + textureColor;
    payload.Depth = RayTCurrent();

    // Aplicar iluminación
    float3 rayOrigin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    LightingPass(payload.Color, rayOrigin, normal, payload.Recursion + 1);
}
