/*
Name: Aaron Winter
Student ID: 57665147, apwinter
*/

#include <nori/accel.h>
#include <Eigen/Geometry>
#include <numeric>
#include <chrono>
#include <tbb/parallel_for.h>

#define PARALLEL true
//Time W/O Parallel (set to false): ~1100 MS
//Time W/ Parallel (set to true):   ~ 500 MS - Cool!


NORI_NAMESPACE_BEGIN

void Accel::addMesh(Mesh *mesh) {
    if (m_mesh)
        throw NoriException("Accel: only a single mesh is supported!");
    m_mesh = mesh;
    m_bbox = m_mesh->getBoundingBox();
}

void Accel::build() {
    //Collect all triangles
    std::vector<uint32_t>* tris = new std::vector<uint32_t>(m_mesh->getTriangleCount());
    std::iota(tris->begin(), tris->end(), 0);

    //Build (& time) octtree
    auto startOct = std::chrono::high_resolution_clock::now();
    m_octtree = build(m_bbox, tris, 0);
    auto endOct = std::chrono::high_resolution_clock::now();
    auto durOct = std::chrono::duration_cast<std::chrono::milliseconds>(endOct-startOct);

    //Print some information
    std::cout << "Nodes: " << m_octtree->nodeCount() << ", Tree Stored Tris: " << m_octtree->triCount() << ", Mesh Tris: " << m_mesh->getTriangleCount() << std::endl;
    std::cout << "Octtree Construction Time: " << durOct.count() << " MS" << endl;
}

bool Accel::rayIntersect(const Ray3f &ray_, Intersection &its, bool shadowRay) const {

    //Use the node tri intersect function on the octtree
    uint32_t inter = nodeCloseTriIntersect(m_octtree, ray_, its, shadowRay);
    uint32_t f = (uint32_t) inter;
    bool foundIntersection = inter != (uint32_t) -1;

    if (foundIntersection && shadowRay) return true;

    if (foundIntersection) {
        /* At this point, we now know that there is an intersection,
           and we know the triangle index of the closest such intersection.

           The following computes a number of additional properties which
           characterize the intersection (normals, texture coordinates, etc..)
        */

        /* Find the barycentric coordinates */
        Vector3f bary;
        bary << 1-its.uv.sum(), its.uv;

        /* References to all relevant mesh buffers */
        const Mesh *mesh   = its.mesh;
        const MatrixXf &V  = mesh->getVertexPositions();
        const MatrixXf &N  = mesh->getVertexNormals();
        const MatrixXf &UV = mesh->getVertexTexCoords();
        const MatrixXu &F  = mesh->getIndices();

        /* Vertex indices of the triangle */
        uint32_t idx0 = F(0, f), idx1 = F(1, f), idx2 = F(2, f);

        Point3f p0 = V.col(idx0), p1 = V.col(idx1), p2 = V.col(idx2);

        /* Compute the intersection positon accurately
           using barycentric coordinates */
        its.p = bary.x() * p0 + bary.y() * p1 + bary.z() * p2;

        /* Compute proper texture coordinates if provided by the mesh */
        if (UV.size() > 0)
            its.uv = bary.x() * UV.col(idx0) +
                bary.y() * UV.col(idx1) +
                bary.z() * UV.col(idx2);

        /* Compute the geometry frame */
        its.geoFrame = Frame((p1-p0).cross(p2-p0).normalized());

        if (N.size() > 0) {
            /* Compute the shading frame. Note that for simplicity,
               the current implementation doesn't attempt to provide
               tangents that are continuous across the surface. That
               means that this code will need to be modified to be able
               use anisotropic BRDFs, which need tangent continuity */

            its.shFrame = Frame(
                (bary.x() * N.col(idx0) +
                 bary.y() * N.col(idx1) +
                 bary.z() * N.col(idx2)).normalized());
        } else {
            its.shFrame = its.geoFrame;
        }
    }

    return foundIntersection;
}

bool Accel::triIntersects(BoundingBox3f bb, uint32_t tri)
{
    return bb.overlaps(m_mesh->getBoundingBox(tri));
}

Accel::Node* Accel::build(BoundingBox3f bb, std::vector<uint32_t>* tris, int depth) {
    //No Triangles
    if (tris == nullptr) return nullptr;
    if (tris->size() == 0)
    {
        delete tris;
        return nullptr;
    }

    //Few triangles
    if (tris->size() <= FEW_TRIANGLES || depth >= MAX_DEPTH)
    {
        return new Node(bb, tris);
    }

    //Set up AABBs
    /*
                    (TR/max)
       z____________
       /  6  /  7  /|
      /_____/_____/ |
    y/  4  /  5  /|7/
    /_____/_____/ |/|
    |  4  |  5  |5/3/
    |_____|_____|/|/
    |  0  |  1  |1/
    |_____|_____|/ x
    (BL/min)
    */
    BoundingBox3f AABBs[8];
    Vector3f middle = bb.getCenter();
    Vector3f diff = middle - bb.min;
    Vector3f adding = Vector3f(0,0,0);
    AABBs[0] = BoundingBox3f(bb.min, middle);
    adding = Vector3f(diff.x(), 0, 0);
    AABBs[1] = BoundingBox3f(bb.min + adding, middle + adding);
    adding = Vector3f(0, 0, diff.z());
    AABBs[2] = BoundingBox3f(bb.min + adding, middle + adding);
    adding = Vector3f(diff.x(), 0, diff.z());
    AABBs[3] = BoundingBox3f(bb.min + adding, middle + adding);
    adding = Vector3f(0, diff.y(), 0);
    AABBs[4] = BoundingBox3f(bb.min + adding, middle + adding);
    adding = Vector3f(diff.x(), diff.y(), 0);
    AABBs[5] = BoundingBox3f(bb.min + adding, middle + adding);
    adding = Vector3f(0, diff.y(), diff.z());
    AABBs[6] = BoundingBox3f(bb.min + adding, middle + adding);
    adding = Vector3f(diff.x(), diff.y(), diff.z());
    AABBs[7] = BoundingBox3f(bb.min + adding, middle + adding);


    //Count up triangles into vector "triangles"
    std::vector<uint32_t>* triangles[8];
    for (int i = 0; i < 8; ++i)
    {
        triangles[i] = new std::vector<uint32_t>();
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
    for(int i = 0; i < 8; ++i)
    {
        if (triangles[i]->size() != tris->size())
        {
            allSame = false;
            break;
        }
    }
    if(allSame)
    {
        for(int i = 0; i < 8; ++i)
        {
            delete triangles[i];
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


std::vector<Accel::Node*> Accel::childNodeRayIntersections(Node* parent, const Ray3f &ray) const {
    std::vector<std::pair<float, Node*>> ints;
    std::vector<Node*> returning;

    if (parent == nullptr)
    {
        return returning;
    }

    for (int i = 0; i < 8; ++i)
    {
        Node* n = parent->children[i];
        if (n != nullptr)
        {
            float close, far;
            if(n->AABB.rayIntersect(ray, close, far))
            {
                ints.push_back(std::pair<float, Node*>(close, n));
            }
        }
    }

    std::sort(ints.begin(),ints.end());

    for (uint32_t i = 0; i < ints.size(); ++i)
    {
        returning.push_back(ints[i].second);
    }

    return returning;
}


uint32_t Accel::leafRayTriIntersect(Node* n, const Ray3f& ray_, Intersection &its, bool shadowRay) const {
    if(!n->isLeaf() || n->tris == nullptr) return (uint32_t) -1;

    uint32_t f = (uint32_t) -1;      // Triangle index of the closest intersection

    Ray3f ray(ray_); /// Make a copy of the ray (we will need to update its '.maxt' value)

    /* Brute force search through all triangles */
    for (auto idx : *(n->tris)) {
        float u, v, t;
        if (m_mesh->rayIntersect(idx, ray, u, v, t)) {
            /* An intersection was found! Can terminate
               immediately if this is a shadow ray query */
            if (shadowRay)
                return idx;
            ray.maxt = its.t = t;
            its.uv = Point2f(u, v);
            its.mesh = m_mesh;
            f = idx;
        }
    }

    return f;
}


uint32_t Accel::nodeCloseTriIntersect(Node* n, const Ray3f& ray_, Intersection &its, bool shadowRay) const {
    if (n == nullptr) return (uint32_t) -1;
    if (n->isLeaf()) return leafRayTriIntersect(n, ray_, its, shadowRay);

    //1. Get all the child nodes that the ray intersects
    std::pair<float, Node*> ints[8];
    uint32_t totalInts = 0;

    for (int i = 0; i < 8; ++i)
    {
        Node* c = n->children[i];
        if (c != nullptr)
        {
            float close, far;
            if(c->AABB.rayIntersect(ray_, close, far))
            {
                ints[totalInts] = std::pair<float, Node*>(close, c);
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
        Node* c = ints[i].second;
        uint32_t interPlace = nodeCloseTriIntersect(c, ray_, its, shadowRay);
        if (interPlace != (uint32_t) -1)
        { //Found a triangle !!! (also quick terminate)
            return interPlace;
        }
    }

    return (uint32_t) -1;
}

NORI_NAMESPACE_END

