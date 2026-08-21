# Building the OpenFOAM Function Object

The [`radexWrite`](https://github.com/CrayLabs/rhapsody-plugins-openfoam/tree/main/src/openFOAM/functionObjects/radexWrite) function object exports OpenFOAM fields to a [RaDex](https://github.com/radical-cybertools/RaDex) key-value store at write intervals, so external Python code can read simulation data directly out of memory instead of parsing case output on disk.

## Prerequisites

- A sourced OpenFOAM installation (`wmake` on `PATH`, `$FOAM_USER_LIBBIN` writable)
- [RaDex](https://github.com/radical-cybertools/RaDex) built and installed (`$RADEX_DIR`)
- [Dragon](https://dragonhpc.github.io/dragon/doc/_build/html/index.html) libraries/includes available via `dragon-config`
- Optionally, [SmartRedis](https://github.com/CrayLabs/SmartRedis) if you plan to use the Redis backend

## Building

The [`Allwmake`](https://github.com/CrayLabs/rhapsody-plugins-openfoam/blob/main/src/openFOAM/functionObjects/Allwmake) script wires up the required environment variables and compiles the function object with `wmake`:

```bash
cd src/openFOAM/functionObjects

export RADEX_DIR=/path/to/RaDex/install
export DRAGON_LIBS=$(dragon-config -l)
export DRAGON_INCLUDES=$(dragon-config -o)
# Optional, only needed for the Redis backend
export SMARTREDIS_DIR=/path/to/SmartRedis/install

./Allwmake
```

This builds `libradexWrite` and installs it into `$FOAM_USER_LIBBIN`, creating the directory first if it doesn't already exist.

## `radexWrite` Function Object

Once built, enable the function object in a case's `controlDict`:

```cpp
radexFields
{
    type            radexWrite;
    libs            (radexWrite);

    fields          (U p nut nuTilda);
    scalars         (someAverage);
}
```

### Key Naming

Each field is written through a RaDex [`OutgoingHandle`](https://github.com/radical-cybertools/RaDex/blob/main/include/radex/handles.hpp) named:

```
<fieldName>_<timeStep>_<subdomainId>
```

or, if an `identifier` is configured:

```
<identifier>_<fieldName>_<timeStep>_<subdomainId>
```

The handle's name is the key under which the field's raw bytes are stored; RaDex derives the associated metadata key (dtype/shape) from it automatically.

### Supported Field Types

| Source | OpenFOAM type | Exported as |
| --- | --- | --- |
| `fields` | `volScalarField` | tensor of shape `[nCells]` |
| `fields` | `volVectorField` | tensor of shape `[nCells, 3]` |
| `scalars` | `uniformDimensionedScalarField` | single scalar |
| `scalars` | result of another function object (e.g. `surfaceFieldValue`) | single scalar |

On finalization, the master rank writes the final timestep once under `<identifier>_final_step` (or `final_step` if no identifier is set).

### Backend Selection

The backend is selected at runtime via the `backend` dictionary entry:

```cpp
backend   dragon;  // default
backend   redis;
```

See [`radexWrite.H`](https://github.com/CrayLabs/rhapsody-plugins-openfoam/blob/main/src/openFOAM/functionObjects/radexWrite/radexWrite.H) for the full set of supported options.
