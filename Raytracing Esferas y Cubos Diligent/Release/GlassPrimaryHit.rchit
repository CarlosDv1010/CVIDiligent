#include "structures.fxh" // Aseg�rate que define CubeMaterialData y PrimaryRayPayload
#include "RayUtils.fxh"   // Aseg�rate que define FresnelSchlick, CastPrimaryRay, DirectionWithinCone, SMALL_OFFSET, etc.

// Buffer con los datos de material para cada instancia de cubo
StructuredBuffer<CubeMaterialData> g_CubeMaterials;

// Buffer con los atributos geom�tricos del cubo (v�rtices, normales, �ndices)
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
// Define IOR del aire si no est� en otro sitio
#define IOR_Air 1.0f

[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // --- 1. Obtener Datos de la Instancia y Geometr�a ---

    // Obtener el ID de la instancia actual
    uint instanceID = InstanceID();
    // Obtener el material espec�fico para esta instancia de cubo
    CubeMaterialData material = g_CubeMaterials[instanceID];

    // Calcular baric�ntricas (ya las tienes)
    float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    // Obtener �ndices de v�rtices del tri�ngulo golpeado
    uint3 primitive = g_CubeAttribsCB.Primitives[PrimitiveIndex()].xyz;

    // Interpolar y transformar la normal del v�rtice a espacio del mundo
    float3 objectNormal = g_CubeAttribsCB.Normals[primitive.x].xyz * barycentrics.x +
                          g_CubeAttribsCB.Normals[primitive.y].xyz * barycentrics.y +
                          g_CubeAttribsCB.Normals[primitive.z].xyz * barycentrics.z;
    float3 normal = normalize(mul((float3x3) ObjectToWorld3x4(), objectNormal));

    // Calcular posici�n del hit en espacio del mundo
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // Direcci�n del rayo incidente
    float3 incidentDir = WorldRayDirection();

    // Direcci�n hacia la c�mara/origen del rayo (desde el punto de hit)
    float3 viewDir = normalize(WorldRayOrigin() - hitPos); // O -incidentDir si prefieres

    // Determinar si estamos golpeando una cara frontal o trasera
    // Si el rayo viene de fuera, dot(incidentDir, normal) < 0 para cara frontal
    // Si el rayo viene de dentro, dot(incidentDir, normal) > 0 para cara frontal (que ahora es trasera desde la perspectiva del rayo)
    bool hittingFrontFace = dot(incidentDir, normal) < 0.0;
    float3 shadingNormal = hittingFrontFace ? normal : -normal; // Normal a usar para shading/reflexi�n/refracci�n

    // --- 2. Calcular Reflejo (Siempre necesario para Fresnel y met�licos) ---
    float3 reflectedRayDir = reflect(incidentDir, shadingNormal);
    RayDesc reflectionRay;
    // Origen ligeramente fuera de la superficie para evitar auto-intersecci�n
    reflectionRay.Origin = hitPos + shadingNormal * SMALL_OFFSET;
    reflectionRay.TMin   = 0.0; // Empezar justo despu�s del origen
    reflectionRay.TMax   = 100.0; // Distancia m�xima del rayo (ajustar si es necesario)

    // Calcular color reflejado trazando rayos (considerando roughness)
    // Usamos una l�gica similar a la esfera para reflejos rugosos
    int numReflectionRays = 1; // Por defecto para superficies lisas o recursi�n profunda
    if (material.Roughness > 0.01 && payload.Recursion <= 2) // Limitar rayos/recursi�n
    {
       numReflectionRays = max(1, min(8, (int)(1.0 + material.Roughness * 7.0)));
    }

    float3 tracedReflectionColor = float3(0.0, 0.0, 0.0);
    // Ajusta el cono seg�n la rugosidad. Un valor mayor dispersa m�s.
    float coneSpread = material.Roughness * material.Roughness * 0.5; // Experimenta con esta f�rmula

    for (int j = 0; j < numReflectionRays; ++j)
    {
        // Genera direcciones dentro de un cono alrededor de la direcci�n de reflejo perfecta
        // Necesitas una funci�n como DirectionWithinCone o similar en RayUtils.fxh
        // Si no la tienes, puedes usar una aproximaci�n simple o implementarla.
        // Ejemplo usando puntos en disco (si los tienes definidos como en el shader de esfera):
        // int pointIndex = j % 8;
        // float2 offset = float2(g_ConstantsCB.DiscPoints[pointIndex / 2][(pointIndex % 2) * 2],
        //                        g_ConstantsCB.DiscPoints[pointIndex / 2][(pointIndex % 2) * 2 + 1]);
        // reflectionRay.Direction = DirectionWithinCone(reflectedRayDir, offset * coneSpread);

        // Alternativa simple si no tienes DirectionWithinCone (menos precisa):
        // Genera un vector aleatorio o pseudoaleatorio y m�zclalo con reflectedRayDir
        // float3 randomDir = RandomDirectionAround(reflectedRayDir, coneSpread, j); // Necesitar�as implementar esto
        // reflectionRay.Direction = normalize(lerp(reflectedRayDir, randomDir, material.Roughness));

        // Si no tienes nada para rugosidad, usa la direcci�n perfecta:
        reflectionRay.Direction = reflectedRayDir; // Simplificaci�n si no hay manejo de rugosidad en el trazado

        // Trazar el rayo de reflejo
        PrimaryRayPayload reflectionPayload = CastPrimaryRay(reflectionRay, payload.Recursion + 1);
        tracedReflectionColor += reflectionPayload.Color;
    }
    tracedReflectionColor /= float(numReflectionRays);


    // --- 3. Calcular Fresnel y Decidir entre Reflexi�n/Refracci�n ---
    // F0: Reflectancia base en incidencia normal.
    // Diel�ctricos puros ~0.04. Metales usan su color base.
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), material.BaseColor.rgb, material.Metallic);

    // Coseno del �ngulo entre la direcci�n de vista y la normal de shading
    // Usamos abs() o max(0,..) porque Fresnel depende del �ngulo, no de la direcci�n exacta
    float cosTheta = max(dot(viewDir, shadingNormal), 0.0); // O usar dot(-incidentDir, shadingNormal)

    // Factor Fresnel: cu�nta luz se refleja especularmente en este �ngulo
    float3 Fresnel = FresnelSchlick(cosTheta, F0);


    // --- 4. Calcular Refracci�n (si no es completamente met�lico y tiene IOR > 1) ---
    float3 refractionColor = float3(0.0, 0.0, 0.0); // Color por defecto si no hay refracci�n v�lida
    bool tir = false; // Flag para Reflexi�n Interna Total

    // Solo calcular refracci�n para materiales no met�licos o parcialmente met�licos
    // y que tengan un �ndice de refracci�n diferente al aire
    if (material.Metallic < 0.95 && material.IndexOfRefraction > 1.001) // Umbrales ajustables
    {
        float eta; // Ratio de IORs (desde / hacia)
        float3 refractionNormal = normal; // Normal geom�trica original

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

        // Calcular direcci�n del rayo refractado
        // Nota: refract necesita la normal apuntando hacia el lado de donde viene el rayo incidente
        float3 refractedRayDir = refract(incidentDir, refractionNormal, eta);

        // Comprobar Reflexi�n Interna Total (TIR)
        // refract devuelve (0,0,0) si ocurre TIR
        if (dot(refractedRayDir, refractedRayDir) < 0.001) // Comparaci�n segura con cero
        {
            tir = true;
        }
        else
        {
            // Trazar el rayo refractado
            RayDesc refractionRay;
            // Origen ligeramente DENTRO de la superficie (o fuera si salimos)
            // Moverse en la direcci�n opuesta a la normal de refracci�n usada
            refractionRay.Origin = hitPos - refractionNormal * SMALL_OFFSET;
            refractionRay.Direction = normalize(refractedRayDir); // Asegurarse que est� normalizado
            refractionRay.TMin   = 0.0;
            refractionRay.TMax   = 100.0; // Ajustar si es necesario

            // Lanzar el rayo de refracci�n
            PrimaryRayPayload refractionPayload = CastPrimaryRay(refractionRay, payload.Recursion + 1);
            refractionColor = refractionPayload.Color;

            // Opcional: Aplicar color base como tinte de absorci�n (simplificado)
            // Solo si el rayo entr� al material (hittingFrontFace)
            // Una absorci�n m�s realista depender�a de la distancia recorrida dentro.
            if (hittingFrontFace) {
                 // Podr�as usar Beer's Law si tienes la distancia, o un tinte simple:
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
        // Podr�amos multiplicar por Fresnel aqu� tambi�n, aunque deber�a ser cercano a 1.
        // finalColor = tracedReflectionColor * Fresnel;
    }
    else
    {
        // Mezclar reflexi�n y refracci�n basado en Fresnel para diel�ctricos/parcialmente met�licos
        // La componente de refracci�n (1.0 - Fresnel) se modula por el color base (albedo)
        // La componente de reflexi�n (Fresnel) se modula por el color especular (F0 o blanco)
        // Para metales, la refracci�n es casi nula y Fresnel es alto y coloreado (F0)

        // Color refractado/difuso (simplificado como color base * componente no reflejada)
        // Para diel�ctricos, esto representa la luz que entra y potencialmente se dispersa o absorbe.
        // Para metales, este t�rmino deber�a ser casi cero.
        float3 diffuseComponent = material.BaseColor.rgb * refractionColor * (1.0 - material.Metallic);

        // Color reflejado especularmente (usando el color trazado)
        float3 specularComponent = tracedReflectionColor; // El color del reflejo ya incluye el entorno

        // Combinar usando Fresnel como factor de mezcla entre lo que se refleja especularmente
        // y lo que entra/se dispersa (para diel�ctricos) o se absorbe (para metales).
        // Usamos Fresnel.r como aproximaci�n del factor de mezcla.
        finalColor = lerp(diffuseComponent, specularComponent, Fresnel.r);

        // Otra forma com�n es mezclar basado en metalicidad primero:
        // float3 dielectricColor = refractionColor * (1.0 - Fresnel.r) + tracedReflectionColor * Fresnel.r;
        // float3 metallicColor = tracedReflectionColor * F0; // O simplemente tracedReflectionColor * Fresnel;
        // finalColor = lerp(dielectricColor, metallicColor, material.Metallic);

        // La forma m�s PBR-like simple podr�a ser:
        // float3 F = FresnelSchlick(cosTheta, F0);
        // float3 kS = F; // Specular reflection coefficient
        // float3 kD = (1.0 - kS) * (1.0 - material.Metallic); // Diffuse/Refraction coefficient (reduced by metallic)
        // finalColor = (kD * material.BaseColor.rgb * refractionColor) + (kS * tracedReflectionColor); // refractionColor aqu� ser�a m�s bien la luz incidente si no traz�ramos refracci�n. Como la trazamos, la usamos.

        // Vamos a usar una mezcla simple basada en Fresnel y Metallic que funcione razonablemente:
        // Combina la refracci�n (tintada por BaseColor para diel�ctricos) y la reflexi�n (pura o tintada por F0 para metales)
        float3 nonMetallicRefraction = refractionColor * material.BaseColor.rgb; // Lo que entra y se ti�e (simplificado)
        // Mezcla entre refracci�n tintada y reflejo basado en Fresnel
        float3 dielectricBRDF = lerp(nonMetallicRefraction, tracedReflectionColor, Fresnel);
        // Mezcla final entre comportamiento diel�ctrico y met�lico (reflejo puro tintado por F0)
        finalColor = lerp(dielectricBRDF, tracedReflectionColor * F0, material.Metallic); // F0 ti�e el reflejo met�lico

    }

    // Asignar resultado final al payload
    payload.Color = finalColor;
    payload.Depth = RayTCurrent(); // Profundidad del hit actual
}