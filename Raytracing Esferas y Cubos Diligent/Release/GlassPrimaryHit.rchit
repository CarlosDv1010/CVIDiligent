#include "structures.fxh" // Asegúrate que define CubeMaterialData y PrimaryRayPayload
#include "RayUtils.fxh"   // Asegúrate que define FresnelSchlick, CastPrimaryRay, DirectionWithinCone, SMALL_OFFSET, etc.

// Buffer con los datos de material para cada instancia de cubo
StructuredBuffer<CubeMaterialData> g_CubeMaterials;

// Buffer con los atributos geométricos del cubo (vértices, normales, índices)
// (Asumo que sigue siendo necesario como en tu shader original)
ConstantBuffer<CubeAttribs>  g_CubeAttribsCB;

// Constantes globales (si RayUtils.fxh no las define o necesitas otras)
// cbuffer GlobalConstants : register(b0) // Ajusta el registro si es necesario
// {
//     float4x4 ViewProjInv;
//     float4   CameraPosition;
//     // ... otras constantes ...
//     float2 DiscPoints[4]; // Para reflejos rugosos si RayUtils no lo tiene
// }
// Define IOR del aire si no está en otro sitio
#define IOR_Air 1.0f

[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // --- 1. Obtener Datos de la Instancia y Geometría ---

    // Obtener el ID de la instancia actual
    uint instanceID = InstanceID();
    // Obtener el material específico para esta instancia de cubo
    CubeMaterialData material = g_CubeMaterials[instanceID];

    // Calcular baricéntricas (ya las tienes)
    float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    // Obtener índices de vértices del triángulo golpeado
    uint3 primitive = g_CubeAttribsCB.Primitives[PrimitiveIndex()].xyz;

    // Interpolar y transformar la normal del vértice a espacio del mundo
    float3 objectNormal = g_CubeAttribsCB.Normals[primitive.x].xyz * barycentrics.x +
                          g_CubeAttribsCB.Normals[primitive.y].xyz * barycentrics.y +
                          g_CubeAttribsCB.Normals[primitive.z].xyz * barycentrics.z;
    float3 normal = normalize(mul((float3x3) ObjectToWorld3x4(), objectNormal));

    // Calcular posición del hit en espacio del mundo
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // Dirección del rayo incidente
    float3 incidentDir = WorldRayDirection();

    // Dirección hacia la cámara/origen del rayo (desde el punto de hit)
    float3 viewDir = normalize(WorldRayOrigin() - hitPos); // O -incidentDir si prefieres

    // Determinar si estamos golpeando una cara frontal o trasera
    // Si el rayo viene de fuera, dot(incidentDir, normal) < 0 para cara frontal
    // Si el rayo viene de dentro, dot(incidentDir, normal) > 0 para cara frontal (que ahora es trasera desde la perspectiva del rayo)
    bool hittingFrontFace = dot(incidentDir, normal) < 0.0;
    float3 shadingNormal = hittingFrontFace ? normal : -normal; // Normal a usar para shading/reflexión/refracción

    // --- 2. Calcular Reflejo (Siempre necesario para Fresnel y metálicos) ---
    float3 reflectedRayDir = reflect(incidentDir, shadingNormal);
    RayDesc reflectionRay;
    // Origen ligeramente fuera de la superficie para evitar auto-intersección
    reflectionRay.Origin = hitPos + shadingNormal * SMALL_OFFSET;
    reflectionRay.TMin   = 0.0; // Empezar justo después del origen
    reflectionRay.TMax   = 100.0; // Distancia máxima del rayo (ajustar si es necesario)

    // Calcular color reflejado trazando rayos (considerando roughness)
    // Usamos una lógica similar a la esfera para reflejos rugosos
    int numReflectionRays = 1; // Por defecto para superficies lisas o recursión profunda
    if (material.Roughness > 0.01 && payload.Recursion <= 2) // Limitar rayos/recursión
    {
       numReflectionRays = max(1, min(8, (int)(1.0 + material.Roughness * 7.0)));
    }

    float3 tracedReflectionColor = float3(0.0, 0.0, 0.0);
    // Ajusta el cono según la rugosidad. Un valor mayor dispersa más.
    float coneSpread = material.Roughness * material.Roughness * 0.5; // Experimenta con esta fórmula

    for (int j = 0; j < numReflectionRays; ++j)
    {
        // Genera direcciones dentro de un cono alrededor de la dirección de reflejo perfecta
        // Necesitas una función como DirectionWithinCone o similar en RayUtils.fxh
        // Si no la tienes, puedes usar una aproximación simple o implementarla.
        // Ejemplo usando puntos en disco (si los tienes definidos como en el shader de esfera):
        // int pointIndex = j % 8;
        // float2 offset = float2(g_ConstantsCB.DiscPoints[pointIndex / 2][(pointIndex % 2) * 2],
        //                        g_ConstantsCB.DiscPoints[pointIndex / 2][(pointIndex % 2) * 2 + 1]);
        // reflectionRay.Direction = DirectionWithinCone(reflectedRayDir, offset * coneSpread);

        // Alternativa simple si no tienes DirectionWithinCone (menos precisa):
        // Genera un vector aleatorio o pseudoaleatorio y mézclalo con reflectedRayDir
        // float3 randomDir = RandomDirectionAround(reflectedRayDir, coneSpread, j); // Necesitarías implementar esto
        // reflectionRay.Direction = normalize(lerp(reflectedRayDir, randomDir, material.Roughness));

        // Si no tienes nada para rugosidad, usa la dirección perfecta:
        reflectionRay.Direction = reflectedRayDir; // Simplificación si no hay manejo de rugosidad en el trazado

        // Trazar el rayo de reflejo
        PrimaryRayPayload reflectionPayload = CastPrimaryRay(reflectionRay, payload.Recursion + 1);
        tracedReflectionColor += reflectionPayload.Color;
    }
    tracedReflectionColor /= float(numReflectionRays);


    // --- 3. Calcular Fresnel y Decidir entre Reflexión/Refracción ---
    // F0: Reflectancia base en incidencia normal.
    // Dieléctricos puros ~0.04. Metales usan su color base.
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), material.BaseColor.rgb, material.Metallic);

    // Coseno del ángulo entre la dirección de vista y la normal de shading
    // Usamos abs() o max(0,..) porque Fresnel depende del ángulo, no de la dirección exacta
    float cosTheta = max(dot(viewDir, shadingNormal), 0.0); // O usar dot(-incidentDir, shadingNormal)

    // Factor Fresnel: cuánta luz se refleja especularmente en este ángulo
    float3 Fresnel = FresnelSchlick(cosTheta, F0);


    // --- 4. Calcular Refracción (si no es completamente metálico y tiene IOR > 1) ---
    float3 refractionColor = float3(0.0, 0.0, 0.0); // Color por defecto si no hay refracción válida
    bool tir = false; // Flag para Reflexión Interna Total

    // Solo calcular refracción para materiales no metálicos o parcialmente metálicos
    // y que tengan un índice de refracción diferente al aire
    if (material.Metallic < 0.95 && material.IndexOfRefraction > 1.001) // Umbrales ajustables
    {
        float eta; // Ratio de IORs (desde / hacia)
        float3 refractionNormal = normal; // Normal geométrica original

        if (hittingFrontFace) // Rayo entra al material (Aire -> Material)
        {
            eta = IOR_Air / material.IndexOfRefraction;
            refractionNormal = normal; // Usar normal original
        }
        else // Rayo sale del material (Material -> Aire)
        {
            eta = material.IndexOfRefraction / IOR_Air;
            refractionNormal = -normal; // Invertir normal porque salimos
        }

        // Calcular dirección del rayo refractado
        // Nota: refract necesita la normal apuntando hacia el lado de donde viene el rayo incidente
        float3 refractedRayDir = refract(incidentDir, refractionNormal, eta);

        // Comprobar Reflexión Interna Total (TIR)
        // refract devuelve (0,0,0) si ocurre TIR
        if (dot(refractedRayDir, refractedRayDir) < 0.001) // Comparación segura con cero
        {
            tir = true;
        }
        else
        {
            // Trazar el rayo refractado
            RayDesc refractionRay;
            // Origen ligeramente DENTRO de la superficie (o fuera si salimos)
            // Moverse en la dirección opuesta a la normal de refracción usada
            refractionRay.Origin = hitPos - refractionNormal * SMALL_OFFSET;
            refractionRay.Direction = normalize(refractedRayDir); // Asegurarse que esté normalizado
            refractionRay.TMin   = 0.0;
            refractionRay.TMax   = 100.0; // Ajustar si es necesario

            // Lanzar el rayo de refracción
            PrimaryRayPayload refractionPayload = CastPrimaryRay(refractionRay, payload.Recursion + 1);
            refractionColor = refractionPayload.Color;

            // Opcional: Aplicar color base como tinte de absorción (simplificado)
            // Solo si el rayo entró al material (hittingFrontFace)
            // Una absorción más realista dependería de la distancia recorrida dentro.
            if (hittingFrontFace) {
                 // Podrías usar Beer's Law si tienes la distancia, o un tinte simple:
                 // refractionColor *= material.BaseColor.rgb; // Tinte simple
            }
        }
    }


    // --- 5. Combinar Resultados ---
    float3 finalColor;

    if (tir)
    {
        // Si hay TIR, toda la luz se refleja (usando el color ya trazado)
        finalColor = tracedReflectionColor;
        // Podríamos multiplicar por Fresnel aquí también, aunque debería ser cercano a 1.
        // finalColor = tracedReflectionColor * Fresnel;
    }
    else
    {
        // Mezclar reflexión y refracción basado en Fresnel para dieléctricos/parcialmente metálicos
        // La componente de refracción (1.0 - Fresnel) se modula por el color base (albedo)
        // La componente de reflexión (Fresnel) se modula por el color especular (F0 o blanco)
        // Para metales, la refracción es casi nula y Fresnel es alto y coloreado (F0)

        // Color refractado/difuso (simplificado como color base * componente no reflejada)
        // Para dieléctricos, esto representa la luz que entra y potencialmente se dispersa o absorbe.
        // Para metales, este término debería ser casi cero.
        float3 diffuseComponent = material.BaseColor.rgb * refractionColor * (1.0 - material.Metallic);

        // Color reflejado especularmente (usando el color trazado)
        float3 specularComponent = tracedReflectionColor; // El color del reflejo ya incluye el entorno

        // Combinar usando Fresnel como factor de mezcla entre lo que se refleja especularmente
        // y lo que entra/se dispersa (para dieléctricos) o se absorbe (para metales).
        // Usamos Fresnel.r como aproximación del factor de mezcla.
        finalColor = lerp(diffuseComponent, specularComponent, Fresnel.r);

        // Otra forma común es mezclar basado en metalicidad primero:
        // float3 dielectricColor = refractionColor * (1.0 - Fresnel.r) + tracedReflectionColor * Fresnel.r;
        // float3 metallicColor = tracedReflectionColor * F0; // O simplemente tracedReflectionColor * Fresnel;
        // finalColor = lerp(dielectricColor, metallicColor, material.Metallic);

        // La forma más PBR-like simple podría ser:
        // float3 F = FresnelSchlick(cosTheta, F0);
        // float3 kS = F; // Specular reflection coefficient
        // float3 kD = (1.0 - kS) * (1.0 - material.Metallic); // Diffuse/Refraction coefficient (reduced by metallic)
        // finalColor = (kD * material.BaseColor.rgb * refractionColor) + (kS * tracedReflectionColor); // refractionColor aquí sería más bien la luz incidente si no trazáramos refracción. Como la trazamos, la usamos.

        // Vamos a usar una mezcla simple basada en Fresnel y Metallic que funcione razonablemente:
        // Combina la refracción (tintada por BaseColor para dieléctricos) y la reflexión (pura o tintada por F0 para metales)
        float3 nonMetallicRefraction = refractionColor * material.BaseColor.rgb; // Lo que entra y se tiñe (simplificado)
        // Mezcla entre refracción tintada y reflejo basado en Fresnel
        float3 dielectricBRDF = lerp(nonMetallicRefraction, tracedReflectionColor, Fresnel);
        // Mezcla final entre comportamiento dieléctrico y metálico (reflejo puro tintado por F0)
        finalColor = lerp(dielectricBRDF, tracedReflectionColor * F0, material.Metallic); // F0 tiñe el reflejo metálico

    }

    // Asignar resultado final al payload
    payload.Color = finalColor;
    payload.Depth = RayTCurrent(); // Profundidad del hit actual
}