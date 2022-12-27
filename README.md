# Ray Tracing Data Structures
 Data structures, such as KD-Trees and BHVs, for accelerating ray tracing.
The most relevant files for the data structure code can be found in:
- AccelTree: [AccelTree.h](include/nori/AccelTree.h), [AccelTree.cpp](src/AccelTree.cpp). This is the abstract base-class for all of the acceleration data structures.
- Octree: [Octree.h](include/nori/Octree.h), [Octree.cpp](src/Octree.cpp)
- KD-Tree: [KDTree.h](include/nori/KDTree.h), [KDTree.cpp](src/KDTree.cpp)
- BVH: [BVH.h](include/nori/BVH.h), [BVH.cpp](src/BVH.cpp)
- Accel: [accel.h](include/nori/accel.h), [accel.cpp](src/accel.cpp). This implements the data structures themselves, and is where the ray-object intersections occur.


# Data Structures
## Octree
### Overview
- The Octree is the most basic of the implemented data structures, consisting of a tree where each node in the tree contains eight children. 
- There is no algorithm for determining the location of each of the child nodes, and each octree node separates its space equally into eight parts for each of the children nodes. The implementation for determining where each child node's bounding box is located can be found in [Octree.cpp](src/Octree.cpp), under the `childBB()` function.
- Construction of this tree is basic, where for each node, the tree checks the triangles contained inside that node to see if they intersect any of the children nodes. If an intersection is found for one (or more) of these child nodes, that triangle is then added to that child node. Any node with less than a specified amount of triangles (currently 10), is pruned and turned into a leaf node.
- Traversal is done simply, in which a ray which intersects a node then checks for intersections of all of that node's children. The intersected children are then checked in an order conforming to which child node was hit first, and continues on this until reaching a leaf node. Within leaf nodes, every triangle is trivially checked to find which is closest. 

### Usage (Nori)
- To use the Octree, the following statement should be placed within the `Accel()` constructor of the [accel.cpp](src/accel.cpp) class.
  - m_tree = new Octree();

## KD-Tree
### Overview

### Usage (Nori)
- To use the KD-Tree, one of the following statements can be placed within the `Accel()` constructor of the [accel.cpp](src/accel.cpp) class, depending on which algorithm you would like to use.
  - m_tree = new KDTree(KDTree::Midpoint); *This generates a KD-Tree using the trivial midpoint method*
  - m_tree = new KDTree(KDTree::SAHFull); *This generates a KD-Tree using the aforementioned SAH algorithm (while checking "all" possible split points)*

## BVH (Bounding Volume Heirarchy)
### Overview

### Usage (Nori)
- To use the BVH, one of the following statements can be placed within the `Accel()` constructor of the [accel.cpp](src/accel.cpp) class, depending on which algorithm you would like to use.
  - m_tree = new BVH(BVH::SAHFull); *This generates a BVH using the aforementioned SAH algorithm (while checking "all" possible partitions of triangles)*
  - m_tree = new BVH(BVH::SAHBuckets); *This generates a BVH using SAH with the addition of bucketing for determining partitions*

# Extra
## Getting Started
### Use in pre-existing projects
- All projects must use [AccelTree.h](include/nori/AccelTree.h) and [AccelTree.cpp](src/AccelTree.cpp), as other classes inherit from this. Specific data structures, such as an `Octree`, can then be included at one's choice.
  - If not using the Nori renderer, certain structs and functions may need to be altered to match the format of triangle/object or mesh storage, such as the `TriInd` struct within the `AccelTree` class.
- For use examples, see [accel.cpp](src/accel.cpp). 

### Build (using [CMake](https://cmake.org/download/))
1. Within CMake, under the `source code` location, navigate to the location of the cloned repo. Under where to `build the libraries`, select a separate folder than that of the repo (such as a new subfolder `\build`).
2. Press `Configure` and select a compiler, such as [Visual Studio](https://visualstudio.microsoft.com/downloads/).
3. Press `Finish`, and then press `Configure` and finally `Generate` as prompted.
4. Open the project and build it.
   - Using VS Community, navigate to your builds folder, and then open `nori.sln`.
   - (Optionally) Change `Debug` to `Release`, and then build the project by navigating to the `Build` menu and `Build Solution` (Ctrl+Shift+B)
   - Navigate to the `\Release` folder, and run `nori.exe`, with an `xml` file as the argument. An example would be `nori.exe ../../scenes/toy_example/bunny.xml`.

### Selecting a Data Structure
- When using the Nori codebase, selecting a data structure can be fully done within the `accel.cpp` file. Within the `Accel` constructor, you can change the currently commented data structure to be one that you choose, with the algorithm of choice. All choices are explained above within the "Data Structures" section.
- If using your own codebase, you can follow the code in `accel.cpp`, where you construct an `AccelTree` of type according to the desired data structure, add meshes to it using `addMesh()`, and build the data structure before ray tracing using `build()`. When raytracing, you need only call the `rayIntersect` method, and it will return the intersected triangle (the `rayIntersect` method can also be altered to return other data like hit point).

## Notes
- To run the ajax or stanford dragon scenes, you must insert your own `.obj` files into the folder that includes the `.xml` file (so an `ajax.obj` would go into `\scenes\ajax`)
- This project was set up using the Nori Educational renderer, and uses it as a foundation. While scenes can be rendered, I did not create any of the ray tracing algorithms themselves, merely the structures for accelerating the speed of ray-object (specifically ray-triangle) intersection across a mesh of triangles. 
- This project uses TBB to utilize paralellism inherent in creating tree-like structures. To disable the multi-threading, in each of the source files (or for whichever structure specifically), set the line `#define PARALLEL true` to `#define PARALLEL false`.

## Credits/Libraries
- The Nori Educational Ray Tracer ([Github](https://wjakob.github.io/nori/))
- Intel's TBB ([Official Site](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html#gs.kw5a3k)) ([Github](https://github.com/oneapi-src/oneTBB))
