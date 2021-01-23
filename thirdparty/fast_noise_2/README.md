[![GitHub Actions CI](https://img.shields.io/github/workflow/status/Auburn/FastNoise2/CI?style=flat-square&logo=GitHub "GitHub Actions CI")](https://github.com/Auburn/FastNoise2/actions)
[![Discord](https://img.shields.io/discord/703636892901441577?style=flat-square&logo=discord "Discord")](https://discord.gg/SHVaVfV)

# FastNoise2

[Documentation WIP](https://github.com/Auburn/FastNoise2/wiki)

WIP successor to [FastNoiseSIMD](https://github.com/Auburn/FastNoiseSIMD)

FastNoise2 is a fully featured noise generation library which aims to meet all your coherent noise needs while being extremely fast

Uses FastSIMD to compile classes with multiple SIMD types

Supports:
- 32/64 bit
- Windows
- Linux
- MacOS
- MSVC
- Clang
- GCC

Check the [releases](https://github.com/Auburns/FastNoise2/releases) for early versions of the Noise Tool

![NoiseTool](https://user-images.githubusercontent.com/1349548/90967950-4e8da600-e4de-11ea-902a-94e72cb86481.png)

# Getting Started

There are 2 ways to use FastNoise 2, creating a node tree structure in code or importing a serialised node tree created using the NoiseTool.

This is creating a Simplex Fractal FBm with 5 octaves from code:
```
auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
auto fnFractal = FastNoise::New<FastNoise::FractalFBm>();

fnFractal->SetSource( fnSimplex );
fnFractal->SetOctaveCount( 5 );

fnFractal->GenUniformGrid2D( ... );
```

Here is the same Simplex Fractal FBm with 5 octaves but using serialised data from the NoiseTool:
```
FastNoise::SmartNode<> fnGenerator = FastNoise::NewFromEncodedNodeTree( "DQAFAAAAAAAAQAgAAAAAAD8=" );

fnGenerator->GenUniformGrid2D( ... );
```
This is the node graph for the above from the NoiseTool

![SimplexFractalNodes](https://user-images.githubusercontent.com/1349548/90897006-72f16180-e3bc-11ea-8cc3-a68daed7b6c1.png)
