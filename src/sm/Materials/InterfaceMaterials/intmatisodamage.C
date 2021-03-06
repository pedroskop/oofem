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

#include "intmatisodamage.h"
#include "gausspoint.h"
#include "floatmatrix.h"
#include "floatarray.h"
#include "mathfem.h"
#include "datastream.h"
#include "contextioerr.h"
#include "classfactory.h"
#include "dynamicinputrecord.h"

namespace oofem {
REGISTER_Material(IntMatIsoDamage);

IntMatIsoDamage :: IntMatIsoDamage(int n, Domain *d) : StructuralInterfaceMaterial(n, d)
{}


FloatArrayF<3>
IntMatIsoDamage :: giveEngTraction_3d(const FloatArrayF<3> &jump, GaussPoint *gp, TimeStep *tStep) const
{
    IntMatIsoDamageStatus *status = static_cast< IntMatIsoDamageStatus * >( this->giveStatus(gp) );

    double tempKappa = 0.0, omega = 0.0;

    // compute equivalent strain
    double equivJump = this->computeEquivalentJump(jump);

    // compute value of loading function 
    double f = equivJump - status->giveKappa();

    if ( f <= 0.0 ) {
        // no damage evolution
        tempKappa = status->giveKappa();
        omega     = status->giveDamage();
    } else {
        // damage evolution
        tempKappa = equivJump;
        // evaluate damage parameter
        omega = this->computeDamageParam(tempKappa);
    }

    double strength = 1.0 - min(omega, maxOmega);

    if ( semiExplicit ) {
        double oldOmega = status->giveDamage();
        strength = 1.0 - min(oldOmega, maxOmega);
    }

    //answer = {ks * jump.at(1) * strength, ks * jump.at(2) * strength, kn * jump.at(3)};
    FloatArrayF<3> answer = {kn * jump.at(1), ks * jump.at(2) * strength, ks * jump.at(3) * strength};
    // damage in tension only
    if ( jump.at(1) >= 0 ) {
        answer.at(1) *= strength;
    }

    // update gp
    status->letTempJumpBe(jump);
    status->letTempTractionBe(answer);

    status->setTempKappa(tempKappa);
    status->setTempDamage(omega);

    return answer;
}


FloatArrayF<3>
IntMatIsoDamage :: giveFirstPKTraction_3d(const FloatArrayF<3> &jump, const FloatMatrixF<3,3> &F, GaussPoint* gp, TimeStep* tStep) const
{
    IntMatIsoDamageStatus *status = static_cast< IntMatIsoDamageStatus * > ( this->giveStatus ( gp ) );
    auto answer = this->giveEngTraction_3d(jump, gp, tStep);
    status->letTempFirstPKTractionBe(answer);
    return answer;
}


FloatMatrixF<2,2>
IntMatIsoDamage :: give2dStiffnessMatrix_Eng(MatResponseMode rMode, GaussPoint *gp, TimeStep *tStep) const
{
    IntMatIsoDamageStatus *status = static_cast< IntMatIsoDamageStatus * >( this->giveStatus(gp) );

    // assemble eleastic stiffness
    auto answer = diag<2>({kn, ks});

    if ( rMode == ElasticStiffness ) {
        return answer;
    }

    const auto &jump3d = status->giveTempJump();
    FloatArray jump2d = {jump3d.at(1),jump3d.at(2)};
    double om = min(status->giveTempDamage(), maxOmega);
    double un = jump2d.at(1);

    if ( rMode == SecantStiffness ) {
        answer.at(2, 2) *= 1.0 - om;
        // damage in tension only
        if ( un >= 0 ) {
            answer.at(1, 1) *= 1.0 - om;
        }
    } else { // Tangent Stiffness
        answer.at(2, 2) *= 1.0 - om;
        // damage in tension only
        if ( un >= 0 ) {
            answer.at(1, 1) *= 1.0 - om; ///@todo this is only the secant stiffness - tangent stiffness is broken!

#if 0
            se.beProductOf(answer, jump2d);
            double omega, omega_plus;
            computeDamageParam(omega, un);
            computeDamageParam(omega_plus, un + 1.0e-8 );
            double dom = (omega_plus - omega)/ 1.0e-8;

            // d( (1-omega)*D*j ) / dj = (1-omega)D - D*j openprod domega/dj
            double fac = ft*(e0 - un)/gf;
            dom = e0*exp(fac) /(un*un + 1.0e-9) + e0*ft*exp(fac) / (gf*un + 1.0e-9);

            dom = -( -e0 / (un * un+1.0e-9) * exp( -( ft / gf ) * ( un - e0 ) ) + e0 / (un+1.0e-9) * exp( -( ft / gf ) * ( un - e0 ) ) * ( -( ft / gf ) ) );
            if ( ( om > 0. ) && ( status->giveTempKappa() > status->giveKappa() ) ) {
                answer.at(1, 2) -= se.at(1) * dom;
                answer.at(2, 2) -= se.at(2) * dom;
            }
#endif
        }
    }
    return answer;
}


FloatMatrixF<3,3>
IntMatIsoDamage :: give3dStiffnessMatrix_Eng(MatResponseMode rMode, GaussPoint *gp, TimeStep *tStep) const
{
    IntMatIsoDamageStatus *status = static_cast< IntMatIsoDamageStatus * >( this->giveStatus(gp) );

    // assemble eleastic stiffness
    auto answer = diag<3>({kn, ks, ks});

    if ( rMode == ElasticStiffness ) {
        return answer;
    }

    const auto &jump3d = status->giveTempJump();
    double om = min(status->giveTempDamage(), maxOmega);
    double un = jump3d.at(1);

    if ( rMode == SecantStiffness ) {
        // damage in tension only
        if ( un >= 0 ) {
            answer.at(1, 1) *= 1.0 - om;
        }
        // Secant stiffness
        answer.at(2, 2) *= 1.0 - om;
        answer.at(3, 3) *= 1.0 - om;
    } else {
        // damage in tension only
        if ( un >= 0 ) {
            answer.at(1, 1) *= 1.0 - om;
        }
        // Tangent Stiffness
        answer.at(2, 2) *= 1.0 - om;
        answer.at(3, 3) *= 1.0 - om;
    }
    return answer;
}


int
IntMatIsoDamage :: giveIPValue(FloatArray &answer, GaussPoint *gp, InternalStateType type, TimeStep *tStep)
{
    IntMatIsoDamageStatus *status = static_cast< IntMatIsoDamageStatus * >( this->giveStatus(gp) );
    if ( type == IST_DamageTensor ) {
        answer.resize(6);
        answer.zero();
        answer.at(1) = answer.at(2) = answer.at(3) = status->giveDamage();
        return 1;
    } else if ( type == IST_DamageTensorTemp ) {
        answer.resize(6);
        answer.zero();
        answer.at(1) = answer.at(2) = answer.at(3) = status->giveTempDamage();
        return 1;
    } else if ( type == IST_PrincipalDamageTensor ) {
        answer.resize(3);
        answer.at(1) = answer.at(2) = answer.at(3) = status->giveDamage();
        return 1;
    } else if ( type == IST_PrincipalDamageTempTensor ) {
        answer.resize(3);
        answer.at(1) = answer.at(2) = answer.at(3) = status->giveTempDamage();
        return 1;
    } else if ( type == IST_MaxEquivalentStrainLevel ) {
        answer.resize(1);
        answer.at(1) = status->giveKappa();
        return 1;
    } else {
        return StructuralInterfaceMaterial :: giveIPValue(answer, gp, type, tStep);
    }
}


void
IntMatIsoDamage :: initializeFrom(InputRecord &ir)
{
    StructuralInterfaceMaterial :: initializeFrom(ir);

    IR_GIVE_FIELD(ir, kn, _IFT_IntMatIsoDamage_kn);
    IR_GIVE_FIELD(ir, ks, _IFT_IntMatIsoDamage_ks);

    IR_GIVE_FIELD(ir, ft, _IFT_IntMatIsoDamage_ft);
    IR_GIVE_FIELD(ir, gf, _IFT_IntMatIsoDamage_gf);
    this->e0 = ft / kn;

    //Set limit on the maximum isotropic damage parameter if needed
    maxOmega = 0.999999;
    IR_GIVE_OPTIONAL_FIELD(ir, maxOmega, _IFT_IntMatIsoDamage_maxOmega);
    maxOmega = min(maxOmega, 0.999999);
    maxOmega = max(maxOmega, 0.0);
}


void
IntMatIsoDamage :: giveInputRecord(DynamicInputRecord &input)
{
    StructuralInterfaceMaterial :: giveInputRecord(input);

    input.setField(this->kn, _IFT_IntMatIsoDamage_kn);
    input.setField(this->ks, _IFT_IntMatIsoDamage_ks);

    input.setField(this->ft, _IFT_IntMatIsoDamage_ft);
    input.setField(this->gf, _IFT_IntMatIsoDamage_gf);

    input.setField(this->maxOmega, _IFT_IntMatIsoDamage_maxOmega);
}


double
IntMatIsoDamage :: computeEquivalentJump(const FloatArray &jump) const
{
    return jump.at(1);
}

double
IntMatIsoDamage :: computeDamageParam(double kappa) const
{
    if ( kappa > this->e0 ) {
        return 1.0 - ( this->e0 / kappa ) * exp( -( ft / gf ) * ( kappa - e0 ) );
    } else {
        return 0.;
    }
}


IntMatIsoDamageStatus :: IntMatIsoDamageStatus(GaussPoint *g) : StructuralInterfaceMaterialStatus(g)
{}

void
IntMatIsoDamageStatus :: printOutputAt(FILE *file, TimeStep *tStep) const
{
    StructuralInterfaceMaterialStatus :: printOutputAt(file, tStep);
    fprintf(file, "status { ");
    if ( this->damage > 0.0 ) {
        fprintf(file, "kappa %f, damage %f ", this->kappa, this->damage);
    }

    fprintf(file, "}\n");
}


void
IntMatIsoDamageStatus :: initTempStatus()
{
    StructuralInterfaceMaterialStatus :: initTempStatus();
    this->tempKappa = this->kappa;
    this->tempDamage = this->damage;
}

void
IntMatIsoDamageStatus :: updateYourself(TimeStep *tStep)
{
    StructuralInterfaceMaterialStatus :: updateYourself(tStep);
    this->kappa = this->tempKappa;
    this->damage = this->tempDamage;
}


void
IntMatIsoDamageStatus :: saveContext(DataStream &stream, ContextMode mode)
{
    StructuralInterfaceMaterialStatus :: saveContext(stream, mode);

    if ( !stream.write(kappa) ) {
        THROW_CIOERR(CIO_IOERR);
    }

    if ( !stream.write(damage) ) {
        THROW_CIOERR(CIO_IOERR);
    }
}

void
IntMatIsoDamageStatus :: restoreContext(DataStream &stream, ContextMode mode)
{
    StructuralInterfaceMaterialStatus :: restoreContext(stream, mode);

    if ( !stream.read(kappa) ) {
        THROW_CIOERR(CIO_IOERR);
    }

    if ( !stream.read(damage) ) {
        THROW_CIOERR(CIO_IOERR);
    }
}

} // end namespace oofem
