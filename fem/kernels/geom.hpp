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

#ifndef MFEM_KERNELS_GEOM
#define MFEM_KERNELS_GEOM

namespace mfem
{
namespace kernels
{
namespace fem
{

// *****************************************************************************
void Geom(const int dim,
          const int ND,
          const int NQ,
          const int NE,
          const double* __restrict G,
          const double* __restrict X,
          double* __restrict Xq,
          double* __restrict J,
          double* __restrict invJ,
          double* __restrict detJ);

} // namespace fem
} // namespace kernels
} // namespace mfem

#endif // MFEM_KERNELS_GEOM