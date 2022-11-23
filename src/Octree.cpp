//
// Created by Aaron Winter on 11/23/22.
//

#include "Octree.h"

#include <chrono>
#include <tbb/parallel_for.h>

//Set to true for parallel construction of Octree
#define PARALLEL true
//Time W/O Parallel (set to false): ~1100 MS
//Time W/ Parallel (set to true):   ~ 500 MS - Cool!

NORI_NAMESPACE_BEGIN

void nori::Octree::build() {
    if(built) return;
    built = true;

    //Collect all triangles
    uint triCt = 0;
    for(auto mesh: meshes)
    {
        triCt += mesh->getTriangleCount();
    }
    std::vector<TriInd>* tris = new std::vector<TriInd>(triCt);
    uint curInd = 0;
    for(uint i = 0; i < meshes.size(); ++i)
    {
        for(uint t = 0; t < meshes[i]->getTriangleCount(); ++t)
        {
            (*tris)[curInd] = TriInd(i, t);
            ++curInd;
        }
    }

    //Build (& time) Octree
    auto startOct = std::chrono::high_resolution_clock::now();
    root = build(bbox, tris, 0);
    auto endOct = std::chrono::high_resolution_clock::now();
    auto durOct = std::chrono::duration_cast<std::chrono::milliseconds>(endOct-startOct);

    //Print some information
    std::cout << "Acceleration Structure: Octree" << std::endl;
    std::cout << "Nodes: " << root->nodeCount() << ", Tree Stored Tris: " << root->triCount() << ", Mesh Tris: " << triCt << std::endl;
    std::cout << "Octree Construction Time: " << durOct.count() << " MS" << endl;
}
nori::Octree::Node *nori::Octree::build(const nori::BoundingBox3f& bb, std::vector<TriInd> *tris, int depth)
{//No Triangles
    if (tris == nullptr) return nullptr;
    if (tris->empty())
    {
        delete tris;
        return nullptr;
    }

    //Few triangles
    if (tris->size() <= FEW_TRIS || depth >= MAX_DEPTH)
    {
        return new Node(bb, tris);
    }

    //Set up AABBs
    BoundingBox3f AABBs[8];
    //& Count up triangles into vector "triangles"
    std::vector<TriInd>* triangles[8];

    for (int i = 0; i < 8; ++i)
    {
        AABBs[i] = childBB(bb, i);
        triangles[i] = new std::vector<TriInd>();
    }
#if PARALLEL
    tbb::parallel_for(int(0), 8,
                      [=](int i)
                      {
                          for (auto tri: *tris) {
                              //Check if triangle in AABB i
                              if (triIntersects(AABBs[i], tri))
                              {
                                  triangles[i]->push_back(tri);
                              }
                          }
                      });
#else
    for (auto tri: *tris)
    {
        for (int i = 0; i < 8; ++i)
        {
            //Check if triangle in AABB i
            if (triIntersects(AABBs[i], tri))
            {
                triangles[i]->push_back(tri);
            }
        }
    }
#endif

    //try to avoid situation where more nodes doesnt change anything
    //Usually only useful if going over depth ~15+, which this does not
    bool allSame = true;
    for(auto & t : triangles)
    {
        if (t->size() != tris->size())
        {
            allSame = false;
            break;
        }
    }
    if(allSame)
    {
        for(auto & t : triangles)
        {
            delete t;
        }
        return new Node(bb, tris);
    }

    Node* n = new Node(bb, nullptr);
#if PARALLEL
    tbb::parallel_for(int(0), 8,
                      [=](int i)
                      {n->children[i] = build(AABBs[i], triangles[i], depth + 1);});
#else
    for (int i = 0; i < 8; ++i)
    {
        n->children[i] = build(AABBs[i], triangles[i], depth + 1);
    }
#endif

    //we arent using this specific vector anymore, so delete it
    delete tris;

    return n;
}

Octree::TriInd Octree::rayIntersect(const nori::Ray3f &ray_, nori::Intersection &its, bool shadowRay) const
{
    //Use the node tri intersect function on the whole octree
    return nodeCloseTriIntersect(root, ray_, its, shadowRay);

}

bool nori::Octree::triIntersects(const BoundingBox3f& bb, const TriInd& tri)
{
    return bb.overlaps(meshes[tri.mesh]->getBoundingBox(tri.i));
}


Octree::TriInd Octree::leafRayTriIntersect(nori::Octree::Node *n, const nori::Ray3f &ray_, nori::Intersection &its,
                                           bool shadowRay) const
{
    //if(!n->isLeaf() || n->tris == nullptr) return {};

    TriInd f = {};      // Triangle index of the closest intersection

    Ray3f ray(ray_); /// Make a copy of the ray (we will need to update its '.maxt' value)

    /* Brute force search through all triangles */
    for (auto idx : *(n->tris)) {
        float u, v, t;
        if (meshes[idx.mesh]->rayIntersect(idx.i, ray, u, v, t)) {
            /* An intersection was found! Can terminate
               immediately if this is a shadow ray query */
            if (shadowRay)
                return idx;
            ray.maxt = its.t = t;
            its.uv = Point2f(u, v);
            its.mesh = meshes[idx.mesh];
            f = idx;
        }
    }

    return f;
}

Octree::TriInd Octree::nodeCloseTriIntersect(nori::Octree::Node *n, const nori::Ray3f &ray_, nori::Intersection &its,
                                             bool shadowRay) const
{
    if (n == nullptr) return {};
    if (n->isLeaf()) return leafRayTriIntersect(n, ray_, its, shadowRay);

    //1. Get all the child nodes that the ray intersects
    NodeComp ints[8];
    uint32_t totalInts = 0;

    for (auto c : n->children)
    {
        if (c != nullptr)
        {
            float close, far;
            if(c->AABB.rayIntersect(ray_, close, far))
            {
                ints[totalInts] = {close, c};
                totalInts++;
            }
        }
    }

    //Sort child nodes by close first
    std::sort(ints,ints+totalInts);

    //2. Search for triangles in child nodes
    //Go through valid child nodes only (Starting with closest)
    for(uint32_t i = 0; i < totalInts; ++i)
    {
        Node* c = ints[i].n;
        TriInd interPlace = nodeCloseTriIntersect(c, ray_, its, shadowRay);
        if (interPlace.isValid())
        { //Found a triangle !!! (also quick terminate)
            return interPlace;
        }
    }

    return {};
}

nori::BoundingBox3f nori::Octree::childBB(const nori::BoundingBox3f& bb, uint index) {
    /*
                    (TR/max)
       z____________
       /  6  /  7  /|
      /_____/_____/ |
    y/  2  /  3  /|7/
    /_____/_____/ |/|
    |  2  |  3  |3/5/
    |_____|_____|/|/
    |  0  |  1  |1/
    |_____|_____|/ x
    (BL/min)
    */
    Vector3f middle = bb.getCenter();
    Vector3f diff = middle - bb.min;
    Vector3f adding = Vector3f((float)(index%2) * diff.x(),(float)((index/2)%2) * diff.y(),(float)((index/4)%2) * diff.z());
    return {bb.min + adding, middle + adding};
}



NORI_NAMESPACE_END