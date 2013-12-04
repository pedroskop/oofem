/*
 *
 *                 #####    #####   ######  ######  ###   ###
 *               ##   ##  ##   ##  ##      ##      ## ### ##
 *              ##   ##  ##   ##  ####    ####    ##  #  ##
 *             ##   ##  ##   ##  ##      ##      ##     ##
 *            ##   ##  ##   ##  ##      ##      ##     ##
 *            #####    #####   ##      ######  ##     ##
 *
 *
 *             OOFEM : Object Oriented Finite Element Code
 *
 *               Copyright (C) 1993 - 2013   Borek Patzak
 *
 *
 *
 *       Czech Technical University, Faculty of Civil Engineering,
 *   Department of Structural Mechanics, 166 29 Prague, Czech Republic
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef supg_h
#define supg_h

#include "fluidmodel.h"
#include "sparselinsystemnm.h"
#include "sparsemtrx.h"
#include "primaryfield.h"
#include "materialinterface.h"

///@name Input fields for SUPG
//@{
#define _IFT_SUPG_Name "supg"
#define _IFT_SUPG_deltat "deltat"
#define _IFT_SUPG_deltatltf "deltatltf"
#define _IFT_SUPG_cmflag "cmflag"
#define _IFT_SUPG_alpha "alpha"
#define _IFT_SUPG_scaleflag "scaleflag"
#define _IFT_SUPG_lscale "lscale"
#define _IFT_SUPG_uscale "uscale"
#define _IFT_SUPG_dscale "dscale"
#define _IFT_SUPG_miflag "miflag"
#define _IFT_SUPG_rtolv "rtolv"
#define _IFT_SUPG_atolv "atolv"
#define _IFT_SUPG_maxiter "maxiter"
#define _IFT_SUPG_stopmaxiter "stopmaxiter"
#define _IFT_SUPG_fsflag "fsflag"
//@}

namespace oofem {
/**
 * This class represents transient incompressible flow problem. Solution is based on
 * algorithm with SUPG/PSPG stabilization.
 */
class SUPG : public FluidModel
{
protected:
    /// Numerical method used to solve the problem
    SparseLinearSystemNM *nMethod;

    LinSystSolverType solverType;
    SparseMtrxType sparseMtrxType;

    SparseMtrx *lhs;
    PrimaryField *VelocityPressureField;
    //PrimaryField VelocityField;
    FloatArray accelerationVector; //, previousAccelerationVector;
    FloatArray incrementalSolutionVector;

    double deltaT;
    int deltaTLTF;
    /// Convergence tolerance.
    double atolv, rtolv;
    /// Max number of iterations.
    int maxiter;
    /**
     * Flag if set to true (default), then when max number of iteration reached, computation stops
     * otherwise computation continues with next step.
     */
    bool stopmaxiter;
    /// Integration constant.
    double alpha;

    int initFlag;
    int consistentMassFlag;

    bool equationScalingFlag;
    // length, velocity, and density scales
    double lscale, uscale, dscale;
    /// Reynolds number.
    double Re;

    // material interface representation for multicomponent flows
    MaterialInterface *materialInterface;
    // map of active dofmans for problems with free surface and only one fluid considered
    // IntArray __DofManActivityMask;
    // free surface flag -> we solve free surface problem by single reference fluid
    // int fsflag;

public:
    /*  SUPG (int i, EngngModel* _master = NULL) : FluidModel (i,_master), VelocityPressureField(this,1,FBID_VelocityPressureField, EID_MomentumBalance_ConservationEquation, 1),accelerationVector()
     */
    SUPG(int i, EngngModel *_master = NULL) : FluidModel(i, _master), accelerationVector() {
        initFlag = 1;
        lhs = NULL;
        ndomains = 1;
        nMethod = NULL;
        VelocityPressureField = NULL;
        consistentMassFlag = 0;
        equationScalingFlag = false;
        lscale = uscale = dscale = 1.0;
        materialInterface = NULL;
    }
    virtual ~SUPG() {
        if ( VelocityPressureField ) { delete VelocityPressureField; }
        if ( materialInterface ) { delete materialInterface; }
        if ( nMethod ) { delete nMethod; }
        if ( lhs ) { delete lhs; }
    }

    virtual void solveYourselfAt(TimeStep *tStep);
    virtual void updateYourself(TimeStep *tStep);

    virtual double giveUnknownComponent(ValueModeType mode, TimeStep *tStep, Domain *d, Dof *dof);
    virtual double giveReynoldsNumber();
    virtual void giveElementCharacteristicVector(FloatArray &answer, int num, CharType type, ValueModeType mode, TimeStep *tStep, Domain *domain);
    virtual void giveElementCharacteristicMatrix(FloatMatrix &answer, int num, CharType type, TimeStep *tStep, Domain *domain);

    virtual contextIOResultType saveContext(DataStream *stream, ContextMode mode, void *obj = NULL);
    virtual contextIOResultType restoreContext(DataStream *stream, ContextMode mode, void *obj = NULL);

    virtual void updateDomainLinks();

    virtual TimeStep *giveNextStep();
    virtual TimeStep *giveSolutionStepWhenIcApply();
    virtual NumericalMethod *giveNumericalMethod(MetaStep *mStep);

    virtual IRResultType initializeFrom(InputRecord *ir);

    virtual int checkConsistency();
    // identification
    virtual const char *giveClassName() const { return "SUPG"; }
    virtual const char *giveInputRecordName() const { return _IFT_SUPG_Name; }

    virtual fMode giveFormulation() { return TL; }

    virtual void printDofOutputAt(FILE *stream, Dof *iDof, TimeStep *atTime);

    virtual int requiresUnknownsDictionaryUpdate() { return renumberFlag; }

    virtual bool giveEquationScalingFlag() { return equationScalingFlag; }
    virtual double giveVariableScale(VarScaleType varId);

    virtual void updateDofUnknownsDictionary(DofManager *dman, TimeStep *tStep);
    virtual int giveUnknownDictHashIndx(ValueModeType mode, TimeStep *stepN);

    virtual MaterialInterface *giveMaterialInterface(int n) { return materialInterface; }

#ifdef __PETSC_MODULE
    virtual void initPetscContexts();
#endif


protected:
    void updateInternalState(TimeStep *tStep);
    void applyIC(TimeStep *tStep);
    void evaluateElementStabilizationCoeffs(TimeStep *tStep);
    void updateElementsForNewInterfacePosition(TimeStep *tStep);

    void updateDofUnknownsDictionary_predictor(TimeStep *tStep);
    void updateDofUnknownsDictionary_corrector(TimeStep *tStep);

    void updateSolutionVectors(FloatArray& solutionVector, FloatArray& accelerationVector, FloatArray& incrementalSolutionVector, TimeStep* tStep);
    void updateSolutionVectors_predictor(FloatArray& solutionVector, FloatArray& accelerationVector, TimeStep* tStep);

    //void initDofManActivityMap ();
    //void updateDofManActivityMap (TimeStep* tStep);
    void updateDofManVals(TimeStep *tStep);
    //void imposeAmbientPressureInOuterNodes(SparseMtrx* lhs, FloatArray* rhs, TimeStep* stepN);
    //void __debug(TimeStep* atTime);
};
} // end namespace oofem
#endif // supg_h
