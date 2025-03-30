Getting Voxel Tools
=====================

This project exists in two editions:

- [Module](#module): bundled as a custom build of Godot Engine and custom export templates. This is the primary way it was developped for a long time.
- [GDExtension](#gdextension): a native add-on that can be added to your project, and will work with an official build of Godot (4.4.1 or later). This is more recent so it had less testing so far.

Each one comes with its own advantages and drawbacks, but generally offer the same functionality.


Module
--------

The following section applies if you use the Module edition of Voxel Tools.

### Precompiled builds

#### Release builds

Builds are provided at [https://github.com/Zylann/godot_voxel/releases](https://github.com/Zylann/godot_voxel/releases).
Module builds are usually prefixed as `Godot 4.x.x + Voxel Tools 1.x.x`.

The project follows a continuous development cycle, so "releases" are merely snapshots of development versions. Because Github requires an account to download latest development versions from Github Actions, releases are published for convenience.

The engine is massive and targets a lot of platforms, while our module is small in comparison and we don't have dedicated build containers, so not all combinations of editors and export templates are available. You can develop your game and test it with the editor on main desktop platforms, but if a combination of platforms/options isn't provided, you will have to build them yourself.

#### Development builds

Development builds contain the very latest features and bug fixes (although they can also contain unknown bugs). They are available on Github Actions.

!!! note
	You need a Github account to download artifacts from Github Actions. Otherwise, links will not work.

Pick your platform:

- [Windows builds](https://github.com/Zylann/godot_voxel/actions/workflows/windows.yml)
- [Linux builds](https://github.com/Zylann/godot_voxel/actions/workflows/linux.yml)
- [MacOS builds](https://github.com/Zylann/godot_voxel/actions/workflows/macos.yml)

Then click on the latest successful build, with a green checkmark:

![Screenshot of a list of builds, with the latest successful one circled in green](images/ci_builds_latest_link.webp)

Then scroll to the bottom, you should see download links:

![Github actions screenshot](images/github_actions_windows_artifacts.webp)

In case there are multiple downloadable artifacts, the editor build will be the one with `editor` in the name.

These builds correspond to the `master` version depicted in the [changelog](https://github.com/Zylann/godot_voxel/blob/master/CHANGELOG.md).
They are built using Godot's latest stable version branch (for example, `4.2` at time of writing), instead of `master`, unless indicated otherwise.
A new build is made each time commits are pushed to the main branch, but also when other developers make Pull Requests, so careful about which one you pick.

!!! note
	Mono builds (C# support) [are also done](https://github.com/Zylann/godot_voxel/actions/workflows/mono.yml), however they no longer work out of the box. For more information, see [C# support](#c-suppport).


### Building yourself

See [Building as a module](module_development.md#building)


### Exporting

!!! note 
	You will need this section if you want to export your game into an executable.

#### Supported platforms

This module supports all platforms Godot supports, on which threads are available.

Some features might not always be available:

- SIMD noise with FastNoise2 0.10 can only benefit from an x86 CPU and falls back to scalar otherwise, which is slower
- GPU features require support for compute shaders (Forward+ renderer)
- Threads might not work on all browsers with the web export

#### Getting a template

In Godot Engine, exporting your game as an executable for a target platform requires a "template". A template is an optimized build of Godot Engine without the editor stuff. Godot combines your project files with that template and makes the final executable.

If you only download the Godot Editor with the module, it will allow you to develop and test your game, but if you export without any other setup, Godot will attempt to use a vanilla template, which won't have the module. Therefore, it will fail to open some scenes.

As mentionned in earlier sections, you can get pre-built templates for some platforms and configurations.

If there is no pre-built template available for your platform, you may build it yourself. This is the same as building Godot with the module, only with different options. See the [Godot Documentation](https://docs.godotengine.org/en/latest/development/compiling/index.html) for more details, under the "building export templates" category of the platform you target.

#### Using a template

Once you have a template build, tell Godot to use it in the Export configurations. Fill in the path to a custom template in the "Custom Template" section:

![Screenshot of Godot export configuration window with a custom template assigned for Windows](images/export_template_window.webp)


GDExtension
-------------

Godot 4 introduced [GDExtension](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html), a native API allowing C++ plugins to be added to a project without requiring a custom build of the engine.

Voxel Tools can also compile as a GDExtension, although it is a recent addition, and has gone through less testing, so it might have bugs that are not present in the Module edition.

### Differences

Module and GDExtension editions aim to be compatible. However, there are a few important differences (list non exhaustive):

- `FastNoise2` is not included
- There are some technical drawbacks, some of which [are listed here](https://github.com/Zylann/godot_voxel/issues/442)


### Release builds

Plugin packages can be found in Releases: [https://github.com/Zylann/godot_voxel/releases](https://github.com/Zylann/godot_voxel/releases).
They usually contain the word `GDExtension` in the title.

The project follows a continuous development cycle, so "releases" are merely snapshots of development versions. Because Github requires an account to download latest development versions from Github Actions, releases are published for convenience.


### Development builds

There are development builds of the GDExtension available on [Github Actions](https://github.com/Zylann/godot_voxel/actions/workflows/extension_ci.yml) as we try to keep the module compiling with GodotCpp, however they lack testing and might crash. Use at your own risk.


### Building yourself

If the provided binaries don't include the platform or options you need, or if you need customizations, you can still make your own build.
See [VoxelGeneratorNoise2D](module_development.md#gdextension)


### Exporting

Exporting your game with Godot's regular templates should work out of the box.

You can check which platforms are provided by the extension in the config file at `addons/zylann.voxel/voxel.gdextension`:

- Windows x86_64
- Linux x86_64
- Linux arm64
- Mac OS x86_64 (universal)
- Mac OS arm64 (universal)
- iOS arm64
- Android x86_64
- Android arm64

If what you need is not in this list, you may try [Building yourslef](#building-yourself).


#### Stripping out icons

Currently, editor icons have to be registered in the GDExtension config file and cannot be excluded with a `.gdignore` file. That means when you export your game, those icons will be needlessly included in your game.

You can exclude the contents of the `addons/zylann.voxel/editor` folder in your export configurations.


Switching between Module and GDExtension
------------------------------------------

If you start using GDExtension in a project that was using the Module edition, you should use an official version of Godot Engine, or one that does not include the module, otherwise classes will conflict.

If you want to switch back to using the Module edition, you can remove all files related to GDExtension in `addons/zylann.voxel/` and then start the editor. Alternatively, you can also change the file extension of `addons/zylann.voxel/voxel.gdextension` so that Godot no longer detects it.


C# support
--------------

C# is a bit of a special case in Godot, especially when it comes to plugins. It requires extra work to setup.

### Module

Working builds used to be available on Github Actions (as "Mono Builds"). Unfortunately, Godot 4 changed the way C# integrates by using the Nuget package manager. This made it harder for module developers to provide ready-to-use executables, and hard for users too:

- When you make a project in Godot C#, it fetches the "vanilla" Godot SDK from Nuget, but it is only available for official stable versions, so you can't use CI builds of the engine that use the latest development version of Godot.
- Modules add new classes to the API which are not present in the official SDK. It would require to create SDKs for every combination of modules you want to use and upload them to Nuget, which isn't practical.
- You could revert to the latest official SDK available on Nuget, but to access module APIs you would have to use workarounds such as `obj.Get(string)`, `Set(string)` and `Call(string, args)` in code, which is hard to use, inefficient and terrible to maintain.

To obtain a working version, you have to generate the SDK yourself and use a local Nuget repository instead of the official one. Follow the steps described in the [Godot Documentation for C#](https://docs.godotengine.org/en/stable/contributing/development/compiling/compiling_with_dotnet.html).


### Ownership checks

Voxel Tools does some sanity checks when running some virtual methods, such as custom generators. These checks involve reference counting. However, that doesn't work in C# because it is a garbage-collected language: `RefCounted` objects going out of scope are not actually freed until the garbage collector runs. This can cause false-positive errors.

You may turn off those checks in Project Settings: `voxel/ownership_checks`


### GDExtension and `C#`

C# support of extensions implemented in C++ is not well defined at the moment.

The issue is that the Godot API C# can use (nicknamed the "glue") is generated when Godot itself is built, so it only contains core vanilla classes. Everything else (extensions, GDScript) is missing from it, so it requires to use Godot's reflection methods.
In theory, C++ extensions could provide a strongly-typed API since they have function pointers that could be bound to C#, but this has not been implemented by Godot so far.

So the only way to interact from C# with classes defined by an extension is to use the following:

- Calling methods: [call](https://docs.godotengine.org/en/stable/classes/class_object.html#class-object-method-call)
- Getting or setting properties: [get](https://docs.godotengine.org/en/stable/classes/class_object.html#class-object-method-get) and [set](https://docs.godotengine.org/en/stable/classes/class_object.html#class-object-method-set) 
- Creating new instances: [ClassDB.instantiate](https://docs.godotengine.org/en/stable/classes/class_classdb.html#class-classdb-method-instantiate).

```cs
// /!\ Pseudo-code, untested
GodotObject model = Godot.ClassDB.Instantiate("VoxelBlockyModelCube");
model.Call("set_tile", Godot.ClassDB.ClassGetIntegerConstant("VoxelBlockyModel", "SIDE_NEGATIVE_X"), new Vector2I(1, 1))
model.Set("atlas_size_in_tiles", new Vector2I(8, 8));
```

This approach however carries a lot of overhead, impacts performance, and is tedious to use.

Similar situations are presented in Godot's documentation about [Cross-Language Scripting](https://docs.godotengine.org/en/stable/tutorials/scripting/cross_language_scripting.html)
