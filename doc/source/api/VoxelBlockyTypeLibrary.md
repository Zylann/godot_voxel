# VoxelBlockyTypeLibrary

Inherits: [VoxelBlockyLibraryBase](VoxelBlockyLibraryBase.md)

!!! warning
    This class is marked as experimental. It is subject to likely change or possible removal in future versions. Use at your own discretion.


## Properties: 


Type                 | Name                             | Default             
-------------------- | -------------------------------- | --------------------
`PackedStringArray`  | [_id_map_data](#i__id_map_data)  | PackedStringArray() 
`VoxelBlockyType[]`  | [types](#i_types)                | []                  
<p></p>

## Methods: 


Return                                                                                            | Signature                                                                                                                                                                                                                                                                     
------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                              | [get_model_index_default](#i_get_model_index_default) ( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) type_name ) const                                                                                                                  
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                              | [get_model_index_single_attribute](#i_get_model_index_single_attribute) ( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) type_name, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) attrib_value ) const     
[int](https://docs.godotengine.org/en/stable/classes/class_int.html)                              | [get_model_index_with_attributes](#i_get_model_index_with_attributes) ( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) type_name, [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html) attribs_dict ) const 
[VoxelBlockyType](VoxelBlockyType.md)                                                             | [get_type_from_name](#i_get_type_from_name) ( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) type_name ) const                                                                                                                            
[Array](https://docs.godotengine.org/en/stable/classes/class_array.html)                          | [get_type_name_and_attributes_from_model_index](#i_get_type_name_and_attributes_from_model_index) ( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) model_index ) const                                                                                  
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                            | [load_id_map_from_json](#i_load_id_map_from_json) ( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) json )                                                                                                                                         
[bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)                            | [load_id_map_from_string_array](#i_load_id_map_from_string_array) ( [PackedStringArray](https://docs.godotengine.org/en/stable/classes/class_packedstringarray.html) str_array )                                                                                              
[String](https://docs.godotengine.org/en/stable/classes/class_string.html)                        | [serialize_id_map_to_json](#i_serialize_id_map_to_json) ( ) const                                                                                                                                                                                                             
[PackedStringArray](https://docs.godotengine.org/en/stable/classes/class_packedstringarray.html)  | [serialize_id_map_to_string_array](#i_serialize_id_map_to_string_array) ( ) const                                                                                                                                                                                             
<p></p>

## Property Descriptions

- [PackedStringArray](https://docs.godotengine.org/en/stable/classes/class_packedstringarray.html)<span id="i__id_map_data"></span> **_id_map_data** = PackedStringArray()


- [VoxelBlockyType[]](https://docs.godotengine.org/en/stable/classes/class_voxelblockytype[].html)<span id="i_types"></span> **types** = []


## Method Descriptions

- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_model_index_default"></span> **get_model_index_default**( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) type_name ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_model_index_single_attribute"></span> **get_model_index_single_attribute**( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) type_name, [Variant](https://docs.godotengine.org/en/stable/classes/class_variant.html) attrib_value ) 


- [int](https://docs.godotengine.org/en/stable/classes/class_int.html)<span id="i_get_model_index_with_attributes"></span> **get_model_index_with_attributes**( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) type_name, [Dictionary](https://docs.godotengine.org/en/stable/classes/class_dictionary.html) attribs_dict ) 


- [VoxelBlockyType](VoxelBlockyType.md)<span id="i_get_type_from_name"></span> **get_type_from_name**( [StringName](https://docs.godotengine.org/en/stable/classes/class_stringname.html) type_name ) 


- [Array](https://docs.godotengine.org/en/stable/classes/class_array.html)<span id="i_get_type_name_and_attributes_from_model_index"></span> **get_type_name_and_attributes_from_model_index**( [int](https://docs.godotengine.org/en/stable/classes/class_int.html) model_index ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_load_id_map_from_json"></span> **load_id_map_from_json**( [String](https://docs.godotengine.org/en/stable/classes/class_string.html) json ) 


- [bool](https://docs.godotengine.org/en/stable/classes/class_bool.html)<span id="i_load_id_map_from_string_array"></span> **load_id_map_from_string_array**( [PackedStringArray](https://docs.godotengine.org/en/stable/classes/class_packedstringarray.html) str_array ) 


- [String](https://docs.godotengine.org/en/stable/classes/class_string.html)<span id="i_serialize_id_map_to_json"></span> **serialize_id_map_to_json**( ) 


- [PackedStringArray](https://docs.godotengine.org/en/stable/classes/class_packedstringarray.html)<span id="i_serialize_id_map_to_string_array"></span> **serialize_id_map_to_string_array**( ) 


_Generated on Jul 23, 2023_
