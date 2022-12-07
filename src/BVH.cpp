//
// Created by Aaron Winter on 11/23/22.
//

#include "nori/BVH.h"

#include <chrono>
#include <tbb/parallel_for.h>

//Set to true for parallel construction of BVH
#define PARALLEL true
//Time W/O Parallel (set to false): ~ MS
//Time W/ Parallel (set to true):   ~ MS

NORI_NAMESPACE_BEGIN

void BVH::build() {
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

    //Build (& time) BVH
    auto startT = std::chrono::high_resolution_clock::now();
    root = build(bbox, tris, 0);
    auto endT = std::chrono::high_resolution_clock::now();
    auto durT = std::chrono::duration_cast<std::chrono::milliseconds>(endT-startT);

    //Print some information
    std::cout << "Acceleration Structure: BVH" << std::endl;
    std::cout << "Nodes: " << root->nodeCount() << ", Tree Stored Tris: " << root->triCount() << ", Mesh Tris: " << triCt << std::endl;
    std::cout << "BVH Construction Time: " << durT.count() << " MS" << endl;
}
BVH::Node *BVH::build(const nori::BoundingBox3f& bb, std::vector<TriInd> *tris, int depth)
{
    //Few triangles
    if (tris->size() <= FEW_TRIS || depth >= MAX_DEPTH)
    {
        return new Node(bb, tris, -1);
    }

    SplitData s = getGoodSplit(bb, tris);

    if( s.dim == -1 )
    { //No advantage to splitting
        //std::cout << "invalid" << std::endl;
        return new Node(bb, tris, -1);
    }


    //Set up AABBs
    BoundingBox3f AABBs[2]{s.bb1,s.bb2};
    //& Count up triangles into vector "triangles"
    std::vector<TriInd>* triangles[2];


    AABBs[0] = getTriBB( (*tris)[0] );
    triangles[0] = new std::vector<TriInd>(s.index);
    for (std::size_t i = 0; i < s.index; ++i) {
        TriInd t = (*tris)[i];
        (*triangles[0])[i] = t;
        AABBs[0].expandBy(getTriBB(t));
    }

    AABBs[1] = getTriBB( (*tris)[s.index] );
    triangles[1] = new std::vector<TriInd>(tris->size()-s.index);
    for (std::size_t i = s.index; i < tris->size(); ++i)
    {
        TriInd t = (*tris)[i];
        (*triangles[1])[i-s.index] = t;
        AABBs[1].expandBy(getTriBB( t ));
    }

    Node* n = new Node(bb, nullptr, s.dim);
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

BVH::TriInd BVH::rayIntersect(const nori::Ray3f &ray_, nori::Intersection &its, bool shadowRay) const
{
    //Use the node tri intersect function on the whole octree
    return nodeCloseTriIntersect(root, ray_, its, shadowRay);

}

BVH::TriInd BVH::leafRayTriIntersect(Node *n, nori::Ray3f &ray, nori::Intersection &its,
                                           bool shadowRay) const
{
    TriInd f = {};      // Triangle index of the closest intersection

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

BVH::TriInd BVH::nodeCloseTriIntersect(Node *n, const nori::Ray3f &ray, nori::Intersection &its,
                                             bool shadowRay) const
{
    Node* stack[MAX_DEPTH + 1];
    ///Stack index
    int si = 0;

    TriInd closeTri = {};
    Ray3f ray_(ray); /// Make a copy of the ray (we will need to update its '.maxt' value)

    float close, far;
    stack[0] = n;
    while(si >= 0)
    {
        Node* cur = stack[si];
        --si;

        if(!cur->AABB.rayIntersect(ray_, close, far)) continue;

        if (cur->isLeaf())
        { //Since this node is "first", it MUST be the closest
            TriInd inter = leafRayTriIntersect(cur, ray_, its, shadowRay);
            if(inter.isValid())
            {
                closeTri = inter;
                //return closeTri;
            }
        }
        else
        {
            ///Add the two child nodes in order
            if(ray.d[cur->dim] >= 0)
            { //0 node closer theoretically
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

    return closeTri;
}

BVH::SplitData BVH::getGoodSplit(const BoundingBox3f &bb, std::vector<TriInd> *tris) const {
    float minSAH = TRI_INT_COST*tris->size() + 1;
    int bestD = -1;
    std::vector<TriInd> bestDCopy;

    std::size_t bestI = 0;
    BoundingBox3f bestBB1, bestBB2;

    float totTriCost = tris->size()*TRI_INT_COST;
    ///SA of the whole BB
    float bbSA = bb.getSurfaceArea();

    //Dimension loop
    for (int d = 0; d < 3; ++d) {

        //Try with the min axis bounds
        sortOnDim(tris, d);

        ///All of the bounding boxes for the second node (first node can be computed on the fly)
        std::vector<BoundingBox3f> backAABBs(tris->size()-1);
        backAABBs[tris->size()-2] = getTriBB((*tris)[tris->size()-1]); //last bb should just be the single triangle
        for (int i = tris->size()-3; i >= 0; --i)
        {
            backAABBs[i] = backAABBs[i+1];
            backAABBs[i].expandBy(getTriBB((*tris)[i+1]));
        }

        BoundingBox3f curBB = getTriBB((*tris)[0]);
        float lCost = -TRI_INT_COST;
        float hCost = totTriCost+TRI_INT_COST;
        for (std::size_t i = 0; i < tris->size()-1; ++i) {

            //Update/expand the BB!
            TriInd t = (*tris)[i];
            curBB.expandBy(getTriBB(t));

            lCost += TRI_INT_COST;
            hCost -= TRI_INT_COST;

            float sah = TRAVERSAL_TIME + (curBB.getSurfaceArea() * lCost +
                                          backAABBs[i].getSurfaceArea() * hCost) / bbSA;

            if (sah <= minSAH) {
                //std::cout << "Dim " << d << ", lCost " << lCost << ", hCost " << hCost << ", totTriCost " << totTriCost << ", SAH " << sah << std::endl;
                //std::cout << "BBSA " << bbSA << ", cur " << curBB.getSurfaceArea() << ", back " << backAABBs[i].getSurfaceArea() << ", totTriCost " << totTriCost << ", SAH " << sah << std::endl;
                minSAH = sah;
                if (bestD != d)
                {
                    bestD = d;
                    bestDCopy = *tris;
                }
                bestI = i;
                bestBB1 = curBB;
                bestBB2 = backAABBs[i];
            }

        }

    }

    //Sort tris to be for the correct dimension
    //if (bestD != 2) sortOnDim(tris, bestD);

    //If the SAH isnt better than just no split, then dont split (invalid split return)
    if (minSAH < TRI_INT_COST*tris->size())
    {
        if (bestD != 2) *tris = bestDCopy;
        return {bestI+1, bestD, bestBB1, bestBB2};
    }
    else
    {
        return {(std::size_t) -1, -1, {},{}};
    }

}

void BVH::sortOnDim(std::vector<TriInd> *tris, int d) const{
    std::sort(tris->begin(), tris->end(), [d, this](const TriInd &a, const TriInd &b) {
        return this->meshes[a.mesh]->getCentroid(a.i)[d] <
                this->meshes[b.mesh]->getCentroid(b.i)[d];
    });
}

NORI_NAMESPACE_END