#ifndef REFLECTION_HLSL_
#define REFLECTION_HLSL_

float AbsCosTheta(float3 w) { return abs(w.z); }

float CosTheta(float3 w) {
    return w.z;
}

float Cos2Theta(float3 w) { return w.z * w.z; }

float Sin2Theta(float3 w) {
    return max((float)0, (float)1 - Cos2Theta(w));
}

float SinTheta(float3 w) {
    return sqrt(Sin2Theta(w));
}

float CosPhi(float3 w) {
    float sinTheta = SinTheta(w);
    return (sinTheta == 0) ? 1 : clamp(w.x / sinTheta, -1, 1);
}

float Cos2Phi(float3 w) {
    return CosPhi(w) * CosPhi(w);
}

float SinPhi(float3 w) {
    float sinTheta = SinTheta(w);
    return (sinTheta == 0) ? 0 : clamp(w.y / sinTheta, -1, 1);
}

float Sin2Phi(float3 w) {
    return SinPhi(w) * SinPhi(w);
}

float AbsDot(float3 v1, float3 v2) {
    return abs(dot(v1, v2));
}

float TanTheta(float3 w) {
    // Trigonometric identity.
    return SinTheta(w) / CosTheta(w);
}

float Tan2Theta(float3 w) {
    return Sin2Theta(w) / Cos2Theta(w);
}

bool SameHemisphere(float3 w, float3 wp) {
    return w.z * wp.z > 0;
}

float3 Reflect(float3 wo, float3 n) {
    return -wo + 2*dot(wo, n)* n;
}

#endif // REFLECTION_HLSL_