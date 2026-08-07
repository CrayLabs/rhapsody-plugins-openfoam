/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           |
     \\/     M anipulation  |
-------------------------------------------------------------------------------
\*---------------------------------------------------------------------------*/

#include "radexWrite.H"
#include "addToRunTimeSelectionTable.H"
#include "volFields.H"
#include "uniformDimensionedFields.H"
#include "Pstream.H"

// Select RaDex backend at compile time (defaults to Dragon DDict)
#if defined(RADEX_BACKEND_REDIS)
#   include "radex/smartredis.hpp"
#else
#   include "radex/dragon.hpp"
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace functionObjects
{

defineTypeNameAndDebug(radexWrite, 0);
addToRunTimeSelectionTable(functionObject, radexWrite, dictionary);

} // End namespace functionObjects
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

Foam::functionObjects::radexWrite::radexWrite
(
    const word& name,
    const Time& runTime,
    const dictionary& dict
)
:
    fvMeshFunctionObject(name, runTime, dict),
    fieldNames_(),
    scalarNames_()
{
    read(dict);

#if defined(RADEX_BACKEND_REDIS)
    client_ = std::make_unique<radex::redis::smartredis::Client>();
#else
    client_ = std::make_unique<radex::drg::ddict::Client>();
#endif

    Info<< type() << " " << name
        << ": initialised RaDex client on subdomain "
        << Pstream::myProcNo() << nl;
}


bool Foam::functionObjects::radexWrite::read(const dictionary& dict)
{
    if (!fvMeshFunctionObject::read(dict))
    {
        return false;
    }

    dict.readEntry("fields", fieldNames_);
    dict.readIfPresent("scalars", scalarNames_);

    Info<< type() << " " << name() << ": fields = " << fieldNames_
        << ", scalars = " << scalarNames_ << nl;

    return true;
}


bool Foam::functionObjects::radexWrite::execute()
{
    return true;
}


bool Foam::functionObjects::radexWrite::write()
{
    const label subdomainId = Pstream::myProcNo();

    for (const word& fieldName : fieldNames_)
    {
        if (mesh_.foundObject<volScalarField>(fieldName))
        {
            writeScalarField(fieldName, subdomainId);
        }
        else if (mesh_.foundObject<volVectorField>(fieldName))
        {
            writeVectorField(fieldName, subdomainId);
        }
        else
        {
            WarningInFunction
                << "Field " << fieldName
                << " not found in objectRegistry; skipping" << nl;
        }
    }

    for (const word& fieldName : scalarNames_)
    {
        if (mesh_.foundObject<uniformDimensionedScalarField>(fieldName))
        {
            writeUniformScalar(fieldName, subdomainId);
        }
        else
        {
            WarningInFunction
                << "Scalar " << fieldName
                << " not found in objectRegistry; skipping" << nl;
        }
    }

    return true;
}


bool Foam::functionObjects::radexWrite::end()
{
    return write();
}


bool Foam::functionObjects::radexWrite::writeScalarField
(
    const word& fieldName,
    label subdomainId
)
{
    const std::string key =
        std::string(fieldName) + "_" + std::to_string(subdomainId);

    const auto& sf =
        mesh_.lookupObject<volScalarField>(fieldName);
    const scalarField& iF = sf.primitiveField();
    const radex::detail::MetaInt nCells = iF.size();

    const std::vector<radex::detail::MetaInt> dims{nCells};

    client_->put_tensor<double>(key, dims.data(), dims.size(), iF.cdata(), nCells);

    return true;
}


bool Foam::functionObjects::radexWrite::writeVectorField
(
    const word& fieldName,
    label subdomainId
)
{
    const std::string key =
        std::string(fieldName) + "_" + std::to_string(subdomainId);

    const auto& vf =
        mesh_.lookupObject<volVectorField>(fieldName);
    const vectorField& iF = vf.primitiveField();
    const radex::detail::MetaInt nCells = iF.size();

    // vector is 3 contiguous doubles, so the field is already [nCells, 3]
    const std::vector<radex::detail::MetaInt> dims{nCells, 3};
    const auto* raw = reinterpret_cast<const double*>(iF.cdata());

    client_->put_tensor<double>(key, dims.data(), dims.size(), raw, nCells * 3);

    return true;
}


bool Foam::functionObjects::radexWrite::writeUniformScalar
(
    const word& fieldName,
    label subdomainId
)
{
    const std::string key =
        std::string(fieldName) + "_" + std::to_string(subdomainId);

    const auto& f =
        mesh_.lookupObject<uniformDimensionedScalarField>(fieldName);

    client_->put_scalar<double>(key, static_cast<double>(f.value()));

    return true;
}


// ************************************************************************* //
