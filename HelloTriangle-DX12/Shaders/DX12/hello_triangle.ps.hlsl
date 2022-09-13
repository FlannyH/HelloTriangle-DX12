struct PixelInput
{
    float3 color : COLOR;
};

struct PixelOutput
{
    float4 attachment0 : SV_Target0;
};

PixelOutput main(PixelInput pixel_input)
{
    float3 in_color = pixel_input.color;
    PixelOutput output;
    output.attachment0 = float4(in_color, 1.0f);
    return output;
}