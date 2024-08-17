# VoxelStreamMemory

Inherits: [VoxelStream](VoxelStream.md)

Keeps data into memory rather than writing it on disk.

## Description: 

This stream is mainly intented for testing purposes. It shouldn't be used as a proper saving system.

## Properties: 


Type                                                                  | Name                                                             | Default 
--------------------------------------------------------------------- | ---------------------------------------------------------------- | --------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)  | [artificial_save_latency_usec](#i_artificial_save_latency_usec)  | 0       
<p></p>

## Property Descriptions

### [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_artificial_save_latency_usec"></span> **artificial_save_latency_usec** = 0

Fakes long saving by making the calling thread sleep for some amount of time.

_Generated on Apr 06, 2024_
