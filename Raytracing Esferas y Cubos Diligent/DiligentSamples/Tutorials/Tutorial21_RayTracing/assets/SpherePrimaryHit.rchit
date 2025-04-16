#include "structures.fxh"
#include "RayUtils.fxh" // Asegúrate que tenga FresnelSchlick y CastPrimaryRay




[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in ProceduralGeomIntersectionAttribs attr)
{
    uint instanceID = InstanceID();
    SphereMaterialData material = g_SphereMaterials[instanceID];

    // Vectores y Normal
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 normal = normalize(mul((float3x3) ObjectToWorld3x4(), attr.Normal));
    // Asegúrate que viewDir apunte DESDE el punto de hit HACIA la cámara/origen del rayo
    float3 viewDir = normalize(WorldRayOrigin() - hitPos);
    float3 incidentDir = WorldRayDirection(); // Dirección del rayo que llega

    // --- 1. Calcular Reflejo (Siempre necesario para Fresnel y metálicos) ---
    float3 reflectedRayDir = reflect(incidentDir, normal);
    RayDesc reflectionRay;
    // Origen ligeramente fuera de la superficie
    reflectionRay.Origin = hitPos + normal * SMALL_OFFSET;
    reflectionRay.TMin   = 0.0;
    reflectionRay.TMax   = 100.0; // O g_ConstantsCB.ClipPlanes.y

    // Calcular color reflejado trazando rayos (considerando roughness)
    int numReflectionRays = max(1, min(8, (int)(1.0 + material.Roughness * 7.0)));
    if (payload.Recursion > 1) { numReflectionRays = 1; }
    float3 tracedReflectionColor = float3(0.0, 0.0, 0.0);
    float coneSpread = material.Roughness * 0.1;
    for (int j = 0; j < numReflectionRays; ++j)
    {
        int pointIndex = j % 8;
        float2 offset = float2(g_ConstantsCB.DiscPoints[pointIndex / 2][(pointIndex % 2) * 2],
                               g_ConstantsCB.DiscPoints[pointIndex / 2][(pointIndex % 2) * 2 + 1]);
        reflectionRay.Direction = DirectionWithinCone(reflectedRayDir, offset * coneSpread);
        PrimaryRayPayload reflectionPayload = CastPrimaryRay(reflectionRay, payload.Recursion + 1);
        tracedReflectionColor += reflectionPayload.Color;
    }
    tracedReflectionColor /= float(numReflectionRays);


    // --- 2. Calcular Fresnel y Decidir entre Reflexión/Refracción ---
    // F0: Reflectancia base (dieléctrico ~0.04, metálico usa BaseColor)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), material.BaseColor.rgb, material.Metallic);
    // Coseno del ángulo de incidencia (o visión, son el mismo ángulo respecto a la normal)
    float cosTheta = max(dot(viewDir, normal), 0.0); // O usar dot(-incidentDir, normal)
    // Factor Fresnel: cuánta luz se refleja
    float3 Fresnel = FresnelSchlick(cosTheta, F0);


    // --- 3. Calcular Refracción (si no es muy metálico) ---
    float3 refractionColor = float3(0,0,0); // Color por defecto si no hay refracción
    bool tir = false; // Flag para Reflexión Interna Total

    // Solo calcular refracción para materiales predominantemente dieléctricos
    if (material.Metallic < 0.9) // Umbral ajustable
    {
        float IOR_Air = 1.0;
        float eta = IOR_Air / material.IndexOfRefraction; // Ratio IOR Aire -> Material
        // Calcular dirección refractada
        float3 refractedRayDir = refract(incidentDir, normal, eta);

        // Comprobar Reflexión Interna Total (TIR)
        if (dot(refractedRayDir, refractedRayDir) == 0.0) // refract devuelve (0,0,0) en TIR
        {
            tir = true;
        }
        else
        {
            // Trazar el rayo refractado
            RayDesc refractionRay;
            // Origen ligeramente DENTRO de la superficie
            refractionRay.Origin = hitPos - normal * SMALL_OFFSET;
            refractionRay.Direction = normalize(refractedRayDir); // Asegurarse que esté normalizado
            refractionRay.TMin   = 0.0;
            refractionRay.TMax   = 100.0; // O g_ConstantsCB.ClipPlanes.y

            // Lanzar el rayo (podríamos necesitar un payload diferente si manejamos absorción)
            PrimaryRayPayload refractionPayload = CastPrimaryRay(refractionRay, payload.Recursion + 1);
            refractionColor = refractionPayload.Color;

            // Opcional: Aplicar tinte/absorción basado en BaseColor (muy simple)
            // refractionColor *= material.BaseColor.rgb;
        }
    }


    // --- 4. Combinar Resultados ---
    float3 finalColor;

    if (tir)
    {
        // Si hay TIR, toda la luz se refleja
        finalColor = tracedReflectionColor * Fresnel; // Fresnel debería ser cercano a 1 aquí
    }
    else
    {
        // Mezclar reflexión y refracción basado en Fresnel
        // Fresnel.r (o .g o .b) indica la proporción de reflexión
        finalColor = lerp(refractionColor, tracedReflectionColor, Fresnel.r);

        // Para materiales muy metálicos, forzar más reflexión (opcional, Fresnel ya lo hace en gran medida)
        // finalColor = lerp(finalColor, tracedReflectionColor * Fresnel, material.Metallic);
    }

    // Alternativa más simple (ignora Fresnel para la mezcla, solo usa Metallic):
    // finalColor = lerp(refractionColor, tracedReflectionColor * F0, material.Metallic); // Usa F0 para el tinte metálico

    // Asignar resultado final
    payload.Color = finalColor;
    payload.Depth = RayTCurrent();
}