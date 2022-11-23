# Ray Tracing Data Structures
 Data structures, such as Octrees and KD-Trees, for accelerating ray tracing.
The most relevant files for the data structure code can be found in:
- Octree: [octree.h](include/nori/octree.h), [octree.cpp](src/octree.cpp)
- KD-Tree: 
- BVH: 
- Accel: [accel.h](include/nori/accel.h), [accel.cpp](src/accel.cpp). This implements the data structures themselves, and is where the ray-object intersections occur.

# Data Structures
## Octree

## KD-Tree

## BVH


# Extra
## Notes
- This project was set up using the Nori Educational renderer, and uses it as a foundation. While scenes can be rendered, I did not create any of the ray tracing algorithms themselves, merely the structures for accelerating the speed of ray-triangle (or ray-object) intersection across a mesh of triangles. 
- This project uses TBB to utilize paralellism inherent in creating tree-like structures. To disable the multi-threading, in each of the source files (or for whichever structure specifically), set the line `#define PARALLEL true` to `#define PARALLEL false`.

## Credits/Libraries
