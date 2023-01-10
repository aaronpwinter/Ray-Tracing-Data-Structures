# Ray Tracing Data Structures
 Data structures, such as KD-Trees and BHVs, for accelerating ray tracing.
The most relevant files for the data structure code can be found in:
- AccelTree: [AccelTree.h](include/nori/AccelTree.h), [AccelTree.cpp](src/AccelTree.cpp). This is the abstract base-class for all of the acceleration data structures.
- Octree: [Octree.h](include/nori/Octree.h), [Octree.cpp](src/Octree.cpp)
- KD-Tree: [KDTree.h](include/nori/KDTree.h), [KDTree.cpp](src/KDTree.cpp)
- BVH: [BVH.h](include/nori/BVH.h), [BVH.cpp](src/BVH.cpp)
- Accel: [accel.h](include/nori/accel.h), [accel.cpp](src/accel.cpp). This implements the data structures themselves, and is where the ray-object intersections occur.

![](/images/ajax%20compare.gif)

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
![](/images/KDVisual.png)
### Overview
- A KD-Tree is similar to an Octree in that its space is partitioned into multiple divisions, with the key difference being that each node only has two children, and the children nodes are determined by splitting the parent node by an axis-aligned plane.
- Since any node can be split on any of its three axes, x, y, or z, at any point along those axes, there are theoretically an infinite amount of split positions, so algorithms must be used to determine these split positions, whether simple or not.
- Construction of the tree is basic assuming a split position algorithm is present. It is similar to the Octree, where for each node, the node is analyzed to find a split point using some algorithm (discussed below), then that split point is used to construct the two child nodes. Triangles are then added to each child node in the same way as the Octree, also pruning in the same fashion.
- Traversal of the KD-Tree is nearly identical to the Octree, with the key difference being that there are only two children nodes to check for each node. 
  - This also allows for a simpler test to check which child node is closer to the ray, where if a ray moves in the same direction as the split axis, then the "first" node is hit first (assuming it is hit), and inversely if the ray is in the opposite direction as the split direction then the "second" node is hit first.
- The algorithms implemented for generating KD-Trees are midpoint and SAH (surface area heuristics). 
  - Midpoint is trivial, where a splitpoint is determined purely based on the parent node's bounding box. The longest axis of the bounding box is effectively cut in half, resulting in quick construction, but poorly balanced trees.
  - The SAH algorithm analyzes the relative positions of triangles and the resulting sizes of bounding boxes after the split to determine a split position. At any given split position, a value, or the "surface area heuristic," is assigned, and the minimum SAH is desired to be optimal.
    - This surface area heuristic is found by comparing the size of a child node with the number of triangles within it, where more condensely packed nodes are favored (while also accompanied by sparsely-inhabited, but large, nodes). 
    - Since *every possible* split location cannot feasibly be tested, split points are located at every triangle bound.

### Usage (Nori)
- To use the KD-Tree, one of the following statements can be placed within the `Accel()` constructor of the [accel.cpp](src/accel.cpp) class, depending on which algorithm you would like to use.
  - m_tree = new KDTree(KDTree::Midpoint); *This generates a KD-Tree using the trivial midpoint method*
  - m_tree = new KDTree(KDTree::SAHFull); *This generates a KD-Tree using the aforementioned SAH algorithm (while checking "all" possible split points)*

## BVH (Bounding Volume Heirarchy)
![](/images/BVHVisual.png)
### Overview
- A BVH differs from the above trees, being one that partitions its *objects* instead of its space. Each node consists of two children, where each child, instead of being a bounding box derived from the parent, merely contains a partition of the parent node's triangles (with bounding boxes being determined by these included triangles).
- Construction of this tree is basic assuming a partition finding algorithm is present.
  - First find a partition using some algorithm, and assign those triangles to two children nodes.
  - For each child node, construct a bounding box. (Note that these bounding boxes can exclusively be within the parent bounding box)
- While traversal of the tree is simple, similar to the above two trees, a key difference is that the algorithm cannot early terminate, as some node bounding boxes may overlap others. Traversal can still avoid exploring nodes which do not intersect a given ray, but *all* nodes that do intersect the ray *must* be traversed.
- The only algorithm implemented for finding BVH partitions is SAH:
  - The general form of the SAH algorithm is near identical to the KD-Tree, with the main difference being that SAH bounding boxes are constructed by continually merging triangle bounding boxes. The base algorithm still iterates over "all" possible partitions, being the partitions along each of the three axes.
  - Since there is a large overhead associated with constantly expanding SAH bounding boxes, in addition to the large amount of possible partitions (*3n*), bucketing has also been implemented to save on BVH construction time while possibly sacrificing tree optimality. Using bucketing, only a small amount of possible partitions are compared per node per dimension (in this case, 12), leading to a greatly decreased construction time of the tree.
    - ![](/images/BVHBuckets.png)

### Usage (Nori)
- To use the BVH, one of the following statements can be placed within the `Accel()` constructor of the [accel.cpp](src/accel.cpp) class, depending on which algorithm you would like to use.
  - m_tree = new BVH(BVH::SAHFull); *This generates a BVH using the aforementioned SAH algorithm (while checking "all" possible partitions of triangles)*
  - m_tree = new BVH(BVH::SAHBuckets); *This generates a BVH using SAH with the addition of bucketing for determining partitions*

# Runtime and Memory Comparisons
- Each of these were run on a model of an Ajax bust, which can be freely found on the Jotero forum, and uses the [ajax-normals.xml](scenes/ajax/ajax-normals.xml) file. *This will not work by default as the model is not included in this repository.*
- To compare with a brute-force rendering method (IE: checking all triangles for every ray), the following statement can be used in the `Accel()` constructor:
  - m_tree = new KDTree(KDTree::BruteForce);
  - This was not included within my data as it was too slow for the Ajax bust mentioned above.  

![](/images/full_compare.png)
- As more complex data structures are used, with algorithms to dictate their construction, the memory consumption and render time generally decreases, while construction time increases. 
- For BVH's, since they only include one reference for each triangle, these of course also have the lowest memory consumption, but at the cost of render time due to not being able to terminate early.
  - While early termination is included on the chart, boasting both fast construction and render times, these often lead to obvious artifacts in the render, especially on smaller models.

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
- All tests were run on an AMD Ryzen 7 5800X processor with 32 GB of RAM running Windows 10.

## Credits/Libraries
- The Nori Educational Ray Tracer ([Github](https://wjakob.github.io/nori/))
- Intel's TBB ([Official Site](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onetbb.html#gs.kw5a3k)) ([Github](https://github.com/oneapi-src/oneTBB))
