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
#include "functionObjectList.H"
#include "functionObjectProperties.H"
#include "profilingTrigger.H"

#include "radex/dragon.hpp"
#include "radex/smartredis.hpp"

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
    scalarNames_(),
    backend_(dict.getOrDefault<word>("backend", "dragon")),
    identifier_("")
{
    read(dict);

    if (backend_ == "redis")
    {
        client_ = std::make_unique<radex::redis::smartredis::Client>();
    }
    else
    {
        if (backend_ != "dragon")
        {
            WarningInFunction
                << "Unknown backend '" << backend_
                << "'; defaulting to dragon" << nl;
        }
        client_ = std::make_unique<radex::drg::ddict::Client>();
    }

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
    identifier_ = dict.getOrDefault<word>("identifier", "");

    Info<< type() << " " << name() << ": fields = " << fieldNames_
        << ", scalars = " << scalarNames_
        << ", identifier = " << identifier_ << nl;

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
        writeUniformScalar(fieldName, subdomainId);
    }

    return true;
}


bool Foam::functionObjects::radexWrite::end()
{
    const bool ok = write();

    if (Pstream::master())
    {
        return writeFinalStep() && ok;
    }

    return ok;
}


std::string Foam::functionObjects::radexWrite::makeKey
(
    const word& fieldName,
    label subdomainId
) const
{
    const std::string timeStep = mesh_.time().timeName();

    if (identifier_.empty())
    {
        return
            std::string(fieldName) + "_" + timeStep
          + "_" + std::to_string(subdomainId);
    }

    std::string key = std::string(identifier_) + "_" +
        std::string(fieldName)
      + "_" + timeStep + "_" + std::to_string(subdomainId);
    Info << type() << " Writing " << key << nl;
    return key;
}


bool Foam::functionObjects::radexWrite::writeScalarField
(
    const word& fieldName,
    label subdomainId
)
{
    addProfiling(radexWriteScalarField, "radexWrite::writeScalarField(", fieldName, ")");

    const std::string key = makeKey(fieldName, subdomainId);

    const scalarField* iFPtr;
    {
        addProfiling(radexWriteScalarFieldLookup, "radexWrite::lookupObject(", fieldName, ")");

        const auto& sf = mesh_.lookupObject<volScalarField>(fieldName);
        iFPtr = &sf.primitiveField();
    }

    const scalarField& iF = *iFPtr;
    const radex::detail::MetaInt dims[] = {static_cast<radex::detail::MetaInt>(iF.size())};

    {
        addProfiling(radexWriteScalarFieldPut, "radexWrite::put_tensor(", fieldName, ")");

        client_->put_tensor<double>(key, dims, 1, iF.cdata(), iF.size());
    }

    return true;
}


bool Foam::functionObjects::radexWrite::writeVectorField
(
    const word& fieldName,
    label subdomainId
)
{
    addProfiling(radexWriteVectorField, "radexWrite::writeVectorField(", fieldName, ")");

    const std::string key = makeKey(fieldName, subdomainId);

    const vectorField* iFPtr;
    {
        addProfiling(radexWriteVectorFieldLookup, "radexWrite::lookupObject(", fieldName, ")");

        const auto& vf = mesh_.lookupObject<volVectorField>(fieldName);
        iFPtr = &vf.primitiveField();
    }

    const vectorField& iF = *iFPtr;
    const radex::detail::MetaInt dims[] = {static_cast<radex::detail::MetaInt>(iF.size()), 3};
    const auto* elements = reinterpret_cast<const double*>(iF.cdata());

    {
        addProfiling(radexWriteVectorFieldPut, "radexWrite::put_tensor(", fieldName, ")");

        client_->put_tensor<double>(key, dims, 2, elements, iF.size() * 3);
    }

    return true;
}


bool Foam::functionObjects::radexWrite::writeUniformScalar
(
    const word& fieldName,
    label subdomainId
)
{
    addProfiling(radexWriteUniformScalar, "radexWrite::writeUniformScalar(", fieldName, ")");

    const std::string key = makeKey(fieldName, subdomainId);

    if (mesh_.foundObject<uniformDimensionedScalarField>(fieldName))
    {
        const auto& f =
            mesh_.lookupObject<uniformDimensionedScalarField>(fieldName);

        addProfiling(radexWriteUniformScalarPut, "radexWrite::put_scalar(", fieldName, ")");

        client_->put_scalar<double>(key, static_cast<double>(f.value()));

        return true;
    }

    // Not a registered field object - look for a result stored by another
    // function object (e.g. surfaceFieldValue), which are kept in the
    // function-object properties/results dictionary rather than the
    // objectRegistry. The entry name is auto-generated by the producing
    // function object (e.g. "areaAverage(inlet,p)"), not user-configurable.
    const auto& propsDict = mesh_.time().functionObjects().propsDict();
    const wordList entries(propsDict.objectResultEntries(fieldName));

    if (entries.empty())
    {
        WarningInFunction
            << "Scalar " << fieldName
            << " not found as a registered field or function object result"
            << "; skipping" << nl;
        return false;
    }

    if (entries.size() > 1)
    {
        WarningInFunction
            << "Function object " << fieldName << " has " << entries.size()
            << " result entries; only the first (" << entries.first()
            << ") will be exported" << nl;
    }

    scalar value;
    if (!propsDict.getObjectResult<scalar>(fieldName, entries.first(), value))
    {
        WarningInFunction
            << "Result " << entries.first() << " of function object "
            << fieldName << " is not a scalar; skipping" << nl;
        return false;
    }

    if (Pstream::master())
    {
        addProfiling(radexWriteResultScalarPut, "radexWrite::put_scalar(", fieldName, ")");

        client_->put_scalar<double>(key, static_cast<double>(value));
    }

    return true;
}


bool Foam::functionObjects::radexWrite::writeFinalStep()
{
    const std::string key =
        identifier_.empty()
      ? std::string("final_step")
      : std::string(identifier_) + "_final_step";

    client_->put_scalar<double>
    (
        key,
        static_cast<double>(mesh_.time().value())
    );

    Info<< type() << " " << key
        << ": wrote final_step from "
        << Pstream::myProcNo() << nl;

    return true;
}


// ************************************************************************* //
