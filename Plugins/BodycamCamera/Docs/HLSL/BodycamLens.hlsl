// Bodycam lens look: body of a single Custom material node.
//
// Material settings:  Domain = Post Process,  Blendable Location = After Tonemapping
// Route this node's output (CMOT_Float3) to Emissive Color.
//
// Custom node inputs (names must match exactly; the scalar ones are Scalar Parameters with these names,
// ClockTime is a Time node):
//   Distortion, RollingShutter, YawRate, Compression, SensorNoise, Aspect, ViewWidth, ViewHeight, ClockTime
//
// BodycamCameraComponent drives all of them at runtime through a MaterialInstanceDynamic.

float2 vuv = GetViewportUV(Parameters);
float2 view = float2(ViewWidth, ViewHeight);

// --- Barrel / fisheye distortion, normalised so the screen corners still map onto the image edge ---
float2 c = vuv - 0.5;
float2 ca = float2(c.x * Aspect, c.y);
float r2 = dot(ca, ca);
float maxR2 = 0.25 * (Aspect * Aspect + 1.0);
float2 src = 0.5 + c * (1.0 + Distortion * r2) / (1.0 + Distortion * maxR2);

// --- Rolling shutter: lower scanlines are captured later, so fast yaw skews the image ---
src.x += RollingShutter * YawRate * (src.y - 0.5);
src = clamp(src, 0.002, 0.998);

float3 col = SceneTextureLookup(ViewportUVToSceneTextureUV(src, 14), 14, false).rgb;
float lum = dot(col, float3(0.299, 0.587, 0.114));
float dark = 1.0 - smoothstep(0.02, 0.45, lum);

// --- Compression artifacts: 16x16 macroblocks that flatten in the dark + colour banding ---
float2 px = src * view;
float2 blk = floor(px / 16.0);
float2 blkUV = clamp((blk * 16.0 + 8.0) / view, 0.002, 0.998);
float3 blkCol = SceneTextureLookup(ViewportUVToSceneTextureUV(blkUV, 14), 14, false).rgb;
float blkRand = frac(sin(dot(blk + floor(ClockTime * 12.0), float2(12.9898, 78.233))) * 43758.5453);

float comp = saturate(Compression);
float blend = comp * (0.35 + 0.65 * dark) * (0.6 + 0.4 * blkRand);
col = lerp(col, blkCol, blend);

float levels = lerp(255.0, 20.0, comp * (0.4 + 0.6 * dark));
col = floor(col * levels + 0.5) / levels;

// --- Sensor noise, mostly in the shadows ---
float n = frac(sin(dot(vuv * view + ClockTime * 61.7, float2(12.9898, 78.233))) * 43758.5453) - 0.5;
col += n * SensorNoise * (0.25 + 0.75 * dark);

return saturate(col);
