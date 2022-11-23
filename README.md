# Ray Tracing Data Structures
 Data structures, such as Octrees and KD-Trees, for accelerating ray tracing.
The most relevant files for the data structure code can be found in:
- AccelTree: [AccelTree.h](include/nori/AccelTree.h), [AccelTree.cpp](src/AccelTree.cpp). This is the abstract base-class for all of the acceleration data structures.
- Octree: [Octree.h](include/nori/Octree.h), [Octree.cpp](src/Octree.cpp)
- KD-Tree: [KDTree.h](include/nori/KDTree.h), [KDTree.cpp](src/KDTree.cpp)
- BVH: [BVH.h](include/nori/BVH.h), [BVH.cpp](src/BVH.cpp)
- Accel: [accel.h](include/nori/accel.h), [accel.cpp](src/accel.cpp). This implements the data structures themselves, and is where the ray-object intersections occur.

## Getting Started


# Data Structures
## Octree

## KD-Tree

## BVH


# Extra
## Notes
- This project was set up using the Nori Educational renderer, and uses it as a foundation. While scenes can be rendered, I did not create any of the ray tracing algorithms themselves, merely the structures for accelerating the speed of ray-object (specifically ray-triangle) intersection across a mesh of triangles. 
- This project uses TBB to utilize paralellism inherent in creating tree-like structures. To disable the multi-threading, in each of the source files (or for whichever structure specifically), set the line `#define PARALLEL true` to `#define PARALLEL false`.

## Credits/Libraries
