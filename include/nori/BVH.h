//
// Created by Aaron Winter on 11/23/22.
//

#pragma once

#include "nori/AccelTree.h"

NORI_NAMESPACE_BEGIN

class BVH: public AccelTree
{
public:
    ///The upper bound for triangles in a node that stops the node from subdividing.
    static constexpr std::size_t FEW_TRIS = 10;
    int MAX_DEPTH = 25;

    ///The "time" to traverse a node. Used in SAH.
    static constexpr float TRAVERSAL_TIME = 1;
    static constexpr float TRI_INT_COST = 2;

    /// The number of buckets in a SAH bucket-based construction
    static constexpr std::size_t BUCKETS = 12;

public:
    enum SplitMethod{SAHFull, SAHBuckets, HLBVH};

    /// A node for the BVH, which contains 2 children, stores its own AABB,
    ///     and a vector of triangle indices.
    struct Node
    {
        Node(BoundingBox3f bb, std::vector<TriInd>* triangles, int d):
                AABB(bb), tris(triangles), dim(d)
        {
            children[0] = nullptr;
            children[1] = nullptr;
        }

        ~Node()
        {
            for (auto & c : children)
            {
                delete c;
            }
            delete tris;

        }

        bool isLeaf() const
        {
            return tris != nullptr;
        }

        uint32_t nodeCount() const
        {
            uint32_t count = 1;
            for (auto & i : children)
            {
                if (i != nullptr)
                {
                    count += i->nodeCount();
                }
            }

            return count;
        }

        uint32_t triCount() const
        {
            if(isLeaf() && tris != nullptr) return tris->size();

            uint32_t count = 0;
            for (auto & i : children)
            {
                if (i != nullptr)
                {
                    count += i->triCount();
                }
            }

            return count;
        }

        Node* children[2];
        BoundingBox3f AABB;
        //The indices of the triangles in the mesh. If nullptr this is not a leaf node
        std::vector<TriInd>* tris;

        /// The dimension of the split.
        int dim;
    };

public:
    ~BVH() override
    {
        delete root;
    }

    void build() override
    {
        build(SAHBuckets);
    }

    void build(SplitMethod method);

    TriInd rayIntersect(const Ray3f &ray_, Intersection &its, bool shadowRay) const override;

private:
    Node* build(const BoundingBox3f& bb, std::vector<TriInd>* tris, int depth,
                SplitMethod method);

    /// Searches through all the triangles in a leaf node for the closest intersection, and
    ///     returns that triangle index. Returns -1 on no intersection
    /// \param n The LEAF node to look through.
    /// \param ray The ray
    /// \param its Intersection
    /// \param shadowRay If this is a shadow ray query
    /// \return TriInd of triangle in the meshes on intersection, or -1 on none.
    TriInd leafRayTriIntersect(Node* n, Ray3f& ray_, Intersection &its, bool shadowRay) const;

    /// Searches through all the triangles in a node for the closest intersection, and
    ///     returns that triangle index. Returns -1 on no intersection
    /// \param n The node to look through.
    /// \param ray The ray
    /// \param its Intersection
    /// \param shadowRay If this is a shadow ray query
    /// \return TriInd of triangle in the meshes on intersection, or -1 on none.
    TriInd nodeCloseTriIntersect(Node* n, const Ray3f& ray, Intersection &its, bool shadowRay) const;

    /// A struct that holds all the needed data from a split
    struct SplitData
    {
        SplitData(): index((std::size_t)-1), dim(-1), bb1(), bb2(), tris1(nullptr), tris2(nullptr) {};
        SplitData(std::size_t index, int dim,
                  BoundingBox3f bb1, BoundingBox3f bb2,
                  std::vector<TriInd> *tris1, std::vector<TriInd> *tris2):
            index(index), dim(dim), bb1(bb1), bb2(bb2), tris1(tris1), tris2(tris2) {};

        std::size_t index;
        int dim;
        BoundingBox3f bb1;
        BoundingBox3f bb2;
        std::vector<TriInd> *tris1;
        std::vector<TriInd> *tris2;
    };

    /// Returns an optimal index for the list, tris. Tris is sorted over the z-axis.
    /// \param bb The AABB bounding the triangles.
    /// \param tris The triangles within the AABB. Is sorted over the z-axis.
    /// \return All the information needed after a split. See SplitData
    SplitData getGoodSplit(const BoundingBox3f &bb, std::vector<TriInd> *tris,
                           SplitMethod method) const;

    BoundingBox3f getTriBB(const TriInd& t) const{
        return meshes[t.mesh]->getBoundingBox(t.i);
    }

    /// Sort a vector of triangle indicies, triind, by their center coordinate over dimension d
    /// \param tris Triangle index vector. Will be sorted/edited in place.
    /// \param d The dimension (0 = x, 1 = y, ...)
    void sortOnDim(std::vector<TriInd> *tris, int d) const;

private:
    Node* root;

};

NORI_NAMESPACE_END