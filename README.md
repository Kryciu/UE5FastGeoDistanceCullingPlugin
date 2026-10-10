# FastGeoDistanceCulling

Simple distance culling support for **FastGeo** in Unreal Engine 5.

The plugin applies cull distances to Static Meshes and Instanced Static Meshes during World Partition cell transformation, based on primitive size.

## Features

- Distance culling for FastGeo Static Meshes
- Instanced Static Mesh (ISM) support
- Automatically groups ISM instances into appropriate culling buckets
- Configurable cull distance / size pairs
- Configurable ISM fade start distance
- Debug visualization for culling buckets

## Usage

Add `FastGeoDistanceCullCellTransformer` to your World Partition runtime cell transformers and configure the `Cull Distances` array.

Each entry contains:

- **Size** – primitive size used to select the closest matching bucket
- **Cull Distance** – maximum draw distance for primitives in that bucket

For ISMs, instances using different culling buckets are automatically split into separate components.

Enable **Debug View** on the transformer to visualize the configured culling buckets in editor PIE mode.

## Installation

Download the appropriate release and copy the plugin into:

```text
YourProject/Plugins/FastGeoDistanceCulling
```

## Requirements

- Unreal Engine 5 (release is compiled only for UE5.6)
- World Partition
- FastGeoStreaming

## Visualizer

<img width="1920" height="1080" alt="Bez tytułu" src="https://github.com/user-attachments/assets/07712336-4ea0-4b69-bde1-39d4a7e719c8" />

