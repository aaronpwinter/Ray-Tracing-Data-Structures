//
// Created by Aaron Winter on 11/23/22.
//
#pragma once

#include <nori/mesh.h>
#include <vector>

NORI_NAMESPACE_BEGIN

class AccelTree {
public:

    /// A simple struct for storing triangle indices within the meshes vector.
    struct TriInd
    {
        /// Initializes an invalid TriInd
        TriInd(): mesh(-1), i(-1) {};

        TriInd(std::size_t meshInd, uint32_t triInd): mesh(meshInd), i(triInd) {};

        // Returns if this is a valid TriInd (IE: would be invalid if a ray didn't hit a Triangle)
        bool isValid() const
        {
            return (mesh != (std::size_t)-1) || (i != (uint32_t)-1);
        }

        /// The index of the mesh within the "meshes" vector containing this triangle.
        std::size_t mesh;
        /// The index of the triangle within the given mesh.
        uint32_t i;
    };

public:
    virtual ~AccelTree() = default;

    /**
     * \brief Register a triangle mesh for inclusion in the acceleration
     * data structure
     *
     * This function can only be used before \ref build() is called
     */
    void addMesh(Mesh *mesh);

    /// Internally builds the acceleration data structure, after all
    ///     meshes have been added.
    /// May only be called once.
    virtual void build() = 0;

    /// Return an axis-aligned box that bounds the scene
    const BoundingBox3f &getBoundingBox() const { return bbox; }

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
     * \return \c The TriInd of the intersected triangle within the mesh(es).
     *              Returns an invalid TriInd if no intersections.
     */
    virtual TriInd rayIntersect(const Ray3f &ray_, Intersection &its, bool shadowRay) const = 0;

protected:

    bool triIntersects(const BoundingBox3f& bb, const TriInd& tri);

protected:
    std::vector<Mesh*>  meshes;         ///< Meshes within the data structure
    BoundingBox3f       bbox;           ///< Bounding box of the entire scene

    bool                built = false;
};


NORI_NAMESPACE_END