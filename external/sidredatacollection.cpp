// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.


#include "../config/config.hpp"

#ifdef MFEM_USE_SIDRE

#include "sidredatacollection.hpp"


#include "../fem/fem.hpp"

#include <string>


#include "sidre/sidre.hpp"
#ifdef MFEM_USE_MPI
  #include "spio/IOManager.hpp"
#endif

namespace mfem
{

// class SidreDataCollection implementation
// This version is a prototype of adding needed MFEM data to Sidre as mostly 'external' data.
// There are some exceptions - individual scalars are copied into Sidre, as long as we know the
// data does not change during a run.  There are some drawbacks to trying to do most data as 'external'
// to Sidre. (For future discussion)
SidreDataCollection::SidreDataCollection(const std::string& collection_name, asctoolkit::sidre::DataGroup* dg)
  : mfem::DataCollection(collection_name.c_str()), parent_datagroup(dg)
{
  namespace sidre = asctoolkit::sidre;

  sidre_dc_group = dg->createGroup( collection_name );

  /*
  // Create group for mesh
  sidre::DataGroup * mesh_grp = sidre_dc_group->createGroup("topology");
  addMesh(mesh_grp);

  if ( mesh->GetNE() > 0)
  {
     // Create group for mesh elements, add material, shape, and vertex indices.
     sidre::DataGroup * mesh_elements_grp = mesh_grp->createGroup("mesh_elements");
     // Get the pointer to the internal elements array in Mesh.
     addElements(mesh_elements_grp, mesh->get_element_allocator());
  }

  if ( mesh->GetNBE() > 0)
  {
     // Create group for boundary elements, add material, shape, and vertex indices.
     sidre::DataGroup * boundary_elements_grp = mesh_grp->createGroup("boundary_elements");
     // Get the pointer to the internal boundary elements array in Mesh.
     addElements(boundary_elements_grp, mesh->get_element_allocator());
  }

  // Add mesh vertices
  sidre::DataGroup * grp = mesh_grp->createGroup("coords");
  if (mesh->GetNV() > 0)
  {
     addVertices(grp);
  }

  // If a grid function is present defining higher order nodes, add that too.
  if (mesh->GetNodes() != NULL)
  {
    sidre::DataGroup * grp = mesh_grp->createGroup("nodes");
    addField(grp, mesh->GetNodes() );
  }
*/
}


void SidreDataCollection::SetupMeshBlueprint()
{
}

void SidreDataCollection::SetMesh(Mesh *new_mesh)
{
    DataCollection::SetMesh(new_mesh);

    // uses conduit's mesh blueprint

    namespace sidre = asctoolkit::sidre;
    sidre::DataGroup* grp = sidre_dc_group;

    sidre::DataView *elements_connectivity;
    sidre::DataView *material_attribute_values;
    sidre::DataView *coordset_values;


    bool isRestart = grp->hasGroup("topology");
    if(!isRestart)
    {
        /// Setup the mesh blueprint

        // Find the element shape
        // Note: Assumes homogeneous elements, so only check the first element

        // Note -- the mapping from Element::Type to string is based on the enum Element::Type
        //   enum Types { POINT, SEGMENT, TRIANGLE, QUADRILATERAL, TETRAHEDRON, HEXAHEDRON};
        // Note: -- the string names are from conduit's blueprint

        std::string eltTypeStr;
        switch(new_mesh->GetElement(0)->GetType())
        {
        case Element::TRIANGLE:
            eltTypeStr = "tris";
            break;
        case Element::QUADRILATERAL:
            eltTypeStr = "quads";
            break;
        case Element::TETRAHEDRON:
            eltTypeStr = "tets";
            break;
        case Element::HEXAHEDRON:
            eltTypeStr = "hexs";
            break;
        case Element::POINT:
        case Element::SEGMENT:
        default:
            eltTypeStr = "<unsupported in blueprint>";
            break;
        }

        grp->createViewString("topology/type", "unstructured");
        grp->createViewString("topology/elements/shape",eltTypeStr);   // <-- Note: this comes form the mesh
        grp->createView("topology/elements/connectivity");

        grp->createViewString("coordset/type", "explicit");
        grp->createView("coordset/x");
        //grp->createView("coordset/y");
        //grp->createView("coordset/z");

        grp->createViewString("fields/material_attribute/association", "Element");
        grp->createView("fields/material_attribute/values");

        grp->createViewScalar("state/cycle", 0);
        grp->createViewScalar("state/time", 0.);
        grp->createViewScalar("state/domain", myid);
    }


    int dim = new_mesh->Dimension();
    int num_elements = new_mesh->GetNE();
    int element_size = new_mesh->GetElement(0)->GetNVertices();
    int num_indices = num_elements * element_size;
    int num_vertices = new_mesh->GetNV();
    int coordset_len = dim * num_vertices;

    elements_connectivity = grp->getView("topology/elements/connectivity");
    material_attribute_values = grp->getView("fields/material_attribute/values");
    coordset_values = grp->getView("coordset/x");


    if (!isRestart)
    {
       elements_connectivity->allocate(sidre::INT_ID,num_indices);
       material_attribute_values->allocate(sidre::INT_ID,num_elements);
       coordset_values->allocate(sidre::DOUBLE_ID,coordset_len);
    }

    new_mesh->ChangeElementDataOwnership(
            elements_connectivity->getArray(),
            element_size * num_elements,
            material_attribute_values->getArray(),
            num_elements,
            isRestart);
    new_mesh->ChangeVertexDataOwnership(
            coordset_values->getArray(),
            dim,
            coordset_len,
            isRestart);

    // copy mesh nodes grid function

    //  When not restart, copy data from mesh to datastore
    //  In both cases, set the mesh version to point to this
    // Remove once we directly load the mesh into the datastore
    // Note: There is likely a much better way to do this

    const FiniteElementSpace* nFes = new_mesh->GetNodalFESpace();
    int sz = nFes->GetVSize();
    double* gfData = GetFieldData("nodes", sz);

    if(! isRestart)
    {
      double* meshNodeData = new_mesh->GetNodes()->GetData();
      std::memcpy(gfData, meshNodeData, sizeof(double) * sz);
    }

    new_mesh->GetNodes()->NewDataAndSize(gfData, sz);
    RegisterField( "nodes", new_mesh->GetNodes());
}

void SidreDataCollection::Load(const std::string& path, const std::string& protocol)
{
	std::cout << "Loading Sidre checkpoint: " << path << " using protocol: " << protocol << std::endl;
//	sidre_dc_group->getDataStore()->load(path, protocol, sidre_dc_group);
	sidre_dc_group->getDataStore()->load(path, protocol);
   // we have to get this again because the group pointer may have changed
   sidre_dc_group = parent_datagroup->getGroup( name );
	SetTime( sidre_dc_group->getView("state/time")->getScalar() );
	SetCycle( sidre_dc_group->getView("state/cycle")->getScalar() );
}

void SidreDataCollection::Save()
{
    namespace sidre = asctoolkit::sidre;
    sidre::DataGroup* grp = sidre_dc_group->getGroup("state");

    grp->getView("cycle")->setScalar(cycle);
    grp->getView("time")->setScalar(time);

    std::string filename, protocol;

    std::stringstream fNameSstr;

    // Note: If non-empty, prefix_path has a separator ('/') at the end
    fNameSstr << prefix_path << name;

    if(cycle >= 0)
    {
        fNameSstr << "_" << cycle;
    }
    fNameSstr << "_" << num_procs ;

    // write out in serial if non-mpi or for debug
    bool useSerial = (myid == 0);

#ifdef MFEM_USE_MPI

    ParMesh *par_mesh = dynamic_cast<ParMesh*>(mesh);
    if (par_mesh)
    {
        asctoolkit::spio::IOManager writer(par_mesh->GetComm());
        writer.write(sidre_dc_group, num_procs, fNameSstr.str(), "sidre_hdf5");
    }
    else
    {
        useSerial = true;
    }

#endif

    // write out in serial for debugging, or if MPI unavailable
    if(useSerial)
    {
        protocol = "conduit_json";
        filename = fNameSstr.str() + "_ser.json";
        sidre_dc_group->getDataStore()->save(filename, protocol);

        protocol = "sidre_hdf5";
        filename = fNameSstr.str() + "_ser.hdf5";
        sidre_dc_group->getDataStore()->save(filename, protocol);
    }
}


double* SidreDataCollection::GetFieldData(const char *field_name, int sz)
{
    // NOTE: WE only handle scalar fields right now
    //       Need to add support for vector fields as well

    namespace sidre = asctoolkit::sidre;

    sidre::DataGroup* f = sidre_dc_group->getGroup("fields");
    if( ! f->hasGroup( field_name ) )
    {
        sidre::DataGroup* grp = f->createGroup( field_name );
        grp->createViewAndAllocate("values", sidre::DOUBLE_ID, sz);
    }
    else
    {
        // Need to handle a case where the user is requesting a larger field
        sidre::DataView* valsView = f->getGroup( field_name)->getView("values");
        int valSz = valsView->getNumElements();

        if(valSz < sz)
        {
            valsView->reallocate(sz);
        }
    }

    return f->getGroup(field_name)->getView("values")->getArray();
}

double* SidreDataCollection::GetFieldData(const char *field_name, int sz, const char *base_field, int offset, int stride)
{
    namespace sidre = asctoolkit::sidre;

    // Try to access /fields/<field_name>/values
    // If not present, try to create it as a different view into /fields/<base_field>/values
    //      with the given sz, stride and offset

    sidre::DataGroup* f = sidre_dc_group->getGroup("fields");
    if( ! f->hasGroup( field_name ) )
    {
        if( f->hasGroup( base_field) && f->getGroup(base_field)->hasView( "values" ) )
        {
            sidre::DataView* baseV = f->getGroup(base_field)->getView("values");
            sidre::DataBuffer* buff = baseV->getBuffer();

            f->createGroup(field_name)->createView("values", buff )
                                      ->apply(sidre::DOUBLE_ID, sz, offset, stride);
        }
        else
        {
            return NULL;
        }
    }

    return f->getGroup(field_name)->getView("values")->getArray();
}


void SidreDataCollection::RegisterField(const char* field_name, GridFunction *gf)
{
  namespace sidre = asctoolkit::sidre;
  sidre::DataGroup* f = sidre_dc_group->getGroup("fields");

  if( gf != NULL )
  {
      if( !f->hasGroup( field_name ) )
          f->createGroup( field_name );

      sidre::DataGroup* grp = f->getGroup( field_name );

      // Set the gf data as external if it is not already in the datastore
      if(!grp->hasView("values"))
      {
          grp->createView("values", sidre::DOUBLE_ID, gf->Size())
             ->setExternalDataPtr( gf->GetData());
      }

      // Set the basis string using the gf's finite element space
      if(!grp->hasView("basis"))
      {
          grp->createViewString("basis", gf->FESpace()->FEColl()->Name());
      }
      else
      {   // overwrite the basis string
          grp->getView("basis")->setString(gf->FESpace()->FEColl()->Name() );
      }
  }

  DataCollection::RegisterField(field_name, gf);
  //addField(sidre_dc_group->createGroup(name), gf);
}

// Private helper functions
//void SidreDataCollection::addElements( asctoolkit::sidre::DataGroup * group, Array<Element *>& elements )
void SidreDataCollection::addElements( asctoolkit::sidre::DataGroup * group, ElementAllocator *a)
{
  // NOTE:
  // This is just a first pass at adding element data.
  // It is not optimal, as we are making one entry per element.
  // If MFEM can guarantee that all the elements are contiguous in memory, we could rework this
  // to be one view with a stride.
  // TODO: Look at code that creates these vertices classes and talk to MFEM team.
  // TODO: The striding strategy would only work if all elements are of the same type ( same # of indices ).
  // Veselin mentioned that tets do have an optimization where they are all allocated from a large
  // block.

  namespace sidre = asctoolkit::sidre;
  using sidre::detail::SidreTT;
  //const mfem::Array<Element*> *elements = a->get_elements();

  group->createView("number")->setScalar( a->get_count() );
  // TODO - Need a helper function to map element shape id to a string name.
  // For now put in the int value.
  // Assume all elements are same type ( for this prototype only ).
  // Add the hex's only.

  group->createView("shape")->setString("hexs");
  sidre::DataView * conn_view = group->createView("connectivity");

  // This can easily be converted to use a data buffer prepared by the datastore instead of a stl vector in mfem.
  // TODO - Discuss refactoring material attributes as a field with MFEM team.
  // the argument here cannot be const
  conn_view->setExternalDataPtr( a->get_indices() );
  conn_view-> apply( SidreTT<int>::id, a->get_indices_count() );

  sidre::DataView * attributes_view = group->createView("material_attributes");
  attributes_view->setExternalDataPtr( a->get_attributes() );
  attributes_view-> apply( SidreTT<int>::id, a->get_count() );
  // Have to build array of material_attributes, unless we want # elems entries in datastore.
  // Unless, the data structure is changed in mfem.
  /*
  sidre::DataView * attributes_view = group->createView("material_attributes", SidreTT<mfem_int_t>::id,
    elements->Size() ) -> allocate();
  mfem_int_t * attributes_ptr = attributes_view->getArray();

  for (int i = 0; i < elements->Size(); i++)
  {
    attributes_ptr[i] = (*elements)[i]->GetAttribute();
  }*/
}

void SidreDataCollection::addField(asctoolkit::sidre::DataGroup * grp, GridFunction* gf)
{
  if (gf->Size() == 0)
  {
    return;
  }

  namespace sidre = asctoolkit::sidre;

  grp->createView("type")->setString("FiniteElementSpace");
  grp->createView("name")->setString( gf->FESpace()->FEColl()->Name() );
// the grid function save goes a bit different route on the vector dim.  It grabs it from the finite element space.  Ask Rob or Tzanio about this.
// calling the frunction on the grid function looks correct.
//  grp->createView("dimension")->setScalar( gf->VectorDim() );

  sidre::DataView * ordering_view = grp->createView("ordering");

  // Add ordering
  // Ordering::byNODES - first nodes, then vector dimension,
  // Ordering::byVDIM  - first vector dimension, then nodes  */

  if ( gf->FESpace()->GetOrdering() == Ordering::byNODES )
  {
    ordering_view->setString("byNode");
  }
  else
  {
    ordering_view->setString("byVDim");
  }
  // TODO - ask if MFEM team cares about typedefs.  Could hard code SIDRE_DOUBLE_ID here instead.
  grp->createView("data")->setExternalDataPtr( const_cast<mfem_double_t*>(
      gf->GetData()) )->apply( sidre::detail::SidreTT<mfem_double_t>::id ,
      gf->Size());
}

void SidreDataCollection::addMesh(asctoolkit::sidre::DataGroup * grp)
{

    // Usa Aaron's


}

void SidreDataCollection::addVertices(asctoolkit::sidre::DataGroup * grp)
{
  // NOTE:
  // This is just a first pass at adding the data.
  // It is not optimal, as we are making one entry per vertex.
  // TODO: Rework this to be one view with a stride, if we can verify the vertices are contiguous in memory.
  namespace sidre = asctoolkit::sidre;

  //MFEM_VERIFY( mesh->Dim == 2 || mesh->Dim == 3, "Expected two or three dimensions." );

  grp->createView("type")->setString( "explicit" );

  // Create single view for all vertices.  An array of length # of vertices
  //   * size of vertex class ( 3 doubles ) is sufficient.
  // This will break if the number of vertices EVER changes in mfem - so check with Tzanio...
  // will need to rethink this for AMR
  size_t total_length = mesh->GetNV() * 3;

  // apply args are 'type, num_elements, offset, stride'
  // Note - the 'z' value might be empty if this is 2D.
//  grp->createView("xyz", mesh->vertices[0]())->apply(sidre::DOUBLE_ID, total_length );

  grp->createView("xyz", mesh->GetVertex(0) )->apply(sidre::DOUBLE_ID, total_length );

  // For restarts we really don't need these separate x, y, z arrays... but the mesh blueprint
  // would like them...
  // I think this is the first example of the blueprint wanting one thing, and restart wants
  // another.
  //grp->createView("x", mesh->vertices[0]())->apply(sidre::DOUBLE_ID, total_length, 0, 3);
  //grp->createView("y", mesh->vertices[0]())->apply(sidre::DOUBLE_ID, total_length, 1, 3);

  //if ( mesh->Dim == 3 )
  //{
  //  grp->createView("z", mesh->vertices[0]())->apply(sidre::DOUBLE_ID, total_length, 2, 3);
  //}
}

} // end namespace mfem

#endif
