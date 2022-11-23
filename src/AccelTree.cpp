//
// Created by Aaron Winter on 11/23/22.
//

#include <nori/AccelTree.h>


NORI_NAMESPACE_BEGIN

void AccelTree::addMesh(Mesh *mesh)
{
    if(built) return;

    meshes.push_back(mesh);

    if (bbox.isValid()) //Expand the current BBOX to fit this new mesh
    {
        bbox.expandBy(mesh->getBoundingBox());
    }
    else //Create a new BBOX (this is the first mesh)
    {
        bbox = BoundingBox3f(mesh->getBoundingBox().min, mesh->getBoundingBox().max);
    }
}

NORI_NAMESPACE_END