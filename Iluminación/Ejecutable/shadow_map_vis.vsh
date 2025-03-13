
struct VSInput
{
    float2 Pos : ATTRIB0; // Posición en NDC
    float2 UV  : ATTRIB1; // Coordenadas de textura
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEX_COORD;
};

void main(in VSInput In, out PSInput Out)
{
    // Para un quad en pantalla, se puede omitir transformaciones complejas
    Out.Pos = float4(In.Pos, 0.0, 1.0);
    Out.UV  = In.UV;
}
