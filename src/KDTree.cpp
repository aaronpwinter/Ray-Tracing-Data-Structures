//
// Created by Aaron Winter on 11/23/22.
//

#include "nori/KDTree.h"

#include <chrono>
#include <tbb/parallel_for.h>

//Set to true for parallel construction of KD-Tree
#define PARALLEL true
//Time W/O Parallel (set to false): ~ MS
//Time W/ Parallel (set to true):   ~ MS

NORI_NAMESPACE_BEGIN

void KDTree::build() {
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

    //Build (& time) KD-Tree
    auto startT = std::chrono::high_resolution_clock::now();
    root = build(bbox, tris, 0);
    auto endT = std::chrono::high_resolution_clock::now();
    auto durT = std::chrono::duration_cast<std::chrono::milliseconds>(endT-startT);

    //Print some information
    std::cout << "Acceleration Structure: KD-Tree" << std::endl;
    std::cout << "Nodes: " << root->nodeCount() << ", Tree Stored Tris: " << root->triCount() << ", Mesh Tris: " << triCt << std::endl;
    std::cout << "KD-Tree Construction Time: " << durT.count() << " MS" << endl;
}
KDTree::Node *KDTree::build(const BoundingBox3f& bb, std::vector<TriInd> *tris, int depth)
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
    BoundingBox3f AABBs[2];
    //& Count up triangles into vector "triangles"
    std::vector<TriInd>* triangles[2];

    Split s = getGoodSplit(bb, tris);

    AABBs[0] = lowBB(bb, s);
    triangles[0] = new std::vector<TriInd>();
    AABBs[1] = highBB(bb, s);
    triangles[1] = new std::vector<TriInd>();
#if PARALLEL
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
    tbb::parallel_for(int(0), 2,
                      [=](int i)
                      {n->children[i] = build(AABBs[i], triangles[i], depth + 1);});
#else
    for (int i = 0; i < 2; ++i)
    {
        n->children[i] = build(AABBs[i], triangles[i], depth + 1);
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

bool KDTree::triIntersects(const BoundingBox3f& bb, const TriInd& tri)
{
    return bb.overlaps(meshes[tri.mesh]->getBoundingBox(tri.i));
}


KDTree::TriInd KDTree::leafRayTriIntersect(Node *n, const nori::Ray3f &ray_, nori::Intersection &its,
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

    for (uint i = 0; i < 2; ++i)
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
    ///Whether this node in the stack actually intersects the ray
    bool intersects[MAX_DEPTH + 1];
    ///Stack index
    int si = 0;

    float close, far;
    stack[0] = n;
    intersects[0] = n->AABB.rayIntersect(ray_, close, far);
    while(si >= 0)
    {
        Node* cur = stack[si];
        float goodBB = intersects[si];
        --si;
        if(cur == nullptr || !goodBB) continue;

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
            //1. Get all the child nodes that the ray intersects
            float dists[2]{999999,999999}; //large values, not too important
            bool valid[2]{false,false};

            for (uint i = 0; i < 2; ++i)
            {
                Node* c = cur->children[i];
                if (c != nullptr)
                {
                    if(c->AABB.rayIntersect(ray_, close, far))
                    {
                        dists[i] = close;
                        valid[i] = true;
                    }
                }
            }

            //if (!valid[0]) dists[0] = dists[1] + 1;
            //if (!valid[1]) dists[1] = dists[0] + 1;

            if(dists[0] < dists[1])
            { //0 node closer (or just invalid idk)
                ++si;
                stack[si] = cur->children[1];
                intersects[si] = valid[1];
                ++si;
                stack[si] = cur->children[0];
                intersects[si] = valid[0];
            }
            else
            {
                ++si;
                stack[si] = cur->children[0];
                intersects[si] = valid[0];
                ++si;
                stack[si] = cur->children[1];
                intersects[si] = valid[1];
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
#define TRIVIAL_SPLIT true
KDTree::Split KDTree::getGoodSplit(const BoundingBox3f &bb, std::vector<TriInd> *tris) const {

    //DUMMY TRIVIAL IMPLEMENTATION
    //      SPLIT LONGEST DIMENSION IN 2

    //The size of the AABB
    Vector3f sz = bb.max-bb.min;
    dim dimension;
    if(sz.x() >= sz.y() && sz.x() >= sz.z())
    {//X is the longest dimension currently
        dimension = x;
    }
    else if (sz.y() >= sz.z())
    {//Y is the longest dimension currently
        dimension = y;
    }
    else
    {//Z is the longest dimension currently
        dimension = z;
    }
    Split triv(dimension, sz[dimension]/2);

#if TRIVIAL_SPLIT
    return triv;
#else
    Split bestS;
    float minSAH = -1;

    //The size of the AABB
    //Vector3f sz = bb.max-bb.min;
    float totTriCost = tris->size()*TRI_INT_COST;
    ///SA of the whole BB
    float bbSA = bb.getSurfaceArea();

    //Construct an array with both start and end pts
    std::vector<TriSAH> triPts(tris->size()*2);
    for(uint i = 0; i < tris->size(); ++i)
    {
        TriInd t = (*tris)[i];
        //Min pt
        triPts[i] = {t, getTriBB(t).min, true};
        //max pt
        triPts[tris->size()+i] = {t, getTriBB(t).max, false};
    }


    //Dimension loop
    for (int d = 0; d < 3; ++d) {
        //AXIS constants
        uint d2 = (d+1)%3, d3 = (d+2)%3;
        //surface area of the two axises that arent being changed
        //float pl = yzSA + getTriBB(t).min.x()*yzDist - bb.min.x()*yzDist;
        //float pl = yzSA + (getTriBB(t).min.x()-bb.min.x())*yzDist;
        //float pl = 2*(sz.y()*sz.z() + sz.y() * getTriBB(t).min.x() + sz.y()*getTriBB(t).min.x());
        //float ph = yzSA + bb.max.x()*yzDist - getTriBB(t).min.x()*yzDist;
        //float ph = yzSA + (bb.max.x()-getTriBB(t).min.x())*yzDist;
        float axSA = 2 * sz[d2] * sz[d3];
        float axDist = 2 * (sz[d2] + sz[d3]);
        float axMinConst = axSA - bb.min[d] * axDist;
        float axMaxConst = axSA + bb.max[d] * axDist;

        //Try with the min axis bounds
        std::sort(triPts.begin(), triPts.end(), [d](const TriSAH &a, const TriSAH &b) {
            return a.pt[d] >
                   b.pt[d];
        });
        float lCost = 0;
        float hCost = totTriCost;
        for (auto t: triPts) {
            if(t.min) lCost += TRI_INT_COST;

            if(bb.min[d] < t.pt[d]  && t.pt[d] < bb.max[d]) {
                ///Probability of intersecting the "lower" node
                float pl = (axMinConst + t.pt[d] * axDist);

                ///Probability of intersecting the "higher" node
                float ph = (axMaxConst - t.pt[d] * axDist);

                float sah = TRAVERSAL_TIME + (pl * lCost + ph * hCost) / bbSA;

                if (minSAH == -1 || sah < minSAH) {
                    //std::cout << "Dim " << d << ", lCost " << lCost << ", hCost " << hCost << ", totTriCost " << totTriCost << ", SAH " << sah << std::endl;
                    //std::cout << "BBSA " << bbSA << ", pl " << pl << ", ph " << ph << ", totTriCost " << totTriCost << ", SAH " << sah << std::endl;
                    minSAH = sah;
                    bestS = {d, t.pt[d]};
                }
            }

            if(!t.min) hCost -= TRI_INT_COST;
        }

    }


    /*
    //Try with the max axis bounds
    std::sort(tris->begin(), tris->end(), [this, d](const TriInd &a, const TriInd &b) {
        return getTriBB(a).max[d] >
               getTriBB(b).max[d];
    });
    curTriCost = totTriCost+triInterCost((*tris)[0]);
    for (auto t: *tris) {
        curTriCost -= triInterCost(t);
        ///Probability of intersecting the "lower" node
        float pl = (axMinConst + getTriBB(t).max[d] * axDist) / bbSA;

        ///Probability of intersecting the "higher" node
        float ph = (axMaxConst - getTriBB(t).max[d] * axDist) / bbSA;

        float sah = TRAVERSAL_TIME + pl * curTriCost + ph * (totTriCost - curTriCost);

        if (sah < minSAH) {
            minSAH = sah;
            bestS = {d, getTriBB(t).max[d]};
        }
    }
     */

    /*
    //Y AXIS Tests
    float xzSA = 2*sz.x()*sz.z();
    float xzDist = 2*(sz.x()+sz.z());
    float xzMinConst = xzSA - bb.min.y()*xzDist;
    float xzMaxConst = xzSA + bb.max.y()*xzDist;

    //Try with the y min axis bounds
    std::sort(tris->begin(), tris->end(), [this](const TriInd& a, const TriInd& b){
        return this->triCompYMin(a,b);
    });
    curTriCost = 0;//-triInterCost((*tris)[0]);
    for(auto t: *tris)
    {
        curTriCost += triInterCost(t);
        ///Probability of intersecting the "lower" node
        float pl = (xzMinConst + getTriBB(t).min.y()*xzDist)/bbSA;

        ///Probability of intersecting the "higher" node
        float ph = (xzMaxConst - getTriBB(t).min.y()*xzDist)/bbSA;

        float sah = TRAVERSAL_TIME + pl*curTriCost + ph*(totTriCost-curTriCost);

        if (sah < minSAH)
        {
            minSAH = sah;
            bestS = {y, getTriBB(t).min.y()};
        }
    }

    //Try with the y max axis bounds
    std::sort(tris->begin(), tris->end(), [this](const TriInd& a, const TriInd& b){
        return this->triCompYMax(a,b);
    });
    curTriCost = totTriCost;//+triInterCost((*tris)[0]);
    for(auto t: *tris)
    {
        curTriCost -= triInterCost(t);
        ///Probability of intersecting the "lower" node
        float pl = (xzMinConst + getTriBB(t).max.y()*xzDist)/bbSA;

        ///Probability of intersecting the "higher" node
        float ph = (xzMaxConst - getTriBB(t).max.y()*xzDist)/bbSA;

        float sah = TRAVERSAL_TIME + pl*curTriCost + ph*(totTriCost-curTriCost);

        if (sah < minSAH)
        {
            minSAH = sah;
            bestS = {y, getTriBB(t).max.y()};
        }
    }


    //Z AXIS Tests
    float xySA = 2*sz.x()*sz.y();
    float xyDist = 2*(sz.x()+sz.y());
    float xyMinConst = xySA - bb.min.z()*xyDist;
    float xyMaxConst = xySA + bb.max.z()*xyDist;

    //Try with the z min axis bounds
    std::sort(tris->begin(), tris->end(), [this](const TriInd& a, const TriInd& b){
        return this->triCompZMin(a,b);
    });
    curTriCost = 0;//-triInterCost((*tris)[0]);
    for(auto t: *tris)
    {
        curTriCost += triInterCost(t);
        ///Probability of intersecting the "lower" node
        float pl = (xyMinConst + getTriBB(t).min.z()*xyDist)/bbSA;

        ///Probability of intersecting the "higher" node
        float ph = (xyMaxConst - getTriBB(t).min.z()*xyDist)/bbSA;

        float sah = TRAVERSAL_TIME + pl*curTriCost + ph*(totTriCost-curTriCost);

        if (sah < minSAH)
        {
            minSAH = sah;
            bestS = {z, getTriBB(t).min.z()};
        }
    }

    //Try with the z max axis bounds
    std::sort(tris->begin(), tris->end(), [this](const TriInd& a, const TriInd& b){
        return this->triCompZMax(a,b);
    });
    curTriCost = totTriCost;//+triInterCost((*tris)[0]);
    for(auto t: *tris)
    {
        curTriCost -= triInterCost(t);
        ///Probability of intersecting the "lower" node
        float pl = (xyMinConst + getTriBB(t).max.z()*xyDist)/bbSA;

        ///Probability of intersecting the "higher" node
        float ph = (xyMaxConst - getTriBB(t).max.z()*xyDist)/bbSA;

        float sah = TRAVERSAL_TIME + pl*curTriCost + ph*(totTriCost-curTriCost);

        if (sah < minSAH)
        {
            minSAH = sah;
            bestS = {z, getTriBB(t).max.z()};
        }
    }
     */

    return minSAH < TRI_INT_COST*tris->size() ? bestS : triv;
#endif

}


NORI_NAMESPACE_END