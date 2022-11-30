//
// Created by Aaron Winter on 11/23/22.
//

#pragma once

///The upper bound for triangles in a node that stops the node from subdividing.
#define FEW_TRIS 10
#define MAX_DEPTH 100

///The "time" to traverse a node. Used in SAH.
#define TRAVERSAL_TIME 1
#define TRI_INT_COST 2


#include "nori/AccelTree.h"

NORI_NAMESPACE_BEGIN

class KDTree: public AccelTree
{
public:
    enum dim {x=0, y=1, z=2};

    ///A simple struct containing a dim and float, used to represent
    /// the split location in a tree node.
    struct Split
    {
        Split(): d(x), l(0){};
        Split(dim di, float dist): d(di), l(dist){};
        Split(int di, float dist): d((di == 0)?x:((di==1)?y:z)), l(dist){};

        ///The split axis
        dim d;
        ///Location along the split axis
        float l;
    };

    /// A node for the Octree, which contains 8 children, stores its own AABB,
    ///     and a vector of triangle indices.
    struct Node
    {
        Node(BoundingBox3f bb, std::vector<TriInd>* triangles):
            AABB(bb), tris(triangles)
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

        ///The split location for this KD Node.
        Split s;
    };

    struct TriSAH
    {
        TriInd t;
        Vector3f pt;
        ///Is this the min point? (or the max point)
        bool min;
    };

public:
    ~KDTree() override
    {
        delete root;
    }

    void build() override;

    TriInd rayIntersect(const Ray3f &ray_, Intersection &its, bool shadowRay) const override;


    /// Takes a bounding box, and returns the lower bounding box in the KD Split
    /// \param bb The original AABB
    /// \param s The location of the split in bb
    /// \return The AABB created by splitting bb and taking the half that uses the minpoint.
    static BoundingBox3f lowBB(const BoundingBox3f& bb, Split s);

    /// Takes a bounding box, and returns the high bounding box in the KD Split
    /// \param bb The original AABB
    /// \param s The location of the split in bb
    /// \return The AABB created by splitting bb and taking the half that uses the maxpoint.
    static BoundingBox3f highBB(const BoundingBox3f& bb, Split s);


private:
    Node* build(const BoundingBox3f& bb, std::vector<TriInd>* tris, int depth);

    bool triIntersects(const BoundingBox3f& bb, const TriInd& tri);

    /// Searches through all the triangles in a leaf node for the closest intersection, and
    ///     returns that triangle index. Returns -1 on no intersection
    /// \param n The LEAF node to look through.
    /// \param ray The ray
    /// \param its Intersection
    /// \param shadowRay If this is a shadow ray query
    /// \return TriInd of triangle in the meshes on intersection, or -1 on none.
    TriInd leafRayTriIntersect(Node* n, const Ray3f& ray_, Intersection &its, bool shadowRay) const;

    /// Searches through all the triangles in a node for the closest intersection, and
    ///     returns that triangle index. Returns -1 on no intersection
    /// \param n The node to look through.
    /// \param ray The ray
    /// \param its Intersection
    /// \param shadowRay If this is a shadow ray query
    /// \return TriInd of triangle in the meshes on intersection, or -1 on none.
    TriInd nodeCloseTriIntersect(Node* n, const Ray3f& ray_, Intersection &its, bool shadowRay) const;

    /// Returns an optimal split for the current AABB and the tris within said AABB.
    /// \param bb The AABB bounding the triangles.
    /// \param tris The triangles within the AABB
    /// \return A split representing where to split the current node.
    Split getGoodSplit(const BoundingBox3f &bb, std::vector<TriInd> *tris) const;

    BoundingBox3f getTriBB(const TriInd& t) const{
        return meshes[t.mesh]->getBoundingBox(t.i);
    }

    /*
    //Trivial comp functions for sorting
    bool triCompXMin(const TriInd& t1, const TriInd&  t2) const {
        return getTriBB(t1).min.x() <
                getTriBB(t2).min.x();
    }
    bool triCompXMax(const TriInd&  t1, const TriInd&  t2) const {
        return getTriBB(t1).max.x() >
                getTriBB(t2).max.x();
    }
    bool triCompYMin(const TriInd&  t1, const TriInd&  t2) const {
        return getTriBB(t1).min.y() <
                getTriBB(t2).min.y();
    }
    bool triCompYMax(const TriInd&  t1, const TriInd&  t2) const {
        return getTriBB(t1).max.y() >
                getTriBB(t2).max.y();
    }
    bool triCompZMin(const TriInd&  t1, const TriInd&  t2) const {
        return getTriBB(t1).min.z() <
                getTriBB(t2).min.z();
    }
    bool triCompZMax(const TriInd&  t1, const TriInd&  t2) const {
        return getTriBB(t1).max.z() >
                getTriBB(t2).max.z();
    }
     */

private:
    Node* root;

};

NORI_NAMESPACE_END