![Path-traced image](https://github.com/TheRealMJP/DXRPathTracer/blob/master/DXRPathTracer.png)

# DXR Path Tracer
This project is a very basic unidirectional path tracer that I wrote using DirectX Raytracing, also known as DXR. I mostly did this to learn the DXR API, and also for a bit of fun. Therefore you shouldn't take this as an example of the best or most performant way to implement a path tracer on the GPU: I mostly did whatever was fastest and/or easiest to implement! However it could be useful for other people looking for samples that are bit more complex than what's currently offered in the [offical DirectX sample repo](https://github.com/Microsoft/DirectX-Graphics-Samples), or for people who want to start hacking on something and don't want to do all of the boring work themselves. Either way, have fun!

# Ray Tracing Gems 2

This repository was used as an example for my chapter "Using Bindless Resources With DirectX Raytracing" from [Ray Tracing Gems 2](http://www.realtimerendering.com/raytracinggems/rtg2/). Since then, this repository has been updated to use the newer-style `ResourceDescriptorHeap` from [Shader Model 6.6](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_ShaderModel6_6.html) in order to implement bindless resources. If you would like to view the code that uses the legacy bindless style described in that chapter, head over to the [legacy_bindless branch](https://github.com/TheRealMJP/DXRPathTracer/tree/legacy_bindless).

# Build Instructions

The repository contains a Visual Studio 2022 project and solution file that's ready to build on Windows 10 or Windows 11 using SDK 10.0.19041.0 or later. All external dependencies are included in the repository (including a recent build of dxcompiler.dll), so there's no need to download additional libraries. Running the demo requires Windows 10 build 1909 or later (or Windows 11), as well as a GPU/driver combination that supports `D3D12_RAYTRACING_TIER_1_1` and Shader Model 6.6. 

The repository does *not* include textures for the Sponza and SunTemple scenes in order to avoid comitting lots of large binary files. To get the textures, download them from [this release](https://github.com/TheRealMJP/DXRPathTracer/releases/tag/v1.0).

# Using The App

To move the camera, press the W/S/A/D/Q/E keys. The camera can also be rotated by right-clicking on the window and dragging the mouse.

The render is progressively updated by shooting one ray per-pixel every frame, which takes anywhere from 14 to 30ms per frame on my RTX 2080 when the max path length is set to 3 (the default). Diffuse and specular sampling is currently supported, with lighting provided by a [procedural sun and sky model](http://cgg.mff.cuni.cz/projects/SkylightModelling/). If you change a setting or move the camera, the render will reset and start accumulating samples again. Path lengths greater than 2 are computed by recursively tracing a new ray inside of the closest hit program, which is convenient but probably not the fastest way to things.

To improve the quality, increase the "Sqrt Num Samples" setting. As the name implies it's the square root of the number of samples, so increasing that value will increase the total render time non-linearly! You can also increase the maximum path length, which is capped at 3 (single-bounce) by default.

To switch between path-traced rendering and standard (boring) rasterization with no GI, toggle the "Enable Ray Tracing" setting.

# Possible To-Do List

* ~~Support alpha-tested triangles for foliage~~
* ~~Investigate performance tradeoffs involved with having one D3D12_RAYTRACING_GEOMETRY_DESC per mesh in the source scene~~
* Investigate higher-performance sampling schemes (currently using a stock implementation of [Correlated Multi-Jittered Sampling](https://graphics.pixar.com/library/MultiJitteredSampling/paper.pdf))
* ~~Local light sources~~
* Area light sampling with soft shadows
* Non-opaque materials
* ~~Progress bar/text for the progressive render~~
* Mip level selection using ray differentials
* ~~Shader Model 6.6 bindless using ResourceDescriptorHeap~~
