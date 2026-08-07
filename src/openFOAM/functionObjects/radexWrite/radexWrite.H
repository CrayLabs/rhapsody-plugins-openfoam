/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           |
     \\/     M anipulation  |
-------------------------------------------------------------------------------
Description
    Sends OpenFOAM fields to a RaDex key-value store at write intervals.

    The key naming convention is: <fieldName>_<subdomainId>

    Supported field types:
      - volScalarField             -> tensor of shape [nCells]  (via `fields`)
      - volVectorField             -> tensor of shape [nCells, 3] (via `fields`)
      - uniformDimensionedScalarField -> single scalar (via `scalars`)

    Backend is selected at compile time:
      -DRADEX_BACKEND_DRAGON   (default) Dragon DDict
      -DRADEX_BACKEND_REDIS    SmartRedis

    Example controlDict entry:
    \verbatim
    radexFields
    {
        type            radexWrite;
        libs            (radexWrite);

        fields          (U p nut nuTilda);
        scalars         (someAverage);
    }
    \endverbatim

\*---------------------------------------------------------------------------*/

#ifndef radexWrite_H
#define radexWrite_H

#include "fvMeshFunctionObject.H"
#include "uniformDimensionedFields.H"
#include "wordList.H"

#include <memory>

namespace radex { class IClient; }

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace functionObjects
{

/*---------------------------------------------------------------------------*\
                         Class radexWrite Declaration
\*---------------------------------------------------------------------------*/

class radexWrite
:
    public fvMeshFunctionObject
{
    // Private Data

        //- Names of volFields to export as tensors
        wordList fieldNames_;

        //- Names of uniformDimensionedScalarFields to export as scalars
        wordList scalarNames_;

        //- RaDex client handle
        std::unique_ptr<radex::IClient> client_;


    // Private Member Functions

        //- Send a volScalarField to the key-value store
        bool writeScalarField(const word& fieldName, label subdomainId);

        //- Send a volVectorField to the key-value store
        bool writeVectorField(const word& fieldName, label subdomainId);

        //- Send a uniformDimensionedScalarField as a single scalar
        bool writeUniformScalar(const word& fieldName, label subdomainId);

        //- No copy construct
        radexWrite(const radexWrite&) = delete;

        //- No copy assignment
        void operator=(const radexWrite&) = delete;


public:

    //- Runtime type information
    TypeName("radexWrite");


    // Constructors

        //- Construct from Time and dictionary
        radexWrite
        (
            const word& name,
            const Time& runTime,
            const dictionary& dict
        );


    //- Destructor
    virtual ~radexWrite() = default;


    // Member Functions

        //- Read/re-read dictionary settings
        virtual bool read(const dictionary& dict);

        //- Execute (no-op; field export happens on write)
        virtual bool execute();

        //- Send all configured fields to the key-value store
        virtual bool write();

        //- Flush on simulation end
        virtual bool end();
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace functionObjects
} // End namespace Foam

#endif

// ************************************************************************* //
