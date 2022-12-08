//
// Created by Aaron Winter on 11/23/22.
//

#include "nori/KDTree.h"

#include <chrono>
#include <tbb/parallel_for.h>

//Set to true for parallel construction of KD-Tree
#define KD_PARALLEL true
//Time W/O Parallel (set to false): ~ MS
//Time W/ Parallel (set to true):   ~ MS

NORI_NAMESPACE_BEGIN

void KDTree::build(SplitMethod method) {
    if(built) return;
    built = true;

    //Collect all triangles
    uint32_t triCt = 0;
    for(auto mesh: meshes)
    {
        triCt += mesh->getTriangleCount();
    }
    auto tris = new std::vector<TriInd>(triCt);
    uint32_t curInd = 0;
    for(std::size_t i = 0; i < meshes.size(); ++i)
    {
        for(uint32_t t = 0; t < meshes[i]->getTriangleCount(); ++t)
        {
            (*tris)[curInd] = TriInd(i, t);
            ++curInd;
        }
    }

    //Build (& time) KD-Tree
    auto startT = std::chrono::high_resolution_clock::now();
    root = build(bbox, tris, 0, method);
    auto endT = std::chrono::high_resolution_clock::now();
    auto durT = std::chrono::duration_cast<std::chrono::milliseconds>(endT-startT);

    //Print some information
    std::cout << "Acceleration Structure: KD-Tree" << std::endl;
    std::cout << "Nodes: " << root->nodeCount() << ", Tree Stored Tris: " << root->triCount() << ", Mesh Tris: " << triCt << std::endl;
    std::cout << "KD-Tree Construction Time: " << durT.count() << " MS" << endl;
}
KDTree::Node *KDTree::build(const BoundingBox3f& bb, std::vector<TriInd> *tris, int depth,
                            SplitMethod method)
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
        return new Node(bb, tris, {});
    }

    Split s = getGoodSplit(bb, tris, method);

    if(!s.isValid())
    { //No advantage to splitting
        //std::cout << "invalid" << std::endl;
        return new Node(bb, tris, s);
    }

    //Set up AABBs
    BoundingBox3f AABBs[2];
    //& Count up triangles into vector "triangles"
    std::vector<TriInd>* triangles[2];

    AABBs[0] = lowBB(bb, s);
    triangles[0] = new std::vector<TriInd>();
    AABBs[1] = highBB(bb, s);
    triangles[1] = new std::vector<TriInd>();
#if KD_PARALLEL
    tbb::parallel_for(int(0), 2,
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
        for (int i = 0; i < 2; ++i)
        {
            //Check if triangle in AABB i
            if (triIntersects(AABBs[i], tri))
            {
                triangles[i]->push_back(tri);
            }
        }
    }
#endif

    if (method == Midpoint)
    {
        //try to avoid situation where more nodes doesnt change anything
        bool allSame = true;
        for (auto &t : triangles) {
            if (t->size() != tris->size()) {
                allSame = false;
                break;
            }
        }
        if (allSame) {
            for (auto &t : triangles) {
                delete t;
            }
            return new Node(bb, tris, {});
        }
    }

    Node* n = new Node(bb, nullptr, s);
#if KD_PARALLEL
    tbb::parallel_for(int(0), 2,
                      [=](int i)
                      {n->children[i] = build(AABBs[i], triangles[i], depth + 1, method);});
#else
    for (int i = 0; i < 2; ++i)
    {
        n->children[i] = build(AABBs[i], triangles[i], depth + 1, method);
    }
#endif

    //we arent using this specific vector anymore, so delete it
    delete tris;

    return n;
}

KDTree::TriInd KDTree::rayIntersect(const nori::Ray3f &ray_, nori::Intersection &its, bool shadowRay) const
{
    //Use the node tri intersect function on the whole octree
    return nodeCloseTriIntersect(root, ray_, its, shadowRay);

}

KDTree::TriInd KDTree::leafRayTriIntersect(Node *n, const nori::Ray3f &ray_, nori::Intersection &its,
                                           bool shadowRay) const
{
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

///Whether the KD tree should search using a recursive or iterative (stack) approach.
#define RECURSIVE_SEARCH false
KDTree::TriInd KDTree::nodeCloseTriIntersect(Node *n, const nori::Ray3f &ray_, nori::Intersection &its,
                                             bool shadowRay) const
{
#if RECURSIVE_SEARCH
    if (n == nullptr) return {};
    if (n->isLeaf()) return leafRayTriIntersect(n, ray_, its, shadowRay);

    //1. Get all the child nodes that the ray intersects
    float dists[2]{0,0};
    bool valid[2]{false,false};

    for (int i = 0; i < 2; ++i)
    {
        Node* c = n->children[i];
        if (c != nullptr)
        {
            float close, far;
            if(c->AABB.rayIntersect(ray_, close, far))
            {
                dists[i] = close;
                valid[i] = true;
            }
        }
    }

    if (!valid[0])
    {
        dists[0] = dists[1] + 1;
    }
    if (!valid[1])
    {
        dists[1] = dists[0] + 1;
    }

    //2. Search for triangles in child nodes
    //Go through valid child nodes only (Starting with closest)
    if(valid[0] && dists[0] < dists[1])
    { //"lower" node is closer
        TriInd interPlace = nodeCloseTriIntersect(n->children[0], ray_, its, shadowRay);
        if (interPlace.isValid())
        { //Found a triangle !!! (also quick terminate)
            return interPlace;
        }
        if(valid[1])
        {
            interPlace = nodeCloseTriIntersect(n->children[1], ray_, its, shadowRay);
            if (interPlace.isValid())
            { //Found a triangle !!! (also quick terminate)
                return interPlace;
            }
        }
    }
    else if (valid[1])
    { //"higher" node is closer
        TriInd interPlace = nodeCloseTriIntersect(n->children[1], ray_, its, shadowRay);
        if (interPlace.isValid())
        { //Found a triangle !!! (also quick terminate)
            return interPlace;
        }
        if(valid[0])
        {
            interPlace = nodeCloseTriIntersect(n->children[0], ray_, its, shadowRay);
            if (interPlace.isValid())
            { //Found a triangle !!! (also quick terminate)
                return interPlace;
            }
        }
    }

    return {};
#else
    Node* stack[MAX_DEPTH + 1];
    ///Stack index
    int si = 0;

    float close, far;
    stack[0] = n;
    while(si >= 0)
    {
        Node* cur = stack[si];
        --si;
        if(cur == nullptr || !cur->AABB.rayIntersect(ray_, close, far)) continue;

        if (cur->isLeaf())
        { //Since this node is "first", it MUST be the closest
            TriInd inter = leafRayTriIntersect(cur, ray_, its, shadowRay);
            if(inter.isValid())
            {
                return inter;
            }
        }
        else
        {
            if(ray_.d[cur->s.d] >= 0)
            { //0 node closer (or just invalid idk)
                ++si;
                stack[si] = cur->children[1];
                ++si;
                stack[si] = cur->children[0];
            }
            else
            {//1 node is closer
                ++si;
                stack[si] = cur->children[0];
                ++si;
                stack[si] = cur->children[1];
            }

        }

    }

    return {};
#endif
}

BoundingBox3f KDTree::lowBB(const BoundingBox3f &bb, Split s) {
    //High point
    Vector3f hp = bb.max;
    hp[s.d] = bb.min[s.d]+s.l;

    return {bb.min, hp};
}

BoundingBox3f KDTree::highBB(const BoundingBox3f &bb, Split s) {
    //Low point
    Vector3f lp = bb.min;
    lp[s.d] += s.l;

    return {lp, bb.max};
}

///Whether the SAH is implemented or not. Also allows for speed comparisons.
KDTree::Split KDTree::getGoodSplit(const BoundingBox3f &bb, std::vector<TriInd> *tris,
                                   SplitMethod method) const {
    if (method == SAHFull)
    {

        Split bestS;
        float minSAH = TRI_INT_COST*tris->size() + 1;

        //The size of the AABB
        Vector3f sz = bb.max-bb.min;
        float totTriCost = tris->size()*TRI_INT_COST;
        ///SA of the whole BB
        float bbSA = bb.getSurfaceArea();

        //Construct an array with both start and end pts
        std::vector<TriSAH> triPts(tris->size()*2);
        for(std::size_t i = 0; i < tris->size(); ++i)
        {
            TriInd t = (*tris)[i];
            //Min pt
            triPts[i] = {t, getTriBB(t).min-bb.min, true};
            //max pt
            triPts[tris->size()+i] = {t, getTriBB(t).max-bb.min, false};
        }


        //Dimension loop
        for (int d = 0; d < 3; ++d)
        {
            //AXIS constants
            int d2 = (d+1)%3, d3 = (d+2)%3;

            //Some constants for use in upcoming calculations
            //Plane ortho to axis surface area
            float axSA = 2 * sz[d2] * sz[d3];
            //basically the perimeter of above
            float axDist = 2 * (sz[d2] + sz[d3]);
            //A constant for the higher box
            float axMaxConst = axSA + sz[d] * axDist;

            //Try with the min axis bounds
            std::sort(triPts.begin(), triPts.end(), [d](const TriSAH &a, const TriSAH &b) {
                return a.pt[d] <
                       b.pt[d];
            });
            float lCost = 0;
            float hCost = totTriCost;
            for (auto t: triPts) {
                if(!t.min) hCost -= TRI_INT_COST;

                if(0 < t.pt[d]  && t.pt[d] < sz[d]) {
                    ///Probability of intersecting the "lower" node
                    float pl = axSA + t.pt[d] * axDist;

                    ///Probability of intersecting the "higher" node
                    float ph = axMaxConst - t.pt[d] * axDist;

                    float sah = TRAVERSAL_TIME + (pl * lCost + ph * hCost) / bbSA;
                    if (lCost == 0 || hCost == 0)
                        sah *= EMPTY_MODIFIER;

                    if (sah <= minSAH) {
                        //std::cout << "Dim " << d << ", lCost " << lCost << ", hCost " << hCost << ", totTriCost " << totTriCost << ", SAH " << sah << std::endl;
                        //std::cout << "BBSA " << bbSA << ", pt " << t.pt[d] << ", pl " << pl << ", ph " << ph << ", totTriCost " << totTriCost << ", SAH " << sah << std::endl;
                        minSAH = sah;
                        bestS = {d, t.pt[d]};
                    }
                }

                if(t.min) lCost += TRI_INT_COST;

            }

        }

        //If the SAH isnt better than just no split, then dont split (invalid split return)
        return minSAH < TRI_INT_COST*tris->size() ? bestS : Split{x, -1};
    }

    else if (method == Midpoint)
    {
        //DUMMY TRIVIAL IMPLEMENTATION
        //      SPLIT LONGEST DIMENSION IN 2

        //The size of the AABB
        Vector3f sz = bb.max - bb.min;
        int dimension = bb.getMajorAxis();

        return {dimension, sz[dimension] / 2};
    }

    else if (method == BruteForce)
    {
        return {};
    }

    else
    {
        std::cout << "Split method " << method << " not yet implemented!" << std::endl;
        return {};
    }

}


NORI_NAMESPACE_END