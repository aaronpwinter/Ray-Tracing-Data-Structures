/*
Name: Aaron Winter
Student ID: 57665147, apwinter
*/

#pragma once

#include <nori/mesh.h>
#include <vector>

NORI_NAMESPACE_BEGIN

/**
 * \brief Acceleration data structure for ray intersection queries
 *
 * The current implementation falls back to a brute force loop
 * through the geometry.
 */
class Accel {
private:
    struct Node
    {
        Node(BoundingBox3f bb, std::vector<uint32_t>* triangles)
        {
            for (int i = 0; i < 8; ++i)
            {
                children[i] = nullptr;
            }
            AABB = bb;
            tris = triangles;
        }

        ~Node()
        {
            for (int i = 0; i < 8; ++i)
            {
                if (children[i] != nullptr)
                {
                    delete children[i];
                }
            }

            if (tris != nullptr)
            {
                delete tris;
            }
        }

        bool isLeaf()
        {
            return tris != nullptr;
        }

        uint32_t nodeCount()
        {
            uint32_t count = 1;
            for (int i = 0; i < 8; ++i)
            {
                if (children[i] != nullptr)
                {
                    count += children[i]->nodeCount();
                }
            }

            return count;
        }

        uint32_t triCount()
        {
            if(isLeaf() && tris != nullptr) return tris->size();

            uint32_t count = 0;
            for (int i = 0; i < 8; ++i)
            {
                if (children[i] != nullptr)
                {
                    count += children[i]->triCount();
                }
            }

            return count;
        }

        Node* children[8];
        BoundingBox3f AABB;
        //The indices of the triangles in the mesh. If nullptr this is not a leaf node
        std::vector<uint32_t>* tris;
    };

private:
    bool triIntersects(BoundingBox3f bb, uint32_t tri);

    Node* build(BoundingBox3f bb, std::vector<uint32_t>* tris, int depth);

    /// Returns a vector of all the children nodes to parent that a ray, &ray, intersects with.
    ///     Nodes are also sorted in close to far order.
    /// **UNUSED - Code implemented as an array directly within nodeCloseTriIntersect() for optimization.
    /// \param parent The parent node.
    /// \param ray The ray to check for intersections with.
    /// \return Sorted vector of child nodes the ray intersects. If empty, the ray did not
    ///     intersect any non-empty child nodes.
    std::vector<Node*> childNodeRayIntersections(Node* parent, const Ray3f &ray) const;

    /// Searches through all the triangles in a leaf node for the closest intersection, and
    ///     returns that triangle index. Returns -1 on no intersection
    /// \param n The LEAF node to look through.
    /// \param ray The ray
    /// \param its Intersection
    /// \param shadowRay If this is a shadow ray query
    /// \return Index of triangle in the mesh on intersection, or -1 on none.
    uint32_t leafRayTriIntersect(Node* n, const Ray3f& ray_, Intersection &its, bool shadowRay) const;

    /// Searches through all the triangles in a node for the closest intersection, and
    ///     returns that triangle index. Returns -1 on no intersection
    /// \param n The node to look through.
    /// \param ray The ray
    /// \param its Intersection
    /// \param shadowRay If this is a shadow ray query
    /// \return Index of triangle in the mesh on intersection, or -1 on none.
    uint32_t nodeCloseTriIntersect(Node* n, const Ray3f& ray_, Intersection &its, bool shadowRay) const;

public:
    ~Accel()
    {
        if(m_octtree != nullptr) delete m_octtree;
    }
public:
    /**
     * \brief Register a triangle mesh for inclusion in the acceleration
     * data structure
     *
     * This function can only be used before \ref build() is called
     */
    void addMesh(Mesh *mesh);

    /// Build the acceleration data structure (currently a no-op)
    void build();

    /// Return an axis-aligned box that bounds the scene
    const BoundingBox3f &getBoundingBox() const { return m_bbox; }

    /**
     * \brief Intersect a ray against all triangles stored in the scene and
     * return detailed intersection information
     *
     * \param ray
     *    A 3-dimensional ray data structure with minimum/maximum extent
     *    information
     *
     * \param its
     *    A detailed intersection record, which will be filled by the
     *    intersection query
     *
     * \param shadowRay
     *    \c true if this is a shadow ray query, i.e. a query that only aims to
     *    find out whether the ray is blocked or not without returning detailed
     *    intersection information.
     *
     * \return \c true if an intersection was found
     */
    bool rayIntersect(const Ray3f &ray, Intersection &its, bool shadowRay) const;

private:
    Mesh         *m_mesh = nullptr; ///< Mesh (only a single one for now)
    BoundingBox3f m_bbox;           ///< Bounding box of the entire scene
    Node         *m_octtree = nullptr;

public:
    const int MAX_DEPTH = 10;
    const long unsigned int FEW_TRIANGLES = 10;

};

NORI_NAMESPACE_END
