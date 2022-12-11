# Ray Tracing Data Structures
 Data structures, such as Octrees and KD-Trees, for accelerating ray tracing.
The most relevant files for the data structure code can be found in:
- AccelTree: [AccelTree.h](include/nori/AccelTree.h), [AccelTree.cpp](src/AccelTree.cpp). This is the abstract base-class for all of the acceleration data structures.
- Octree: [Octree.h](include/nori/Octree.h), [Octree.cpp](src/Octree.cpp)
- KD-Tree: [KDTree.h](include/nori/KDTree.h), [KDTree.cpp](src/KDTree.cpp)
- BVH: [BVH.h](include/nori/BVH.h), [BVH.cpp](src/BVH.cpp)
- Accel: [accel.h](include/nori/accel.h), [accel.cpp](src/accel.cpp). This implements the data structures themselves, and is where the ray-object intersections occur.


# Data Structures
## Octree

## KD-Tree

## BVH

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
