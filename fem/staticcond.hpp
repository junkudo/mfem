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

#ifndef MFEM_STATIC_CONDENSATION
#define MFEM_STATIC_CONDENSATION

#include "../config/config.hpp"
#include "fespace.hpp"

#ifdef MFEM_USE_MPI
#include "pfespace.hpp"
#endif

namespace mfem
{

/** TODO: add description. */
class StaticCondensation
{
   FiniteElementSpace *fes, *tr_fes;
   Table elem_pdof;           // Element to private dof
   int npdofs;                // Number of private dofs
   Array<int> rdof_edof;      // Map from reduced dofs to exposed dofs

   // Schur complement: S = A_ee - A_ep (A_pp)^{-1} A_pe.
   SparseMatrix *S, *S_e;
#ifdef MFEM_USE_MPI
   ParFiniteElementSpace *tr_pfes;
   HypreParMatrix *pS, *pS_e;
   bool Parallel() const { return (tr_pfes != NULL); }
#else
   bool Parallel() const { return false; }
#endif

   bool symm; // TODO: handle the symmetric case correctly.
   Array<int> A_offsets, A_ipiv_offsets;
   double *A_data;
   int *A_ipiv;

public:
   /// Construct a StaticCondensation object.
   StaticCondensation(FiniteElementSpace *fespace,
                      FiniteElementSpace *trace_fespace);
   /// Destroy a StaticCondensation object.
   ~StaticCondensation();

   /// Return the number of vector private dofs.
   int GetNPrDofs() const { return npdofs; }
   /// Return the number of vector exposed/reduced dofs.
   int GetNExDofs() const { return tr_fes->GetVSize(); }

   /** Prepare the StaticCondensation object to assembly: allocate the Schur
       complement matrix and the other element-wise blocks. */
   void Init(bool symmetric, bool block_diagonal);

   /// Return a pointer to the reduced/trace FE space.
   FiniteElementSpace *GetTraceFESpace() { return tr_fes; }

   /** Assemble the contribution to the Schur complement from the given
       element matrix 'elmat'; save the other blocks internally: A_pp_inv, A_pe,
       and A_ep. */
   void AssembleMatrix(int el, const DenseMatrix &elmat);

   /** Assemble the contribution to the Schur complement from the given boundary
       element matrix 'elmat'. */
   void AssembleBdrMatrix(int el, const DenseMatrix &elmat);

   /// Finalize the construction of the Schur complement matrix.
   void Finalize();

   /// Eliminate the given reduced true dofs from the Schur complement matrix S.
   void EliminateReducedTrueDofs(const Array<int> &ess_rtdof_list,
                                 int keep_diagonal);

   bool HasEliminatedBC() const
   {
#ifndef MFEM_USE_MPI
      return S_e;
#else
      return S_e || pS_e;
#endif
   }

   /// Return the serial Schur complement matrix.
   SparseMatrix &GetMatrix() { return *S; }

   /// Return the eliminated part of the serial Schur complement matrix.
   SparseMatrix &GetMatrixElim() { return *S_e; }

#ifdef MFEM_USE_MPI
   /// Return the parallel Schur complement matrix.
   HypreParMatrix &GetParallelMatrix() { return *pS; }

   /// Return the eliminated part of the parallel Schur complement matrix.
   HypreParMatrix &GetParallelMatrixElim() { return *pS_e; }
#endif

   /** Given a RHS vector for the full linear system, compute the RHS for the
       reduced linear system: sc_b = b_e - A_ep A_pp_inv b_p. */
   void ReduceRHS(const Vector &b, Vector &sc_b) const;

   /** Restrict a solution vector on the full FE space dofs to a vector on the
       reduced/trace true FE space dofs. */
   void ReduceSolution(const Vector &sol, Vector &sc_sol) const;

   /** Restrict a marker Array on the true FE space dofs to a marker Array on
       the reduced/trace true FE space dofs. */
   void ConvertMarkerToReducedTrueDofs(const Array<int> &ess_tdof_marker,
                                       Array<int> &ess_rtdof_marker) const;

   /** Restrict a list of true FE space dofs to a list of reduced/trace true FE
       space dofs. */
   void ConvertListToReducedTrueDofs(const Array<int> &ess_tdof_list,
                                     Array<int> &ess_rtdof_list) const
   {
      Array<int> ess_tdof_marker, ess_rtdof_marker;
      FiniteElementSpace::ListToMarker(ess_tdof_list, fes->GetTrueVSize(),
                                       ess_tdof_marker);
      ConvertMarkerToReducedTrueDofs(ess_tdof_marker, ess_rtdof_marker);
      FiniteElementSpace::MarkerToList(ess_rtdof_marker, ess_rtdof_list);
   }

   /** Given a solution of the reduced system 'sc_sol' and the RHS 'b' for the
       full linear system, compute the solution of the full system 'sol'. */
   void ComputeSolution(const Vector &b, const Vector &sc_sol,
                        Vector &sol) const;
};

}

#endif
